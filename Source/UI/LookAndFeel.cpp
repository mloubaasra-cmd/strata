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

StringArray StrataLook::accentNames()
{
    return { "Cyan", "Amber", "Lime", "Magenta", "Violet" };
}

Colour StrataLook::accentColour (int i)
{
    static const Colour c[] = { Colour (0xff3fd8f2), Colour (0xffffb340), Colour (0xff9be35a), Colour (0xffff5fa8), Colour (0xff9d84ff) };
    return c[jlimit (0, 4, i)];
}

Font StrataLook::font (float size, bool bold)
{
    return Font (FontOptions (size).withStyle (bold ? "Bold" : "Regular"));
}

StrataLook::StrataLook()
{
    setAccent (0);
    setColour (ResizableWindow::backgroundColourId, colours::bg);
    setColour (Label::textColourId, colours::text);
    setColour (TextButton::buttonColourId, colours::panelLight);
    setColour (TextButton::textColourOffId, colours::text);
    setColour (TextButton::textColourOnId, colours::bg);
    setColour (ComboBox::backgroundColourId, colours::panelLight);
    setColour (ComboBox::textColourId, colours::text);
    setColour (ComboBox::outlineColourId, colours::outline);
    setColour (ComboBox::arrowColourId, colours::textDim);
    setColour (PopupMenu::backgroundColourId, colours::panel);
    setColour (PopupMenu::textColourId, colours::text);
    setColour (PopupMenu::headerTextColourId, colours::textDim);
    setColour (TextEditor::backgroundColourId, colours::panelLight);
    setColour (TextEditor::textColourId, colours::text);
    setColour (TextEditor::outlineColourId, colours::outline);
    setColour (ListBox::backgroundColourId, colours::panel);
    setColour (ScrollBar::thumbColourId, colours::outline);
    setColour (AlertWindow::backgroundColourId, colours::panel);
    setColour (AlertWindow::textColourId, colours::text);
    setColour (AlertWindow::outlineColourId, colours::outline);
    setColour (Slider::textBoxTextColourId, colours::text);
    setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xffe9edf3));
    setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff1d222b));
    setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff9aa3b2));
    setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.25f));
    setColour (MidiKeyboardComponent::upDownButtonBackgroundColourId, colours::panelLight);
    setColour (MidiKeyboardComponent::upDownButtonArrowColourId, colours::text);
}

void StrataLook::setAccent (int index)
{
    accent = accentColour (index);
    setColour (PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (PopupMenu::highlightedTextColourId, colours::text);
    setColour (TextEditor::focusedOutlineColourId, accent);
    setColour (TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (ListBox::outlineColourId, colours::outline);
    setColour (MidiKeyboardComponent::keyDownOverlayColourId, accent.withAlpha (0.7f));
    setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, accent.withAlpha (0.25f));
    setColour (TextButton::buttonOnColourId, accent);
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
