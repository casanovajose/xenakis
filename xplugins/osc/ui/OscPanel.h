#pragma once

#include "xen/EditorContext.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <deque>
#include <string>

namespace xen::osc::ui {

class OscPanel final : public juce::Component {
public:
    explicit OscPanel(EditorContext& context);
    ~OscPanel() override;
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    EditorContext& context_;
    juce::TextButton test_{"Test"};
    juce::Label status_;
    std::deque<std::string> lines_;
    int token_ = 0;
};

}
