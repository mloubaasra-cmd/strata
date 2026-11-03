#pragma once
#include "../Common.h"

namespace strata
{
namespace colours
{
    inline const juce::Colour bg          { 0xff0d1016 };
    inline const juce::Colour panel       { 0xff161b24 };
    inline const juce::Colour panelLight  { 0xff222935 };
    inline const juce::Colour outline     { 0xff2c3443 };
    inline const juce::Colour text        { 0xffd5dce8 };
    inline const juce::Colour textDim     { 0xff7c879b };
    inline const juce::Colour mod         { 0xffff9b42 };
    inline const juce::Colour envColour   { 0xff6fe3a1 };
    inline const juce::Colour macroColour { 0xffe57cff };
}

class StrataLook : public juce::LookAndFeel_V4
{
public:
    enum KnobStyle { styleArc, styleVintage, styleHex, styleChicken, styleMinimal, styleNeon, styleSquare, kNumKnobStyles };

    static juce::StringArray knobStyleNames();
    static juce::StringArray accentNames();
    static juce::Colour accentColour (int index);

    StrataLook();

    void setAccent (int index);
    juce::Colour getAccent() const { return accent; }
    int globalKnobStyle = styleArc;

    /** Draws a knob of the given style into a square area. Used by sliders and the style picker. */
    void drawKnob (juce::Graphics&, juce::Rectangle<float> area, float pos, float startAngle, float endAngle,
                   int style, bool bipolar, bool enabled, bool highlighted) const;

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getPopupMenuFont() override;

    static juce::Font font (float size, bool bold = false);

private:
    juce::Colour accent;
};
}
