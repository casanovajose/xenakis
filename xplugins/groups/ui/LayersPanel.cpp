#include "LayersPanel.h"

namespace xen::groups::ui {
namespace {
constexpr juce::uint32 kPanel = 0xff1c1e22;
constexpr juce::uint32 kText = 0xffe6e6e6;
constexpr juce::uint32 kMuted = 0xff8a8f98;
constexpr juce::uint32 kAccent = 0xff3d8bfd;

std::string kindOf(const Document& document, const std::string& id) {
    if (document.findComponent(id, "xen.groups", "Group") != nullptr)
        return "Group";
    if (document.findComponent(id, "xen.paths", "Path") != nullptr)
        return "Path";
    if (document.findComponent(id, "xen.paths", "Cursor") != nullptr)
        return "Cursor";
    if (document.findComponent(id, "xen.paths", "Trigger") != nullptr)
        return "Trigger";
    return {};
}

std::string nameOf(const Document& document, const std::string& id, const std::string& kind) {
    const char* owner = kind == "Group" ? "xen.groups" : "xen.paths";
    const Json* component = document.findComponent(id, owner, kind);
    if (component != nullptr)
        return component->value("name", id);
    return id;
}
} // namespace

LayersPanel::LayersPanel(EditorContext& context) : context_(context) {
    add_.onClick = [this] {
        if (context_.engine == nullptr)
            return;
        context_.engine->execute("AddGroup", Json{{"name", "Group"}});
        if (context_.changed)
            context_.changed();
    };
    addAndMakeVisible(add_);
}

void LayersPanel::rebuild() {
    rows_.clear();
    if (context_.engine == nullptr)
        return;
    const Document& document = context_.engine->document();
    for (const auto& id : document.entities()) {
        const Json* group = document.findComponent(id, "xen.groups", "Group");
        if (group == nullptr)
            continue;
        if (const Json* member = document.findComponent(id, "xen.groups", "GroupMember")) {
            if (member->contains("parent"))
                continue;
        }
        Row row;
        row.id = id;
        row.label = id + "  " + nameOf(document, id, "Group");
        row.mute = group->value("mute", false);
        rows_.push_back(row);
        if (!group->contains("children"))
            continue;
        for (const auto& child : group->at("children")) {
            if (!child.is_string())
                continue;
            const auto childId = child.get<std::string>();
            const auto kind = kindOf(document, childId);
            if (kind.empty())
                continue;
            Row childRow;
            childRow.id = childId;
            childRow.label = childId + "  " + nameOf(document, childId, kind);
            childRow.depth = 1;
            rows_.push_back(childRow);
            if (kind != "Path")
                continue;
            for (const auto& other : document.entities()) {
                const Json* cursor = document.findComponent(other, "xen.paths", "Cursor");
                const Json* trigger = document.findComponent(other, "xen.paths", "Trigger");
                const Json* owner = cursor != nullptr ? cursor : trigger;
                if (owner == nullptr || owner->value("path", std::string()) != childId)
                    continue;
                Row leaf;
                leaf.id = other;
                leaf.depth = 2;
                leaf.label = other + "  " + owner->value("name", other);
                rows_.push_back(leaf);
            }
        }
    }
}

void LayersPanel::paint(juce::Graphics& graphics) {
    rebuild();
    graphics.fillAll(juce::Colour(kPanel));
    graphics.setColour(juce::Colour(kMuted));
    graphics.drawText("Layers", 8, 36, getWidth() - 16, 20, juce::Justification::centredLeft);
    int y = 58;
    for (const auto& row : rows_) {
        const bool selected = row.id == context_.selection;
        if (selected) {
            graphics.setColour(juce::Colour(0xff243044));
            graphics.fillRect(0, y, getWidth(), 22);
        }
        graphics.setColour(selected ? juce::Colour(kAccent) : juce::Colour(kText));
        auto text = row.label;
        if (row.mute)
            text += "  M";
        graphics.drawText(text, 8 + row.depth * 16, y, getWidth() - 16, 22, juce::Justification::centredLeft, true);
        y += 22;
    }
}

void LayersPanel::resized() {
    add_.setBounds(8, 8, getWidth() - 16, 24);
}

void LayersPanel::mouseDown(const juce::MouseEvent& event) {
    const int index = (event.y - 58) / 22;
    if (index < 0 || index >= static_cast<int>(rows_.size()))
        return;
    context_.selection = rows_[static_cast<std::size_t>(index)].id;
    if (context_.changed)
        context_.changed();
}

} // namespace xen::groups::ui
