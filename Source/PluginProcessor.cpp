#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
juce::NormalisableRange<float> logarithmic(float low, float high)
{
    return {low, high,
        [](float a, float b, float x) { return a * std::pow(b / a, x); },
        [](float a, float b, float x) { return std::log(x / a) / std::log(b / a); }};
}
juce::String timeText(float ms, int)
{
    return juce::String(juce::roundToInt(ms)) + " ms";
}
float timeValue(const juce::String& text)
{
    return text.getFloatValue() * (text.containsIgnoreCase("s") && !text.containsIgnoreCase("ms") ? 1000.0f : 1.0f);
}
juce::NormalisableRange<float> signedFrequency()
{
    return {-5000.0f, 5000.0f,
        [](float, float, float x) { const auto v = 2.0f * x - 1.0f; return 5000.0f * v * v * v; },
        [](float, float, float v) { return 0.5f + 0.5f * std::cbrt(v / 5000.0f); },
        [](float, float, float v) { return std::round(v); }};
}
}

juce::AudioProcessorValueTreeState::ParameterLayout SlowBloomProcessor::makeParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    using Float = juce::AudioParameterFloat;
    using ID = juce::ParameterID;
    using Attr = juce::AudioParameterFloatAttributes;
    layout.add(std::make_unique<Float>(ID{"attack", 1}, "Attack", logarithmic(1.0f, 20000.0f), 500.0f,
        Attr().withStringFromValueFunction(timeText).withValueFromStringFunction(timeValue)));
    layout.add(std::make_unique<Float>(ID{"sensitivity", 1}, "Sensitivity", juce::NormalisableRange<float>(0, 100, 0.1f), 50.0f,
        Attr().withStringFromValueFunction([](float v, int) { return juce::String(v, 0) + " %"; })));
    layout.add(std::make_unique<juce::AudioParameterChoice>(ID{"mode", 1}, "Mode",
        juce::StringArray{"Bypass", "Volume", "High-pass", "Low-pass", "Frequency shift"}, 1));
    layout.add(std::make_unique<Float>(ID{"cutoff", 1}, "Cutoff", logarithmic(20.0f, 20000.0f), 8000.0f,
        Attr().withStringFromValueFunction([](float v, int) {
            return juce::String(juce::roundToInt(v)) + " Hz";
        }).withValueFromStringFunction([](const juce::String& s) {
            return s.getFloatValue() * (s.containsIgnoreCase("k") ? 1000.0f : 1.0f);
        })));
    layout.add(std::make_unique<Float>(ID{"resonance", 1}, "Resonance", logarithmic(0.5f, 8.0f), 0.70710678f,
        Attr().withStringFromValueFunction([](float v, int) { return juce::String(v, 2) + " Q"; })));
    for (const auto& id : { "input", "output" })
        layout.add(std::make_unique<Float>(ID{id, 1}, juce::String(id) == "input" ? "Input" : "Output",
            juce::NormalisableRange<float>(-24, 24, 0.1f), 0.0f,
            Attr().withStringFromValueFunction([](float v, int) {
                return (v > 0 ? "+" : "") + juce::String(v, 1) + " dB";
            })));
    layout.add(std::make_unique<juce::AudioParameterBool>(ID{"direction", 1}, "Direction Out", false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction([](bool out, int) {
            return out ? juce::String("Out") : juce::String("In");
        }).withValueFromStringFunction([](const juce::String& s) { return s.equalsIgnoreCase("Out"); })));
    layout.add(std::make_unique<Float>(ID{"retrigger", 1}, "Retrigger Time",
        juce::NormalisableRange<float>(0.0f, 2000.0f, 1.0f, 0.45f), 120.0f,
        Attr().withStringFromValueFunction(timeText).withValueFromStringFunction(timeValue)));
    layout.add(std::make_unique<juce::AudioParameterBool>(ID{"triggerFilter", 1}, "Trigger Filter", false));
    for (const auto* id : {"triggerLowCut", "triggerHighCut"})
    {
        const bool isLow = juce::String(id) == "triggerLowCut";
        layout.add(std::make_unique<Float>(ID{id, 1}, isLow ? "Trigger Low-Cut" : "Trigger High-Cut",
            logarithmic(20.0f, 20000.0f), isLow ? 120.0f : 8000.0f,
            Attr().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + " Hz"; })
                .withValueFromStringFunction([](const juce::String& s) {
                    return s.getFloatValue() * (s.containsIgnoreCase("k") ? 1000.0f : 1.0f);
                })));
    }
    for (const auto* id : {"shiftTop", "shiftBottom"})
        layout.add(std::make_unique<Float>(ID{id, 2}, juce::String(id) == "shiftTop" ? "Shift To" : "Shift From",
            signedFrequency(), 0.0f, Attr().withStringFromValueFunction([](float v, int) {
                const auto value = juce::roundToInt(v);
                return (value > 0 ? "+" : "") + juce::String(value) + " Hz";
            }).withValueFromStringFunction([](const juce::String& s) {
                return s.getFloatValue() * (s.containsIgnoreCase("k") ? 1000.0f : 1.0f);
            })));
    layout.add(std::make_unique<Float>(ID{"mix", 2}, "Master Mix", juce::NormalisableRange<float>(0, 100, 1), 100.0f,
        Attr().withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v)) + " %"; })));
    return layout;
}

SlowBloomProcessor::SlowBloomProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      state(*this, nullptr, "SlowBloomState", makeParameters())
{
    const std::array<const char*, 15> ids {"attack", "sensitivity", "mode", "cutoff", "resonance", "input", "output", "direction",
        "retrigger", "triggerFilter", "triggerLowCut", "triggerHighCut", "shiftTop", "shiftBottom", "mix"};
    for (size_t i = 0; i < ids.size(); ++i) values[i] = state.getRawParameterValue(ids[i]);
    setLatencySamples(0);
}

bloom::Parameters SlowBloomProcessor::readParameters() const
{
    bloom::Parameters p;
    p.attackMs = values[0]->load(); p.sensitivity = values[1]->load();
    p.mode = static_cast<bloom::Mode>(static_cast<int>(values[2]->load()));
    p.cutoffHz = values[3]->load(); p.resonance = values[4]->load();
    p.inputDb = values[5]->load(); p.outputDb = values[6]->load(); p.outward = values[7]->load() > 0.5f;
    p.retriggerMs = values[8]->load(); p.triggerFilterEnabled = values[9]->load() > 0.5f;
    p.triggerLowCutHz = values[10]->load(); p.triggerHighCutHz = values[11]->load();
    p.shiftTopHz = values[12]->load(); p.shiftBottomHz = values[13]->load(); p.mixPercent = values[14]->load();
    return p;
}

void SlowBloomProcessor::prepareToPlay(double sr, int)
{
    engine.setParameters(readParameters()); engine.prepare(sr); hostWasBypassed = false;
    setLatencySamples(0);
}
bool SlowBloomProcessor::isBusesLayoutSupported(const BusesLayout& layout) const
{
    const auto in = layout.getMainInputChannelSet(), out = layout.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}
void SlowBloomProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    engine.setParameters(readParameters());
    if (hostWasBypassed) { engine.reset(); hostWasBypassed = false; }
    float inPeak = 0.0f, outPeak = 0.0f;
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        inPeak = std::max(inPeak, buffer.getMagnitude(c, 0, buffer.getNumSamples()));
    engine.process(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    for (int c = 0; c < buffer.getNumChannels(); ++c)
        outPeak = std::max(outPeak, buffer.getMagnitude(c, 0, buffer.getNumSamples()));
    inputLevel.store(inPeak, std::memory_order_relaxed);
    outputLevel.store(outPeak, std::memory_order_relaxed);
    visualEnvelope.store(engine.getEnvelope(), std::memory_order_relaxed);
    ready.store(engine.isReady(), std::memory_order_relaxed);
}
void SlowBloomProcessor::processBlockBypassed(juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    hostWasBypassed = true;
    visualEnvelope.store(0.0f, std::memory_order_relaxed); ready.store(true, std::memory_order_relaxed);
}
juce::AudioProcessorEditor* SlowBloomProcessor::createEditor() { return new SlowBloomEditor(*this); }
void SlowBloomProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    if (auto xml = state.copyState().createXml()) copyXmlToBinary(*xml, dest);
}
void SlowBloomProcessor::setStateInformation(const void* data, int bytes)
{
    if (auto xml = getXmlFromBinary(data, bytes))
        if (xml->hasTagName(state.state.getType()))
        {
            auto restored = juce::ValueTree::fromXml(*xml);
            // Old sessions lack advanced controls. Restore their defaults even
            // when loading into an instance whose advanced settings were changed.
            for (const auto* id : {"retrigger", "triggerFilter", "triggerLowCut", "triggerHighCut", "shiftTop", "shiftBottom", "mix"})
                if (!restored.getChildWithProperty("id", id).isValid())
                {
                    juce::ValueTree parameter("PARAM");
                    parameter.setProperty("id", id, nullptr);
                    auto* ranged = state.getParameter(id);
                    parameter.setProperty("value", ranged->convertFrom0to1(ranged->getDefaultValue()), nullptr);
                    restored.addChild(parameter, -1, nullptr);
                }
            state.replaceState(restored);
        }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SlowBloomProcessor(); }
