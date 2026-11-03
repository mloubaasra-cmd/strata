#pragma once
#include "Controls.h"
#include "Editors.h"

namespace strata
{
//==============================================================================
class OscPanel : public Panel
{
public:
    OscPanel (UIContext&, int osc);
    void resized() override;

    ParamToggle power;
    ParamCombo bank, warpMode;
    juce::TextButton loadButton { "LOAD" };
    WavetableDisplay display;
    ModKnob pos, warp, oct, semi, fine, level, uni, detune, blend, width, pan;

private:
    UIContext& ctx;
    const int osc;
    std::unique_ptr<juce::FileChooser> chooser;
};

class SubNoisePanel : public Panel
{
public:
    explicit SubNoisePanel (UIContext&);
    void resized() override;
    void paint (juce::Graphics&) override;

    ParamToggle subPower, noisePower;
    ParamCombo subWave;
    ModKnob subOct, subLevel, noiseColor, noiseLevel;
};

class FilterPanel : public Panel
{
public:
    explicit FilterPanel (UIContext&);
    void resized() override;
    void paint (juce::Graphics&) override;

    void tick();

    ParamToggle power;
    ParamCombo type;
    ModKnob cutoff, res, drive, envAmt, keyTrack;

private:
    float currentCutoffNorm() const;
    UIContext& ctx;
    juce::Rectangle<int> graphArea;
    float lastCut = -1.0f, lastRes = -1.0f;
    int lastType = -1;
    bool lastOn = false;
};

class EnvPanel : public Panel
{
public:
    explicit EnvPanel (UIContext&);
    void resized() override;
    void select (int e);
    void tick() { editor.tick(); }

    juce::OwnedArray<SourceChip> chips;
    EnvelopeEditor editor;
    juce::OwnedArray<ModKnob> knobs;   // 3 envelopes x 4
    int selected = 0;
};

class LfoPanel : public Panel
{
public:
    explicit LfoPanel (UIContext&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void select (int l);
    void tick() { editor.tick(); }

    juce::OwnedArray<SourceChip> chips;
    LfoEditor editor;
    juce::OwnedArray<ModKnob> rates;
    juce::OwnedArray<ParamCombo> shapes, divs;
    juce::OwnedArray<ParamToggle> syncs, retrigs;
    int selected = 0;
};

class GlobalPanel : public Panel
{
public:
    explicit GlobalPanel (UIContext&);
    void resized() override;
    void paint (juce::Graphics&) override;

    juce::OwnedArray<SourceChip> macroChips, otherChips;
    juce::OwnedArray<ModKnob> macros;
    ParamCombo voiceMode;
    ModKnob glide, bend, master;
};

class SynthPage : public juce::Component
{
public:
    explicit SynthPage (UIContext&);
    void resized() override;
    void tick();

    OscPanel oscA, oscB;
    SubNoisePanel subNoise;
    FilterPanel filter;
    EnvPanel envs;
    LfoPanel lfos;
    GlobalPanel global;
};

//==============================================================================
class MatrixPage : public juce::Component
{
public:
    explicit MatrixPage (UIContext&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();

    juce::TextButton addButton { "+ ADD SLOT" }, clearButton { "CLEAR ALL" };

private:
    struct Row : public juce::Component
    {
        juce::Label index;
        juce::ComboBox src, dst;
        juce::Slider amount;
        juce::Label value;
        juce::TextButton remove { "x" };
        void resized() override;
        void paint (juce::Graphics&) override;
    };

    UIContext& ctx;
    juce::OwnedArray<Row> rows;
    int numVisible = 0;
};

//==============================================================================
class FxOrderList : public juce::Component
{
public:
    explicit FxOrderList (UIContext&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> rowBounds (int i) const;
    int rowAt (int y) const;
    UIContext& ctx;
    int dragFrom = -1, dropAt = -1;
};

class FxPage : public juce::Component
{
public:
    explicit FxPage (UIContext&);
    void resized() override;
    void tick() { orderList.repaint(); }

    FxOrderList orderList;

private:
    struct FxPanel : public Panel
    {
        FxPanel (UIContext&, const juce::String& title, const juce::String& powerId, const juce::String& blurb);
        void resized() override;
        void paint (juce::Graphics&) override;
        juce::String blurb;
        ParamToggle power;
        juce::OwnedArray<ModKnob> knobs;
        juce::OwnedArray<ParamCombo> combos;
        juce::OwnedArray<ParamToggle> toggles;
        UIContext& ctx;
        ModKnob& knob (const juce::String& id, const juce::String& label);
        ParamCombo& combo (const juce::String& id);
        ParamToggle& toggle (const juce::String& id, const juce::String& text);
    };

    juce::OwnedArray<FxPanel> panels;
};
}
