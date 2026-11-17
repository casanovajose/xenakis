#include "LayersPanel.h"
#include "OscPanel.h"
#include "PathsCanvas.h"
#include "TransportBar.h"
#include "TransportXPlugin.h"
#include "xen/EditorContext.h"
#include "xen/Version.h"
#include "xen/XPluginRegistration.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <string>

namespace {

class Inspector final : public juce::Component {
public:
    explicit Inspector(xen::EditorContext& context) : context_(context) {}

    void refresh() {
        lines_.clear();
        if (context_.engine == nullptr || context_.selection.empty()) {
            repaint();
            return;
        }
        const auto& document = context_.engine->document();
        for (const auto& property : context_.engine->properties().all()) {
            const xen::Json* component = document.findComponent(context_.selection, property.ownerId, property.component);
            if (component == nullptr || !component->contains(property.field))
                continue;
            lines_.push_back(property.field + "  " + component->at(property.field).dump());
        }
        repaint();
    }

    void paint(juce::Graphics& graphics) override {
        graphics.fillAll(juce::Colour(0xff1c1e22));
        graphics.setColour(juce::Colour(0xffe6e6e6));
        graphics.drawText("Inspector", 8, 8, getWidth() - 16, 22, juce::Justification::centredLeft);
        graphics.setColour(juce::Colour(0xff8a8f98));
        graphics.drawText(context_.selection.empty() ? "Nothing selected" : context_.selection, 8, 32, getWidth() - 16, 20,
                          juce::Justification::centredLeft, true);
        graphics.setColour(juce::Colour(0xffe6e6e6));
        int y = 58;
        for (const auto& line : lines_) {
            graphics.drawText(line, 8, y, getWidth() - 16, 18, juce::Justification::centredLeft, true);
            y += 18;
            if (y > getHeight() - 8)
                break;
        }
    }

private:
    xen::EditorContext& context_;
    std::vector<std::string> lines_;
};

class Frame final : public juce::Component, private juce::Timer {
public:
    Frame() {
        engine_.setPlugins(xen::createXPlugins());
        engine_.startup();
        context_.engine = &engine_;
        context_.tool = 1;
        context_.changed = [this] { refresh(); };
        context_.transportSeconds = [this] { return transportPlugin().reading().seconds; };
        context_.playheadUnits = [this] {
            const auto reading = transportPlugin().reading();
            return reading.bpmMode ? reading.beats : reading.seconds;
        };
        context_.transportReadout = [this] { return transportPlugin().formatPosition(); };
        context_.bpm = [this] { return transportPlugin().reading().bpm; };

        select_.onClick = [this] { context_.tool = 0; };
        path_.onClick = [this] { context_.tool = 1; };
        cursor_.onClick = [this] { context_.tool = 2; };
        trigger_.onClick = [this] { context_.tool = 3; };
        undo_.onClick = [this] { engine_.undo(); refresh(); };
        redo_.onClick = [this] { engine_.redo(); refresh(); };
        save_.onClick = [this] { save(); };
        load_.onClick = [this] { load(); };
        for (auto* button : {&select_, &path_, &cursor_, &trigger_, &undo_, &redo_, &save_, &load_})
            addAndMakeVisible(button);

        transport_ = std::make_unique<xen::transport::ui::TransportBar>(context_);
        layers_ = std::make_unique<xen::groups::ui::LayersPanel>(context_);
        canvas_ = std::make_unique<xen::paths::ui::PathsCanvas>(context_);
        inspector_ = std::make_unique<Inspector>(context_);
        osc_ = std::make_unique<xen::osc::ui::OscPanel>(context_);
        addAndMakeVisible(transport_.get());
        addAndMakeVisible(layers_.get());
        addAndMakeVisible(canvas_.get());
        addAndMakeVisible(inspector_.get());
        addAndMakeVisible(osc_.get());
        setWantsKeyboardFocus(true);
        startTimerHz(30);
    }

    void resized() override {
        auto area = getLocalBounds();
        auto top = area.removeFromTop(48);
        select_.setBounds(top.removeFromLeft(84).reduced(4));
        path_.setBounds(top.removeFromLeft(72).reduced(4));
        cursor_.setBounds(top.removeFromLeft(84).reduced(4));
        trigger_.setBounds(top.removeFromLeft(88).reduced(4));
        load_.setBounds(top.removeFromRight(64).reduced(4));
        save_.setBounds(top.removeFromRight(64).reduced(4));
        redo_.setBounds(top.removeFromRight(64).reduced(4));
        undo_.setBounds(top.removeFromRight(64).reduced(4));
        transport_->setBounds(top.reduced(4, 0));
        osc_->setBounds(area.removeFromBottom(96));
        layers_->setBounds(area.removeFromLeft(220));
        inspector_->setBounds(area.removeFromRight(260));
        canvas_->setBounds(area);
    }

    void paint(juce::Graphics& graphics) override { graphics.fillAll(juce::Colour(0xff141518)); }

    bool keyPressed(const juce::KeyPress& key) override {
        const auto character = key.getTextCharacter();
        if (key == juce::KeyPress::spaceKey) {
            if (std::string(transportPlugin().reading().state) == "playing")
                engine_.execute("transport.pause", xen::Json::object());
            else
                engine_.execute("transport.play", xen::Json::object());
        } else if (key == juce::KeyPress::returnKey) {
            if (context_.tool == 1)
                canvas_->finishPath();
            else
                engine_.execute("transport.stop", xen::Json::object());
        } else if (character == 'v' || character == 'V') {
            context_.tool = 0;
        } else if (character == 'p' || character == 'P') {
            context_.tool = 1;
        } else if (character == 'c' || character == 'C') {
            context_.tool = 2;
        } else if (character == 't' || character == 'T') {
            context_.tool = 3;
        } else if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) {
            if (!context_.selection.empty())
                engine_.execute("Delete", xen::Json{{"entity", context_.selection}});
        } else if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)) {
            engine_.undo();
        } else if (key == juce::KeyPress('y', juce::ModifierKeys::commandModifier, 0)) {
            engine_.redo();
        } else if (key == juce::KeyPress('s', juce::ModifierKeys::commandModifier, 0)) {
            save();
        } else {
            return false;
        }
        refresh();
        return true;
    }

private:
    const xen::transport::TransportXPlugin& transportPlugin() const {
        const auto* plugin = xen::transport::TransportXPlugin::find(engine_.plugins());
        return *plugin;
    }

    void timerCallback() override {
        engine_.pump();
        refresh();
    }

    void refresh() {
        if (inspector_ != nullptr)
            inspector_->refresh();
        repaint();
    }

    void save() {
        chooser_ = std::make_unique<juce::FileChooser>("Save Xenakis project", juce::File(file_), "*.xenproj");
        const auto mode = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles |
                          juce::FileBrowserComponent::warnAboutOverwriting;
        chooser_->launchAsync(mode, [this](const juce::FileChooser& chooser) {
            const auto result = chooser.getResult();
            if (result == juce::File())
                return;
            file_ = result.getFullPathName().toStdString();
            engine_.saveProject(file_);
        });
    }

    void load() {
        chooser_ = std::make_unique<juce::FileChooser>("Open Xenakis project", juce::File(file_), "*.xenproj");
        const auto mode = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        chooser_->launchAsync(mode, [this](const juce::FileChooser& chooser) {
            const auto result = chooser.getResult();
            if (result == juce::File())
                return;
            file_ = result.getFullPathName().toStdString();
            engine_.loadProject(file_);
            refresh();
        });
    }

    xen::Engine engine_;
    xen::EditorContext context_;
    juce::TextButton select_{"V Select"};
    juce::TextButton path_{"P Path"};
    juce::TextButton cursor_{"C Cursor"};
    juce::TextButton trigger_{"T Trigger"};
    juce::TextButton undo_{"Undo"};
    juce::TextButton redo_{"Redo"};
    juce::TextButton save_{"Save"};
    juce::TextButton load_{"Open"};
    std::unique_ptr<xen::transport::ui::TransportBar> transport_;
    std::unique_ptr<xen::groups::ui::LayersPanel> layers_;
    std::unique_ptr<xen::paths::ui::PathsCanvas> canvas_;
    std::unique_ptr<Inspector> inspector_;
    std::unique_ptr<xen::osc::ui::OscPanel> osc_;
    std::unique_ptr<juce::FileChooser> chooser_;
    std::string file_;
};

class MainWindow final : public juce::DocumentWindow {
public:
    MainWindow()
        : DocumentWindow("Xenakis", juce::Colour(0xff141518), DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        setContentOwned(new Frame(), true);
        centreWithSize(1280, 800);
        setVisible(true);
        getContentComponent()->grabKeyboardFocus();
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

} // namespace

class XenakisApplication final : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "Xenakis"; }
    const juce::String getApplicationVersion() override { return xen::kVersion; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void initialise(const juce::String&) override { window_ = std::make_unique<MainWindow>(); }
    void shutdown() override { window_.reset(); }

private:
    std::unique_ptr<MainWindow> window_;
};

START_JUCE_APPLICATION(XenakisApplication)
