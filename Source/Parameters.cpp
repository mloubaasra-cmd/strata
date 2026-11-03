#include "Parameters.h"

namespace strata
{
using namespace juce;

namespace
{
    String fmtPercent (float v, int)      { return String (roundToInt (v * 100.0f)) + "%"; }
    String fmtBipolar (float v, int)      { auto p = roundToInt (v * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; }
    String fmtHz (float v, int)           { return v < 1000.0f ? String (v, v < 10.0f ? 2 : 0) + " Hz" : String (v / 1000.0f, 2) + " kHz"; }
    String fmtTime (float v, int)         { return v < 1.0f ? String (v * 1000.0f, v < 0.1f ? 1 : 0) + " ms" : String (v, 2) + " s"; }
    String fmtMs (float v, int)           { return String (roundToInt (v)) + " ms"; }
    String fmtSemis (float v, int)        { return (v > 0.05f ? "+" : "") + String (v, std::abs (v - std::round (v)) < 0.05f ? 0 : 1) + " st"; }
    String fmtCents (float v, int)        { auto c = roundToInt (v); return (c > 0 ? "+" : "") + String (c) + " ct"; }
    String fmtPan (float v, int)
    {
        auto p = roundToInt (v * 100.0f);
        return p == 0 ? String ("C") : (p < 0 ? String (-p) + "L" : String (p) + "R");
    }
    String fmtDb (float v, int)           { return v <= 0.0001f ? String ("-inf") : String (Decibels::gainToDecibels (v), 1) + " dB"; }

    NormalisableRange<float> skewed (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    }
}

namespace params
{
    const StringArray& bankChoices()
    {
        static const StringArray s { "Basic Shapes", "PWM", "Harmonic Sweep", "Formant", "FM Metal", "Sync Sweep",
                                     "Growl", "Digital", "Organ", "Spectral", "Saw Fold", "Pluck Spectra", "User" };
        return s;
    }
    const StringArray& warpModes()    { static const StringArray s { "Off", "Sync", "Bend", "PWM", "Mirror", "Fold", "Crush" }; return s; }
    const StringArray& filterTypes()  { static const StringArray s { "LP 12", "LP 24", "HP 12", "HP 24", "Band", "Notch" }; return s; }
    const StringArray& lfoShapes()    { static const StringArray s { "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H", "Smooth Rnd", "Custom" }; return s; }
    const StringArray& lfoDivisions() { static const StringArray s { "4 Bars", "2 Bars", "1/1", "1/2", "1/2T", "1/4D", "1/4", "1/4T", "1/8D", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; return s; }
    const StringArray& voiceModes()   { static const StringArray s { "Poly", "Mono", "Legato" }; return s; }
    const StringArray& distTypes()    { static const StringArray s { "Soft", "Hard", "Fold", "Bitcrush", "Downsample" }; return s; }
    const StringArray& subWaves()     { static const StringArray s { "Sine", "Triangle", "Square" }; return s; }
    const StringArray& fxNames()      { static const StringArray s { "Distortion", "Chorus", "Phaser", "Delay", "Reverb" }; return s; }

    double divisionToBeats (int i)
    {
        static const double beats[] = { 16.0, 8.0, 4.0, 2.0, 4.0 / 3.0, 1.5, 1.0, 2.0 / 3.0, 0.75, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return beats[jlimit (0, (int) std::size (beats) - 1, i)];
    }

    AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<RangedAudioParameter>> p;

        auto fl = [&p] (const String& id, const String& name, NormalisableRange<float> r, float def,
                        std::function<String (float, int)> fmt)
        {
            p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                            AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
        };
        auto in = [&p] (const String& id, const String& name, int lo, int hi, int def)
        {
            p.push_back (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def));
        };
        auto ch = [&p] (const String& id, const String& name, const StringArray& items, int def)
        {
            p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def));
        };
        auto bo = [&p] (const String& id, const String& name, bool def)
        {
            p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
        };

        const NormalisableRange<float> unit (0.0f, 1.0f);
        const NormalisableRange<float> bip (-1.0f, 1.0f);
        const auto timeRange = NormalisableRange<float> (0.0005f, 10.0f, 0.0f, 0.3f);

        // Oscillators
        for (int o = 0; o < 2; ++o)
        {
            const String n = o == 0 ? "Osc A " : "Osc B ";
            bo (ids::osc (o, "on"),      n + "On", o == 0);
            ch (ids::osc (o, "bank"),    n + "Wavetable", bankChoices(), o == 0 ? 0 : 2);
            fl (ids::osc (o, "pos"),     n + "WT Pos", unit, 0.0f, fmtPercent);
            in (ids::osc (o, "oct"),     n + "Octave", -3, 3, 0);
            fl (ids::osc (o, "semi"),    n + "Semi", NormalisableRange<float> (-24.0f, 24.0f), 0.0f, fmtSemis);
            fl (ids::osc (o, "fine"),    n + "Fine", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, fmtCents);
            in (ids::osc (o, "uni"),     n + "Unison", 1, 8, 1);
            fl (ids::osc (o, "detune"),  n + "Detune", unit, 0.2f, fmtPercent);
            fl (ids::osc (o, "blend"),   n + "Blend", unit, 0.75f, fmtPercent);
            fl (ids::osc (o, "width"),   n + "Width", unit, 0.8f, fmtPercent);
            ch (ids::osc (o, "warpMode"),n + "Warp Mode", warpModes(), 0);
            fl (ids::osc (o, "warp"),    n + "Warp", unit, 0.0f, fmtPercent);
            fl (ids::osc (o, "level"),   n + "Level", unit, 0.75f, fmtPercent);
            fl (ids::osc (o, "pan"),     n + "Pan", bip, 0.0f, fmtPan);
        }

        // Sub & noise
        bo ("sub_on", "Sub On", false);
        ch ("sub_wave", "Sub Wave", subWaves(), 0);
        in ("sub_oct", "Sub Octave", -2, 0, -1);
        fl ("sub_level", "Sub Level", unit, 0.6f, fmtPercent);
        bo ("noise_on", "Noise On", false);
        fl ("noise_level", "Noise Level", unit, 0.3f, fmtPercent);
        fl ("noise_color", "Noise Color", bip, 0.0f, fmtBipolar);

        // Filter
        bo ("flt_on", "Filter On", true);
        ch ("flt_type", "Filter Type", filterTypes(), 1);
        fl ("flt_cutoff", "Cutoff", skewed (20.0f, 20000.0f, 800.0f), 20000.0f, fmtHz);
        fl ("flt_res", "Resonance", unit, 0.1f, fmtPercent);
        fl ("flt_drive", "Filter Drive", unit, 0.0f, fmtPercent);
        fl ("flt_env", "Filter Env Amt", bip, 0.0f, fmtBipolar);
        fl ("flt_key", "Key Track", unit, 0.0f, fmtPercent);

        // Envelopes (Env 1 = amp, Env 2 = filter env, Env 3 = free)
        for (int e = 0; e < kNumEnvs; ++e)
        {
            const String n = "Env " + String (e + 1) + " ";
            fl (ids::env (e, "atk"), n + "Attack",  timeRange, e == 0 ? 0.002f : 0.005f, fmtTime);
            fl (ids::env (e, "dec"), n + "Decay",   timeRange, e == 0 ? 0.6f : 0.4f, fmtTime);
            fl (ids::env (e, "sus"), n + "Sustain", unit, e == 0 ? 1.0f : 0.0f, fmtPercent);
            fl (ids::env (e, "rel"), n + "Release", timeRange, e == 0 ? 0.15f : 0.3f, fmtTime);
        }

        // LFOs
        for (int l = 0; l < kNumLfos; ++l)
        {
            const String n = "LFO " + String (l + 1) + " ";
            fl (ids::lfo (l, "rate"),  n + "Rate", skewed (0.01f, 40.0f, 2.0f), 2.0f, fmtHz);
            bo (ids::lfo (l, "sync"),  n + "Sync", false);
            ch (ids::lfo (l, "div"),   n + "Division", lfoDivisions(), 6);
            ch (ids::lfo (l, "shape"), n + "Shape", lfoShapes(), 0);
            bo (ids::lfo (l, "retrig"),n + "Retrigger", true);
        }

        for (int m = 0; m < kNumMacros; ++m)
            fl (ids::macro (m), "Macro " + String (m + 1), unit, 0.0f, fmtPercent);

        // Voice / master
        ch ("voice_mode", "Voice Mode", voiceModes(), 0);
        fl ("glide", "Glide", NormalisableRange<float> (0.0f, 2.0f, 0.0f, 0.4f), 0.0f, fmtTime);
        in ("bend_range", "Bend Range", 1, 24, 2);
        fl ("master", "Master", NormalisableRange<float> (0.0f, 1.5f, 0.0f, 0.6f), 0.7f, fmtDb);

        // Effects
        bo ("dist_on", "Dist On", false);
        ch ("dist_type", "Dist Type", distTypes(), 0);
        fl ("dist_drive", "Dist Drive", unit, 0.3f, fmtPercent);
        fl ("dist_mix", "Dist Mix", unit, 1.0f, fmtPercent);

        bo ("cho_on", "Chorus On", false);
        fl ("cho_rate", "Chorus Rate", skewed (0.05f, 5.0f, 0.8f), 0.6f, fmtHz);
        fl ("cho_depth", "Chorus Depth", unit, 0.35f, fmtPercent);
        fl ("cho_mix", "Chorus Mix", unit, 0.4f, fmtPercent);

        bo ("pha_on", "Phaser On", false);
        fl ("pha_rate", "Phaser Rate", skewed (0.02f, 5.0f, 0.5f), 0.3f, fmtHz);
        fl ("pha_depth", "Phaser Depth", unit, 0.6f, fmtPercent);
        fl ("pha_fb", "Phaser Feedback", NormalisableRange<float> (-0.95f, 0.95f), 0.4f, fmtBipolar);
        fl ("pha_mix", "Phaser Mix", unit, 0.5f, fmtPercent);

        bo ("dly_on", "Delay On", false);
        fl ("dly_time", "Delay Time", skewed (10.0f, 2000.0f, 350.0f), 375.0f, fmtMs);
        bo ("dly_sync", "Delay Sync", true);
        ch ("dly_div", "Delay Division", lfoDivisions(), 8);
        fl ("dly_fb", "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.4f, fmtPercent);
        fl ("dly_mix", "Delay Mix", unit, 0.25f, fmtPercent);
        bo ("dly_pp", "Ping Pong", true);
        fl ("dly_tone", "Delay Tone", unit, 0.6f, fmtPercent);

        bo ("rev_on", "Reverb On", false);
        fl ("rev_size", "Reverb Size", unit, 0.6f, fmtPercent);
        fl ("rev_damp", "Reverb Damping", unit, 0.5f, fmtPercent);
        fl ("rev_width", "Reverb Width", unit, 1.0f, fmtPercent);
        fl ("rev_mix", "Reverb Mix", unit, 0.25f, fmtPercent);

        return { p.begin(), p.end() };
    }
}

namespace mod
{
    const StringArray& sourceNames()
    {
        static const StringArray s { "Env 1", "Env 2", "Env 3", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
                                     "Velocity", "Mod Wheel", "Note", "Macro 1", "Macro 2", "Macro 3", "Macro 4", "Random" };
        return s;
    }
    const StringArray& sourceShortNames()
    {
        static const StringArray s { "ENV1", "ENV2", "ENV3", "LFO1", "LFO2", "LFO3", "LFO4",
                                     "VEL", "MW", "NOTE", "M1", "M2", "M3", "M4", "RND" };
        return s;
    }
    bool isBipolarSource (int s)
    {
        return (s >= srcLfo1 && s <= srcLfo4) || s == srcNote || s == srcRandom;
    }

    const StringArray& destIDs()
    {
        static const StringArray s = []
        {
            StringArray a;
            for (int o = 0; o < 2; ++o)
                for (auto* n : { "pos", "semi", "fine", "detune", "blend", "warp", "level", "pan" })
                    a.add (ids::osc (o, n));
            a.addArray (StringArray { "sub_level", "noise_level", "noise_color", "flt_cutoff", "flt_res", "flt_drive", "flt_env" });
            for (int l = 0; l < params::kNumLfos; ++l)
                a.add (ids::lfo (l, "rate"));
            return a;
        }();
        return s;
    }

    int numDests() { return destIDs().size(); }

    String destName (int d)
    {
        static const StringArray names = []
        {
            StringArray a;
            for (int o = 0; o < 2; ++o)
                for (auto* n : { "WT Pos", "Semi", "Fine", "Detune", "Blend", "Warp", "Level", "Pan" })
                    a.add (String (o == 0 ? "Osc A " : "Osc B ") + n);
            a.addArray (StringArray { "Sub Level", "Noise Level", "Noise Color", "Cutoff", "Resonance", "Filter Drive", "Filter Env" });
            for (int l = 0; l < params::kNumLfos; ++l)
                a.add ("LFO " + String (l + 1) + " Rate");
            return a;
        }();
        return isPositiveAndBelow (d, names.size()) ? names[d] : String ("-");
    }

    int destIndexForParam (const String& id) { return destIDs().indexOf (id); }
}
}
