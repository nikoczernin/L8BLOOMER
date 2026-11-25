#include "PluginEditor.h"

namespace
{
const juce::Colour bg(0xff111b20), panel(0xff19282e), ink(0xffedf4ed), muted(0xff94aaa8), accent(0xffc8f08c);
juce::Font font(float size, bool bold = false)
{
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
}
BloomLookAndFeel::BloomLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, ink);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::textColourOffId, muted);
    setColour(juce::TextButton::textColourOnId, bg);
    setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.3f));
    setColour(juce::TooltipWindow::backgroundColourId, panel);
    setColour(juce::TooltipWindow::textColourId, ink);
}
void BloomLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                      float value, float start, float end, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(width), static_cast<float>(height)).reduced(10.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle = start + value * (end - start);
    juce::Path track, fill;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, start, end, true);
    const bool bipolar = slider.getProperties().getWithDefault("bipolar", false);
    const auto origin = bipolar ? (start + end) * 0.5f : start;
    fill.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, std::min(origin, angle), std::max(origin, angle), true);
    const float opacity = slider.isEnabled() ? 1.0f : 0.25f;
    g.setColour(muted.withAlpha(0.18f)); g.strokePath(track, juce::PathStrokeType(3.0f));
    g.setColour(accent.withAlpha(opacity)); g.strokePath(fill, juce::PathStrokeType(3.0f));
    if (bipolar)
    {
        g.setColour(muted.withAlpha(opacity));
        g.drawLine(centre.x, centre.y - radius - 5.0f, centre.x, centre.y - radius + 2.0f, 1.5f);
    }
    auto body = juce::Rectangle<float>(radius * 1.58f, radius * 1.58f).withCentre(centre);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3c4c50), body.getTopLeft(),
                                         juce::Colour(0xff202d32), body.getBottomRight(), false));
    g.fillEllipse(body);
    g.setColour(juce::Colours::black.withAlpha(0.35f)); g.drawEllipse(body, 1.0f);
    auto point = centre.getPointOnCircumference(radius * 0.62f, angle);
    auto inner = centre.getPointOnCircumference(radius * 0.38f, angle);
    g.setColour(ink.withAlpha(opacity)); g.drawLine({inner, point}, 2.5f);
}
void BloomLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                          const juce::Colour&, bool hover, bool down)
{
    auto colour = button.getToggleState() ? accent : panel;
    if (hover || down) colour = colour.brighter(0.12f);
    g.setColour(colour); g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 7.0f);
    if (!button.getToggleState())
    {
        g.setColour(muted.withAlpha(0.22f));
        g.drawRoundedRectangle(button.getLocalBounds().toFloat().reduced(0.5f), 7.0f, 1.0f);
    }
}
SlowBloomEditor::SlowBloomEditor(SlowBloomProcessor& p) : AudioProcessorEditor(p), audioProcessor(p)
{
    setLookAndFeel(&look);
    const std::array<const char*, 10> ids {"attack", "sensitivity", "cutoff", "resonance", "input", "output",
        "retrigger", "triggerLowCut", "triggerHighCut", "mix"};
    const std::array<const char*, 10> names {"ATTACK", "SENSITIVITY", "CUTOFF", "RESONANCE", "INPUT", "OUTPUT",
        "RETRIGGER TIME", "TRIGGER LOW-CUT", "TRIGGER HIGH-CUT", "MIX"};
    const std::array<const char*, 10> tips {
        "Duration of the one-way sweep: 1 millisecond to 20 seconds. Double-click to reset; type a value below.",
        "Required jump above the recent input level. Higher sensitivity detects smaller transients, even over amp noise or sustain.",
        "Open end of the filter sweep, 20 Hz to 20 kHz. The other end is 20 Hz (LP) or 20 kHz (HP).",
        "Filter resonance: 0.50 to 8.00 Q. Higher settings emphasise the moving cutoff frequency.",
        "Input gain before the trigger detector and effect. Bypass ignores this control.",
        "Output gain after the effect. Reduce it if the output meter clips. Bypass ignores this control.",
        "Minimum time between accepted triggers, 0 to 2000 ms. Increase this to suppress double triggers. Blocked picks are discarded.",
        "Ignore low-frequency energy in the trigger detector. Try 1000 Hz to focus on pick noise. Does not EQ the audio output.",
        "Ignore high-frequency energy in the trigger detector. Must be above Low-Cut. Does not EQ the audio output.",
        "Master dry/wet blend: 0% dry, 100% effect. Input and Output trims apply to both. Double-click resets to 100%."};
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 110, 27);
        k.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        k.setName(names[i]); k.setTooltip(tips[i]); addAndMakeVisible(k);
        labels[i].setText(names[i], juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setFont(font(11.5f, true)); labels[i].setColour(juce::Label::textColourId, muted);
        addAndMakeVisible(labels[i]);
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.state, ids[i], k);
        auto* param = p.state.getParameter(ids[i]);
        k.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
    }
    const std::array<const char*, 5> titles {"BYPASS", "VOLUME", "HIGH-PASS", "LOW-PASS", "FREQ SHIFT"};
    for (size_t i = 0; i < modes.size(); ++i)
    {
        auto& button = modes[i]; button.setButtonText(titles[i]); addAndMakeVisible(button);
        button.onClick = [this, i] {
            auto* parameter = audioProcessor.state.getParameter("mode");
            parameter->beginChangeGesture(); parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(i)));
            parameter->endChangeGesture(); updateMode();
        };
    }
    direction.setClickingTogglesState(true); direction.setButtonText("DIRECTION: IN");
    direction.setTooltip("IN: volume 0 to full, filter closed to cutoff, or frequency FROM to TO. OUT reverses the sweep.");
    addAndMakeVisible(direction);
    directionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "direction", direction);
    triggerFilter.setClickingTogglesState(true);
    triggerFilter.setName("Trigger Filter");
    triggerFilter.setTooltip("Enable frequency filtering for the trigger detector only. OFF skips the extra filters entirely.");
    addAndMakeVisible(triggerFilter);
    triggerFilterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.state, "triggerFilter", triggerFilter);
    advancedToggle.setName("Advanced"); advancedToggle.setButtonText("ADVANCED  -");
    advancedToggle.onClick = [this] {
        advancedOpen = !advancedOpen;
        advancedToggle.setButtonText(advancedOpen ? "ADVANCED  -" : "ADVANCED  +");
        setSize(820, advancedOpen ? 750 : 580);
    };
    addAndMakeVisible(advancedToggle);
    setSize(820, 750); updateMode(); startTimerHz(30);
}
SlowBloomEditor::~SlowBloomEditor() { stopTimer(); setLookAndFeel(nullptr); }
void SlowBloomEditor::resized()
{
    for (size_t i = 0; i < modes.size(); ++i) modes[i].setBounds(32 + static_cast<int>(i) * 110, 103, 102, 38);
    direction.setBounds(594, 103, 194, 38);
    const std::array<int, 6> x {34, 222, 410, 598, 426, 550};
    for (size_t i = 0; i < 4; ++i)
    {
        labels[i].setBounds(x[i], 168, 184, 22);
        knobs[i].setBounds(x[i] + 12, 194, 160, 160);
    }
    for (size_t i = 4; i < 6; ++i)
    {
        labels[i].setBounds(x[i], 379, 108, 20);
        knobs[i].setBounds(x[i], 400, 108, 112);
        knobs[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 106, 27);
    }
    labels[9].setBounds(674, 379, 108, 20); knobs[9].setBounds(674, 400, 108, 112);
    knobs[9].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 106, 27);
    advancedToggle.setBounds(644, 538, 144, 28);
    const std::array<int, 3> advancedX {50, 473, 635};
    for (size_t i = 6; i < 9; ++i)
    {
        labels[i].setBounds(advancedX[i - 6] - 8, 591, 152, 20);
        knobs[i].setBounds(advancedX[i - 6], 615, 136, 112);
        labels[i].setVisible(advancedOpen); knobs[i].setVisible(advancedOpen);
    }
    triggerFilter.setBounds(221, 599, 207, 32); triggerFilter.setVisible(advancedOpen);
}
void SlowBloomEditor::bindEndpointKnobs(bool frequencyMode)
{
    for (size_t i = 2; i <= 3; ++i)
    {
        const auto* id = frequencyMode ? (i == 2 ? "shiftBottom" : "shiftTop") : (i == 2 ? "cutoff" : "resonance");
        const auto* title = frequencyMode ? (i == 2 ? "FROM" : "TO") : (i == 2 ? "CUTOFF" : "RESONANCE");
        knobAttachments[i].reset();
        knobs[i].setName(title); labels[i].setText(title, juce::dontSendNotification);
        knobs[i].getProperties().set("bipolar", frequencyMode);
        knobs[i].setTooltip(frequencyMode
            ? "Signed frequency-shift endpoint, -5000 to +5000 Hz. Zero is at 12 o'clock. Double-click resets to zero. IN sweeps FROM to TO."
            : (i == 2 ? "Open endpoint of the filter sweep. Double-click resets to 8000 Hz."
                      : "Filter resonance, 0.5 to 8 Q. Double-click resets to 0.707 Q."));
        knobAttachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.state, id, knobs[i]);
        auto* parameter = audioProcessor.state.getParameter(id);
        knobs[i].setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    }
}
void SlowBloomEditor::updateMode()
{
    const int nextMode = static_cast<int>(audioProcessor.state.getRawParameterValue("mode")->load());
    if (selectedMode < 0 || (nextMode == 4) != (selectedMode == 4)) bindEndpointKnobs(nextMode == 4);
    selectedMode = nextMode;
    for (size_t i = 0; i < modes.size(); ++i) modes[i].setToggleState(static_cast<int>(i) == selectedMode, juce::dontSendNotification);
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i].setEnabled(selectedMode != 0 && ((i != 2 && i != 3) || selectedMode >= 2)
            && (i < 7 || i == 9 || triggerFilter.getToggleState()));
    direction.setEnabled(selectedMode != 0);
    triggerFilter.setEnabled(selectedMode != 0);
    triggerFilter.setButtonText(triggerFilter.getToggleState() ? "TRIGGER FILTER: ON" : "TRIGGER FILTER: OFF");
}
void SlowBloomEditor::timerCallback()
{
    updateMode();
    direction.setButtonText(direction.getToggleState() ? "DIRECTION: OUT" : "DIRECTION: IN");
    displayIn = std::max(audioProcessor.inputLevel.load(std::memory_order_relaxed), displayIn * 0.9f);
    displayOut = std::max(audioProcessor.outputLevel.load(std::memory_order_relaxed), displayOut * 0.9f);
    repaint();
}
void SlowBloomEditor::drawEnvelope(juce::Graphics& g, juce::Rectangle<float> box)
{
    g.setColour(panel); g.fillRoundedRectangle(box, 10.0f);
    g.setColour(muted); g.setFont(font(10.5f, true));
    g.drawText(selectedMode == 4 ? "FREQUENCY / HOLD" : "ONE SHOT / HOLD", box.reduced(16, 10).removeFromTop(17), juce::Justification::left);
    const bool isReady = audioProcessor.ready.load(std::memory_order_relaxed);
    g.setColour(selectedMode == 0 ? muted : isReady ? accent : juce::Colour(0xffefbb80));
    auto status = selectedMode == 0 ? "BYPASSED" : isReady ? "LISTENING" : "WAIT";
    g.drawText(status, box.reduced(16, 10).removeFromTop(17), juce::Justification::right);
    auto plot = box.reduced(18.0f, 0.0f).withTrimmedTop(40.0f).withTrimmedBottom(24.0f);
    g.setColour(muted.withAlpha(0.14f));
    for (int i = 0; i <= 4; ++i)
        g.drawHorizontalLine(static_cast<int>(plot.getY() + plot.getHeight() * static_cast<float>(i) / 4.0f), plot.getX(), plot.getRight());
    const bool reverse = direction.getToggleState();
    const auto bottom = audioProcessor.state.getRawParameterValue("shiftBottom")->load();
    const auto top = audioProcessor.state.getRawParameterValue("shiftTop")->load();
    const auto extent = std::max({1.0f, std::abs(bottom), std::abs(top)});
    const auto plotPosition = [&](float position) {
        return selectedMode == 4 ? 0.5f + 0.42f * (bottom + position * (top - bottom)) / extent : position;
    };
    juce::Path curve;
    for (int i = 0; i <= 100; ++i)
    {
        const float x = static_cast<float>(i) / 100.0f;
        const float t = std::min(1.0f, x / 0.82f);
        float y = t * t * (3.0f - 2.0f * t);
        if (reverse) y = 1.0f - y;
        y = plotPosition(y);
        const auto point = juce::Point<float>(plot.getX() + x * plot.getWidth(), plot.getBottom() - y * plot.getHeight());
        if (i == 0) curve.startNewSubPath(point); else curve.lineTo(point);
    }
    g.setColour(accent.withAlpha(selectedMode == 0 ? 0.2f : 0.85f)); g.strokePath(curve, juce::PathStrokeType(2.0f));
    float progress = audioProcessor.visualEnvelope.load(std::memory_order_relaxed);
    // Invert smoothstep for a marker that follows the displayed envelope curve.
    const float t = 0.5f - std::sin(std::asin(1.0f - 2.0f * juce::jlimit(0.0f, 1.0f, progress)) / 3.0f);
    const float y = plotPosition(reverse ? 1.0f - progress : progress);
    if (selectedMode != 0)
    {
        g.setColour(ink); g.fillEllipse(plot.getX() + t * 0.82f * plot.getWidth() - 4.0f,
                                      plot.getBottom() - y * plot.getHeight() - 4.0f, 8, 8);
    }
    g.setColour(muted); g.setFont(font(10));
    const auto startText = selectedMode == 4 ? juce::String(juce::roundToInt(reverse ? top : bottom)) + " Hz" : juce::String("TRIGGER");
    const auto endText = selectedMode == 4 ? juce::String(juce::roundToInt(reverse ? bottom : top)) + " Hz" : juce::String("HOLD");
    g.drawText(startText, box.withTrimmedTop(box.getHeight() - 19).reduced(18, 0), juce::Justification::left);
    g.drawText(endText, box.withTrimmedTop(box.getHeight() - 19).reduced(18, 0), juce::Justification::right);
}
void SlowBloomEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg);
    g.setColour(accent); g.setFont(font(30, true)); g.drawText("L8BLOOMER", 32, 24, 300, 38, juce::Justification::left);
    g.setColour(muted); g.setFont(font(11)); g.drawText("AUTO SWELL  /  VOLUME + FILTER + SHIFT", 34, 65, 410, 17, juce::Justification::left);
    g.setFont(font(10, true)); g.drawText("NIKO AUDIO", 620, 30, 168, 20, juce::Justification::right);
    g.setColour(accent.withAlpha(0.8f)); g.drawText("0 SAMPLES LATENCY", 610, 57, 178, 18, juce::Justification::right);
    g.setColour(muted.withAlpha(0.14f)); g.drawHorizontalLine(365, 32, 788);
    drawEnvelope(g, {32, 382, 368, 132});
    for (int i = 0; i < 2; ++i)
    {
        const float level = i == 0 ? displayIn : displayOut;
        auto bar = juce::Rectangle<float>(static_cast<float>(i == 0 ? 435 : 559), 518, 90, 3);
        g.setColour(muted.withAlpha(0.16f)); g.fillRoundedRectangle(bar, 1.5f);
        g.setColour(level >= 1.0f ? juce::Colour(0xfff38e7b) : accent);
        const float db = juce::Decibels::gainToDecibels(level, -60.0f);
        g.fillRoundedRectangle(bar.withWidth(bar.getWidth() * juce::jlimit(0.0f, 1.0f, (db + 60) / 60)), 1.5f);
    }
    g.setColour(muted); g.setFont(font(11));
    const juce::String hint = selectedMode == 0 ? "True bypass. Original audio, unchanged." :
        selectedMode == 1 ? "Pick to restart the sweep. Sensitivity sets the required jump in level." :
        selectedMode == 4 ? "IN: FROM to TO. OUT: TO to FROM. Pick to restart the frequency sweep." :
        "Pick to restart the filter sweep. Cutoff sets its open endpoint.";
    g.drawText(hint, 32, 540, 600, 18, juce::Justification::left);
    if (advancedOpen)
    {
        g.setColour(panel); g.fillRoundedRectangle(32, 580, 756, 158, 10.0f);
        g.setColour(muted); g.setFont(font(11));
        g.drawText("Filters affect triggering only.", 221, 643, 236, 18, juce::Justification::left);
        g.drawText("Low-cut rejects rumble.", 221, 666, 236, 18, juce::Justification::left);
        g.drawText("High-cut rejects hiss.", 221, 685, 236, 18, juce::Justification::left);
        if (triggerFilter.getToggleState() && knobs[7].getValue() >= knobs[8].getValue())
        {
            g.setColour(juce::Colour(0xfff38e7b));
            g.drawText("Set Low-Cut below High-Cut.", 221, 711, 236, 18, juce::Justification::left);
        }
    }
}
