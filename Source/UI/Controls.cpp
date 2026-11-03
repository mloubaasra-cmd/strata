#include "Controls.h"

namespace strata
{
using namespace juce;

//==============================================================================
ModSlider::ModSlider()
{
    setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (180);
}

void ModSlider::mouseDown (const MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onRightClick) onRightClick (e);
        return;
    }
    if (e.mods.isAltDown() && canAltDrag && canAltDrag())
    {
        altDragging = true;
        lastY = e.position.y;
        return;
    }
    Slider::mouseDown (e);
}

void ModSlider::mouseDrag (const MouseEvent& e)
{
    if (altDragging)
    {
        const float delta = (lastY - e.position.y) * 0.005f;
        lastY = e.position.y;
        if (onAltDrag) onAltDrag (delta);
        return;
    }
    if (! e.mods.isPopupMenu())
        Slider::mouseDrag (e);
}

void ModSlider::mouseUp (const MouseEvent& e)
{
    if (altDragging) { altDragging = false; return; }
    if (! e.mods.isPopupMenu())
        Slider::mouseUp (e);
}

//==============================================================================
ModKnob::ModKnob (UIContext& c, const String& id, const String& text)
    : paramID (id), dest (mod::destIndexForParam (id)), ctx (c), label (text)
{
    param = ctx.proc.apvts.getParameter (id);
    jassert (param != nullptr);

    addAndMakeVisible (slider);
    attachment = std::make_unique<SliderAttachment> (ctx.proc.apvts, id, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.getProperties().set ("bipolar", param->convertFrom0to1 (0.0f) < 0.0f);
    slider.addMouseListener (this, false);
    slider.onValueChange = [this] { repaint(); };
    slider.setPopupDisplayEnabled (false, false, nullptr);

    slider.onRightClick = [this] (const MouseEvent&) { showMenu(); };
    slider.canAltDrag = [this] { return dest >= 0 && ! myMods.empty(); };
    slider.onAltDrag = [this] (float delta)
    {
        if (myMods.empty()) return;
        auto& m = myMods.back();
        m.amount = jlimit (-1.0f, 1.0f, m.amount + delta);
        ctx.proc.setModulation (m.index, m.src, dest, m.amount);
        repaint();
    };

    ctx.knobs.push_back (this);
    refreshStyle();
    refreshMods();
}

ModKnob::~ModKnob()
{
    auto& k = ctx.knobs;
    k.erase (std::remove (k.begin(), k.end(), this), k.end());
}

void ModKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (14);
    slider.setBounds (r);
}

Rectangle<float> ModKnob::knobArea() const
{
    const auto b = slider.getBounds().toFloat();
    const float size = jmin (b.getWidth(), b.getHeight()) - 10.0f;
    return Rectangle<float> (size, size).withCentre (b.getCentre());
}

void ModKnob::paint (Graphics& g)
{
    auto r = getLocalBounds().removeFromBottom (14);
    const bool showValue = slider.isMouseOverOrDragging();
    g.setColour (showValue ? ctx.look.getAccent() : colours::textDim);
    g.setFont (StrataLook::font (10.5f, true));
    g.drawFittedText (showValue ? param->getCurrentValueAsText() : label, r, Justification::centred, 1, 0.8f);
}

void ModKnob::paintOverChildren (Graphics& g)
{
    const auto area = knobArea();
    const auto c = area.getCentre();
    const float R = area.getWidth() * 0.5f;
    const auto rp = slider.getRotaryParameters();
    const float a0 = rp.startAngleRadians, a1 = rp.endAngleRadians;
    const float base = param->getValue();

    if (dragOver)
    {
        g.setColour (colours::mod);
        Path p;
        p.addEllipse (area.expanded (4.0f));
        const float dashes[] = { 4.0f, 3.0f };
        PathStrokeType (1.5f).createDashedStroke (p, p, dashes, 2);
        g.fillPath (p);
    }

    int ring = 0;
    for (auto& m : myMods)
    {
        const float r = R + 3.0f + (float) ring * 3.0f;
        const float from = a0 + base * (a1 - a0);
        const float to = a0 + jlimit (0.0f, 1.0f, base + m.amount) * (a1 - a0);
        if (std::abs (to - from) > 0.001f)
        {
            Path p;
            p.addCentredArc (c.x, c.y, r, r, 0.0f, jmin (from, to), jmax (from, to), true);
            g.setColour (colours::mod.withAlpha (ring == 0 ? 1.0f : 0.7f));
            g.strokePath (p, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
        }
        if (++ring >= 3) break;
    }

    if (! myMods.empty() && ctx.proc.engine.activeVoices.load() > 0 && dest >= 0)
    {
        const float v = ctx.proc.engine.displayValue[(size_t) dest].load();
        const float a = a0 + jlimit (0.0f, 1.0f, v) * (a1 - a0);
        const Point<float> pt (c.x + (R + 3.0f) * std::sin (a), c.y - (R + 3.0f) * std::cos (a));
        g.setColour (colours::text);
        g.fillEllipse (Rectangle<float> (5.0f, 5.0f).withCentre (pt));
    }
}

bool ModKnob::isInterestedInDragSource (const SourceDetails& d)
{
    return dest >= 0 && d.description.toString().startsWith ("mod:");
}

void ModKnob::itemDropped (const SourceDetails& d)
{
    dragOver = false;
    const int src = d.description.toString().fromFirstOccurrenceOf ("mod:", false, false).getIntValue();
    const float amount = mod::isBipolarSource (src) ? 0.3f : 0.4f;
    if (ctx.proc.addModulation (src, dest, amount) < 0)
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::InfoIcon, "Modulation matrix full",
                                          "All " + String (mod::kMaxSlots) + " slots are in use. Remove one on the MATRIX page first.");
    refreshMods();
    repaint();
}

void ModKnob::refreshMods()
{
    myMods.clear();
    if (dest < 0) return;
    const auto mods = ctx.proc.getModulations();
    for (int i = 0; i < (int) mods.size(); ++i)
        if (mods[(size_t) i].dst == dest)
            myMods.push_back ({ i, mods[(size_t) i].src, mods[(size_t) i].amount });
    repaint();
}

void ModKnob::refreshStyle()
{
    slider.getProperties().set ("knobStyle", ctx.proc.uiSettings.getKnobOverride (paramID));
    repaint();
}

void ModKnob::tick()
{
    if (myMods.empty() || dest < 0) return;
    const float v = ctx.proc.engine.activeVoices.load() > 0 ? ctx.proc.engine.displayValue[(size_t) dest].load() : -1.0f;
    if (std::abs (v - lastDisplay) > 0.002f)
    {
        lastDisplay = v;
        repaint();
    }
}

void ModKnob::showMenu()
{
    Component::SafePointer<ModKnob> safe (this);
    PopupMenu m;

    PopupMenu shapes;
    const int current = ctx.proc.uiSettings.getKnobOverride (paramID);
    auto setStyle = [safe] (int s)
    {
        if (safe == nullptr) return;
        safe->ctx.proc.uiSettings.setKnobOverride (safe->paramID, s);
        safe->refreshStyle();
    };
    shapes.addItem ("Use global shape", true, current < 0, [setStyle] { setStyle (-1); });
    shapes.addSeparator();
    const auto names = StrataLook::knobStyleNames();
    for (int i = 0; i < names.size(); ++i)
        shapes.addItem (names[i], true, current == i, [setStyle, i] { setStyle (i); });
    m.addSubMenu ("Knob shape for this knob", shapes);
    m.addItem ("Reset to default", [safe]
    {
        if (safe != nullptr)
            safe->param->setValueNotifyingHost (safe->param->getDefaultValue());
    });

    if (dest >= 0)
    {
        m.addSeparator();
        m.addSectionHeader ("Modulation");
        for (auto& mm : myMods)
        {
            PopupMenu sub;
            const int index = mm.index, src = mm.src, d = dest;
            for (float a : { 1.0f, 0.5f, 0.25f, 0.1f, -0.1f, -0.25f, -0.5f, -1.0f })
                sub.addItem ((a > 0 ? "+" : "") + String (roundToInt (a * 100)) + "%", [safe, index, src, d, a]
                {
                    if (safe != nullptr) safe->ctx.proc.setModulation (index, src, d, a);
                });
            const float amt = mm.amount;
            sub.addItem ("Invert", [safe, index, src, d, amt]
            {
                if (safe != nullptr) safe->ctx.proc.setModulation (index, src, d, -amt);
            });
            sub.addSeparator();
            sub.addItem ("Remove", [safe, index]
            {
                if (safe != nullptr) safe->ctx.proc.removeModulation (index);
            });
            m.addSubMenu (mod::sourceNames()[mm.src] + "   " + (mm.amount > 0 ? "+" : "")
                            + String (roundToInt (mm.amount * 100)) + "%", sub);
        }

        PopupMenu add;
        for (int s = 0; s < mod::kNumSources; ++s)
            add.addItem (mod::sourceNames()[s], [safe, s]
            {
                if (safe != nullptr) safe->ctx.proc.addModulation (s, safe->dest, mod::isBipolarSource (s) ? 0.3f : 0.4f);
            });
        m.addSubMenu ("Add modulation from", add);
    }

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this));
}

//==============================================================================
SourceChip::SourceChip (int src, const String& t) : source (src), text (t)
{
    setMouseCursor (MouseCursor::DraggingHandCursor);
    setTooltip ("Drag onto any knob to modulate it");
}

void SourceChip::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    Colour c = colours::mod;
    if (source <= mod::srcEnv3) c = colours::envColour;
    else if (source <= mod::srcLfo4)
    {
        if (auto* look = dynamic_cast<StrataLook*> (&getLookAndFeel())) c = look->getAccent();
    }
    else if (source >= mod::srcMacro1 && source <= mod::srcMacro4) c = colours::macroColour;

    g.setColour (selected ? c.withAlpha (0.22f) : colours::panelLight);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (selected ? c : c.withAlpha (0.45f));
    g.drawRoundedRectangle (r, 4.0f, 1.0f);

    // grip dots
    g.setColour (c.withAlpha (0.8f));
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillEllipse (r.getX() + 5.0f + (float) j * 3.5f, r.getCentreY() - 4.5f + (float) i * 3.5f, 2.0f, 2.0f);

    g.setColour (selected ? colours::text : colours::textDim);
    g.setFont (StrataLook::font (11.0f, true));
    g.drawText (text, r.withTrimmedLeft (12.0f), Justification::centred);
}

void SourceChip::mouseDown (const MouseEvent&) { dragging = false; }

void SourceChip::mouseDrag (const MouseEvent& e)
{
    if (! dragging && e.getDistanceFromDragStart() > 4)
    {
        if (auto* container = DragAndDropContainer::findParentDragContainerFor (this))
        {
            dragging = true;
            container->startDragging ("mod:" + String (source), this);
        }
    }
}

void SourceChip::mouseUp (const MouseEvent&)
{
    if (! dragging && onClick) onClick();
    dragging = false;
}

//==============================================================================
void Panel::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (colours::text);
        g.setFont (StrataLook::font (12.5f, true));
        g.drawText (title, titleIndent, 6, 200, 18, Justification::centredLeft);
    }
}

void ParamCombo::attach (StrataProcessor& p, const String& id)
{
    if (auto* choice = dynamic_cast<AudioParameterChoice*> (p.apvts.getParameter (id)))
        box.addItemList (choice->choices, 1);
    attachment = std::make_unique<ComboAttachment> (p.apvts, id, box);
}

void ParamToggle::attach (StrataProcessor& p, const String& id, const String& text)
{
    button.setButtonText (text);
    attachment = std::make_unique<ButtonAttachment> (p.apvts, id, button);
}

void drawSectionLabel (Graphics& g, Rectangle<int> r, const String& text)
{
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (10.5f, true));
    g.drawText (text, r, Justification::centredLeft);
}
}
