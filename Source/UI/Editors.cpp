#include "Editors.h"

namespace strata
{
using namespace juce;

//==============================================================================
WavetableDisplay::WavetableDisplay (UIContext& c, int o) : ctx (c), osc (o)
{
    posParam = ctx.proc.apvts.getParameter (ids::osc (o, "pos"));
    posDest = mod::destIndexForParam (ids::osc (o, "pos"));
    setMouseCursor (MouseCursor::LeftRightResizeCursor);
}

float WavetableDisplay::currentPos() const
{
    auto& e = ctx.proc.engine;
    if (e.activeVoices.load() > 0 && posDest >= 0)
        return e.displayValue[(size_t) posDest].load();
    return posParam->getValue();
}

void WavetableDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (colours::display);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (fileOver ? colours::mod : colours::outline);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, fileOver ? 2.0f : 1.0f);

    const auto* wt = ctx.proc.getOscTable (osc);
    if (wt == nullptr || wt->numFrames == 0) return;

    const auto accent = ctx.look.getAccent();
    const float pos = jlimit (0.0f, 1.0f, currentPos());
    const int nf = wt->numFrames;
    const int nShow = jmin (nf, 20);
    const auto inner = r.reduced (10.0f, 8.0f);
    const float frameW = inner.getWidth() * 0.74f;
    const float amp = inner.getHeight() * 0.2f;
    constexpr int pts = 96;

    auto framePath = [&] (int frame, float depth, bool closed) -> Path
    {
        const float x0 = inner.getX() + depth * inner.getWidth() * 0.26f;
        const float yBase = inner.getBottom() - amp - 4.0f - depth * inner.getHeight() * 0.42f;
        Path p;
        const float* data = wt->rawFrames.data() + (size_t) frame * Wavetable::kFrameSize;
        for (int i = 0; i <= pts; ++i)
        {
            const int idx = jmin (Wavetable::kFrameSize - 1, i * Wavetable::kFrameSize / pts);
            const float x = x0 + frameW * (float) i / (float) pts;
            const float y = yBase - data[idx] * amp;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (closed)
        {
            p.lineTo (x0 + frameW, yBase);
            p.lineTo (x0, yBase);
            p.closeSubPath();
        }
        return p;
    };

    // back to front so nearer frames overlap
    for (int k = nShow - 1; k >= 0; --k)
    {
        const float depth = nShow > 1 ? (float) k / (float) (nShow - 1) : 0.0f;
        const int frame = nShow > 1 ? k * (nf - 1) / (nShow - 1) : 0;
        g.setColour (colours::textDim.withAlpha (0.10f + 0.12f * (1.0f - depth)));
        g.strokePath (framePath (frame, depth, false), PathStrokeType (1.0f));
    }

    const int current = roundToInt (pos * (float) (nf - 1));
    const float depth = pos;
    auto filled = framePath (current, depth, true);
    g.setGradientFill (ColourGradient (accent.withAlpha (0.35f), 0.0f, filled.getBounds().getY(),
                                       accent.withAlpha (0.02f), 0.0f, filled.getBounds().getBottom(), false));
    g.fillPath (filled);
    g.setColour (accent);
    g.strokePath (framePath (current, depth, false), PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    g.setFont (StrataLook::font (11.0f, true));
    g.setColour (colours::text);
    g.drawText (wt->name, r.reduced (8.0f, 4.0f).removeFromTop (14.0f), Justification::topLeft);
    g.setColour (colours::textDim);
    g.drawText (String (current + 1) + " / " + String (nf), r.reduced (8.0f, 4.0f).removeFromTop (14.0f), Justification::topRight);

    if (isMouseOver() || fileOver)
    {
        g.setFont (StrataLook::font (10.0f));
        g.setColour (colours::textDim);
        g.drawText (fileOver ? "Drop to load wavetable" : "drag to morph  -  drop a .wav here",
                    r.reduced (8.0f, 4.0f), Justification::bottomRight);
    }
}

void WavetableDisplay::mouseDown (const MouseEvent&)
{
    dragStartValue = posParam->getValue();
    posParam->beginChangeGesture();
}

void WavetableDisplay::mouseDrag (const MouseEvent& e)
{
    const float v = jlimit (0.0f, 1.0f, dragStartValue + (float) e.getDistanceFromDragStartX() / (float) jmax (1, getWidth()));
    posParam->setValueNotifyingHost (v);
    repaint();
}

void WavetableDisplay::mouseUp (const MouseEvent&) { posParam->endChangeGesture(); }

bool WavetableDisplay::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac")) return true;
    return false;
}

void WavetableDisplay::filesDropped (const StringArray& files, int, int)
{
    fileOver = false;
    for (auto& f : files)
        if (File (f).hasFileExtension ("wav;aif;aiff;flac")) { loadFile (ctx, osc, File (f)); break; }
    repaint();
}

void WavetableDisplay::loadFile (UIContext& c, int o, const File& f)
{
    String error;
    if (! c.proc.loadUserWavetable (o, f, error))
        AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Couldn't load wavetable", error);
}

void WavetableDisplay::tick()
{
    const float p = currentPos();
    const auto* t = ctx.proc.getOscTable (osc);
    if (std::abs (p - lastPos) > 0.001f || t != lastTable)
    {
        lastPos = p;
        lastTable = t;
        repaint();
    }
}

//==============================================================================
EnvelopeEditor::EnvelopeEditor (UIContext& c, int e) : ctx (c) { setEnvelope (e); }

void EnvelopeEditor::setEnvelope (int e)
{
    env = e;
    auto& a = ctx.proc.apvts;
    pA = a.getParameter (ids::env (e, "atk"));
    pD = a.getParameter (ids::env (e, "dec"));
    pS = a.getParameter (ids::env (e, "sus"));
    pR = a.getParameter (ids::env (e, "rel"));
    repaint();
}

EnvelopeEditor::Geometry EnvelopeEditor::geometry() const
{
    Geometry geo;
    geo.area = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
    const auto& r = geo.area;
    const float seg = r.getWidth() / 3.8f;
    geo.start = { r.getX(), r.getBottom() };
    geo.attack = { r.getX() + pA->getValue() * seg, r.getY() };
    geo.decay = { geo.attack.x + pD->getValue() * seg, r.getBottom() - pS->getValue() * r.getHeight() };
    geo.sustainEnd = { geo.decay.x + seg * 0.6f, geo.decay.y };
    geo.release = { geo.sustainEnd.x + pR->getValue() * seg, r.getBottom() };
    return geo;
}

int EnvelopeEditor::handleAt (Point<float> p) const
{
    const auto geo = geometry();
    const Point<float> hs[] = { geo.attack, geo.decay, geo.release };
    for (int i = 0; i < 3; ++i)
        if (hs[i].getDistanceFrom (p) < 10.0f) return i;
    return -1;
}

void EnvelopeEditor::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::display);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);

    const auto geo = geometry();
    Path p;
    p.startNewSubPath (geo.start);
    p.lineTo (geo.attack);
    p.quadraticTo ({ geo.attack.x + (geo.decay.x - geo.attack.x) * 0.2f, geo.decay.y }, geo.decay);
    p.lineTo (geo.sustainEnd);
    p.quadraticTo ({ geo.sustainEnd.x + (geo.release.x - geo.sustainEnd.x) * 0.2f, geo.release.y }, geo.release);

    Path fill (p);
    fill.lineTo (geo.release.x, geo.area.getBottom());
    fill.lineTo (geo.start);
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (colours::envColour.withAlpha (0.3f), 0.0f, geo.area.getY(),
                                       colours::envColour.withAlpha (0.02f), 0.0f, geo.area.getBottom(), false));
    g.fillPath (fill);
    g.setColour (colours::envColour);
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    const Point<float> hs[] = { geo.attack, geo.decay, geo.release };
    for (int i = 0; i < 3; ++i)
    {
        const bool hot = hover == i || dragging == i;
        g.setColour (hot ? colours::text : colours::envColour);
        g.fillEllipse (Rectangle<float> (hot ? 10.0f : 8.0f, hot ? 10.0f : 8.0f).withCentre (hs[i]));
        g.setColour (colours::display);
        g.drawEllipse (Rectangle<float> (hot ? 10.0f : 8.0f, hot ? 10.0f : 8.0f).withCentre (hs[i]), 1.5f);
    }

    // level meter for the newest voice
    const float lvl = ctx.proc.engine.displayEnvLevel[(size_t) env].load();
    if (lvl > 0.001f)
    {
        g.setColour (colours::envColour.withAlpha (0.6f));
        const float y = geo.area.getBottom() - lvl * geo.area.getHeight();
        g.fillRect (Rectangle<float> (b.getRight() - 6.0f, y, 3.0f, geo.area.getBottom() - y));
    }

    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (10.0f));
    const String names[] = { "ENV 1 - amp", "ENV 2 - filter", "ENV 3 - free" };
    g.drawText (names[env], b.reduced (8.0f, 4.0f), Justification::topRight);
}

void EnvelopeEditor::mouseMove (const MouseEvent& e)
{
    const int h = handleAt (e.position);
    if (h != hover) { hover = h; repaint(); }
    setMouseCursor (h >= 0 ? MouseCursor::DraggingHandCursor : MouseCursor::NormalCursor);
}

void EnvelopeEditor::mouseDown (const MouseEvent& e)
{
    dragging = handleAt (e.position);
    if (dragging == 0) pA->beginChangeGesture();
    if (dragging == 1) { pD->beginChangeGesture(); pS->beginChangeGesture(); }
    if (dragging == 2) pR->beginChangeGesture();
}

void EnvelopeEditor::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0) return;
    const auto geo = geometry();
    const float seg = geo.area.getWidth() / 3.8f;
    const auto p = e.position;
    if (dragging == 0)
        pA->setValueNotifyingHost (jlimit (0.0f, 1.0f, (p.x - geo.area.getX()) / seg));
    else if (dragging == 1)
    {
        pD->setValueNotifyingHost (jlimit (0.0f, 1.0f, (p.x - geo.attack.x) / seg));
        pS->setValueNotifyingHost (jlimit (0.0f, 1.0f, (geo.area.getBottom() - p.y) / geo.area.getHeight()));
    }
    else
        pR->setValueNotifyingHost (jlimit (0.0f, 1.0f, (p.x - geo.sustainEnd.x) / seg));
    repaint();
}

void EnvelopeEditor::mouseUp (const MouseEvent&)
{
    if (dragging == 0) pA->endChangeGesture();
    if (dragging == 1) { pD->endChangeGesture(); pS->endChangeGesture(); }
    if (dragging == 2) pR->endChangeGesture();
    dragging = -1;
}

void EnvelopeEditor::tick()
{
    const std::array<float, 4> v { pA->getValue(), pD->getValue(), pS->getValue(), pR->getValue() };
    const float lvl = ctx.proc.engine.displayEnvLevel[(size_t) env].load();
    if (v != lastValues || std::abs (lvl - lastLevel) > 0.005f)
    {
        lastValues = v;
        lastLevel = lvl;
        repaint();
    }
}

//==============================================================================
LfoEditor::LfoEditor (UIContext& c, int l) : ctx (c) { setLfo (l); }

void LfoEditor::setLfo (int l)
{
    lfo = l;
    refreshCurve();
}

void LfoEditor::refreshCurve()
{
    if (dragging >= 0) return;
    points = ctx.proc.getLfoCurve (lfo);
    repaint();
}

bool LfoEditor::isCustom() const
{
    return (int) ctx.proc.apvts.getRawParameterValue (ids::lfo (lfo, "shape"))->load() == params::lfoCustom;
}

Rectangle<float> LfoEditor::plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 12.0f); }

Point<float> LfoEditor::toScreen (Point<float> p) const
{
    const auto r = plotArea();
    return { r.getX() + p.x * r.getWidth(), r.getBottom() - p.y * r.getHeight() };
}

Point<float> LfoEditor::fromScreen (Point<float> p) const
{
    const auto r = plotArea();
    return { jlimit (0.0f, 1.0f, (p.x - r.getX()) / r.getWidth()), jlimit (0.0f, 1.0f, (r.getBottom() - p.y) / r.getHeight()) };
}

int LfoEditor::pointAt (Point<float> s) const
{
    for (int i = 0; i < (int) points.size(); ++i)
        if (toScreen (points[(size_t) i]).getDistanceFrom (s) < 9.0f) return i;
    return -1;
}

float LfoEditor::shapeValue (int shape, float x) const
{
    static const float rnd[] = { 0.7f, 0.2f, 0.9f, 0.45f, 0.1f, 0.65f, 0.35f, 0.8f };
    switch (shape)
    {
        case params::lfoSine:         return 0.5f + 0.5f * std::sin (MathConstants<float>::twoPi * x);
        case params::lfoTri:          return x < 0.25f ? 0.5f + 2.0f * x : (x < 0.75f ? 1.5f - 2.0f * x : 2.0f * x - 1.5f);
        case params::lfoSawUp:        return x;
        case params::lfoSawDown:      return 1.0f - x;
        case params::lfoSquare:       return x < 0.5f ? 1.0f : 0.0f;
        case params::lfoSampleHold:   return rnd[jmin (7, (int) (x * 8.0f))];
        case params::lfoSmoothRandom:
        {
            const float s = x * 8.0f;
            const int i = jmin (7, (int) s);
            const float t = s - (float) i;
            const float a = rnd[i], b = rnd[(i + 1) % 8];
            return a + (b - a) * (0.5f - 0.5f * std::cos (MathConstants<float>::pi * t));
        }
        default: return 0.5f;
    }
}

void LfoEditor::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (colours::display);
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);

    const auto r = plotArea();
    g.setColour (colours::outline.withAlpha (0.6f));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine (roundToInt (r.getX() + r.getWidth() * (float) i / 8.0f), r.getY(), r.getBottom());
    g.drawHorizontalLine (roundToInt (r.getCentreY()), r.getX(), r.getRight());

    const auto accent = ctx.look.getAccent();
    const bool custom = isCustom();
    const int shape = (int) ctx.proc.apvts.getRawParameterValue (ids::lfo (lfo, "shape"))->load();

    Path p;
    if (custom)
    {
        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto s = toScreen (points[i]);
            if (i == 0) p.startNewSubPath (s); else p.lineTo (s);
        }
    }
    else
    {
        for (int i = 0; i <= 200; ++i)
        {
            const float x = (float) i / 200.0f;
            const auto s = toScreen ({ x, shapeValue (shape, x) });
            if (i == 0) p.startNewSubPath (s); else p.lineTo (s);
        }
    }

    Path fill (p);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setGradientFill (ColourGradient (accent.withAlpha (0.25f), 0.0f, r.getY(), accent.withAlpha (0.02f), 0.0f, r.getBottom(), false));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    if (custom)
        for (int i = 0; i < (int) points.size(); ++i)
        {
            const bool hot = hover == i || dragging == i;
            const auto s = toScreen (points[(size_t) i]);
            g.setColour (hot ? colours::text : accent);
            g.fillEllipse (Rectangle<float> (hot ? 10.0f : 7.0f, hot ? 10.0f : 7.0f).withCentre (s));
        }

    // playhead
    const float ph = ctx.proc.engine.displayLfoPhase[(size_t) lfo].load();
    g.setColour (colours::text.withAlpha (0.5f));
    g.drawVerticalLine (roundToInt (r.getX() + ph * r.getWidth()), r.getY(), r.getBottom());

    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (10.0f));
    g.drawText (custom ? "drag points  -  double-click to add/remove  -  shift snaps"
                       : "click to edit as a custom shape",
                b.reduced (8.0f, 3.0f), Justification::bottomRight);
}

void LfoEditor::mouseMove (const MouseEvent& e)
{
    const int h = isCustom() ? pointAt (e.position) : -1;
    if (h != hover) { hover = h; repaint(); }
}

void LfoEditor::mouseDown (const MouseEvent& e)
{
    if (! isCustom())
    {
        // Convert the current shape into editable points and switch to Custom
        const int shape = (int) ctx.proc.apvts.getRawParameterValue (ids::lfo (lfo, "shape"))->load();
        std::vector<Point<float>> pts;
        if (shape == params::lfoSquare)
            pts = { { 0.0f, 1.0f }, { 0.49f, 1.0f }, { 0.5f, 0.0f }, { 1.0f, 0.0f } };
        else if (shape == params::lfoSawUp)   pts = { { 0.0f, 0.0f }, { 1.0f, 1.0f } };
        else if (shape == params::lfoSawDown) pts = { { 0.0f, 1.0f }, { 1.0f, 0.0f } };
        else
            for (int i = 0; i <= 8; ++i)
                pts.push_back ({ (float) i / 8.0f, shapeValue (shape, (float) i / 8.0f) });
        ctx.proc.setLfoCurve (lfo, pts);
        if (auto* p = ctx.proc.apvts.getParameter (ids::lfo (lfo, "shape")))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) params::lfoCustom));
        refreshCurve();
        return;
    }
    dragging = pointAt (e.position);
}

void LfoEditor::mouseDrag (const MouseEvent& e)
{
    if (dragging < 0 || ! isCustom()) return;
    auto v = fromScreen (e.position);
    if (e.mods.isShiftDown())
    {
        v.x = std::round (v.x * 16.0f) / 16.0f;
        v.y = std::round (v.y * 8.0f) / 8.0f;
    }
    const int n = (int) points.size();
    if (dragging == 0) v.x = 0.0f;
    else if (dragging == n - 1) v.x = 1.0f;
    else v.x = jlimit (points[(size_t) dragging - 1].x + 0.001f, points[(size_t) dragging + 1].x - 0.001f, v.x);
    points[(size_t) dragging] = v;
    ctx.proc.setLfoCurve (lfo, points);
    repaint();
}

void LfoEditor::mouseUp (const MouseEvent&) { dragging = -1; }

void LfoEditor::mouseDoubleClick (const MouseEvent& e)
{
    if (! isCustom()) return;
    const int hit = pointAt (e.position);
    if (hit > 0 && hit < (int) points.size() - 1)
        points.erase (points.begin() + hit);
    else if (hit < 0 && points.size() < 32)
    {
        const auto v = fromScreen (e.position);
        auto it = std::upper_bound (points.begin(), points.end(), v, [] (auto a, auto b) { return a.x < b.x; });
        points.insert (it, v);
    }
    ctx.proc.setLfoCurve (lfo, points);
    repaint();
}

void LfoEditor::tick()
{
    const float ph = ctx.proc.engine.displayLfoPhase[(size_t) lfo].load();
    const int shape = (int) ctx.proc.apvts.getRawParameterValue (ids::lfo (lfo, "shape"))->load();
    if (std::abs (ph - lastPhase) > 0.002f || shape != lastShape)
    {
        lastPhase = ph;
        lastShape = shape;
        repaint();
    }
}
}
