#pragma once
#include "../Common.h"
#include "../PluginProcessor.h"
#include "LookAndFeel.h"

namespace strata
{
class ModKnob;

/** Shared by every UI component of one editor instance. */
struct UIContext
{
    StrataProcessor& proc;
    StrataLook& look;
    std::vector<ModKnob*> knobs;
    std::function<void()> onKnobStylesChanged;
};

using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

/** Rotary slider with right-click menu and Alt-drag (adjusts modulation depth). */
class ModSlider : public juce::Slider
{
public:
    ModSlider();
    std::function<void (const juce::MouseEvent&)> onRightClick;
    std::function<bool()> canAltDrag;
    std::function<void (float delta)> onAltDrag;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool altDragging = false;
    float lastY = 0.0f;
};

/** A labelled parameter knob that accepts modulation sources via drag-and-drop. */
class ModKnob : public juce::Component,
                public juce::DragAndDropTarget
{
public:
    ModKnob (UIContext&, const juce::String& paramID, const juce::String& label);
    ~ModKnob() override;

    void resized() override;
    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragEnter (const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit (const SourceDetails&) override  { dragOver = false; repaint(); }
    void itemDropped (const SourceDetails&) override;

    void refreshMods();
    void refreshStyle();
    void tick();
    bool hasModulation() const { return ! myMods.empty(); }

    const juce::String paramID;
    const int dest;
    ModSlider slider;

private:
    void showMenu();
    juce::Rectangle<float> knobArea() const;

    UIContext& ctx;
    juce::String label;
    std::unique_ptr<SliderAttachment> attachment;
    juce::RangedAudioParameter* param = nullptr;
    struct MyMod { int index, src; float amount; };
    std::vector<MyMod> myMods;
    bool dragOver = false;
    float lastDisplay = -1.0f;
};

/** Draggable chip representing a modulation source; also works as a tab button. */
class SourceChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    SourceChip (int source, const juce::String& text);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;
    bool selected = false;
    const int source;

private:
    juce::String text;
    bool dragging = false;
};

/** Background panel with an optional title. */
class Panel : public juce::Component
{
public:
    explicit Panel (const juce::String& title = {}) : title (title) {}
    void paint (juce::Graphics&) override;
    juce::String title;
    int titleIndent = 34;
};

/** Small helper: a combo box filled from a choice parameter and attached to it. */
struct ParamCombo
{
    juce::ComboBox box;
    std::unique_ptr<ComboAttachment> attachment;
    void attach (StrataProcessor& p, const juce::String& id);
};

struct ParamToggle
{
    juce::ToggleButton button;
    std::unique_ptr<ButtonAttachment> attachment;
    void attach (StrataProcessor& p, const juce::String& id, const juce::String& text = {});
};

void drawSectionLabel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& text);
}
