#pragma once

#include "xen/EditorContext.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace xen::transport::ui {

class TransportBar final : public juce::Component {
public:
    explicit TransportBar(EditorContext& context);
    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    void commitBpm();

    EditorContext& context_;
    juce::TextButton play_{"Play"};
    juce::TextButton pause_{"Pause"};
    juce::TextButton stop_{"Stop"};
    juce::TextButton loop_{"Loop"};
    juce::Label readout_;
    juce::TextEditor bpm_;
    juce::Label bpmLabel_{"bpm", "BPM"};
};

}
