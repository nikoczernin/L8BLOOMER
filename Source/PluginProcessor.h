#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "SwellEngine.h"

class SlowBloomProcessor final : public juce::AudioProcessor
{
public:
    SlowBloomProcessor();
    const juce::String getName() const override { return JucePlugin_Name; }
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void reset() override { engine.reset(); }
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    juce::AudioProcessorValueTreeState state;
    std::atomic<float> visualEnvelope { 0.0f }, inputLevel { 0.0f }, outputLevel { 0.0f };
    std::atomic<bool> ready { true };
private:
    static juce::AudioProcessorValueTreeState::ParameterLayout makeParameters();
    bloom::Parameters readParameters() const;
    bloom::SwellEngine engine;
    std::array<std::atomic<float>*, 15> values {};
    bool hostWasBypassed = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowBloomProcessor)
};
