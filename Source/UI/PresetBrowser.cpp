#include "PresetBrowser.h"

namespace strata
{
using namespace juce;

//==============================================================================
PresetBar::PresetBar (UIContext& c) : ctx (c)
{
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    nameButton.setTooltip ("Browse presets");
    save.setTooltip ("Save the current sound as a user preset");
    prev.onClick = [this] { ctx.proc.getPresetManager().previous(); update(); };
    next.onClick = [this] { ctx.proc.getPresetManager().next(); update(); };
    nameButton.onClick = [this] { if (onOpenBrowser) onOpenBrowser(); };
    save.onClick = [this] { if (onSave) onSave(); };
    for (auto* b : { &prev, &next, &nameButton, &save })
        addAndMakeVisible (b);
    update();
}

void PresetBar::update()
{
    nameButton.setButtonText (ctx.proc.getPresetName() + "   |   " + ctx.proc.getPresetCategory());
}

void PresetBar::paint (Graphics&) {}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    prev.setBounds (r.removeFromLeft (30));
    save.setBounds (r.removeFromRight (60));
    r.removeFromRight (6);
    next.setBounds (r.removeFromRight (30));
    r.reduce (4, 0);
    nameButton.setBounds (r);
}

//==============================================================================
PresetBrowser::PresetBrowser (UIContext& c) : ctx (c)
{
    search.setTextToShowWhenEmpty ("Search presets...", colours::textDim);
    search.onTextChange = [this] { rebuild(); };
    categoryList.setRowHeight (28);
    presetList.setRowHeight (26);
    categoryList.setColour (ListBox::backgroundColourId, colours::panel);
    presetList.setColour (ListBox::backgroundColourId, colours::panel);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    folderButton.onClick = [] { PresetManager::getUserPresetFolder().startAsProcess(); };
    deleteButton.onClick = [this]
    {
        const int row = presetList.getSelectedRow();
        if (! isPositiveAndBelow (row, (int) visible.size())) return;
        const int index = visible[(size_t) row];
        const auto& info = ctx.proc.getPresetManager().getAll()[(size_t) index];
        if (info.isFactory)
        {
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Factory preset", "Factory presets can't be deleted.");
            return;
        }
        Component::SafePointer<PresetBrowser> safe (this);
        AlertWindow::showOkCancelBox (MessageBoxIconType::QuestionIcon, "Delete preset",
                                      "Move \"" + info.name + "\" to the trash?", "Delete", "Cancel", nullptr,
                                      ModalCallbackFunction::create ([safe, index] (int result)
                                      {
                                          if (safe == nullptr || result == 0) return;
                                          safe->ctx.proc.getPresetManager().deletePreset (index);
                                          safe->open();
                                      }));
    };

    for (Component* comp : std::initializer_list<Component*> { &categoryList, &presetList, &search, &closeButton, &deleteButton, &folderButton })
        addAndMakeVisible (comp);
}

void PresetBrowser::open()
{
    auto& pm = ctx.proc.getPresetManager();
    pm.refresh();
    categories = StringArray { "All" };
    categories.addArray (pm.getCategories());
    categoryList.updateContent();
    const int catRow = jmax (0, categories.indexOf (ctx.proc.getPresetCategory()));
    categoryList.selectRow (catRow);
    rebuild();
    setVisible (true);
    toFront (true);
    search.grabKeyboardFocus();
}

void PresetBrowser::rebuild()
{
    auto& pm = ctx.proc.getPresetManager();
    const auto cat = categories[jmax (0, categoryList.getSelectedRow())];
    const auto text = search.getText().trim();
    visible.clear();
    const auto& all = pm.getAll();
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& p = all[(size_t) i];
        const bool catOk = cat == "All" || p.category == cat || (cat == "User" && ! p.isFactory);
        const bool textOk = text.isEmpty() || p.name.containsIgnoreCase (text) || p.category.containsIgnoreCase (text);
        if (catOk && textOk) visible.push_back (i);
    }
    presetList.updateContent();
    presetList.deselectAllRows();
    for (int r = 0; r < (int) visible.size(); ++r)
        if (visible[(size_t) r] == pm.getCurrentIndex()) { presetList.selectRow (r, false, true); break; }
    repaint();
}

void PresetBrowser::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.55f));
    g.setColour (colours::panel.brighter (0.03f));
    g.fillRoundedRectangle (panelArea.toFloat(), 8.0f);
    g.setColour (ctx.look.getAccent().withAlpha (0.6f));
    g.drawRoundedRectangle (panelArea.toFloat().reduced (0.5f), 8.0f, 1.2f);

    g.setColour (colours::text);
    g.setFont (StrataLook::font (16.0f, true));
    g.drawText ("PRESETS", panelArea.getX() + 18, panelArea.getY() + 12, 200, 24, Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (12.0f));
    g.drawText (String ((int) ctx.proc.getPresetManager().getAll().size()) + " presets  -  click to audition, double-click to load and close",
                panelArea.getX() + 120, panelArea.getY() + 12, 460, 24, Justification::centredLeft);
}

void PresetBrowser::resized()
{
    panelArea = getLocalBounds().reduced (60, 24);
    auto r = panelArea.reduced (16);
    auto top = r.removeFromTop (30);
    closeButton.setBounds (top.removeFromRight (80));
    top.removeFromRight (8);
    search.setBounds (top.removeFromRight (240));
    r.removeFromTop (10);
    auto bottom = r.removeFromBottom (30);
    folderButton.setBounds (bottom.removeFromLeft (120));
    bottom.removeFromLeft (8);
    deleteButton.setBounds (bottom.removeFromLeft (90));
    r.removeFromBottom (10);
    categoryList.setBounds (r.removeFromLeft (200));
    r.removeFromLeft (10);
    presetList.setBounds (r);
}

void PresetBrowser::CategoryModel::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (selected)
    {
        g.setColour (owner.ctx.look.getAccent().withAlpha (0.2f));
        g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f);
    }
    g.setColour (selected ? colours::text : colours::textDim);
    g.setFont (StrataLook::font (13.0f, selected));
    g.drawText (owner.categories[row], 12, 0, w - 16, h, Justification::centredLeft);
}

void PresetBrowser::PresetModel::paintListBoxItem (int row, Graphics& g, int w, int h, bool selected)
{
    if (! isPositiveAndBelow (row, (int) owner.visible.size())) return;
    const auto& p = owner.ctx.proc.getPresetManager().getAll()[(size_t) owner.visible[(size_t) row]];
    if (selected)
    {
        g.setColour (owner.ctx.look.getAccent().withAlpha (0.2f));
        g.fillRoundedRectangle (2.0f, 1.0f, (float) w - 4.0f, (float) h - 2.0f, 4.0f);
    }
    else if (row % 2 == 1)
    {
        g.setColour (Colours::white.withAlpha (0.02f));
        g.fillRect (0, 0, w, h);
    }
    g.setColour (selected ? colours::text : colours::text.withAlpha (0.85f));
    g.setFont (StrataLook::font (13.0f, selected));
    g.drawText (p.name, 12, 0, w / 2, h, Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (11.5f));
    g.drawText (p.category + (p.isFactory ? "" : "  (user)"), w / 2, 0, w / 2 - 12, h, Justification::centredRight);
}

void PresetBrowser::PresetModel::listBoxItemClicked (int row, const MouseEvent&)
{
    if (! isPositiveAndBelow (row, (int) owner.visible.size())) return;
    owner.ctx.proc.getPresetManager().load (owner.visible[(size_t) row]);
    if (owner.onPresetLoaded) owner.onPresetLoaded();
}

void PresetBrowser::PresetModel::listBoxItemDoubleClicked (int row, const MouseEvent& e)
{
    listBoxItemClicked (row, e);
    if (owner.onClose) owner.onClose();
}
}
