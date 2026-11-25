#pragma once
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class BloomLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    BloomLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
};

class SlowBloomEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SlowBloomEditor(SlowBloomProcessor&);
    ~SlowBloomEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    void updateMode();
    void bindEndpointKnobs(bool frequencyMode);
    void drawEnvelope(juce::Graphics&, juce::Rectangle<float>);
    SlowBloomProcessor& audioProcessor;
    BloomLookAndFeel look;
    std::array<juce::Slider, 10> knobs;
    std::array<juce::Label, 10> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 10> knobAttachments;
    std::array<juce::TextButton, 5> modes;
    juce::TextButton direction, advancedToggle, triggerFilter;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> directionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> triggerFilterAttachment;
    juce::TooltipWindow tooltips {this, 500};
    int selectedMode = -1;
    float displayIn = 0.0f, displayOut = 0.0f;
    bool advancedOpen = true;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowBloomEditor)
};
