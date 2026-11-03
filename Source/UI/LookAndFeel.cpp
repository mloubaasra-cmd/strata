#include "LookAndFeel.h"

namespace strata
{
using namespace juce;

namespace
{
    Point<float> polar (Point<float> c, float r, float angle)
    {
        return { c.x + r * std::sin (angle), c.y - r * std::cos (angle) };
    }

    void arc (Graphics& g, Point<float> c, float r, float from, float to, float thickness, Colour col)
    {
        if (std::abs (to - from) < 0.001f) return;
        Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, jmin (from, to), jmax (from, to), true);
        g.setColour (col);
        g.strokePath (p, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::rounded));
    }
}

StringArray StrataLook::knobStyleNames()
{
    return { "Arc", "Vintage", "Hex", "Chicken Head", "Minimal", "Neon", "Square" };
}

const std::vector<Theme>& StrataLook::themes()
{
    //                       bg          panel       panelLight  outline     text        textDim     display     accent      mod         env         macro
    static const std::vector<Theme> t
    {
        { "Classic",       0xff0d1016, 0xff161b24, 0xff222935, 0xff2c3443, 0xffd5dce8, 0xff7c879b, 0xff0a0d12, 0xff3fd8f2, 0xffff9b42, 0xff6fe3a1, 0xffe57cff },
        { "Synthwave",     0xff150b22, 0xff21123a, 0xff2e1b4d, 0xff45296b, 0xfff3e6ff, 0xffa48cc4, 0xff0e0618, 0xffff4fa3, 0xffffa24a, 0xff4fe0ff, 0xffffe35a },
        { "Toxic",         0xff0a0f08, 0xff121a0f, 0xff1c2817, 0xff2b3d22, 0xffe4f5da, 0xff83a374, 0xff060a05, 0xffa6ff3c, 0xffff5ad1, 0xff3cf0ff, 0xffffd23c },
        { "Ocean Deep",    0xff061219, 0xff0b1e2b, 0xff12303f, 0xff1c4256, 0xffd8f1f7, 0xff6f9fb2, 0xff030c12, 0xff2fe6c8, 0xffff7a59, 0xff8fd3ff, 0xffc29bff },
        { "Lava",          0xff150806, 0xff23100c, 0xff331812, 0xff4b241a, 0xfffbe5dc, 0xffb08576, 0xff0d0403, 0xffff5a1f, 0xffffd23f, 0xff59e0c5, 0xffff6fa8 },
        { "Candy Pop",     0xfff4eefb, 0xffffffff, 0xffece3f7, 0xffd4c6e8, 0xff2a2140, 0xff7d7096, 0xfffaf7ff, 0xffff4f9a, 0xffff9f1c, 0xff16b3a4, 0xff8e5cff },
        { "Forest Gold",   0xff0c1310, 0xff15201a, 0xff1f2e25, 0xff2e4234, 0xffe9f0e3, 0xff8ea392, 0xff07100b, 0xffe8b43a, 0xffff6b5a, 0xff7be08a, 0xff6ec9ff },
        { "Aurora",        0xff0a0f1f, 0xff121a31, 0xff1b2645, 0xff2a395f, 0xffe3ecff, 0xff8093bd, 0xff060a17, 0xff7cf5c0, 0xffff7ad9, 0xff6fb7ff, 0xffd9f56b },
        { "Cyber Yellow",  0xff101010, 0xff1a1a1a, 0xff262626, 0xff383838, 0xfff2f2f2, 0xff8c8c8c, 0xff080808, 0xfffff03a, 0xffff3d6e, 0xff3dd6ff, 0xffb26bff },
        { "Royal Velvet",  0xff120a1d, 0xff1d1130, 0xff2a1946, 0xff3c2661, 0xfff1e8ff, 0xff9d8abf, 0xff0b0514, 0xffb07cff, 0xffffc14d, 0xff4df0c8, 0xffff5c8a },
        { "Retro Cream",   0xffece2cf, 0xfffaf3e6, 0xffefe2cb, 0xffd3bf9c, 0xff3a2c1c, 0xff8a765c, 0xfffdf8ef, 0xffe0582b, 0xff2b8fb3, 0xff4f9a32, 0xffb04f9c },
    };
    return t;
}

StringArray StrataLook::themeNames()
{
    StringArray s;
    for (auto& t : themes()) s.add (t.name);
    return s;
}

Font StrataLook::font (float size, bool bold)
{
    return Font (FontOptions (size).withStyle (bold ? "Bold" : "Regular"));
}

StrataLook::StrataLook()
{
    setTheme (0);
}

void StrataLook::setTheme (int index)
{
    themeIndex = jlimit (0, (int) themes().size() - 1, index);
    const auto& t = themes()[(size_t) themeIndex];
    colours::bg = Colour (t.bg);
    colours::panel = Colour (t.panel);
    colours::panelLight = Colour (t.panelLight);
    colours::outline = Colour (t.outline);
    colours::text = Colour (t.text);
    colours::textDim = Colour (t.textDim);
    colours::display = Colour (t.display);
    colours::mod = Colour (t.mod);
    colours::envColour = Colour (t.env);
    colours::macroColour = Colour (t.macro);
    accent = Colour (t.accent);

    setColour (ResizableWindow::backgroundColourId, colours::bg);
    setColour (Label::textColourId, colours::text);
    setColour (TextButton::buttonColourId, colours::panelLight);
    setColour (TextButton::textColourOffId, colours::text);
    setColour (TextButton::textColourOnId, accent.getPerceivedBrightness() > 0.6f ? Colour (0xff111111) : Colours::white);
    setColour (TextButton::buttonOnColourId, accent);
    setColour (ComboBox::backgroundColourId, colours::panelLight);
    setColour (ComboBox::textColourId, colours::text);
    setColour (ComboBox::outlineColourId, colours::outline);
    setColour (ComboBox::arrowColourId, colours::textDim);
    setColour (PopupMenu::backgroundColourId, colours::panel);
    setColour (PopupMenu::textColourId, colours::text);
    setColour (PopupMenu::headerTextColourId, colours::textDim);
    setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, colours::text);
    setColour (TextEditor::backgroundColourId, colours::panelLight);
    setColour (TextEditor::textColourId, colours::text);
    setColour (TextEditor::outlineColourId, colours::outline);
    setColour (TextEditor::focusedOutlineColourId, accent);
    setColour (TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (ListBox::backgroundColourId, colours::panel);
    setColour (ListBox::outlineColourId, colours::outline);
    setColour (ScrollBar::thumbColourId, colours::outline);
    setColour (AlertWindow::backgroundColourId, colours::panel);
    setColour (AlertWindow::textColourId, colours::text);
    setColour (AlertWindow::outlineColourId, colours::outline);
    setColour (Slider::textBoxTextColourId, colours::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (TooltipWindow::backgroundColourId, colours::panelLight);
    setColour (TooltipWindow::textColourId, colours::text);
    setColour (TooltipWindow::outlineColourId, colours::outline);
    setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffe9edf3));
    setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff1d222b));
    setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff9aa3b2));
    setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.25f));
    setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, colours::panelLight);
    setColour (MidiKeyboardComponent::upDownButtonArrowColourId, colours::text);
    setColour (MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.7f));
    setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.25f));
}

//==============================================================================
void StrataLook::drawKnob (Graphics& g, Rectangle<float> area, float pos, float a0, float a1,
                           int style, bool bipolar, bool enabled, bool highlighted) const
{
    const auto c = area.getCentre();
    const float R = jmin (area.getWidth(), area.getHeight()) * 0.5f;
    const float angle = a0 + pos * (a1 - a0);
    const float mid = (a0 + a1) * 0.5f;
    const float from = bipolar ? mid : a0;
    const Colour acc = enabled ? accent : colours::textDim;
    const Colour body = highlighted ? colours::panelLight.brighter (0.15f) : colours::panelLight;

    switch (style)
    {
        case styleVintage:
        {
            for (int i = 0; i <= 10; ++i)
            {
                const float a = a0 + (a1 - a0) * (float) i / 10.0f;
                g.setColour (colours::textDim.withAlpha (0.8f));
                g.drawLine (Line<float> (polar (c, R * 0.86f, a), polar (c, R, a)), i % 5 == 0 ? 1.6f : 1.0f);
            }
            const float br = R * 0.74f;
            ColourGradient grad (Colour (0xff5a606c), c.x - br * 0.4f, c.y - br * 0.6f, Colour (0xff181b21), c.x + br * 0.5f, c.y + br, true);
            g.setGradientFill (grad);
            g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
            g.setColour (Colours::black.withAlpha (0.35f));
            for (int i = 0; i < 40; ++i)
            {
                const float a = angle + MathConstants<float>::twoPi * (float) i / 40.0f;
                g.drawLine (Line<float> (polar (c, br * 0.9f, a), polar (c, br, a)), 1.0f);
            }
            g.setColour (Colour (0xfff2ead6));
            g.drawLine (Line<float> (polar (c, br * 0.2f, angle), polar (c, br * 0.85f, angle)), 2.4f);
            g.setColour (acc);
            g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre (polar (c, R * 0.93f, angle)));
            break;
        }

        case styleHex:
        {
            arc (g, c, R * 0.95f, a0, a1, 2.0f, colours::outline);
            arc (g, c, R * 0.95f, from, angle, 2.5f, acc);
            Path hex;
            const float hr = R * 0.74f;
            for (int i = 0; i < 6; ++i)
            {
                const auto p = polar (c, hr, angle + MathConstants<float>::pi / 3.0f * (float) i);
                if (i == 0) hex.startNewSubPath (p); else hex.lineTo (p);
            }
            hex.closeSubPath();
            g.setColour (body);
            g.fillPath (hex);
            g.setColour (acc.withAlpha (0.5f));
            g.strokePath (hex, PathStrokeType (1.4f));
            g.setColour (acc);
            g.fillEllipse (Rectangle<float> (R * 0.2f, R * 0.2f).withCentre (polar (c, hr * 0.62f, angle)));
            break;
        }

        case styleChicken:
        {
            for (int i = 0; i <= 10; ++i)
            {
                const float t = (float) i / 10.0f;
                const float a = a0 + (a1 - a0) * t;
                const bool lit = bipolar ? ((t >= 0.5f && t <= pos) || (t <= 0.5f && t >= pos)) : t <= pos + 0.001f;
                g.setColour (lit ? acc : colours::outline);
                g.fillEllipse (Rectangle<float> (3.5f, 3.5f).withCentre (polar (c, R * 0.94f, a)));
            }
            g.setColour (Colour (0xff15181e));
            g.fillEllipse (c.x - R * 0.62f, c.y - R * 0.62f, R * 1.24f, R * 1.24f);
            Path head;
            head.startNewSubPath (0.0f, -R * 0.84f);
            head.lineTo (R * 0.2f, R * 0.05f);
            head.quadraticTo (R * 0.26f, R * 0.5f, 0.0f, R * 0.52f);
            head.quadraticTo (-R * 0.26f, R * 0.5f, -R * 0.2f, R * 0.05f);
            head.closeSubPath();
            head.applyTransform (AffineTransform::rotation (angle).translated (c));
            g.setColour (Colour (0xffece4cf).withMultipliedBrightness (highlighted ? 1.05f : 0.95f));
            g.fillPath (head);
            g.setColour (Colours::black.withAlpha (0.6f));
            g.strokePath (head, PathStrokeType (1.0f));
            g.setColour (Colour (0xff2a2a2a));
            g.drawLine (Line<float> (polar (c, R * 0.15f, angle), polar (c, R * 0.72f, angle)), 1.6f);
            break;
        }

        case styleMinimal:
        {
            arc (g, c, R * 0.92f, from, angle, 2.0f, acc);
            g.setColour (body);
            g.fillEllipse (c.x - R * 0.72f, c.y - R * 0.72f, R * 1.44f, R * 1.44f);
            g.setColour (acc);
            g.fillEllipse (Rectangle<float> (R * 0.22f, R * 0.22f).withCentre (polar (c, R * 0.5f, angle)));
            break;
        }

        case styleNeon:
        {
            arc (g, c, R * 0.84f, a0, a1, 3.0f, acc.withAlpha (0.12f));
            arc (g, c, R * 0.84f, from, angle, 9.0f, acc.withAlpha (0.12f));
            arc (g, c, R * 0.84f, from, angle, 6.0f, acc.withAlpha (0.25f));
            arc (g, c, R * 0.84f, from, angle, 2.5f, acc.brighter (0.4f));
            g.setColour (Colour (0xff0a0c10));
            g.fillEllipse (c.x - R * 0.6f, c.y - R * 0.6f, R * 1.2f, R * 1.2f);
            g.setColour (acc.withAlpha (0.35f));
            g.drawEllipse (c.x - R * 0.6f, c.y - R * 0.6f, R * 1.2f, R * 1.2f, 1.0f);
            g.setColour (acc.brighter (0.5f));
            g.fillEllipse (Rectangle<float> (4.0f, 4.0f).withCentre (polar (c, R * 0.84f, angle)));
            break;
        }

        case styleSquare:
        {
            const int segs = 15;
            for (int i = 0; i < segs; ++i)
            {
                const float t = (float) i / (float) (segs - 1);
                const float a = a0 + (a1 - a0) * t;
                const bool lit = bipolar ? ((t >= 0.5f && t <= pos + 0.001f) || (t <= 0.5f && t >= pos - 0.001f)) : t <= pos + 0.001f;
                g.setColour (lit ? acc : colours::outline);
                g.drawLine (Line<float> (polar (c, R * 0.84f, a), polar (c, R, a)), 2.2f);
            }
            const float s = R * 0.66f;
            auto sq = Rectangle<float> (s * 2, s * 2).withCentre (c);
            ColourGradient grad (body.brighter (0.1f), sq.getTopLeft(), body.darker (0.3f), sq.getBottomRight(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (sq, R * 0.18f);
            g.setColour (colours::outline);
            g.drawRoundedRectangle (sq, R * 0.18f, 1.0f);
            g.setColour (colours::text);
            g.drawLine (Line<float> (polar (c, R * 0.15f, angle), polar (c, s * 0.9f, angle)), 2.2f);
            break;
        }

        case styleArc:
        default:
        {
            arc (g, c, R * 0.86f, a0, a1, R * 0.15f, colours::outline);
            arc (g, c, R * 0.86f, from, angle, R * 0.15f, acc);
            const float br = R * 0.62f;
            ColourGradient grad (body.brighter (0.15f), c.x, c.y - br, body.darker (0.25f), c.x, c.y + br, false);
            g.setGradientFill (grad);
            g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
            g.setColour (colours::text);
            g.drawLine (Line<float> (polar (c, br * 0.35f, angle), polar (c, br * 0.92f, angle)), 2.0f);
            break;
        }
    }
}

void StrataLook::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, Slider& s)
{
    const auto& props = s.getProperties();
    int style = props.contains ("knobStyle") ? (int) props["knobStyle"] : -1;
    if (style < 0) style = globalKnobStyle;
    const bool bipolar = props.contains ("bipolar") && (bool) props["bipolar"];
    const float size = (float) jmin (w, h) - 10.0f;
    auto area = Rectangle<float> (size, size).withCentre (Rectangle<int> (x, y, w, h).toFloat().getCentre());
    drawKnob (g, area, pos, a0, a1, style, bipolar, s.isEnabled(), s.isMouseOverOrDragging());
}

void StrataLook::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                                   Slider::SliderStyle, Slider& s)
{
    auto r = Rectangle<int> (x, y, w, h).toFloat().reduced (0.0f, (float) h * 0.3f);
    g.setColour (colours::bg);
    g.fillRoundedRectangle (r, 3.0f);
    const float zero = s.getMinimum() < 0.0 ? (float) s.valueToProportionOfLength (0.0) * r.getWidth() + r.getX() : r.getX();
    const float lo = jmin (zero, sliderPos), hi = jmax (zero, sliderPos);
    g.setColour (s.getValue() < 0.0 ? colours::mod.darker (0.2f) : colours::mod);
    g.fillRoundedRectangle (Rectangle<float> (lo, r.getY(), jmax (2.0f, hi - lo), r.getHeight()), 3.0f);
    if (s.getMinimum() < 0.0)
    {
        g.setColour (colours::textDim);
        g.drawVerticalLine ((int) zero, r.getY() - 2.0f, r.getBottom() + 2.0f);
    }
}

void StrataLook::drawToggleButton (Graphics& g, ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto text = b.getButtonText();

    if (text.isEmpty())
    {
        // Power-button style
        auto c = r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight()) - 2.0f, jmin (r.getWidth(), r.getHeight()) - 2.0f);
        g.setColour (on ? accent.withAlpha (0.2f) : colours::bg);
        g.fillEllipse (c);
        g.setColour (on ? accent : colours::textDim);
        g.drawEllipse (c.reduced (0.5f), 1.2f);
        const auto ctr = c.getCentre();
        const float rr = c.getWidth() * 0.26f;
        Path p;
        p.addCentredArc (ctr.x, ctr.y, rr, rr, 0.0f, 0.7f, MathConstants<float>::twoPi - 0.7f, true);
        g.strokePath (p, PathStrokeType (1.4f, PathStrokeType::curved, PathStrokeType::rounded));
        g.drawLine (ctr.x, ctr.y - rr * 1.3f, ctr.x, ctr.y - rr * 0.2f, 1.4f);
        return;
    }

    auto box = r.reduced (1.0f);
    g.setColour (on ? accent.withAlpha (0.22f) : (highlighted ? colours::panelLight.brighter (0.1f) : colours::panelLight));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (on ? accent : colours::outline);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);
    g.setColour (on ? accent.brighter (0.3f) : colours::textDim);
    g.setFont (font (11.0f, true));
    g.drawText (text, box, Justification::centred);
}

void StrataLook::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    Colour fill = on ? accent : colours::panelLight;
    if (highlighted && ! on) fill = fill.brighter (0.12f);
    if (down) fill = fill.darker (0.15f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? accent.brighter (0.2f) : colours::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void StrataLook::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? colours::panelLight.brighter (0.08f) : colours::panelLight);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
    Path arrow;
    const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 3.5f, ay - 2.0f, ax + 3.5f, ay - 2.0f, ax, ay + 2.5f);
    g.setColour (colours::textDim);
    g.fillPath (arrow);
}

void StrataLook::positionComboBoxText (ComboBox& box, Label& label)
{
    label.setBounds (4, 0, box.getWidth() - 18, box.getHeight());
    label.setFont (getComboBoxFont (box));
}

Font StrataLook::getComboBoxFont (ComboBox& box)  { return font (jmin (13.0f, (float) box.getHeight() * 0.55f)); }
Font StrataLook::getTextButtonFont (TextButton&, int h) { return font (jmin (13.0f, (float) h * 0.5f), true); }
Font StrataLook::getPopupMenuFont() { return font (14.0f); }
}
