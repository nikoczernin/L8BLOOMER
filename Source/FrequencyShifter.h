#pragma once
#include <algorithm>
#include <array>
#include <cmath>

namespace bloom
{
// Causal single-sideband shifter. No block buffering or lookahead. Allpass
// sections have frequency-dependent phase/group delay, like ordinary IIR EQs.
class FrequencyShifter
{
public:
    void prepare(double sr)
    {
        sampleRate = sr; phase = 0.0; lane = 0;
        branchA = {}; branchB = {}; previousA = {}; guard = {};
        guardFrequency = -1; guardRemaining = 0;
        setEndpoints(0, 0, true);
    }
    void setEndpoints(double bottom, double top, bool immediate = false)
    {
        const double maximumUp = std::clamp(std::max({0.0, bottom, top}), 0.0, maxShift());
        // Leave a transition band before frequencies that would wrap at Nyquist.
        const double hz = 0.8 * (sampleRate * 0.5 - maximumUp);
        if (!immediate && std::abs(hz - guardFrequency) < 0.0001) return;
        guardFrequency = hz;
        guardTarget = std::tan(pi * hz / sampleRate);
        if (immediate) { guardG = guardTarget; guardRemaining = 0; }
        else
        {
            guardRemaining = std::max(1, static_cast<int>(sampleRate * 0.02));
            guardStep = (guardTarget - guardG) / guardRemaining;
        }
        updateGuard();
    }
    void beginSample(double shiftHz)
    {
        if (guardRemaining > 0)
        {
            guardG += guardStep;
            if (--guardRemaining == 0) guardG = guardTarget;
            updateGuard();
        }
        cosine = std::cos(phase); sine = std::sin(phase);
        phase += 2.0 * pi * std::clamp(shiftHz, -maxShift(), maxShift()) / sampleRate;
        if (phase >= 2.0 * pi) phase -= 2.0 * pi;
        if (phase < 0.0) phase += 2.0 * pi;
        lane ^= 1;
    }
    double process(double input, int channel)
    {
        const auto c = static_cast<size_t>(channel);
        for (auto& section : guard) input = section.process(input, c);
        double a = input, b = input;
        for (size_t i = 0; i < aCoefficients.size(); ++i)
        {
            a = branchA[i].process(a, aCoefficients[i], c, lane);
            b = branchB[i].process(b, bCoefficients[i], c, lane);
        }
        const double quadrature = previousA[c]; previousA[c] = a;
        return b * cosine - quadrature * sine;
    }
    double maxShift() const { return std::min(5000.0, sampleRate * 0.25); }

private:
    struct Allpass
    {
        std::array<std::array<double, 2>, 2> x {}, y {};
        double process(double input, double p, size_t channel, size_t index)
        {
            const double result = p * (input + y[channel][index]) - x[channel][index];
            x[channel][index] = input;
            y[channel][index] = std::abs(result) < 1e-30 ? 0.0 : result;
            return result;
        }
    };
    struct LowPass
    {
        std::array<double, 2> s1 {}, s2 {};
        double a1 = 1, a2 = 0, a3 = 0;
        void set(double g, double q)
        {
            a1 = 1.0 / (1.0 + g * (g + 1.0 / q)); a2 = g * a1; a3 = g * a2;
        }
        double process(double x, size_t c)
        {
            const double v3 = x - s2[c], v1 = a1 * s1[c] + a2 * v3;
            const double v2 = s2[c] + a2 * s1[c] + a3 * v3;
            s1[c] = 2.0 * v1 - s1[c]; s2[c] = 2.0 * v2 - s2[c];
            if (std::abs(s1[c]) < 1e-30) s1[c] = 0;
            if (std::abs(s2[c]) < 1e-30) s2[c] = 0;
            return v2;
        }
    };
    void updateGuard()
    {
        // 12th-order Butterworth anti-alias guard. Coefficients only change when
        // the endpoint controls change, not during each envelope cycle.
        constexpr std::array<double, 6> q {0.5043144802900764, 0.541196100146197,
            0.6302362070051323, 0.8213398158522908, 1.306562964876377, 3.830648787770194};
        for (size_t i = 0; i < guard.size(); ++i) guard[i].set(guardG, q[i]);
    }
    static constexpr double pi = 3.14159265358979323846;
    // Original optimised coefficients; reproduce with scripts/design/hilbert.py.
    // Topology reference: Olli Niemitalo's 90-degree IIR allpass pair.
    static constexpr std::array<double, 10> aCoefficients {
        0.1940006065270036, 0.54556478359364324, 0.79087190972980692, 0.91201550347327587,
        0.96423018130454907, 0.98559956156337758, 0.99419437799254851, 0.99766016248644163,
        0.99911767423778253, 0.99985244685400931};
    static constexpr std::array<double, 10> bCoefficients {
        0.053650104018270868, 0.37316112626160558, 0.68654565417257274, 0.86343931082602043,
        0.94377064771382424, 0.97729687164357204, 0.99086143174126728, 0.99630960578799332,
        0.99853459650972698, 0.99952968833308531};
    std::array<Allpass, 10> branchA, branchB;
    std::array<double, 2> previousA {};
    std::array<LowPass, 6> guard;
    double sampleRate = 44100, phase = 0, cosine = 1, sine = 0;
    double guardG = 0, guardTarget = 0, guardStep = 0, guardFrequency = -1;
    int guardRemaining = 0;
    size_t lane = 0;
};
}
