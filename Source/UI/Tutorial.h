#pragma once
#include "Controls.h"

namespace strata
{
struct TutorialStep
{
    juce::String title, body, tryIt;
    std::function<std::vector<juce::Component*>()> targets;   // highlighted & clickable through the overlay
    std::function<void()> onEnter;
    std::function<bool()> isDone;                            // optional: step completes when this returns true
};

/**
    Interactive tour. Dims the editor, cuts holes around the controls being explained
    (they stay fully usable), and advances on its own when the user does what it asks.
*/
class TutorialOverlay : public juce::Component, private juce::Timer
{
public:
    explicit TutorialOverlay (UIContext&);
    void setSteps (std::vector<TutorialStep>);
    void start();
    void stop();
    void goToStep (int i) { enterStep (i); }

    void paint (juce::Graphics&) override;
    void resized() override { layout(); }
    bool hitTest (int x, int y) override;

    std::function<void()> onFinished;

private:
    void timerCallback() override;
    void enterStep (int index);
    void layout();
    std::vector<juce::Rectangle<int>> holes() const;

    UIContext& ctx;
    std::vector<TutorialStep> steps;
    int index = 0;
    bool completed = false;
    int completedTicks = 0;
    juce::Rectangle<int> callout;
    juce::TextButton back { "BACK" }, next { "NEXT" }, close { "END TOUR" };
};
}
