#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "FrequencyShifter.h"

namespace bloom
{
enum class Mode { bypass, volume, highPass, lowPass, frequencyShift };
struct Parameters
{
    Mode mode = Mode::volume;
    float attackMs = 500.0f, sensitivity = 50.0f, cutoffHz = 8000.0f;
    float resonance = 0.70710678f, inputDb = 0.0f, outputDb = 0.0f;
    bool outward = false;
    float retriggerMs = 120.0f, triggerLowCutHz = 120.0f, triggerHighCutHz = 8000.0f;
    bool triggerFilterEnabled = false;
    float shiftTopHz = 0.0f, shiftBottomHz = 0.0f, mixPercent = 100.0f;
};

// Fixed-memory, sample-by-sample DSP. No block buffering or lookahead.
class SwellEngine
{
public:
    void prepare(double newSampleRate)
    {
        sampleRate = std::max(8000.0, newSampleRate);
        detectorDecay = std::exp(-1.0 / (0.020 * sampleRate));
        baselineRate = 1.0 - std::exp(-1.0 / (0.020 * sampleRate));
        peakHoldSamples = static_cast<int>(0.025 * sampleRate);
        maxTriggerAge = static_cast<int>(2.0 * sampleRate) + 1;
        resetStep = 1.0 / (0.002 * sampleRate);
        reset();
        setParameters(params);
    }

    void reset()
    {
        stage = Stage::waiting;
        detector = baseline = phase = envelope = gate = 0.0;
        peakHold = detectorSettling = 0; samplesSinceTrigger = maxTriggerAge;
        transientArmed = true; triggerCount = 0;
        lowFilter = {}; highFilter = {};
        detectorHighPass = {}; detectorLowPass = {};
        configureDetectorFilters(true);
        inputGain.reset(dbToGain(params.inputDb));
        outputGain.reset(dbToGain(params.outputDb));
        cutoff.reset(std::log(clampCutoff(params.cutoffHz)));
        q.reset(params.resonance);
        direction.reset(params.outward ? 1.0 : 0.0);
        shiftTop.reset(params.shiftTopHz); shiftBottom.reset(params.shiftBottomHz);
        masterMix.reset(params.mixPercent * 0.01);
        shifter.prepare(sampleRate); shifter.setEndpoints(params.shiftBottomHz, params.shiftTopHz, true);
        for (int i = 0; i < 4; ++i)
            modeMix[static_cast<size_t>(i)].reset(static_cast<int>(params.mode) == i + 1 ? 1.0 : 0.0);
        lastMode = params.mode;
    }

    void setParameters(Parameters p)
    {
        p.attackMs = std::clamp(p.attackMs, 1.0f, 20000.0f);
        p.sensitivity = std::clamp(p.sensitivity, 0.0f, 100.0f);
        p.resonance = std::clamp(p.resonance, 0.5f, 8.0f);
        p.retriggerMs = std::clamp(p.retriggerMs, 0.0f, 2000.0f);
        p.triggerLowCutHz = std::clamp(p.triggerLowCutHz, 20.0f, 20000.0f);
        p.triggerHighCutHz = std::clamp(p.triggerHighCutHz, 20.0f, 20000.0f);
        p.shiftTopHz = std::clamp(p.shiftTopHz, -5000.0f, 5000.0f);
        p.shiftBottomHz = std::clamp(p.shiftBottomHz, -5000.0f, 5000.0f);
        p.mixPercent = std::clamp(p.mixPercent, 0.0f, 100.0f);
        const bool filterToggled = p.triggerFilterEnabled != params.triggerFilterEnabled;
        const bool filterMoved = p.triggerFilterEnabled &&
            (std::abs(p.triggerLowCutHz - params.triggerLowCutHz) > 0.001f ||
             std::abs(p.triggerHighCutHz - params.triggerHighCutHz) > 0.001f);
        params = p;
        refractorySamples = static_cast<int>(std::ceil(sampleRate * p.retriggerMs * 0.001));
        if (filterToggled || filterMoved)
        {
            detectorSettling = static_cast<int>(0.050 * sampleRate);
            if (filterToggled) { detectorHighPass = {}; detectorLowPass = {}; }
            configureDetectorFilters(filterToggled);
        }
        riseRatio = dbToGain(sensitivityToRiseDb(p.sensitivity));
        attackStep = 1000.0 / (sampleRate * p.attackMs);
        const auto smoothingSamples = static_cast<int>(0.02 * sampleRate);
        inputGain.set(dbToGain(p.inputDb), smoothingSamples);
        outputGain.set(dbToGain(p.outputDb), smoothingSamples);
        cutoff.set(std::log(clampCutoff(p.cutoffHz)), smoothingSamples);
        q.set(p.resonance, smoothingSamples);
        direction.set(p.outward ? 1.0 : 0.0, smoothingSamples);
        shiftTop.set(p.shiftTopHz, smoothingSamples); shiftBottom.set(p.shiftBottomHz, smoothingSamples);
        masterMix.set(p.mixPercent * 0.01, smoothingSamples);
        if (p.mode == Mode::frequencyShift) shifter.setEndpoints(p.shiftBottomHz, p.shiftTopHz);
        if (p.mode != lastMode)
        {
            // Hard bypass is deliberately exact, including both gain controls.
            if (p.mode == Mode::bypass || lastMode == Mode::bypass)
                reset();
            else
                for (int i = 0; i < 4; ++i)
                    modeMix[static_cast<size_t>(i)].set(static_cast<int>(p.mode) == i + 1 ? 1.0 : 0.0,
                                                       smoothingSamples);
            lastMode = p.mode;
        }
    }

    void process(float* const* channels, int channelCount, int samples)
    {
        if (params.mode == Mode::bypass)
            return; // Bit-for-bit identity; no gain, filtering, or buffer offset.
        channelCount = std::clamp(channelCount, 0, 2);
        if (channelCount == 0) return;
        for (int n = 0; n < samples; ++n)
        {
            if (params.triggerFilterEnabled) updateDetectorCoefficients();
            const double inGain = inputGain.next(), outGain = outputGain.next();
            std::array<double, 2> input {};
            double peak = 0.0;
            for (int c = 0; c < channelCount; ++c)
            {
                input[static_cast<size_t>(c)] = channels[c][n] * inGain;
                double triggerInput = input[static_cast<size_t>(c)];
                if (params.triggerFilterEnabled)
                {
                    for (auto& filter : detectorHighPass) triggerInput = filter.process(triggerInput, c, true);
                    for (auto& filter : detectorLowPass) triggerInput = filter.process(triggerInput, c, false);
                    if (!detectorBandValid) triggerInput = 0.0;
                }
                peak = std::max(peak, std::abs(triggerInput));
            }
            updateTrigger(peak);

            const double reverse = direction.next();
            const double position = envelope * (1.0 - reverse) + (1.0 - envelope) * reverse;
            const double bottom = shiftBottom.next(), top = shiftTop.next();
            currentShiftHz = std::clamp(bottom + position * (top - bottom), -shifter.maxShift(), shifter.maxShift());
            const double targetLog = cutoff.next();
            const double lowLog = std::log(20.0), highLog = std::log(maxFrequency());
            currentLowHz = std::exp(lowLog + position * (targetLog - lowLog));
            currentHighHz = std::exp(highLog + position * (targetLog - highLog));
            const double resonance = q.next();
            lowFilter.set(currentLowHz, resonance, sampleRate);
            highFilter.set(currentHighHz, resonance, sampleRate);
            const double volumeMix = modeMix[0].next();
            const double hpMix = modeMix[1].next(), lpMix = modeMix[2].next();
            const double shiftMix = modeMix[3].next(), blend = masterMix.next();
            const bool runShifter = params.mode == Mode::frequencyShift || shiftMix > 0.0;
            if (runShifter) shifter.beginSample(currentShiftHz);
            for (int c = 0; c < channelCount; ++c)
            {
                const double x = input[static_cast<size_t>(c)];
                // Keep both filters warm for smooth changes between active modes.
                const double lp = lowFilter.process(x, c, false);
                const double hp = highFilter.process(x, c, true);
                const double shifted = runShifter ? shifter.process(x, c) : 0.0;
                const double wet = (x * position * volumeMix + lp * lpMix + hp * hpMix + shifted * shiftMix) * gate;
                channels[c][n] = static_cast<float>((x * (1.0 - blend) + wet * blend) * outGain);
            }
            advanceEnvelope();
        }
    }

    float getEnvelope() const { return static_cast<float>(envelope); }
    float getDetector() const { return static_cast<float>(detector); }
    float getLowFrequency() const { return static_cast<float>(currentLowHz); }
    float getHighFrequency() const { return static_cast<float>(currentHighHz); }
    float getShiftFrequency() const { return static_cast<float>(currentShiftHz); }
    bool isReady() const
    {
        return transientArmed && samplesSinceTrigger >= refractorySamples && detectorSettling == 0
            && (!params.triggerFilterEnabled || detectorBandValid);
    }
    std::uint32_t getTriggerCount() const { return triggerCount; }
    static double sensitivityToRiseDb(double sensitivity)
    {
        return 12.0 * std::pow(1.0 / 16.0, std::clamp(sensitivity, 0.0, 100.0) / 100.0);
    }
    static double dbToGain(double db) { return std::pow(10.0, db / 20.0); }

private:
    struct Ramp
    {
        double value = 0.0, target = 0.0, step = 0.0;
        int remaining = 0;
        void reset(double x) { value = target = x; step = 0.0; remaining = 0; }
        void set(double x, int length)
        {
            if (std::abs(x - target) < 1.0e-12) return;
            target = x; remaining = std::max(1, length);
            step = (target - value) / remaining;
        }
        double next()
        {
            if (remaining > 0) { value += step; if (--remaining == 0) value = target; }
            return value;
        }
    };
    // Topology-preserving state-variable filter, 12 dB/octave. Double precision
    // integrators remain stable while cutoff and resonance change every sample.
    struct Filter
    {
        std::array<double, 2> s1 {}, s2 {};
        double g = 0.0, k = 1.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
        void set(double frequency, double resonance, double sr)
        {
            setG(std::tan(3.14159265358979323846 * frequency / sr), resonance);
        }
        void setG(double coefficient, double resonance)
        {
            g = coefficient;
            k = 1.0 / resonance;
            a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2;
        }
        double process(double x, int channel, bool high)
        {
            const auto c = static_cast<size_t>(channel);
            const double v3 = x - s2[c];
            const double v1 = a1 * s1[c] + a2 * v3;
            const double v2 = s2[c] + a2 * s1[c] + a3 * v3;
            s1[c] = 2.0 * v1 - s1[c]; s2[c] = 2.0 * v2 - s2[c];
            if (std::abs(s1[c]) < 1.0e-30) s1[c] = 0.0;
            if (std::abs(s2[c]) < 1.0e-30) s2[c] = 0.0;
            return high ? x - k * v1 - v2 : v2;
        }
    };
    void configureDetectorFilters(bool immediate)
    {
        if (!params.triggerFilterEnabled) return;
        const double low = clampCutoff(params.triggerLowCutHz), high = clampCutoff(params.triggerHighCutHz);
        detectorBandValid = low < high;
        const double hpG = std::tan(3.14159265358979323846 * low / sampleRate);
        const double lpG = std::tan(3.14159265358979323846 * high / sampleRate);
        if (immediate) { detectorHpG.reset(hpG); detectorLpG.reset(lpG); }
        else
        {
            detectorHpG.set(hpG, static_cast<int>(0.020 * sampleRate));
            detectorLpG.set(lpG, static_cast<int>(0.020 * sampleRate));
        }
        setDetectorCoefficients();
    }
    void setDetectorCoefficients()
    {
        // Two Butterworth sections per edge: 24 dB/octave, no resonance control.
        constexpr std::array<double, 2> quality {0.541196100146197, 1.306562964876377};
        for (size_t i = 0; i < quality.size(); ++i)
        {
            detectorHighPass[i].setG(detectorHpG.value, quality[i]);
            detectorLowPass[i].setG(detectorLpG.value, quality[i]);
        }
    }
    void updateDetectorCoefficients()
    {
        if (detectorHpG.remaining == 0 && detectorLpG.remaining == 0) return;
        detectorHpG.next(); detectorLpG.next(); setDetectorCoefficients();
    }
    enum class Stage { waiting, attack, sustain, retrigger };
    void updateTrigger(double peak)
    {
        // Hold waveform peaks so individual cycles (including bass notes) do not
        // look like new picks. The baseline follows input, never the wet output.
        if (peak >= detector * 0.995)
        {
            detector = std::max(peak, detector);
            peakHold = peakHoldSamples;
        }
        else if (peakHold > 0) --peakHold;
        else detector = std::max(peak, detector * detectorDecay);

        if (samplesSinceTrigger < maxTriggerAge) ++samplesSinceTrigger;
        if (detectorSettling > 0) --detectorSettling;
        // A fixed -80 dBFS denominator floor avoids amplifying numerical silence.
        // Sensitivity controls relative level delta, not an absolute noise gate.
        const double reference = std::max(baseline, 0.0001);
        if (detector <= reference * (1.0 + 0.25 * (riseRatio - 1.0)))
            transientArmed = true;
        if (transientArmed && detector > reference * riseRatio)
        {
            transientArmed = false;
            if (samplesSinceTrigger >= refractorySamples && detectorSettling == 0
                && (!params.triggerFilterEnabled || detectorBandValid))
            {
                ++triggerCount;
                samplesSinceTrigger = 0;
                phase = 0.0;
                if (stage == Stage::waiting)
                {
                    envelope = 0.0; stage = Stage::attack; gate = 1.0;
                }
                else
                {
                    // A brief control-envelope reset prevents a hard gain/filter
                    // jump on sustained audio. Audio samples are never delayed.
                    resetStart = envelope; resetPhase = 0.0; stage = Stage::retrigger;
                }
            }
        }
        baseline += baselineRate * (detector - baseline);
    }
    static double ease(double x) { return x * x * (3.0 - 2.0 * x); }
    void advanceEnvelope()
    {
        if (stage == Stage::attack)
        {
            phase = std::min(1.0, phase + attackStep); envelope = ease(phase);
            if (phase >= 1.0) stage = Stage::sustain;
        }
        else if (stage == Stage::retrigger)
        {
            resetPhase = std::min(1.0, resetPhase + resetStep);
            envelope = resetStart * (1.0 - ease(resetPhase));
            if (resetPhase >= 1.0)
            {
                stage = Stage::attack; phase = envelope = 0.0;
            }
        }
    }
    double maxFrequency() const { return std::min(20000.0, sampleRate * 0.45); }
    double clampCutoff(double x) const { return std::clamp(x, 20.0, maxFrequency()); }
    Parameters params;
    Mode lastMode = Mode::volume;
    Stage stage = Stage::waiting;
    double sampleRate = 44100.0, detectorDecay = 0.99, detector = 0.0;
    double baseline = 0.0, baselineRate = 0.001, riseRatio = dbToGain(3.0), attackStep = 1.0 / 22050.0;
    double phase = 0.0, envelope = 0.0, gate = 0.0, resetPhase = 0.0, resetStep = 0.01, resetStart = 0.0;
    double currentLowHz = 20.0, currentHighHz = 20000.0;
    double currentShiftHz = 0.0;
    int peakHold = 0, peakHoldSamples = 1102, samplesSinceTrigger = 88201, maxTriggerAge = 88201;
    int refractorySamples = 5292, detectorSettling = 0;
    bool detectorBandValid = true;
    bool transientArmed = true;
    std::uint32_t triggerCount = 0;
    Ramp inputGain, outputGain, cutoff, q, direction;
    std::array<Ramp, 4> modeMix;
    Ramp shiftTop, shiftBottom, masterMix;
    FrequencyShifter shifter;
    Filter lowFilter, highFilter;
    std::array<Filter, 2> detectorHighPass, detectorLowPass;
    Ramp detectorHpG, detectorLpG;
};
}
