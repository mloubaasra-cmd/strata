#include "Tutorial.h"

namespace strata
{
using namespace juce;

TutorialOverlay::TutorialOverlay (UIContext& c) : ctx (c)
{
    back.onClick = [this] { if (index > 0) enterStep (index - 1); };
    next.onClick = [this]
    {
        if (index + 1 < (int) steps.size()) enterStep (index + 1);
        else stop();
    };
    close.onClick = [this] { stop(); };
    for (auto* b : { &back, &next, &close })
        addAndMakeVisible (b);
    setInterceptsMouseClicks (true, true);
}

void TutorialOverlay::setSteps (std::vector<TutorialStep> s) { steps = std::move (s); }

void TutorialOverlay::start()
{
    if (steps.empty()) return;
    setVisible (true);
    toFront (false);
    enterStep (0);
    startTimerHz (10);
}

void TutorialOverlay::stop()
{
    stopTimer();
    setVisible (false);
    if (onFinished) onFinished();
}

void TutorialOverlay::enterStep (int i)
{
    index = jlimit (0, (int) steps.size() - 1, i);
    completed = false;
    completedTicks = 0;
    if (steps[(size_t) index].onEnter) steps[(size_t) index].onEnter();
    layout();
    repaint();
}

static bool isVisibleUpTo (Component* c, Component* root)
{
    for (; c != nullptr && c != root; c = c->getParentComponent())
        if (! c->isVisible()) return false;
    return c == root;
}

std::vector<Rectangle<int>> TutorialOverlay::holes() const
{
    std::vector<Rectangle<int>> r;
    if (! steps.empty() && steps[(size_t) index].targets)
        for (auto* comp : steps[(size_t) index].targets())
            if (comp != nullptr && isVisibleUpTo (comp, getParentComponent()))
                r.push_back (getLocalArea (comp, comp->getLocalBounds()).expanded (5));
    return r;
}

bool TutorialOverlay::hitTest (int x, int y)
{
    for (auto& h : holes())
        if (h.contains (x, y)) return false;
    return true;
}

void TutorialOverlay::layout()
{
    if (steps.empty()) return;
    const auto& s = steps[(size_t) index];
    const int w = 360;

    AttributedString body;
    body.append (s.body, StrataLook::font (13.5f), colours::text);
    TextLayout tl;
    tl.createLayout (body, (float) (w - 32));
    const int h = 128 + (int) tl.getHeight() + (s.tryIt.isNotEmpty() ? 30 : 0);

    const auto hs = holes();
    Rectangle<int> target;
    for (auto& r : hs) target = target.isEmpty() ? r : target.getUnion (r);

    auto bounds = getLocalBounds().reduced (12);
    Rectangle<int> c (w, h);
    if (target.isEmpty())
        c.setCentre (bounds.getCentre());
    else if (target.getBottom() + 14 + h < bounds.getBottom())
        c.setPosition (target.getCentreX() - w / 2, target.getBottom() + 14);
    else if (target.getY() - 14 - h > bounds.getY())
        c.setPosition (target.getCentreX() - w / 2, target.getY() - 14 - h);
    else if (target.getRight() + 14 + w < bounds.getRight())
        c.setPosition (target.getRight() + 14, target.getCentreY() - h / 2);
    else
        c.setPosition (target.getX() - 14 - w, target.getCentreY() - h / 2);
    callout = c.constrainedWithin (bounds);

    auto row = callout.reduced (16).removeFromBottom (28);
    close.setBounds (row.removeFromLeft (90));
    next.setBounds (row.removeFromRight (90));
    row.removeFromRight (8);
    back.setBounds (row.removeFromRight (80));
    back.setVisible (index > 0);

    const bool waiting = s.isDone && ! completed;
    next.setButtonText (index + 1 == (int) steps.size() ? "FINISH" : (waiting ? "SKIP" : "NEXT"));
}

void TutorialOverlay::paint (Graphics& g)
{
    if (steps.empty()) return;
    const auto& s = steps[(size_t) index];
    const auto accent = ctx.look.getAccent();

    Path dim;
    dim.addRectangle (getLocalBounds().toFloat());
    const auto hs = holes();
    for (auto& h : hs)
        dim.addRoundedRectangle (h.toFloat(), 6.0f);
    dim.setUsingNonZeroWinding (false);
    g.setColour (Colours::black.withAlpha (0.62f));
    g.fillPath (dim);

    for (auto& h : hs)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.drawRoundedRectangle (h.toFloat().expanded (2.0f), 8.0f, 5.0f);
        g.setColour (accent);
        g.drawRoundedRectangle (h.toFloat(), 6.0f, 2.0f);
    }

    // callout
    auto c = callout.toFloat();
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (c.translated (0.0f, 4.0f), 10.0f);
    g.setColour (colours::panel.brighter (0.05f));
    g.fillRoundedRectangle (c, 10.0f);
    g.setColour (accent);
    g.drawRoundedRectangle (c.reduced (0.5f), 10.0f, 1.5f);

    auto r = callout.reduced (16);
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (11.0f, true));
    g.drawText ("STEP " + String (index + 1) + " OF " + String ((int) steps.size()), r.removeFromTop (14), Justification::centredLeft);
    g.setColour (accent);
    g.setFont (StrataLook::font (17.0f, true));
    g.drawText (s.title, r.removeFromTop (26), Justification::centredLeft);

    AttributedString body;
    body.append (s.body, StrataLook::font (13.5f), colours::text);
    TextLayout tl;
    tl.createLayout (body, (float) r.getWidth());
    tl.draw (g, r.removeFromTop ((int) tl.getHeight() + 2).toFloat());

    if (s.tryIt.isNotEmpty())
    {
        r.removeFromTop (8);
        auto line = r.removeFromTop (22).toFloat();
        const bool done = completed || (s.isDone == nullptr);
        g.setColour ((done ? colours::envColour : colours::mod).withAlpha (0.15f));
        g.fillRoundedRectangle (line, 4.0f);
        g.setColour (done ? colours::envColour : colours::mod);
        g.setFont (StrataLook::font (12.0f, true));
        g.drawText (completed ? "Nice! Moving on..." : "TRY IT:  " + s.tryIt, line.reduced (8.0f, 0.0f), Justification::centredLeft);
    }
}

void TutorialOverlay::timerCallback()
{
    if (steps.empty()) return;
    const auto& s = steps[(size_t) index];

    if (s.isDone && ! completed && s.isDone())
    {
        completed = true;
        completedTicks = 0;
        layout();
    }

    if (completed && ++completedTicks > 14)
    {
        if (index + 1 < (int) steps.size()) enterStep (index + 1);
        else { completed = false; layout(); }
        return;
    }

    layout();
    repaint();
}
}
