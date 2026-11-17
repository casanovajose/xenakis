#pragma once

#include "xen/EditorContext.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <string>
#include <vector>

namespace xen::groups::ui {

class LayersPanel final : public juce::Component {
public:
    explicit LayersPanel(EditorContext& context);
    void paint(juce::Graphics& graphics) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

private:
    struct Row {
        std::string id;
        std::string label;
        int depth = 0;
        bool mute = false;
    };

    void rebuild();

    EditorContext& context_;
    juce::TextButton add_{"Group"};
    std::vector<Row> rows_;
};

}
