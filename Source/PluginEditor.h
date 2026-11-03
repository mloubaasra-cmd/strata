#pragma once
#include "PluginProcessor.h"
#include "UI/LookAndFeel.h"
#include "UI/Controls.h"
#include "UI/Pages.h"
#include "UI/PresetBrowser.h"
#include "UI/Tutorial.h"

namespace strata
{
/** Everything visible, laid out at a fixed 1100 x 712 and scaled by the editor. */
class MainContent : public juce::Component
{
public:
    static constexpr int kWidth = 1100, kHeight = 712;

    MainContent (StrataProcessor&, StrataLook&);
    ~MainContent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void setTab (int tab);
    void startTutorial();
    TutorialOverlay& getTutorial() { return tutorial; }
    void showSaveDialog();

    UIContext ctx;

private:
    void buildTutorial();
    void knobStylesChanged();

    juce::TextButton synthTab { "SYNTH" }, matrixTab { "MATRIX" }, fxTab { "FX" };
    PresetBar presetBar;
    juce::ComboBox knobStyleBox, accentBox;
    juce::TextButton tourButton { "? TOUR" };

    SynthPage synth;
    MatrixPage matrix;
    FxPage fxPage;
    juce::MidiKeyboardComponent keyboard;
    PresetBrowser browser;
    TutorialOverlay tutorial;

    int currentTab = 0;
    int lastVersion = -1;
    juce::String lastPresetName;
};

class StrataEditor : public juce::AudioProcessorEditor,
                     public juce::DragAndDropContainer,
                     private juce::Timer
{
public:
    explicit StrataEditor (StrataProcessor&);
    ~StrataEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    StrataProcessor& proc;
    StrataLook look;
    MainContent content;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrataEditor)
};
}
