#include "TransportBar.h"

#include "TransportXPlugin.h"

namespace xen::transport::ui {
namespace {
constexpr juce::uint32 kPanel = 0xff1c1e22;
constexpr juce::uint32 kText = 0xffe6e6e6;
constexpr juce::uint32 kAccent = 0xff3d8bfd;
} // namespace

TransportBar::TransportBar(EditorContext& context) : context_(context) {
    for (auto* button : {&play_, &pause_, &stop_, &loop_}) {
        addAndMakeVisible(button);
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(kPanel));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(kText));
    }
    play_.onClick = [this] {
        if (context_.engine != nullptr)
            context_.engine->execute("transport.play", Json::object());
        if (context_.changed)
            context_.changed();
    };
    pause_.onClick = [this] {
        if (context_.engine != nullptr)
            context_.engine->execute("transport.pause", Json::object());
        if (context_.changed)
            context_.changed();
    };
    stop_.onClick = [this] {
        if (context_.engine != nullptr)
            context_.engine->execute("transport.stop", Json::object());
        if (context_.changed)
            context_.changed();
    };
    loop_.onClick = [this] {
        if (context_.engine != nullptr)
            context_.engine->execute("transport.toggleLoop", Json::object());
        if (context_.changed)
            context_.changed();
    };
    readout_.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 16.0f, juce::Font::plain)));
    readout_.setColour(juce::Label::textColourId, juce::Colour(kAccent));
    readout_.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(readout_);
    bpm_.setText("120", juce::dontSendNotification);
    bpm_.setInputRestrictions(3, "0123456789");
    bpm_.onReturnKey = [this] { commitBpm(); };
    bpm_.onFocusLost = [this] { commitBpm(); };
    addAndMakeVisible(bpm_);
    bpmLabel_.setColour(juce::Label::textColourId, juce::Colour(kText));
    addAndMakeVisible(bpmLabel_);
}

void TransportBar::commitBpm() {
    if (context_.engine == nullptr)
        return;
    const int bpm = bpm_.getText().getIntValue();
    if (bpm < 20 || bpm > 999)
        return;
    context_.engine->setProperty("transport", "xen.transport.bpm", bpm);
    if (context_.changed)
        context_.changed();
}

void TransportBar::paint(juce::Graphics& graphics) {
    graphics.fillAll(juce::Colour(kPanel));
    if (context_.transportReadout)
        readout_.setText(context_.transportReadout(), juce::dontSendNotification);
    else
        readout_.setText("0001:1:000", juce::dontSendNotification);
}

void TransportBar::resized() {
    auto row = getLocalBounds().reduced(8, 6);
    play_.setBounds(row.removeFromLeft(64));
    row.removeFromLeft(4);
    pause_.setBounds(row.removeFromLeft(72));
    row.removeFromLeft(4);
    stop_.setBounds(row.removeFromLeft(64));
    row.removeFromLeft(12);
    readout_.setBounds(row.removeFromLeft(140));
    loop_.setBounds(row.removeFromRight(64));
    row.removeFromRight(8);
    bpm_.setBounds(row.removeFromRight(56));
    bpmLabel_.setBounds(row.removeFromRight(40));
}

}
