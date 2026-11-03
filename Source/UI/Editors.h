#pragma once
#include "Controls.h"

namespace strata
{
/** 3D stacked view of an oscillator's wavetable. Drag sideways to morph, drop a WAV to load it. */
class WavetableDisplay : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    WavetableDisplay (UIContext&, int osc);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { fileOver = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { fileOver = false; repaint(); }
    void tick();

    static void loadFile (UIContext&, int osc, const juce::File&);

private:
    float currentPos() const;
    UIContext& ctx;
    const int osc;
    juce::RangedAudioParameter* posParam;
    int posDest;
    float dragStartValue = 0.0f;
    float lastPos = -1.0f;
    const Wavetable* lastTable = nullptr;
    bool fileOver = false;
};

/** ADSR display with draggable handles. */
class EnvelopeEditor : public juce::Component
{
public:
    EnvelopeEditor (UIContext&, int env);
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void setEnvelope (int e);
    void tick();

private:
    struct Geometry { juce::Rectangle<float> area; juce::Point<float> attack, decay, sustainEnd, release, start; };
    Geometry geometry() const;
    int handleAt (juce::Point<float>) const;

    UIContext& ctx;
    int env = 0;
    juce::RangedAudioParameter* pA = nullptr; juce::RangedAudioParameter* pD = nullptr;
    juce::RangedAudioParameter* pS = nullptr; juce::RangedAudioParameter* pR = nullptr;
    int hover = -1, dragging = -1;
    float lastLevel = 0.0f;
    std::array<float, 4> lastValues {};
};

/** LFO shape view; in Custom mode the points can be dragged, added and removed. */
class LfoEditor : public juce::Component
{
public:
    LfoEditor (UIContext&, int lfo);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void setLfo (int l);
    void refreshCurve();
    void tick();

private:
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> toScreen (juce::Point<float> p) const;
    juce::Point<float> fromScreen (juce::Point<float> p) const;
    int pointAt (juce::Point<float> screen) const;
    bool isCustom() const;
    float shapeValue (int shape, float x) const;

    UIContext& ctx;
    int lfo = 0;
    std::vector<juce::Point<float>> points;
    int hover = -1, dragging = -1;
    float lastPhase = -1.0f;
    int lastShape = -1;
};
}
