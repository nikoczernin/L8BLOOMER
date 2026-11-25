#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>

static void check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
static void setParameter(SlowBloomProcessor& processor, const char* id, float value)
{
    auto* parameter = processor.state.getParameter(id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
static void checkKnobDefaults(SlowBloomProcessor& processor)
{
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    auto checkKnobs = [&] {
        for (auto* child : editor->getChildren())
            if (auto* slider = dynamic_cast<juce::Slider*>(child))
            {
                if (!slider->isEnabled()) continue;
                check(slider->isDoubleClickReturnEnabled(), "Every knob supports double-click reset");
                slider->setValue(slider->getMaximum(), juce::sendNotificationSync);
                const auto now = juce::Time::getCurrentTime();
                juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), {50, 50}, {},
                    1, 0, 0, 0, 0, slider, slider, now, {50, 50}, now, 2, false);
                slider->mouseDoubleClick(event);
                check(std::abs(slider->getValue() - slider->getDoubleClickReturnValue()) < 0.01,
                      "Double-click returns the actual knob to its parameter default");
            }
    };
    checkKnobs();
    setParameter(processor, "cutoff", 4321); setParameter(processor, "resonance", 2.0f);
    auto clickMode = [&](const char* title) {
        for (auto* child : editor->getChildren())
            if (auto* button = dynamic_cast<juce::TextButton*>(child))
                if (button->getButtonText() == title) { button->onClick(); return; }
        check(false, "Mode button exists");
    };
    clickMode("FREQ SHIFT");
    int signedKnobs = 0;
    for (auto* child : editor->getChildren())
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
            if (slider->getName() == "FROM" || slider->getName() == "TO")
            {
                ++signedKnobs;
                check(std::abs(slider->valueToProportionOfLength(0) - 0.5) < 1e-9, "Zero is exactly at the centre of the signed knob");
                check(std::abs(slider->getDoubleClickReturnValue()) < 1e-9, "Frequency endpoints reset to zero");
                const auto* id = slider->getName() == "FROM" ? "shiftBottom" : "shiftTop";
                check(std::abs(slider->getValue() - processor.state.getRawParameterValue(id)->load()) < 0.01,
                      "Renamed endpoints retain the saved start and destination values");
            }
    check(signedKnobs == 2, "Cutoff and resonance become From and To");
    checkKnobs();
    clickMode("LOW-PASS");
    check(std::abs(processor.state.getRawParameterValue("cutoff")->load() - 4321) < 0.01,
          "Mode changes preserve the filter cutoff independently of From");
    check(std::abs(processor.state.getRawParameterValue("resonance")->load() - 2.0f) < 0.01,
          "Mode changes preserve resonance independently of To");
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    try
    {
        SlowBloomProcessor p;
        check(p.getName() == "L8BLOOMER", "Plugin exposes its new name");
        check(p.getLatencySamples() == 0, "Latency must be zero");
        for (const auto* id : {"attack", "cutoff"})
        {
            auto* parameter = p.state.getParameter(id);
            for (float value : {1.0f, 20.0f, 127.4f, 1500.0f, 8500.0f, 20000.0f})
            {
                const auto text = parameter->getText(parameter->convertTo0to1(value), 64);
                check(!text.containsChar('.') && !text.containsChar(','), "Attack and cutoff display whole numbers throughout their ranges");
            }
        }
        const std::array<std::pair<const char*, float>, 15> settings {{
            {"attack", 1234.0f}, {"sensitivity", 73.0f}, {"mode", 3.0f}, {"cutoff", 3000.0f},
            {"resonance", 2.5f}, {"input", 4.0f}, {"output", -3.0f}, {"direction", 1.0f},
            {"retrigger", 350.0f}, {"triggerFilter", 1.0f}, {"triggerLowCut", 1000.0f}, {"triggerHighCut", 6000.0f},
            {"shiftTop", 700.0f}, {"shiftBottom", -300.0f}, {"mix", 62.0f}}};
        for (const auto& setting : settings)
        {
            auto* parameter = p.state.getParameter(setting.first);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(setting.second));
        }
        juce::MemoryBlock saved;
        p.getStateInformation(saved);
        SlowBloomProcessor restored;
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        for (const auto& setting : settings)
            check(std::abs(restored.state.getRawParameterValue(setting.first)->load() - setting.second) < 0.01f,
                  "Session state round trip must preserve every control");
        restored.prepareToPlay(48000, 512);
        juce::AudioBuffer<float> buffer(2, 512); juce::MidiBuffer midi;
        for (int c = 0; c < 2; ++c)
            for (int n = 0; n < 512; ++n) buffer.setSample(c, n, static_cast<float>(n) / 1000.0f);
        juce::AudioBuffer<float> copy(buffer);
        restored.processBlockBypassed(buffer, midi);
        for (int c = 0; c < 2; ++c)
            check(std::memcmp(buffer.getReadPointer(c), copy.getReadPointer(c), 512 * sizeof(float)) == 0,
                  "Host bypass must preserve exact samples");
        restored.processBlock(buffer, midi);
        restored.setStateInformation("malformed", 9);
        check(restored.getLatencySamples() == 0, "Latency stays zero while processing");
        auto legacyState = p.state.copyState();
        for (const auto* id : {"retrigger", "triggerFilter", "triggerLowCut", "triggerHighCut", "shiftTop", "shiftBottom", "mix"})
            legacyState.removeChild(legacyState.getChildWithProperty("id", id), nullptr);
        juce::MemoryBlock legacyBytes;
        juce::AudioProcessor::copyXmlToBinary(*legacyState.createXml(), legacyBytes);
        restored.setStateInformation(legacyBytes.getData(), static_cast<int>(legacyBytes.getSize()));
        check(restored.state.getRawParameterValue("triggerFilter")->load() < 0.5f, "Older sessions default to detector filters OFF");
        check(std::abs(restored.state.getRawParameterValue("retrigger")->load() - 120.0f) < 0.01f,
              "Older sessions receive the default retrigger interval");
        check(std::abs(restored.state.getRawParameterValue("attack")->load() - 1234.0f) < 0.01f,
              "Adding advanced controls preserves existing settings");
        check(std::abs(restored.state.getRawParameterValue("mix")->load() - 100.0f) < 0.01f,
              "Older sessions remain fully wet");
        check(std::abs(restored.state.getRawParameterValue("shiftTop")->load()) < 0.01f
              && std::abs(restored.state.getRawParameterValue("shiftBottom")->load()) < 0.01f,
              "Older sessions initialise both shift endpoints to zero");
        checkKnobDefaults(p);
        if (argc == 3 && (juce::String(argv[1]) == "--render" || juce::String(argv[1]) == "--render-shift"))
        {
            SlowBloomProcessor preview;
            if (juce::String(argv[1]) == "--render-shift") setParameter(preview, "mode", 4);
            std::unique_ptr<juce::AudioProcessorEditor> editor(preview.createEditor());
            auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
            juce::File output(argv[2]);
            auto stream = output.createOutputStream();
            check(stream != nullptr, "Cannot open screenshot output");
            check(stream->setPosition(0) && stream->truncate().wasOk(), "Cannot replace screenshot output");
            juce::PNGImageFormat png;
            check(png.writeImageToStream(image, *stream), "Cannot write screenshot");
        }
        std::cout << "PASS: plugin state, malformed state handling, host bypass, zero latency.\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
