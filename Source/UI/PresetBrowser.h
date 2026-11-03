#pragma once
#include "Controls.h"
#include "../Presets.h"

namespace strata
{
/** Top-bar strip: previous / name / next / save. */
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (UIContext&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void update();

    juce::TextButton prev { "<" }, next { ">" }, nameButton, save { "SAVE" };
    std::function<void()> onOpenBrowser, onSave;

private:
    UIContext& ctx;
};

/** Full-page preset browser with categories and search. Clicking a preset loads it right away. */
class PresetBrowser : public juce::Component
{
public:
    explicit PresetBrowser (UIContext&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void open();
    void rebuild();

    std::function<void()> onClose, onPresetLoaded;

private:
    struct CategoryModel : public juce::ListBoxModel
    {
        PresetBrowser& owner;
        explicit CategoryModel (PresetBrowser& o) : owner (o) {}
        int getNumRows() override { return owner.categories.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void selectedRowsChanged (int) override { owner.rebuild(); }
    };

    struct PresetModel : public juce::ListBoxModel
    {
        PresetBrowser& owner;
        explicit PresetModel (PresetBrowser& o) : owner (o) {}
        int getNumRows() override { return (int) owner.visible.size(); }
        void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;
    };

    UIContext& ctx;
    juce::StringArray categories;
    std::vector<int> visible;
    CategoryModel categoryModel { *this };
    PresetModel presetModel { *this };
    juce::ListBox categoryList { "categories", &categoryModel }, presetList { "presets", &presetModel };
    juce::TextEditor search;
    juce::TextButton closeButton { "CLOSE" }, deleteButton { "DELETE" }, folderButton { "OPEN FOLDER" };
    juce::Rectangle<int> panelArea;
};
}
