#include "OscPanel.h"

namespace xen::osc::ui {
namespace {
constexpr juce::uint32 kPanel = 0xff1c1e22;
constexpr juce::uint32 kText = 0xffe6e6e6;
constexpr juce::uint32 kMuted = 0xff8a8f98;
} // namespace

OscPanel::OscPanel(EditorContext& context) : context_(context) {
    addAndMakeVisible(test_);
    test_.onClick = [this] {
        if (context_.engine != nullptr)
            context_.engine->execute("osc.test", Json::object());
        if (context_.changed)
            context_.changed();
    };
    status_.setColour(juce::Label::textColourId, juce::Colour(kMuted));
    status_.setText("127.0.0.1:9001", juce::dontSendNotification);
    addAndMakeVisible(status_);
    if (context_.engine != nullptr) {
        token_ = context_.engine->events().subscribe("osc.sent", [this](const Event& event) {
            lines_.push_back(event.payload.value("text", std::string()));
            while (lines_.size() > 8)
                lines_.pop_front();
        });
    }
}

OscPanel::~OscPanel() {
    if (context_.engine != nullptr && token_ != 0)
        context_.engine->events().unsubscribe(token_);
}

void OscPanel::paint(juce::Graphics& graphics) {
    graphics.fillAll(juce::Colour(kPanel));
    graphics.setColour(juce::Colour(kText));
    graphics.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::plain)));
    auto row = getLocalBounds().reduced(8).withTrimmedLeft(280);
    for (auto it = lines_.rbegin(); it != lines_.rend() && row.getHeight() > 0; ++it) {
        graphics.drawText(*it, row.removeFromTop(18), juce::Justification::centredLeft, true);
    }
}

void OscPanel::resized() {
    auto row = getLocalBounds().reduced(8, 6);
    test_.setBounds(row.removeFromLeft(72));
    row.removeFromLeft(8);
    status_.setBounds(row.removeFromLeft(180).removeFromTop(28));
}

}
