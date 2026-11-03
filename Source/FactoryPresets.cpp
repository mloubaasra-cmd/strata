// Factory presets. Each category has a "recipe" and every named patch is a variation of it,
// driven by a small table (wavetable choices + flavour) and a deterministic random seed.

#include "Presets.h"

namespace strata
{
using namespace juce;

namespace
{
    enum Bank { bBasic, bPwm, bHarm, bFormant, bFm, bSync, bGrowl, bDigital, bOrgan, bSpectral, bSawFold, bPluck };
    enum Div  { d4Bars, d2Bars, d1, d2, d2T, d4D, d4, d4T, d8D, d8, d8T, d16, d16T, d32 };

    constexpr float posSine = 0.0f, posTri = 0.333f, posSaw = 0.667f, posSquare = 1.0f;

    struct Entry { const char* name; int bankA; int bankB; int flavour; };

    // ---- small helpers --------------------------------------------------------
    String O (int o, const char* p) { return ids::osc (o, p); }

    void osc (PresetBuilder& b, int o, int bank, float pos, int uni, float detune, float level, int oct = 0, float semi = 0.0f)
    {
        b.set (O (o, "on"), 1).set (O (o, "bank"), (float) bank).set (O (o, "pos"), pos)
         .set (O (o, "uni"), (float) uni).set (O (o, "detune"), detune).set (O (o, "level"), level)
         .set (O (o, "oct"), (float) oct).set (O (o, "semi"), semi);
    }
    void env (PresetBuilder& b, int e, float a, float d, float s, float r)
    {
        b.set (ids::env (e, "atk"), a).set (ids::env (e, "dec"), d).set (ids::env (e, "sus"), s).set (ids::env (e, "rel"), r);
    }
    void filter (PresetBuilder& b, int type, float cutoff, float res, float envAmt, float drive = 0.0f, float key = 0.0f)
    {
        b.set ("flt_on", 1).set ("flt_type", (float) type).set ("flt_cutoff", cutoff).set ("flt_res", res)
         .set ("flt_env", envAmt).set ("flt_drive", drive).set ("flt_key", key);
    }
    void lfoFree (PresetBuilder& b, int l, int shape, float hz)
    {
        b.set (ids::lfo (l, "shape"), (float) shape).set (ids::lfo (l, "rate"), hz).set (ids::lfo (l, "sync"), 0);
    }
    void lfoSync (PresetBuilder& b, int l, int shape, int div)
    {
        b.set (ids::lfo (l, "shape"), (float) shape).set (ids::lfo (l, "div"), (float) div).set (ids::lfo (l, "sync"), 1);
    }
    void reverb (PresetBuilder& b, float mix, float size, float damp = 0.5f)
    {
        b.set ("rev_on", 1).set ("rev_mix", mix).set ("rev_size", size).set ("rev_damp", damp);
    }
    void delay (PresetBuilder& b, float mix, int div, float fb, bool pingPong = true)
    {
        b.set ("dly_on", 1).set ("dly_mix", mix).set ("dly_sync", 1).set ("dly_div", (float) div)
         .set ("dly_fb", fb).set ("dly_pp", pingPong ? 1.0f : 0.0f);
    }
    void chorus (PresetBuilder& b, float mix, float rate = 0.6f, float depth = 0.35f)
    {
        b.set ("cho_on", 1).set ("cho_mix", mix).set ("cho_rate", rate).set ("cho_depth", depth);
    }
    void dist (PresetBuilder& b, int type, float drive, float mix = 1.0f)
    {
        b.set ("dist_on", 1).set ("dist_type", (float) type).set ("dist_drive", drive).set ("dist_mix", mix);
    }
    void vibrato (PresetBuilder& b, int lfo, float cents)
    {
        lfoFree (b, lfo, params::lfoSine, 5.5f);
        b.modulate (mod::srcLfo1 + lfo, "oscA_fine", cents / 200.0f);
        b.modulate (mod::srcLfo1 + lfo, "oscB_fine", cents / 200.0f);
    }
    float bankDefaultPos (int bank, FastRandom& r)
    {
        switch (bank)
        {
            case bBasic: { const float p[] = { posSaw, posSaw, posSquare, posTri }; return p[r.nextInt (4)]; }
            case bPwm:   return r.range (0.0f, 0.6f);
            default:     return r.range (0.1f, 0.7f);
        }
    }
    uint32 seedFor (const char* name) { return (uint32) String (name).hashCode() ^ 0x9e3779b9u; }

    // ---- category recipes ------------------------------------------------------
    void makeBass (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, bankDefaultPos (e.bankA, r), r.nextInt (2) + 1, r.range (0.05f, 0.15f), 0.8f);
        b.set ("sub_on", 1).set ("sub_wave", 0).set ("sub_oct", -1).set ("sub_level", r.range (0.45f, 0.75f));
        filter (b, params::fLP24, r.range (180.0f, 800.0f), r.range (0.1f, 0.4f), r.range (0.25f, 0.5f));
        env (b, 0, 0.001f, 0.6f, 0.85f, 0.08f);
        env (b, 1, 0.001f, r.range (0.12f, 0.45f), r.range (0.05f, 0.3f), 0.15f);
        b.set ("voice_mode", e.flavour % 2 == 0 ? (float) params::modeMono : (float) params::modePoly)
         .set ("glide", e.flavour % 2 == 0 ? r.range (0.02f, 0.08f) : 0.0f);

        switch (e.flavour)
        {
            case 1: dist (b, params::distSoft, r.range (0.25f, 0.5f)); break;
            case 2:
                env (b, 2, 0.001f, r.range (0.2f, 0.6f), 0.0f, 0.2f);
                b.modulate (mod::srcEnv3, "oscA_pos", r.range (0.3f, 0.5f));
                break;
            case 3:
                b.set (O (0, "warpMode"), (float) params::warpBend).set (O (0, "warp"), r.range (0.3f, 0.7f));
                lfoSync (b, 0, params::lfoSine, d4);
                b.modulate (mod::srcLfo1, "flt_cutoff", 0.25f);
                break;
            case 4:
                osc (b, 1, e.bankB >= 0 ? e.bankB : bBasic, posSaw, 2, 0.2f, 0.5f, 0, 0.0f);
                b.set (O (1, "fine"), 7.0f);
                break;
            case 5:
                lfoSync (b, 0, params::lfoTri, d8);
                b.modulate (mod::srcLfo1, "flt_cutoff", 0.3f);
                b.modulate (mod::srcLfo1, "oscA_pos", 0.3f);
                break;
            default: break;
        }
        b.modulate (mod::srcVelocity, "flt_cutoff", 0.15f);
        b.modulate (mod::srcMacro1, "flt_cutoff", 0.4f);
        b.set ("master", e.flavour == 1 ? 0.8f : 1.05f);
    }

    void makeGrowl (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        const bool reese = e.flavour == 0;
        osc (b, 0, e.bankA, reese ? posSaw : r.range (0.2f, 0.5f), reese ? 3 : 2, reese ? r.range (0.25f, 0.4f) : 0.15f, 0.8f);
        osc (b, 1, e.bankB >= 0 ? e.bankB : e.bankA, reese ? posSaw : r.range (0.3f, 0.7f), 2, 0.3f, 0.6f, 0, reese ? 0.0f : 12.0f);
        b.set ("sub_on", 1).set ("sub_level", 0.55f).set ("sub_oct", -1);
        filter (b, params::fLP12, r.range (700.0f, 2500.0f), r.range (0.15f, 0.45f), 0.0f, r.range (0.2f, 0.5f));
        env (b, 0, 0.002f, 0.5f, 1.0f, 0.1f);
        b.set ("voice_mode", (float) params::modeLegato).set ("glide", r.range (0.03f, 0.1f)).set ("master", 0.6f);

        if (reese)
        {
            lfoFree (b, 0, params::lfoSine, r.range (0.1f, 0.4f));
            b.modulate (mod::srcLfo1, "flt_cutoff", 0.15f);
            b.modulate (mod::srcLfo1, "oscB_fine", 0.08f);
            dist (b, params::distSoft, 0.35f, 0.7f);
        }
        else
        {
            const int divs[] = { d4, d8, d8T, d16, d4T };
            lfoSync (b, 0, e.flavour == 3 ? params::lfoCustom : params::lfoSine, divs[r.nextInt (5)]);
            if (e.flavour == 3)
                b.curve (0, { { 0.0f, 0.0f }, { 0.1f, 1.0f }, { 0.35f, 0.6f }, { 0.5f, 0.0f }, { 0.6f, 0.9f }, { 0.8f, 0.3f }, { 1.0f, 0.0f } });
            b.modulate (mod::srcLfo1, "oscA_pos", r.range (0.25f, 0.45f));
            b.modulate (mod::srcLfo1, "oscB_pos", r.range (0.2f, 0.4f));
            b.modulate (mod::srcLfo1, "flt_cutoff", r.range (0.2f, 0.35f));
            const int warps[] = { params::warpFold, params::warpSync, params::warpBend, params::warpMirror };
            b.set (O (0, "warpMode"), (float) warps[e.flavour % 4]).set (O (0, "warp"), r.range (0.2f, 0.5f));
            b.modulate (mod::srcMacro1, "oscA_warp", 0.5f);
            dist (b, e.flavour % 2 == 0 ? params::distFold : params::distSoft, r.range (0.3f, 0.6f));
        }
        b.set ("cho_on", 0);
    }

    void makeLead (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        const int uni = 3 + r.nextInt (5);
        osc (b, 0, e.bankA, bankDefaultPos (e.bankA, r), uni, r.range (0.18f, 0.35f), 0.75f);
        if (e.bankB >= 0)
            osc (b, 1, e.bankB, bankDefaultPos (e.bankB, r), 2, 0.2f, 0.45f, e.flavour == 2 ? 1 : 0, e.flavour == 3 ? 7.0f : 0.0f);
        filter (b, params::fLP12, r.range (3000.0f, 12000.0f), r.range (0.1f, 0.3f), r.range (0.0f, 0.2f));
        env (b, 0, 0.004f, 0.4f, 0.8f, r.range (0.15f, 0.35f));
        env (b, 1, 0.001f, 0.3f, 0.3f, 0.3f);
        vibrato (b, 1, r.range (6.0f, 12.0f));
        b.modulate (mod::srcModWheel, "oscA_fine", 0.06f);
        if (e.flavour == 1 || e.flavour == 4)
            b.set ("voice_mode", (float) params::modeLegato).set ("glide", r.range (0.05f, 0.15f));
        if (e.bankA == bSync)
        {
            b.set (O (0, "warpMode"), (float) params::warpSync).set (O (0, "warp"), 0.3f);
            env (b, 2, 0.001f, 0.6f, 0.2f, 0.3f);
            b.modulate (mod::srcEnv3, "oscA_warp", 0.5f);
        }
        if (e.flavour == 5) dist (b, params::distHard, r.range (0.2f, 0.4f), 0.6f);
        delay (b, r.range (0.15f, 0.28f), r.nextInt (2) == 0 ? d8D : d4, 0.35f);
        reverb (b, 0.18f, 0.6f);
        b.modulate (mod::srcMacro1, "flt_cutoff", 0.3f);
    }

    void makePad (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, r.range (0.1f, 0.6f), 6 + r.nextInt (3), r.range (0.15f, 0.3f), 0.7f);
        if (e.bankB >= 0)
            osc (b, 1, e.bankB, r.range (0.2f, 0.8f), 4, 0.25f, 0.5f, e.flavour == 1 ? 1 : 0, e.flavour == 2 ? 7.0f : 0.0f);
        filter (b, params::fLP12, r.range (1500.0f, 6000.0f), r.range (0.05f, 0.25f), 0.1f);
        env (b, 0, r.range (0.4f, 1.8f), 1.5f, 0.85f, r.range (1.5f, 4.0f));
        env (b, 1, r.range (0.8f, 2.5f), 2.0f, 0.6f, 2.0f);
        lfoFree (b, 0, params::lfoSine, r.range (0.05f, 0.3f));
        b.set (ids::lfo (0, "retrig"), 0);
        b.modulate (mod::srcLfo1, "oscA_pos", r.range (0.2f, 0.4f));
        lfoFree (b, 2, params::lfoSmoothRandom, r.range (0.1f, 0.5f));
        b.modulate (mod::srcLfo3, "flt_cutoff", 0.12f);
        b.modulate (mod::srcLfo3, "oscB_pos", 0.2f);
        b.modulate (mod::srcMacro1, "oscA_pos", 0.5f);
        b.modulate (mod::srcMacro2, "flt_cutoff", 0.4f);
        if (e.flavour == 3) { b.set ("noise_on", 1).set ("noise_level", 0.12f).set ("noise_color", -0.5f); }
        chorus (b, 0.4f);
        reverb (b, r.range (0.35f, 0.5f), 0.85f, 0.4f);
        if (e.flavour == 4) b.set ("pha_on", 1).set ("pha_mix", 0.4f).set ("pha_rate", 0.15f);
    }

    void makePluck (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, bankDefaultPos (e.bankA, r), 1 + r.nextInt (4), 0.15f, 0.8f);
        if (e.bankB >= 0) osc (b, 1, e.bankB, r.range (0.0f, 0.5f), 1, 0.1f, 0.4f, 1);
        filter (b, params::fLP24, r.range (300.0f, 1000.0f), r.range (0.1f, 0.35f), r.range (0.5f, 0.7f), 0.0f, 0.3f);
        const float dec = r.range (0.2f, 0.6f);
        env (b, 0, 0.001f, dec, 0.0f, dec * 0.8f);
        env (b, 1, 0.001f, r.range (0.08f, 0.3f), 0.0f, 0.2f);
        if (e.flavour == 1) b.modulate (mod::srcEnv2, "oscA_pos", 0.4f);
        if (e.flavour == 2) { b.set (O (0, "warpMode"), (float) params::warpPwm).set (O (0, "warp"), 0.4f); }
        b.modulate (mod::srcVelocity, "flt_cutoff", 0.2f);
        delay (b, r.range (0.18f, 0.3f), r.nextInt (2) == 0 ? d8D : d8, 0.4f);
        reverb (b, 0.22f, 0.6f);
    }

    void makeKeys (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, e.bankA == bBasic ? posTri : r.range (0.1f, 0.5f), 1, 0.1f, 0.8f);
        if (e.bankB >= 0) osc (b, 1, e.bankB, r.range (0.0f, 0.4f), 2, 0.12f, 0.35f, e.flavour == 1 ? 1 : 0);
        filter (b, params::fLP12, r.range (2000.0f, 6000.0f), 0.1f, 0.2f, 0.0f, 0.5f);
        env (b, 0, 0.003f, r.range (1.0f, 2.2f), r.range (0.3f, 0.6f), 0.4f);
        env (b, 1, 0.001f, 0.8f, 0.2f, 0.4f);
        b.modulate (mod::srcVelocity, "flt_cutoff", 0.25f);
        if (e.bankA == bFm) { env (b, 2, 0.001f, 0.5f, 0.1f, 0.4f); b.modulate (mod::srcEnv3, "oscA_pos", 0.4f); }
        if (e.flavour == 2) { lfoFree (b, 0, params::lfoTri, 4.5f); b.modulate (mod::srcLfo1, "oscA_level", 0.15f); }
        if (e.flavour == 3) dist (b, params::distCrush, 0.45f, 0.5f);
        if (r.nextInt (2) == 0) chorus (b, 0.35f); else b.set ("pha_on", 1).set ("pha_mix", 0.35f);
        reverb (b, 0.2f, 0.5f);
    }

    void makeBell (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, r.range (0.35f, 0.9f), 1, 0.1f, 0.75f);
        osc (b, 1, e.bankB >= 0 ? e.bankB : bFm, r.range (0.2f, 0.8f), 1, 0.1f, 0.35f, 1, e.flavour == 1 ? 7.0f : 0.0f);
        b.set ("flt_on", 0);
        env (b, 0, 0.001f, r.range (1.8f, 4.0f), 0.0f, r.range (1.5f, 3.0f));
        env (b, 2, 0.001f, r.range (0.3f, 1.2f), 0.0f, 1.0f);
        b.modulate (mod::srcEnv3, "oscA_pos", -0.3f);
        delay (b, 0.2f, d4D, 0.45f);
        reverb (b, 0.4f, 0.85f, 0.3f);
    }

    void makeBrass (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, e.bankA == bBasic ? posSaw : r.range (0.3f, 0.7f), 3 + r.nextInt (3), r.range (0.12f, 0.22f), 0.75f);
        if (e.bankB >= 0) osc (b, 1, e.bankB, posSaw, 3, 0.2f, 0.4f, e.flavour == 1 ? -1 : 0);
        const bool strings = e.flavour >= 2;
        filter (b, params::fLP24, r.range (500.0f, 1200.0f), 0.15f, strings ? 0.2f : 0.45f);
        env (b, 1, strings ? 0.4f : r.range (0.05f, 0.12f), 0.5f, 0.5f, 0.3f);
        env (b, 0, strings ? r.range (0.25f, 0.6f) : r.range (0.03f, 0.08f), 0.5f, 0.9f, strings ? 0.8f : 0.2f);
        vibrato (b, 1, 7.0f);
        b.modulate (mod::srcModWheel, "flt_cutoff", 0.3f);
        chorus (b, strings ? 0.5f : 0.25f);
        reverb (b, strings ? 0.35f : 0.2f, 0.7f);
    }

    void makeArp (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, bankDefaultPos (e.bankA, r), 1 + r.nextInt (3), 0.15f, 0.8f);
        filter (b, params::fLP24, r.range (500.0f, 1500.0f), r.range (0.25f, 0.5f), 0.3f);
        env (b, 0, 0.002f, 0.6f, 0.7f, 0.25f);
        env (b, 1, 0.001f, 0.15f, 0.0f, 0.15f);
        b.set (ids::lfo (0, "retrig"), 0);

        switch (e.flavour)
        {
            case 0: case 4:   // stepped custom pattern on the filter
                lfoSync (b, 0, params::lfoCustom, d1);
                b.curve (0, { { 0.0f, 0.9f }, { 0.124f, 0.9f }, { 0.125f, 0.2f }, { 0.249f, 0.2f }, { 0.25f, 0.6f }, { 0.374f, 0.6f },
                              { 0.375f, 1.0f }, { 0.499f, 1.0f }, { 0.5f, 0.3f }, { 0.624f, 0.3f }, { 0.625f, 0.8f }, { 0.749f, 0.8f },
                              { 0.75f, 0.1f }, { 0.874f, 0.1f }, { 0.875f, 0.7f }, { 1.0f, 0.7f } });
                b.modulate (mod::srcLfo1, "flt_cutoff", 0.35f);
                break;
            case 1: case 6:   // random gates
                lfoSync (b, 0, params::lfoSampleHold, d16);
                b.modulate (mod::srcLfo1, "flt_cutoff", 0.3f);
                b.modulate (mod::srcLfo1, "oscA_pos", 0.3f);
                break;
            case 2: case 5:   // trance gate
                lfoSync (b, 0, params::lfoSquare, d16);
                b.modulate (mod::srcLfo1, "oscA_level", 0.35f);
                b.set (O (0, "level"), 0.45f);
                break;
            case 3:           // bubbling resonance
                lfoSync (b, 0, params::lfoSawDown, d16);
                filter (b, params::fLP12, 600.0f, 0.75f, 0.0f);
                b.modulate (mod::srcLfo1, "flt_cutoff", 0.3f);
                break;
            default:          // octave bounce
                lfoSync (b, 0, params::lfoSquare, d8);
                b.modulate (mod::srcLfo1, "oscA_semi", 0.125f);
                b.set (O (0, "semi"), 6.0f);
                break;
        }
        b.modulate (mod::srcMacro1, "flt_res", 0.4f);
        delay (b, 0.25f, d8D, 0.45f);
        reverb (b, 0.2f, 0.6f);
    }

    void makeAtmos (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, r.range (0.2f, 0.8f), 5 + r.nextInt (4), r.range (0.2f, 0.4f), 0.6f);
        if (e.bankB >= 0) osc (b, 1, e.bankB, r.range (0.2f, 0.8f), 3, 0.3f, 0.4f, -1);
        b.set ("noise_on", 1).set ("noise_level", r.range (0.15f, 0.4f)).set ("noise_color", r.range (-0.8f, 0.3f));
        filter (b, e.flavour % 2 == 0 ? params::fBP : params::fLP12, r.range (900.0f, 3000.0f), r.range (0.3f, 0.6f), 0.0f);
        env (b, 0, r.range (0.8f, 2.5f), 2.0f, 0.9f, r.range (3.0f, 6.0f));
        b.set ("master", 1.1f);
        lfoFree (b, 0, params::lfoSmoothRandom, r.range (0.05f, 0.2f));
        lfoFree (b, 1, params::lfoSine, r.range (0.03f, 0.12f));
        b.set (ids::lfo (0, "retrig"), 0).set (ids::lfo (1, "retrig"), 0);
        b.modulate (mod::srcLfo1, "oscA_pos", 0.4f);
        b.modulate (mod::srcLfo2, "flt_cutoff", 0.25f);
        b.modulate (mod::srcLfo1, "noise_color", 0.4f);
        b.modulate (mod::srcMacro1, "noise_level", 0.4f);
        b.set ("pha_on", 1).set ("pha_mix", 0.3f).set ("pha_rate", 0.08f);
        delay (b, 0.3f, d4D, 0.55f);
        reverb (b, 0.55f, 0.95f, 0.3f);
    }

    void makeFx (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, e.bankA, r.range (0.2f, 0.8f), 4, 0.3f, 0.7f);
        env (b, 0, 0.005f, 1.0f, 1.0f, 1.5f);
        filter (b, params::fLP12, 2000.0f, 0.3f, 0.0f);

        switch (e.flavour)
        {
            case 0:   // riser
                env (b, 2, 8.0f, 1.0f, 1.0f, 1.0f);
                b.set ("noise_on", 1).set ("noise_level", 0.4f);
                b.modulate (mod::srcEnv3, "oscA_semi", 0.25f);
                b.modulate (mod::srcEnv3, "flt_cutoff", 0.45f);
                lfoFree (b, 0, params::lfoSine, 4.0f);
                b.modulate (mod::srcEnv3, "lfo1_rate", 0.5f);
                b.modulate (mod::srcLfo1, "oscA_level", 0.2f);
                break;
            case 1:   // zap
                env (b, 0, 0.001f, 0.25f, 0.0f, 0.2f);
                env (b, 2, 0.001f, 0.18f, 0.0f, 0.1f);
                b.modulate (mod::srcEnv3, "oscA_semi", 0.5f);
                b.set (O (0, "semi"), -12.0f);
                break;
            case 2:   // siren
                lfoFree (b, 0, params::lfoTri, 0.6f);
                b.modulate (mod::srcLfo1, "oscA_semi", 0.15f);
                break;
            case 3:   // downlifter
                env (b, 2, 0.001f, 4.0f, 0.0f, 1.0f);
                b.set ("noise_on", 1).set ("noise_level", 0.35f).set (O (0, "semi"), -24.0f);
                b.modulate (mod::srcEnv3, "oscA_semi", 0.5f);
                b.modulate (mod::srcEnv3, "flt_cutoff", 0.4f);
                break;
            case 4:   // alarm / S&H pitch
                lfoSync (b, 0, params::lfoSampleHold, d16);
                b.modulate (mod::srcLfo1, "oscA_semi", 0.2f);
                break;
            case 5:   // noise sweep
                b.set (O (0, "on"), 0).set ("noise_on", 1).set ("noise_level", 0.9f);
                filter (b, params::fBP, 800.0f, 0.6f, 0.0f);
                lfoFree (b, 0, params::lfoSine, 0.25f);
                b.modulate (mod::srcLfo1, "flt_cutoff", 0.4f);
                break;
            case 6:   // robot talk
                lfoSync (b, 0, params::lfoSampleHold, d16);
                b.modulate (mod::srcLfo1, "oscA_pos", 0.5f);
                b.set (O (0, "bank"), (float) bFormant);
                b.set (O (0, "uni"), 1);
                break;
            case 7:   // bit storm
                dist (b, params::distDownsample, 0.6f);
                lfoFree (b, 0, params::lfoSampleHold, 12.0f);
                b.modulate (mod::srcLfo1, "oscA_semi", 0.15f);
                break;
            case 8:   // spaceship
                lfoFree (b, 0, params::lfoSine, 0.2f);
                lfoFree (b, 1, params::lfoSine, 7.0f);
                b.modulate (mod::srcLfo1, "lfo2_rate", 0.4f);
                b.modulate (mod::srcLfo2, "oscA_fine", 0.4f);
                b.modulate (mod::srcLfo1, "flt_cutoff", 0.3f);
                break;
            default:  // impact boom
                osc (b, 0, bBasic, posSine, 1, 0.0f, 0.9f, -1);
                env (b, 0, 0.001f, 2.5f, 0.0f, 2.0f);
                env (b, 2, 0.001f, 0.12f, 0.0f, 0.1f);
                b.modulate (mod::srcEnv3, "oscA_semi", 0.5f);
                b.set ("noise_on", 1).set ("noise_level", 0.5f);
                dist (b, params::distSoft, 0.4f);
                break;
        }
        reverb (b, 0.4f, 0.9f);
        delay (b, 0.2f, d4D, 0.5f);
        if (e.flavour == 1 || e.flavour == 5 || e.flavour == 6) b.set ("master", 1.3f);
        juce::ignoreUnused (r);
    }

    void makeSub (PresetBuilder& b, const Entry& e, FastRandom& r)
    {
        osc (b, 0, bBasic, posSine, 1, 0.0f, 0.95f);
        b.set ("voice_mode", (float) params::modeMono).set ("flt_on", 0);
        const bool eightOhEight = e.flavour <= 1 || e.flavour == 4;
        env (b, 0, 0.001f, eightOhEight ? r.range (1.2f, 2.5f) : 0.6f, eightOhEight ? 0.0f : 1.0f, 0.12f);
        if (eightOhEight)
        {
            env (b, 2, 0.001f, e.flavour == 4 ? 0.05f : r.range (0.06f, 0.12f), 0.0f, 0.05f);
            b.modulate (mod::srcEnv3, "oscA_semi", e.flavour == 4 ? 0.5f : 0.25f);
        }
        if (e.flavour == 1) dist (b, params::distSoft, 0.55f);
        if (e.flavour == 3) b.set ("glide", 0.12f).set ("voice_mode", (float) params::modeLegato);
        if (e.flavour == 5) { b.set (O (0, "pos"), 0.12f); dist (b, params::distSoft, 0.25f); }
        b.modulate (mod::srcMacro1, "oscA_pos", 0.3f);
        b.set ("master", e.flavour == 1 ? 0.9f : 1.25f);
    }

    using Recipe = void (*) (PresetBuilder&, const Entry&, FastRandom&);

    struct Category { const char* name; Recipe recipe; std::vector<Entry> entries; };

    const std::vector<Category>& categories()
    {
        static const std::vector<Category> c
        {
            { "Bass", makeBass, {
                { "Sub Pressure", bBasic, -1, 0 }, { "Rubber Floor", bBasic, -1, 2 }, { "Night Drive Bass", bBasic, bBasic, 4 },
                { "Concrete Thump", bDigital, -1, 1 }, { "Velvet Low", bOrgan, -1, 0 }, { "Detroit Pulse", bPwm, -1, 5 },
                { "Mono Grit", bSawFold, -1, 1 }, { "Analog Muscle", bBasic, bPwm, 4 }, { "Wobble Seed", bGrowl, -1, 5 },
                { "Neon Bassline", bSync, -1, 2 }, { "Tape Saturated", bHarm, -1, 1 }, { "Deep Cellar", bPluck, -1, 0 },
                { "Square Jaw", bPwm, -1, 3 }, { "Formant Funk", bFormant, -1, 2 } } },

            { "Growl", makeGrowl, {
                { "Reese Tunnel", bBasic, bBasic, 0 }, { "Grinder", bGrowl, bFm, 1 }, { "Howl Machine", bFormant, bGrowl, 2 },
                { "Metal Jaw", bFm, bSync, 3 }, { "Yoi Talk", bFormant, bFormant, 1 }, { "Wub Factory", bGrowl, bGrowl, 3 },
                { "Serrated", bSawFold, bDigital, 2 }, { "Gravel Throat", bDigital, bGrowl, 1 }, { "Feral", bSync, bFm, 3 },
                { "Chainsaw Choir", bFormant, bSawFold, 2 } } },

            { "Lead", makeLead, {
                { "Supernova Lead", bBasic, bBasic, 0 }, { "Glass Cutter", bHarm, -1, 1 }, { "Sync Screamer", bSync, bBasic, 5 },
                { "Hyper Saw", bBasic, bBasic, 2 }, { "Chip Hero", bPwm, -1, 4 }, { "Silver Whistle", bOrgan, -1, 1 },
                { "Laser Lane", bSync, -1, 3 }, { "Arena Anthem", bBasic, bHarm, 2 }, { "Soft Focus Lead", bPluck, -1, 0 },
                { "Portamento Dream", bBasic, -1, 4 }, { "Razor Lead", bSawFold, bBasic, 5 }, { "Vowel Lead", bFormant, -1, 1 },
                { "Square Sky", bPwm, bPwm, 3 }, { "Fuzz Pilot", bGrowl, -1, 5 } } },

            { "Pad", makePad, {
                { "Aurora Field", bSpectral, bHarm, 0 }, { "Slow Horizon", bBasic, bBasic, 1 }, { "Glass Cathedral", bOrgan, bFm, 2 },
                { "Velvet Clouds", bFormant, bBasic, 0 }, { "Breathing Wall", bSpectral, -1, 3 }, { "Polar Light", bHarm, bPluck, 2 },
                { "Dust & Halo", bDigital, bSpectral, 3 }, { "Choir of Machines", bFormant, bFormant, 4 }, { "Warm Blanket", bBasic, bPwm, 1 },
                { "Lunar Tide", bFm, bSpectral, 4 }, { "Spectral Drift", bSpectral, bSpectral, 2 }, { "Hollow Bloom", bPwm, bOrgan, 0 },
                { "Evening Haze", bPluck, bHarm, 3 }, { "Organ Mist", bOrgan, bOrgan, 4 } } },

            { "Pluck", makePluck, {
                { "Crystal Pluck", bPluck, bFm, 0 }, { "Marimbot", bOrgan, -1, 1 }, { "Tropic Drop", bBasic, -1, 0 },
                { "Pizzicato Pixel", bDigital, -1, 1 }, { "Water Drop", bFm, -1, 1 }, { "Short Fuse", bSawFold, -1, 0 },
                { "Kalimba Circuit", bFm, bOrgan, 1 }, { "Glass Tap", bHarm, -1, 0 }, { "Future Pluck", bBasic, bBasic, 2 },
                { "Hollow Nylon", bPwm, -1, 2 }, { "Plucked Formant", bFormant, -1, 1 }, { "Digital Harp", bDigital, bPluck, 0 } } },

            { "Keys", makeKeys, {
                { "E-Piano Ghost", bFm, bBasic, 2 }, { "Drawbar Diner", bOrgan, -1, 2 }, { "Soft Tines", bBasic, bFm, 0 },
                { "FM Tines", bFm, -1, 0 }, { "Toy Piano", bDigital, -1, 3 }, { "Chord Stab", bBasic, bBasic, 1 },
                { "Lofi Keys", bPluck, -1, 3 }, { "House Organ", bOrgan, bOrgan, 1 }, { "Felt Keys", bHarm, -1, 0 },
                { "Vowel Keys", bFormant, -1, 2 } } },

            { "Bells", makeBell, {
                { "Ice Bells", bFm, bFm, 0 }, { "Temple Chime", bFm, bOrgan, 1 }, { "Music Box", bPluck, bFm, 0 },
                { "Bell Tower", bFm, bSpectral, 1 }, { "FM Glock", bFm, bHarm, 0 }, { "Celestial", bSpectral, bFm, 1 },
                { "Wind Chimes", bHarm, bFm, 1 }, { "Metal Rain", bDigital, bFm, 0 } } },

            { "Brass & Strings", makeBrass, {
                { "Brass Section", bBasic, bBasic, 0 }, { "Synth Horns", bBasic, bPwm, 1 }, { "Soft Strings", bBasic, bBasic, 2 },
                { "Stab Brass", bSawFold, -1, 0 }, { "Bowed Glass", bHarm, bSpectral, 3 }, { "String Machine", bPwm, bBasic, 2 },
                { "Fanfare", bBasic, bSync, 1 }, { "Trumpet Haze", bFormant, bBasic, 0 } } },

            { "Arp & Seq", makeArp, {
                { "Step Machine", bBasic, -1, 0 }, { "Random Gates", bDigital, -1, 1 }, { "Sequenced Pulse", bPwm, -1, 2 },
                { "Pattern Drift", bSpectral, -1, 4 }, { "Clockwork", bFm, -1, 3 }, { "Trance Gate", bBasic, -1, 5 },
                { "Morse Code", bPwm, -1, 2 }, { "Bubbling", bBasic, -1, 3 }, { "Glitch Steps", bDigital, -1, 6 },
                { "Octave Bounce", bSawFold, -1, 7 } } },

            { "Atmos", makeAtmos, {
                { "Deep Space", bSpectral, bHarm, 0 }, { "Underwater City", bFormant, -1, 1 }, { "Wind Tunnel", bHarm, -1, 0 },
                { "Static Field", bDigital, bSpectral, 1 }, { "Frozen Lake", bPluck, bOrgan, 0 }, { "Cave Echo", bHarm, bFm, 1 },
                { "Solar Wind", bSync, bSpectral, 0 }, { "Dark Matter", bGrowl, bFormant, 1 }, { "Radio Ghosts", bFormant, bDigital, 0 },
                { "Fog Machine", bBasic, bSpectral, 1 } } },

            { "FX", makeFx, {
                { "Riser 8 Bars", bSync, -1, 0 }, { "Laser Zap", bBasic, -1, 1 }, { "Siren Sweep", bBasic, -1, 2 },
                { "Downlifter", bHarm, -1, 3 }, { "Alarm Clock", bPwm, -1, 4 }, { "Noise Sweep", bBasic, -1, 5 },
                { "Robot Talk", bFormant, -1, 6 }, { "Bit Storm", bDigital, -1, 7 }, { "Spaceship", bFm, -1, 8 },
                { "Impact Boom", bBasic, -1, 9 } } },

            { "Sub & 808", makeSub, {
                { "808 Classic", bBasic, -1, 0 }, { "808 Distorted", bBasic, -1, 1 }, { "Clean Sub", bBasic, -1, 2 },
                { "Sub Glide", bBasic, -1, 3 }, { "Kick Bass", bBasic, -1, 4 }, { "Boom Sine", bBasic, -1, 5 } } },
        };
        return c;
    }
}

const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = []
    {
        std::vector<FactoryPreset> p;
        p.push_back ({ "Init", "Init", [] (PresetBuilder& b)
        {
            osc (b, 0, bBasic, posSaw, 1, 0.2f, 0.75f);
            b.set (O (1, "on"), 0);
        } });

        for (auto& cat : categories())
            for (auto& entry : cat.entries)
            {
                const auto recipe = cat.recipe;
                const Entry e = entry;
                p.push_back ({ entry.name, cat.name, [recipe, e] (PresetBuilder& b)
                {
                    FastRandom r (seedFor (e.name));
                    b.set (O (1, "on"), 0);   // osc B only when the recipe asks for it
                    recipe (b, e, r);
                } });
            }
        return p;
    }();
    return presets;
}

StringArray getFactoryCategories()
{
    StringArray s { "Init" };
    for (auto& c : categories()) s.add (c.name);
    return s;
}
}
