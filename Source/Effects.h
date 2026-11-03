#pragma once
#include "Common.h"
#include "Parameters.h"

namespace strata
{
/** The global effects rack. Order is user-rearrangeable from the FX page. */
class FxChain
{
public:
    explicit FxChain (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate, int maxBlockSize);
    void reset();
    void process (juce::AudioBuffer<float>& buffer, double bpm);

    std::array<std::atomic<int>, params::kNumFx> order;

private:
    void processDistortion (juce::AudioBuffer<float>&);
    void processDelay (juce::AudioBuffer<float>&, double bpm);

    struct Raw { std::atomic<float>* p = nullptr; float get() const { return p->load (std::memory_order_relaxed); } };
    Raw get (const juce::String& id) { return { apvts.getRawParameterValue (id) }; }

    juce::AudioProcessorValueTreeState& apvts;
    double sampleRate = 44100.0;

    Raw distOn, distType, distDrive, distMix;
    Raw choOn, choRate, choDepth, choMix;
    Raw phaOn, phaRate, phaDepth, phaFb, phaMix;
    Raw dlyOn, dlyTime, dlySync, dlyDiv, dlyFb, dlyMix, dlyPP, dlyTone;
    Raw revOn, revSize, revDamp, revWidth, revMix;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Phaser<float> phaser;
    juce::Reverb reverb;

    juce::AudioBuffer<float> delayBuffer;
    int delayWrite = 0;
    float delaySmoothed = 0.0f;
    float delayLP[2] {};

    float dsHold[2] {};
    int dsCounter = 0;
};
}
