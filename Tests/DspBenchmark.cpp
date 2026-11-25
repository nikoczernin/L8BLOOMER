#include "SwellEngine.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

int main()
{
    constexpr int blockSize = 256;
    constexpr double seconds = 5.0;
    volatile float checksum = 0;
    std::cout << "Stereo DSP benchmark, median of five runs, 256-frame blocks.\n";
    for (auto mode : {bloom::Mode::volume, bloom::Mode::frequencyShift})
    for (double rate : {48000.0, 192000.0})
    {
        const int frames = static_cast<int>(rate * seconds);
        std::vector<float> source(static_cast<size_t>(frames));
        for (int i = 0; i < frames; ++i)
            source[static_cast<size_t>(i)] = static_cast<float>(0.2 * std::sin(i * 2.0 * 3.141592653589793 * 440.0 / rate));
        std::array<std::vector<double>, 2> costs;
        for (int repeat = 0; repeat < 5; ++repeat)
            for (int which = 0; which < 2; ++which)
            {
                const bool enabled = ((repeat + which) % 2) != 0;
                bloom::Parameters p; p.triggerFilterEnabled = enabled; p.mode = mode;
                p.shiftBottomHz = -1000; p.shiftTopHz = 1000;
                bloom::SwellEngine engine; engine.setParameters(p); engine.prepare(rate);
                auto left = source, right = source;
                const auto start = std::chrono::steady_clock::now();
                for (int offset = 0; offset < frames; offset += blockSize)
                {
                    engine.setParameters(p);
                    float* channels[] {left.data() + offset, right.data() + offset};
                    engine.process(channels, 2, std::min(blockSize, frames - offset));
                }
                const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                checksum += left.back();
                costs[enabled ? 1 : 0].push_back(100.0 * elapsed / seconds);
            }
        for (auto& cost : costs) std::sort(cost.begin(), cost.end());
        std::cout << (mode == bloom::Mode::frequencyShift ? "Frequency shift, " : "Volume, ")
                  << std::fixed << std::setprecision(3) << rate / 1000 << " kHz: filters OFF " << costs[0][2]
                  << "% of one core; ON " << costs[1][2] << "%; added " << costs[1][2] - costs[0][2]
                  << " percentage points.\n";
    }
    return std::isfinite(checksum) ? 0 : 1;
}
