#include "SwellEngine.h"
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace
{
int checks = 0;
void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void near(float a, float b, float tolerance, const char* message) { require(std::abs(a - b) <= tolerance, message); }
std::vector<float> constant(bloom::SwellEngine& engine, int samples, float amplitude = 0.5f)
{
    std::vector<float> data(static_cast<size_t>(samples), amplitude);
    float* pointer = data.data(); engine.process(&pointer, 1, samples); return data;
}
bloom::SwellEngine make(double sr, bloom::Parameters p)
{
    bloom::SwellEngine engine; engine.setParameters(p); engine.prepare(sr); return engine;
}
void bypass()
{
    bloom::Parameters p; p.mode = bloom::Mode::bypass; p.inputDb = 24; p.outputDb = -24;
    auto engine = make(48000, p);
    std::mt19937 random(27); std::uniform_real_distribution<float> dist(-2, 2);
    std::vector<float> left(4097), right(4097);
    for (auto& x : left) x = dist(random);
    for (auto& x : right) x = dist(random);
    left[0] = -0.0f;
    auto a = left, b = right;
    float* pointers[] {left.data(), right.data()}; engine.process(pointers, 2, 4097);
    require(std::memcmp(a.data(), left.data(), a.size() * sizeof(float)) == 0, "Bypass must be bit-identical on left");
    require(std::memcmp(b.data(), right.data(), b.size() * sizeof(float)) == 0, "Bypass must be bit-identical on right");
    p.mode = bloom::Mode::volume; engine.setParameters(p); constant(engine, 256);
    p.mode = bloom::Mode::bypass; engine.setParameters(p);
    auto dry = constant(engine, 256); for (auto x : dry) near(x, 0.5f, 0.0f, "Switch to bypass is immediate and unity");
}
void timingAndDirection()
{
    for (double sr : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0})
        for (float attack : {1.0f, 500.0f, 20000.0f})
            for (bool out : {false, true})
            {
                bloom::Parameters p; p.attackMs = attack; p.outward = out;
                auto engine = make(sr, p);
                const int count = static_cast<int>(std::ceil(attack * sr / 1000.0));
                auto data = constant(engine, count + 10);
                near(data.front(), out ? 0.5f : 0.0f, 1e-6f, "Exact starting amplitude");
                near(data[static_cast<size_t>(count)], out ? 0.0f : 0.5f, 1e-5f, "Attack duration matches parameter");
                for (size_t i = 1; i < data.size(); ++i)
                    require(out ? data[i] <= data[i-1] + 1e-6f : data[i] >= data[i-1] - 1e-6f,
                            "Swell is monotonic and never ping-pongs");
                auto held = constant(engine, 4096);
                near(held.back(), out ? 0.0f : 0.5f, 1e-6f, "One-shot holds its endpoint");
                require(engine.getTriggerCount() == 1, "Constant signal does not retrigger");
                constant(engine, static_cast<int>(sr * 0.5), 0.0f);
                require(engine.isReady(), "Detector is armed after silence");
                constant(engine, static_cast<int>(std::ceil(sr * 0.002)));
                require(engine.getTriggerCount() == 2, "Next note restarts sweep");
                near(engine.getEnvelope(), 0.0f, 1e-6f, "Retrigger returns to start in 2 ms");
            }
}
void sensitivityAndStereo()
{
    bloom::Parameters p; p.sensitivity = 0;
    auto insensitive = make(48000, p); constant(insensitive, 24000, 0.1f);
    constant(insensitive, 1000, 0.16f);
    require(insensitive.getTriggerCount() == 1, "Low sensitivity ignores a modest rise over sustained input");
    p.sensitivity = 100; auto sensitive = make(48000, p); constant(sensitive, 24000, 0.1f);
    constant(sensitive, 1000, 0.16f);
    require(sensitive.getTriggerCount() == 2, "High sensitivity detects the same volume delta");
    p.sensitivity = 50; p.attackMs = 100;
    auto engine = make(48000, p);
    std::vector<float> left(12000, 0.1f), right(12000, -0.4f);
    float* pointers[] {left.data(), right.data()}; engine.process(pointers, 2, 12000);
    for (size_t i = 0; i < left.size(); ++i) near(right[i], -4.0f * left[i], 1e-6f, "Stereo detector preserves image");
    // A low guitar/bass note must not rearm at each waveform zero crossing.
    engine = make(48000, p);
    std::vector<float> bass(48000);
    for (size_t i = 0; i < bass.size(); ++i) bass[i] = 0.2f * std::sin(static_cast<float>(i) * 2.0f * 3.14159265f * 30.0f / 48000.0f);
    float* bassPtr = bass.data(); engine.process(&bassPtr, 1, static_cast<int>(bass.size()));
    near(engine.getEnvelope(), 1.0f, 1e-6f, "Bass zero crossings do not retrigger");
    require(engine.getTriggerCount() == 1, "Bass waveform cycles are not pick attacks");
    require(engine.isReady(), "New transients can trigger while a note sustains");
}

void transientsOverContinuousInput()
{
    for (double sr : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0})
        for (auto mode : {bloom::Mode::volume, bloom::Mode::lowPass, bloom::Mode::highPass})
            for (bool outward : {false, true})
            {
                bloom::Parameters p; p.mode = mode; p.outward = outward; p.attackMs = 500;
                auto engine = make(sr, p);
                constant(engine, static_cast<int>(sr * 0.5), 0.1f);
                require(engine.isReady(), "Detector rearms without silence while envelope holds");
                for (int pick = 0; pick < 4; ++pick)
                {
                    const auto before = engine.getTriggerCount();
                    constant(engine, 1, 0.3f);
                    require(engine.getTriggerCount() == before + 1, "New transient retriggers during a swell in every mode/direction");
                    constant(engine, static_cast<int>(std::ceil(sr * 0.002)), 0.3f);
                    require(engine.getEnvelope() < 0.001f, "Every retrigger returns the curve to its beginning");
                    constant(engine, static_cast<int>(sr * 0.07), 0.3f);
                    constant(engine, static_cast<int>(sr * 0.2), 0.1f);
                    require(engine.getTriggerCount() == before + 1, "Steady sustain and a fall in volume do not retrigger");
                }
            }
    // The highest sensitivity must still distinguish individual waveform cycles
    // from onsets; include clean bass and heavily clipped sustained tones.
    for (double sr : {44100.0, 48000.0, 96000.0})
        for (double frequency : {30.0, 82.4, 220.0, 1000.0})
            for (double drive : {1.0, 12.0})
            {
                bloom::Parameters p; p.sensitivity = 100; p.attackMs = 50;
                auto engine = make(sr, p);
                std::vector<float> tone(static_cast<size_t>(sr * 2.0));
                for (size_t i = 0; i < tone.size(); ++i)
                    tone[i] = static_cast<float>(0.2 * std::tanh(drive * std::sin(i * 2.0 * 3.141592653589793 * frequency / sr)));
                float* ptr = tone.data(); engine.process(&ptr, 1, static_cast<int>(tone.size()));
                require(engine.getTriggerCount() == 1, "Sustained clean and distorted notes do not machine-gun at high sensitivity");
            }
}

void noisyAmpPicks()
{
    for (double sr : {44100.0, 48000.0, 96000.0})
    {
        bloom::Parameters p; p.attackMs = 400;
        auto engine = make(sr, p);
        std::mt19937 rng(564); std::uniform_real_distribution<double> noise(-0.003, 0.003);
        int clock = 0;
        const auto render = [&](int samples, bool picked) {
            std::vector<float> data(static_cast<size_t>(samples));
            for (int n = 0; n < samples; ++n, ++clock)
            {
                const double time = clock / sr;
                const double hum = 0.01 * std::sin(time * 2.0 * 3.141592653589793 * 50.0);
                const double note = picked ? 0.18 * std::exp(-n / (sr * 0.13))
                    * (std::sin(time * 2.0 * 3.141592653589793 * 220.0)
                       + 0.2 * std::sin(time * 2.0 * 3.141592653589793 * 660.0)) : 0.0;
                data[static_cast<size_t>(n)] = static_cast<float>(0.7 * std::tanh(6.0 * (hum + noise(rng) + note)));
            }
            auto* ptr = data.data(); engine.process(&ptr, 1, samples);
        };
        render(static_cast<int>(sr * 0.5), false);
        const auto noiseCount = engine.getTriggerCount();
        render(static_cast<int>(sr), false);
        require(engine.getTriggerCount() == noiseCount, "Continuous amp hum and hiss do not repeatedly trigger");
        for (int pick = 0; pick < 6; ++pick)
        {
            const auto before = engine.getTriggerCount();
            render(static_cast<int>(sr * 0.25), true);
            require(engine.getTriggerCount() == before + 1, "One trigger per distorted pick over noise and previous note tail");
        }
    }
}
void filterSweeps()
{
    for (double sr : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0})
        for (auto mode : {bloom::Mode::lowPass, bloom::Mode::highPass})
            for (bool out : {false, true})
            {
                bloom::Parameters p; p.mode = mode; p.outward = out; p.attackMs = 100; p.cutoffHz = 1700;
                auto engine = make(sr, p); constant(engine, 1);
                const float closed = mode == bloom::Mode::lowPass ? 20.0f : static_cast<float>(std::min(20000.0, 0.45 * sr));
                auto frequency = [&] { return mode == bloom::Mode::lowPass ? engine.getLowFrequency() : engine.getHighFrequency(); };
                near(frequency(), out ? p.cutoffHz : closed, 0.1f, "Filter starts at correct endpoint");
                const bool increasing = (mode == bloom::Mode::lowPass) != out;
                float previous = frequency();
                for (int i = 0; i < static_cast<int>(sr * 0.12); ++i)
                {
                    constant(engine, 1);
                    require(increasing ? frequency() >= previous - 0.01f : frequency() <= previous + 0.01f, "Filter sweep is one-way");
                    previous = frequency();
                }
                near(frequency(), out ? closed : p.cutoffHz, 0.1f, "Filter holds destination");
            }
}
double toneRms(bloom::Mode mode, double frequency)
{
    bloom::Parameters p; p.mode = mode; p.attackMs = 1; p.cutoffHz = 1000;
    auto engine = make(48000, p);
    std::vector<float> audio(48000);
    for (size_t i = 0; i < audio.size(); ++i) audio[i] = 0.1f * static_cast<float>(std::sin(i * 2.0 * 3.141592653589793 * frequency / 48000.0));
    auto* ptr = audio.data(); engine.process(&ptr, 1, static_cast<int>(audio.size()));
    double energy = 0; for (size_t i = 24000; i < audio.size(); ++i) energy += audio[i] * audio[i];
    return std::sqrt(energy / 24000);
}
void filteringAndStress()
{
    require(toneRms(bloom::Mode::lowPass, 100) > toneRms(bloom::Mode::lowPass, 10000) * 50, "LP attenuates highs");
    require(toneRms(bloom::Mode::highPass, 10000) > toneRms(bloom::Mode::highPass, 100) * 50, "HP attenuates lows");
    std::mt19937 random(982); std::uniform_real_distribution<float> noise(-0.1f, 0.1f);
    for (double sr : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0})
    {
        bloom::Parameters p; p.resonance = 8; p.attackMs = 1; p.sensitivity = 100;
        auto engine = make(sr, p);
        for (int block = 0; block < 1000; ++block)
        {
            p.mode = block % 2 == 0 ? bloom::Mode::lowPass : bloom::Mode::highPass;
            p.cutoffHz = block % 3 == 0 ? 20 : 20000; p.outward = block % 5 == 0;
            engine.setParameters(p);
            std::array<float, 127> data;
            for (auto& x : data) x = noise(random);
            float* ptr = data.data(); engine.process(&ptr, 1, static_cast<int>(data.size()));
            for (auto x : data) require(std::isfinite(x) && std::abs(x) < 20, "Automated resonant filter remains finite and bounded");
        }
    }
}
void blockSizeAndGain()
{
    bloom::Parameters p; p.attackMs = 10; p.inputDb = 6; p.outputDb = -6;
    auto a = make(48000, p), b = make(48000, p);
    auto whole = constant(a, 7000);
    std::vector<float> chunked;
    for (int offset = 0; offset < 7000;)
    {
        int size = std::min(127, 7000 - offset);
        b.setParameters(p); auto chunk = constant(b, size);
        chunked.insert(chunked.end(), chunk.begin(), chunk.end()); offset += size;
    }
    require(std::memcmp(whole.data(), chunked.data(), whole.size() * sizeof(float)) == 0, "DSP independent of host block size");
    near(whole.back(), 0.5f, 1e-6f, "Input and output gain correct");
    a.process(nullptr, 0, 0);
}

void retriggerLockout()
{
    for (double sr : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0})
        for (float interval : {250.0f, 2000.0f})
        {
            bloom::Parameters p; p.retriggerMs = interval;
            auto engine = make(sr, p);
            constant(engine, 1, 0.5f); // First accepted trigger at sample zero.
            auto atBoundary = engine;
            const int wait = static_cast<int>(std::ceil(interval * sr / 1000.0));
            constant(engine, wait - 2, 0.01f);
            constant(engine, 1, 0.5f); // One sample before the interval expires.
            require(engine.getTriggerCount() == 1, "A trigger cannot slip through before the minimum interval");
            constant(engine, wait, 0.5f);
            require(engine.getTriggerCount() == 1, "Suppressed triggers are discarded, not queued");
            constant(atBoundary, wait - 1, 0.01f);
            constant(atBoundary, 1, 0.5f);
            require(atBoundary.getTriggerCount() == 2, "A fresh transient at the interval boundary can trigger");

            p.retriggerMs = 2000;
            atBoundary.setParameters(p);
            constant(atBoundary, static_cast<int>(sr * 0.2), 0.01f);
            constant(atBoundary, 1, 0.5f);
            require(atBoundary.getTriggerCount() == 2, "Increasing the interval applies to the last accepted trigger");
            constant(atBoundary, static_cast<int>(sr * 0.2), 0.01f);
            p.retriggerMs = 0; atBoundary.setParameters(p);
            constant(atBoundary, 1, 0.5f);
            require(atBoundary.getTriggerCount() == 3, "Zero removes the time lockout without waiting out the old setting");
        }
}

void detectorFiltering()
{
    constexpr double pi = 3.14159265358979323846;
    for (double sr : {44100.0, 48000.0, 96000.0, 192000.0})
    {
        bloom::Parameters p; p.triggerFilterEnabled = true; p.triggerLowCutHz = 1000; p.triggerHighCutHz = 4000;
        auto filtered = make(sr, p);
        p.triggerFilterEnabled = false; auto plain = make(sr, p);
        int time = 0;
        const auto render = [&](int samples, double burstHz, double burstLevel) {
            std::vector<float> a(static_cast<size_t>(samples));
            for (int i = 0; i < samples; ++i, ++time)
            {
                const double t = time / sr;
                const double fade = std::min(1.0, i / (sr * 0.005));
                const double onset = fade * fade * (3.0 - 2.0 * fade);
                a[static_cast<size_t>(i)] = static_cast<float>(0.02 * std::sin(2 * pi * 2000 * t)
                    + burstLevel * onset * std::sin(2 * pi * burstHz * t));
            }
            auto b = a;
            float* fa = a.data(); float* fb = b.data();
            filtered.process(&fa, 1, samples); plain.process(&fb, 1, samples);
        };
        render(static_cast<int>(sr * 0.5), 0, 0);
        const auto filteredStart = filtered.getTriggerCount(), plainStart = plain.getTriggerCount();
        render(static_cast<int>(sr * 0.5), 100, 0.3);
        require(filtered.getTriggerCount() == filteredStart, "Trigger low-cut rejects rumble bursts over in-band ambience");
        require(plain.getTriggerCount() > plainStart, "The same unfiltered rumble would trigger");
        render(static_cast<int>(sr * 0.5), 0, 0);
        const auto beforeHigh = filtered.getTriggerCount(), plainBeforeHigh = plain.getTriggerCount();
        render(static_cast<int>(sr * 0.5), 16000, 0.3);
        require(filtered.getTriggerCount() == beforeHigh, "Trigger high-cut rejects high-frequency bursts");
        require(plain.getTriggerCount() > plainBeforeHigh, "The same unfiltered high-frequency burst would trigger");
        render(static_cast<int>(sr * 0.5), 0, 0);
        const auto beforePick = filtered.getTriggerCount();
        render(static_cast<int>(sr * 0.5), 2000, 0.15);
        require(filtered.getTriggerCount() == beforePick + 1, "Pick transients inside the detector band still trigger");
    }

    bloom::Parameters p; p.attackMs = 1; p.triggerLowCutHz = 1000; p.triggerHighCutHz = 4000;
    auto dryDetector = make(48000, p); p.triggerFilterEnabled = true; auto bandDetector = make(48000, p);
    std::vector<float> input(96000);
    for (size_t i = 0; i < input.size(); ++i) input[i] = 0.2f * static_cast<float>(std::sin(i * 2 * pi * 2000 / 48000));
    auto a = input, b = input;
    float* pa = a.data(); float* pb = b.data();
    dryDetector.process(&pa, 1, static_cast<int>(a.size()));
    bandDetector.process(&pb, 1, static_cast<int>(b.size()));
    require(std::memcmp(a.data() + 48000, b.data() + 48000, 48000 * sizeof(float)) == 0,
            "Detector filters do not EQ or delay the audible path once the swell reaches unity");
    // No artificial onset when enabling/disabling detector filters on a steady note.
    const auto beforeToggle = bandDetector.getTriggerCount();
    p.triggerFilterEnabled = false; bandDetector.setParameters(p); b = input; pb = b.data();
    bandDetector.process(&pb, 1, static_cast<int>(b.size()));
    p.triggerFilterEnabled = true; bandDetector.setParameters(p); b = input; pb = b.data();
    bandDetector.process(&pb, 1, static_cast<int>(b.size()));
    require(bandDetector.getTriggerCount() == beforeToggle, "Filter switches settle without triggering the curve");

    p.triggerLowCutHz = 12000; p.triggerHighCutHz = 1000;
    auto invalid = make(22050, p); constant(invalid, 22050);
    require(invalid.getTriggerCount() == 0 && !invalid.isReady(), "Crossed frequency bounds disable detection safely");
    p.mode = bloom::Mode::bypass; invalid.setParameters(p);
    auto bypassed = constant(invalid, 512);
    for (float x : bypassed) near(x, 0.5f, 0, "Advanced settings leave true bypass untouched");
}

void detectorAutomation()
{
    for (double sr : {11025.0, 44100.0, 192000.0})
    {
        bloom::Parameters p; p.sensitivity = 100;
        auto engine = make(sr, p);
        for (int block = 0; block < 600; ++block)
        {
            p.triggerFilterEnabled = block % 7 != 0;
            p.triggerLowCutHz = block % 2 == 0 ? 20 : 15000;
            p.triggerHighCutHz = block % 3 == 0 ? 30 : 20000;
            p.retriggerMs = block % 2 == 0 ? 0 : 2000;
            engine.setParameters(p);
            const auto output = constant(engine, 127, block % 5 == 0 ? 0.5f : -0.2f);
            for (auto x : output) require(std::isfinite(x) && std::abs(x) <= 1, "Detector filter automation remains bounded");
        }
    }
}
}
int main()
{
    try
    {
        bypass(); timingAndDirection(); sensitivityAndStereo(); transientsOverContinuousInput(); noisyAmpPicks();
        filterSweeps(); filteringAndStress(); blockSizeAndGain();
        retriggerLockout(); detectorFiltering(); detectorAutomation();
        std::cout << "PASS: " << checks << " DSP assertions (bypass, timing, direction, reset, stereo, filters, automation, gains).\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
