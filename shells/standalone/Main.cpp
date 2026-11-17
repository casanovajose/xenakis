#include "xen/Engine.h"
#include "xen/Version.h"
#include "xen/XPluginRegistration.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace {

juce::String pluginList(const xen::Engine& engine) {
    juce::String text;
    text << "Xenakis " << xen::kVersion << "\n";
    text << "Kernel frame. The time grid, transport, and OSC panel are later milestones.\n\n";
    text << "xPlugins\n";
    for (const auto& id : engine.plugins().order()) {
        const auto* plugin = engine.plugins().find(id);
        text << plugin->info().name << "  " << id << "  v" << plugin->info().version << "\n";
    }
    return text;
}

class MainComponent final : public juce::Component {
public:
    explicit MainComponent(juce::String text) : text_(std::move(text)) {}

    void paint(juce::Graphics& graphics) override {
        graphics.fillAll(juce::Colour(0xff141518));
        graphics.setColour(juce::Colour(0xffe6e6e6));
        graphics.setFont(juce::Font(juce::FontOptions(16.0f)));
        graphics.drawFittedText(text_, getLocalBounds().reduced(24), juce::Justification::topLeft, 12);
    }

private:
    juce::String text_;
};

class MainWindow final : public juce::DocumentWindow {
public:
    explicit MainWindow(juce::String text)
        : DocumentWindow("Xenakis", juce::Colour(0xff141518), DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setContentOwned(new MainComponent(std::move(text)), true);
        centreWithSize(720, 420);
        setVisible(true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

} // namespace

class XenakisApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Xenakis"; }
    const juce::String getApplicationVersion() override { return xen::kVersion; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise(const juce::String&) override {
        engine_.setPlugins(xen::createXPlugins());
        engine_.startup();
        window_ = std::make_unique<MainWindow>(pluginList(engine_));
    }

    void shutdown() override {
        window_.reset();
        engine_.shutdown();
    }

private:
    xen::Engine engine_;
    std::unique_ptr<MainWindow> window_;
};

START_JUCE_APPLICATION(XenakisApplication)
