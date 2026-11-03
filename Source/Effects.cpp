#include "Effects.h"

namespace strata
{
using namespace juce;

FxChain::FxChain (AudioProcessorValueTreeState& s) : apvts (s)
{
    for (int i = 0; i < params::kNumFx; ++i)
        order[(size_t) i].store (i);

    distOn = get ("dist_on"); distType = get ("dist_type"); distDrive = get ("dist_drive"); distMix = get ("dist_mix");
    choOn = get ("cho_on"); choRate = get ("cho_rate"); choDepth = get ("cho_depth"); choMix = get ("cho_mix");
    phaOn = get ("pha_on"); phaRate = get ("pha_rate"); phaDepth = get ("pha_depth"); phaFb = get ("pha_fb"); phaMix = get ("pha_mix");
    dlyOn = get ("dly_on"); dlyTime = get ("dly_time"); dlySync = get ("dly_sync"); dlyDiv = get ("dly_div");
    dlyFb = get ("dly_fb"); dlyMix = get ("dly_mix"); dlyPP = get ("dly_pp"); dlyTone = get ("dly_tone");
    revOn = get ("rev_on"); revSize = get ("rev_size"); revDamp = get ("rev_damp"); revWidth = get ("rev_width"); revMix = get ("rev_mix");
}

void FxChain::prepare (double sr, int maxBlock)
{
    sampleRate = sr;
    dsp::ProcessSpec spec { sr, (uint32) jmax (1, maxBlock), 2 };
    chorus.prepare (spec);
    phaser.prepare (spec);
    reverb.setSampleRate (sr);
    delayBuffer.setSize (2, (int) (sr * 6.0) + 4);
    reset();
}

void FxChain::reset()
{
    chorus.reset();
    phaser.reset();
    reverb.reset();
    delayBuffer.clear();
    delayWrite = 0;
    delaySmoothed = 0.0f;
    delayLP[0] = delayLP[1] = 0.0f;
    dsHold[0] = dsHold[1] = 0.0f;
    dsCounter = 0;
}

void FxChain::process (AudioBuffer<float>& buffer, double bpm)
{
    if (buffer.getNumChannels() < 2) return;

    int localOrder[params::kNumFx];
    for (int i = 0; i < params::kNumFx; ++i)
        localOrder[i] = order[(size_t) i].load (std::memory_order_relaxed);

    for (int slot = 0; slot < params::kNumFx; ++slot)
    {
        switch (localOrder[slot])
        {
            case params::fxDistortion:
                if (distOn.get() > 0.5f) processDistortion (buffer);
                break;

            case params::fxChorus:
                if (choOn.get() > 0.5f)
                {
                    chorus.setRate (choRate.get());
                    chorus.setDepth (choDepth.get());
                    chorus.setCentreDelay (8.0f);
                    chorus.setFeedback (0.1f);
                    chorus.setMix (choMix.get());
                    dsp::AudioBlock<float> block (buffer);
                    chorus.process (dsp::ProcessContextReplacing<float> (block));
                }
                break;

            case params::fxPhaser:
                if (phaOn.get() > 0.5f)
                {
                    phaser.setRate (phaRate.get());
                    phaser.setDepth (phaDepth.get());
                    phaser.setFeedback (phaFb.get());
                    phaser.setCentreFrequency (900.0f);
                    phaser.setMix (phaMix.get());
                    dsp::AudioBlock<float> block (buffer);
                    phaser.process (dsp::ProcessContextReplacing<float> (block));
                }
                break;

            case params::fxDelay:
                if (dlyOn.get() > 0.5f) processDelay (buffer, bpm);
                break;

            case params::fxReverb:
                if (revOn.get() > 0.5f)
                {
                    Reverb::Parameters p;
                    p.roomSize = revSize.get();
                    p.damping = revDamp.get();
                    p.width = revWidth.get();
                    p.wetLevel = revMix.get() * 0.8f;
                    p.dryLevel = 1.0f - revMix.get() * 0.5f;
                    p.freezeMode = 0.0f;
                    reverb.setParameters (p);
                    reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
                }
                break;

            default: break;
        }
    }
}

void FxChain::processDistortion (AudioBuffer<float>& buffer)
{
    const int type = (int) distType.get();
    const float drive = distDrive.get();
    const float mix = distMix.get();
    const float g = Decibels::decibelsToGain (drive * 36.0f);
    const float levels = std::exp2 (16.0f - drive * 13.0f);
    const int dsFactor = 1 + (int) (drive * 31.0f);
    const int n = buffer.getNumSamples();

    for (int i = 0; i < n; ++i)
    {
        const bool takeSample = (dsCounter++ % dsFactor) == 0;
        for (int c = 0; c < 2; ++c)
        {
            float* d = buffer.getWritePointer (c);
            const float x = d[i];
            float y = x;
            switch (type)
            {
                case params::distSoft: y = std::tanh (x * g) * 0.8f; break;
                case params::distHard: y = jlimit (-1.0f, 1.0f, x * g) * 0.75f; break;
                case params::distFold:
                {
                    const float t = 0.25f * x * g + 0.25f;
                    y = (4.0f * std::abs (t - std::round (t)) - 1.0f) * 0.7f;
                    break;
                }
                case params::distCrush: y = std::round (x * levels) / levels; break;
                case params::distDownsample:
                default:
                    if (takeSample) dsHold[c] = x;
                    y = dsHold[c];
                    break;
            }
            d[i] = x + mix * (y - x);
        }
    }
}

void FxChain::processDelay (AudioBuffer<float>& buffer, double bpm)
{
    const int n = buffer.getNumSamples();
    const int size = delayBuffer.getNumSamples();
    const double beats = params::divisionToBeats ((int) dlyDiv.get());
    const float targetSamples = dlySync.get() > 0.5f
        ? (float) (beats * 60.0 / jmax (20.0, bpm) * sampleRate)
        : dlyTime.get() * 0.001f * (float) sampleRate;
    const float target = jlimit (1.0f, (float) size - 2.0f, targetSamples);
    if (delaySmoothed <= 0.0f) delaySmoothed = target;

    const float fb = dlyFb.get();
    const float mix = dlyMix.get();
    const bool pingPong = dlyPP.get() > 0.5f;
    const float toneHz = 800.0f * std::pow (20.0f, dlyTone.get());
    const float lpCoef = 1.0f - std::exp (-MathConstants<float>::twoPi * toneHz / (float) sampleRate);

    auto* inL = buffer.getWritePointer (0);
    auto* inR = buffer.getWritePointer (1);
    auto* dl = delayBuffer.getWritePointer (0);
    auto* dr = delayBuffer.getWritePointer (1);

    for (int i = 0; i < n; ++i)
    {
        delaySmoothed += 0.0005f * (target - delaySmoothed);
        float readPos = (float) delayWrite - delaySmoothed;
        if (readPos < 0.0f) readPos += (float) size;
        const int r0 = (int) readPos;
        const int r1 = (r0 + 1) % size;
        const float fr = readPos - (float) r0;
        float yL = dl[r0] + fr * (dl[r1] - dl[r0]);
        float yR = dr[r0] + fr * (dr[r1] - dr[r0]);

        delayLP[0] += lpCoef * (yL - delayLP[0]);
        delayLP[1] += lpCoef * (yR - delayLP[1]);
        yL = delayLP[0];
        yR = delayLP[1];

        if (pingPong)
        {
            dl[delayWrite] = 0.5f * (inL[i] + inR[i]) + yR * fb;
            dr[delayWrite] = yL * fb;
        }
        else
        {
            dl[delayWrite] = inL[i] + yL * fb;
            dr[delayWrite] = inR[i] + yR * fb;
        }

        inL[i] += yL * mix;
        inR[i] += yR * mix;
        delayWrite = (delayWrite + 1) % size;
    }
}
}
