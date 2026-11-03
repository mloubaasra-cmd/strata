#pragma once
#include "Common.h"

namespace strata
{
namespace ids
{
    inline juce::String osc (int o, const char* name)   { return juce::String (o == 0 ? "oscA_" : "oscB_") + name; }
    inline juce::String env (int e, const char* name)   { return "env" + juce::String (e + 1) + "_" + name; }
    inline juce::String lfo (int l, const char* name)   { return "lfo" + juce::String (l + 1) + "_" + name; }
    inline juce::String macro (int m)                   { return "macro" + juce::String (m + 1); }
}

namespace params
{
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    const juce::StringArray& warpModes();
    const juce::StringArray& filterTypes();
    const juce::StringArray& lfoShapes();
    const juce::StringArray& lfoDivisions();
    const juce::StringArray& voiceModes();
    const juce::StringArray& distTypes();
    const juce::StringArray& subWaves();
    const juce::StringArray& bankChoices();   // built-in banks + "User"

    double divisionToBeats (int index);       // length of one LFO / delay cycle in quarter notes

    enum WarpMode   { warpOff, warpSync, warpBend, warpPwm, warpMirror, warpFold, warpCrush };
    enum FilterType { fLP12, fLP24, fHP12, fHP24, fBP, fNotch };
    enum LfoShape   { lfoSine, lfoTri, lfoSawUp, lfoSawDown, lfoSquare, lfoSampleHold, lfoSmoothRandom, lfoCustom };
    enum VoiceMode  { modePoly, modeMono, modeLegato };
    enum DistType   { distSoft, distHard, distFold, distCrush, distDownsample };

    constexpr int kNumLfos = 4;
    constexpr int kNumEnvs = 3;
    constexpr int kNumMacros = 4;
    constexpr int kNumFx = 5;
    enum FxId { fxDistortion, fxChorus, fxPhaser, fxDelay, fxReverb };
    const juce::StringArray& fxNames();
}

namespace mod
{
    enum Source
    {
        srcEnv1, srcEnv2, srcEnv3,
        srcLfo1, srcLfo2, srcLfo3, srcLfo4,
        srcVelocity, srcModWheel, srcNote,
        srcMacro1, srcMacro2, srcMacro3, srcMacro4,
        srcRandom,
        kNumSources
    };

    constexpr int kMaxSlots = 16;

    const juce::StringArray& sourceNames();
    const juce::StringArray& sourceShortNames();
    bool isBipolarSource (int src);

    // Destinations are continuous parameters that can be modulated per voice.
    const juce::StringArray& destIDs();
    juce::String destName (int dest);
    int destIndexForParam (const juce::String& paramID);   // -1 if not modulatable
    int numDests();
}
}
