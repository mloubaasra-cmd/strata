#include "Engine.h"

namespace strata
{
using namespace juce;

namespace
{
    // Destination indices (must match mod::destIDs() order)
    enum Dest
    {
        dPos = 0, dSemi, dFine, dDetune, dBlend, dWarp, dLevel, dPan,   // + 8 * osc
        dSubLevel = 16, dNoiseLevel, dNoiseColor, dCutoff, dRes, dDrive, dFltEnv,
        dLfoRate0 = 23
    };

    constexpr float kVoiceGain = 0.3f;

    inline float wrap01 (float x) noexcept { return x - std::floor (x); }

    inline float warpPhase (int mode, float a, float k, float p) noexcept
    {
        switch (mode)
        {
            case params::warpSync:   return wrap01 (p * (1.0f + a * 7.0f));
            case params::warpBend:
            {
                const float x = 2.0f * p - 1.0f;
                const float y = x * (1.0f + k) / (1.0f + k * std::abs (x));
                return 0.5f * (y + 1.0f) * 0.9999f;
            }
            case params::warpPwm:
            {
                const float w = 0.5f - 0.48f * a;
                return p < w ? 0.5f * p / w : 0.5f + 0.5f * (p - w) / (1.0f - w);
            }
            case params::warpMirror:
            {
                const float m = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p;
                return jlimit (0.0f, 0.9999f, p + a * (m - p));
            }
            case params::warpCrush:  return std::floor (p * k) / k;
            default:                 return p;
        }
    }

    inline float foldSample (float x, float a) noexcept
    {
        const float y = x * (1.0f + a * 5.0f);
        const float t = 0.25f * y + 0.25f;
        return 4.0f * std::abs (t - std::round (t)) - 1.0f;
    }

    inline float polyBlep (float t, float dt) noexcept
    {
        if (t < dt)            { t /= dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt)     { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    inline float midiToHz (float note) noexcept { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
}

//==============================================================================
Engine::Engine (AudioProcessorValueTreeState& s) : apvts (s)
{
    for (int o = 0; o < 2; ++o)
    {
        auto& p = oscP[o];
        p.on = get (ids::osc (o, "on"));         p.bank = get (ids::osc (o, "bank"));
        p.pos = get (ids::osc (o, "pos"));       p.oct = get (ids::osc (o, "oct"));
        p.semi = get (ids::osc (o, "semi"));     p.fine = get (ids::osc (o, "fine"));
        p.uni = get (ids::osc (o, "uni"));       p.detune = get (ids::osc (o, "detune"));
        p.blend = get (ids::osc (o, "blend"));   p.width = get (ids::osc (o, "width"));
        p.warpMode = get (ids::osc (o, "warpMode")); p.warp = get (ids::osc (o, "warp"));
        p.level = get (ids::osc (o, "level"));   p.pan = get (ids::osc (o, "pan"));
    }
    subOn = get ("sub_on"); subWave = get ("sub_wave"); subOct = get ("sub_oct"); subLevel = get ("sub_level");
    noiseOn = get ("noise_on"); noiseLevel = get ("noise_level"); noiseColor = get ("noise_color");
    fltOn = get ("flt_on"); fltType = get ("flt_type"); fltCutoff = get ("flt_cutoff"); fltRes = get ("flt_res");
    fltDrive = get ("flt_drive"); fltEnv = get ("flt_env"); fltKey = get ("flt_key");
    for (int e = 0; e < params::kNumEnvs; ++e)
    {
        envA[e] = get (ids::env (e, "atk")); envD[e] = get (ids::env (e, "dec"));
        envS[e] = get (ids::env (e, "sus")); envR[e] = get (ids::env (e, "rel"));
    }
    for (int l = 0; l < params::kNumLfos; ++l)
    {
        lfoRate[l] = get (ids::lfo (l, "rate")); lfoSync[l] = get (ids::lfo (l, "sync"));
        lfoDiv[l] = get (ids::lfo (l, "div"));   lfoShape[l] = get (ids::lfo (l, "shape"));
        lfoRetrig[l] = get (ids::lfo (l, "retrig"));
    }
    for (int m = 0; m < params::kNumMacros; ++m)
        macros[m] = get (ids::macro (m));
    mode = get ("voice_mode"); glide = get ("glide"); bendRange = get ("bend_range");

    numDests = jmin (kMaxDests, mod::numDests());
    for (int d = 0; d < numDests; ++d)
        destParams[(size_t) d] = apvts.getParameter (mod::destIDs()[d]);

    for (auto& curve : lfoCurves)
        for (int i = 0; i < kCurveSize; ++i)
            curve[(size_t) i].store (1.0f - std::abs (2.0f * (float) i / (float) (kCurveSize - 1) - 1.0f));

    for (size_t i = 0; i < voices.size(); ++i)
        voices[i].rng = FastRandom (1000u + (uint32_t) i * 7919u);

    for (int l = 0; l < params::kNumLfos; ++l)
    {
        globalRandA[l] = rng.nextBipolar();
        globalRandB[l] = rng.nextBipolar();
    }
}

void Engine::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void Engine::reset()
{
    for (auto& v : voices) v.kill();
    heldNotes.clear();
    sustainPedal = false;
}

int Engine::voiceMode() const { return (int) mode.get(); }

float Engine::lfoSample (int l, float ph, float ra, float rb) const
{
    switch ((int) lfoShape[l].get())
    {
        case params::lfoSine:         return std::sin (MathConstants<float>::twoPi * ph);
        case params::lfoTri:          return ph < 0.25f ? 4.0f * ph : (ph < 0.75f ? 2.0f - 4.0f * ph : 4.0f * ph - 4.0f);
        case params::lfoSawUp:        return 2.0f * ph - 1.0f;
        case params::lfoSawDown:      return 1.0f - 2.0f * ph;
        case params::lfoSquare:       return ph < 0.5f ? 1.0f : -1.0f;
        case params::lfoSampleHold:   return ra;
        case params::lfoSmoothRandom: return ra + (rb - ra) * (0.5f - 0.5f * std::cos (MathConstants<float>::pi * ph));
        case params::lfoCustom:
        default:
        {
            const auto& c = lfoCurves[(size_t) l];
            const float x = jlimit (0.0f, 1.0f, ph) * (float) (kCurveSize - 1);
            const int i = jmin (kCurveSize - 2, (int) x);
            const float fr = x - (float) i;
            const float a = c[(size_t) i].load (std::memory_order_relaxed);
            const float b = c[(size_t) i + 1].load (std::memory_order_relaxed);
            return (a + fr * (b - a)) * 2.0f - 1.0f;
        }
    }
}

//==============================================================================
void Engine::render (AudioBuffer<float>& buffer, const MidiBuffer& midi, double bpm)
{
    const int numSamples = buffer.getNumSamples();
    buffer.clear();
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    // Mono hosts: render into a scratch right channel
    float scratchR[kChunk];

    auto it = midi.cbegin();
    for (int start = 0; start < numSamples; start += kChunk)
    {
        const int len = jmin (kChunk, numSamples - start);

        while (it != midi.cend() && (*it).samplePosition < start + len)
        {
            handleMidi ((*it).getMessage());
            ++it;
        }

        updateChunkGlobals (len, bpm);

        float* r = outR != nullptr ? outR + start : scratchR;
        if (outR == nullptr) std::fill (scratchR, scratchR + len, 0.0f);

        for (auto& v : voices)
            if (v.active)
                v.render (*this, outL + start, r, len);
    }

    // Display values from the newest sounding voice
    Voice* newest = nullptr;
    int count = 0;
    for (auto& v : voices)
        if (v.active)
        {
            ++count;
            if (newest == nullptr || v.age > newest->age) newest = &v;
        }
    activeVoices.store (count);

    for (int d = 0; d < numDests; ++d)
        displayValue[(size_t) d].store (newest != nullptr ? newest->modFinal[(size_t) d] : baseNorm[(size_t) d]);
    for (int l = 0; l < params::kNumLfos; ++l)
        displayLfoPhase[(size_t) l].store (newest != nullptr && lfoRetrig[l].get() > 0.5f ? newest->lfoPhase[l] : globalLfoPhase[l]);
    for (int e = 0; e < params::kNumEnvs; ++e)
        displayEnvLevel[(size_t) e].store (newest != nullptr ? newest->env[e].level : 0.0f);
}

void Engine::updateChunkGlobals (int len, double bpm)
{
    currentBpm = bpm > 0.0 ? bpm : 120.0;

    numActiveSlots = 0;
    destModulated.fill (false);
    const int n = jmin (numSlots.load(), mod::kMaxSlots);
    for (int i = 0; i < n; ++i)
    {
        const int s = slots[(size_t) i].src.load (std::memory_order_relaxed);
        const int d = slots[(size_t) i].dst.load (std::memory_order_relaxed);
        const float a = slots[(size_t) i].amount.load (std::memory_order_relaxed);
        if (isPositiveAndBelow (s, (int) mod::kNumSources) && isPositiveAndBelow (d, numDests) && a != 0.0f)
        {
            activeSlots[(size_t) numActiveSlots++] = { s, d, a };
            destModulated[(size_t) d] = true;
        }
    }

    for (int d = 0; d < numDests; ++d)
    {
        auto* p = destParams[(size_t) d];
        const float actual = p->convertFrom0to1 (p->getValue());
        baseActual[(size_t) d] = actual;
        baseNorm[(size_t) d] = p->getValue();
    }

    for (int l = 0; l < params::kNumLfos; ++l)
    {
        lfoHz[l] = lfoSync[l].get() > 0.5f
                     ? (float) (currentBpm / 60.0 / params::divisionToBeats ((int) lfoDiv[l].get()))
                     : lfoRate[l].get();
        globalLfoPhase[l] += lfoHz[l] * (float) len / (float) sampleRate;
        if (globalLfoPhase[l] >= 1.0f)
        {
            globalLfoPhase[l] = wrap01 (globalLfoPhase[l]);
            globalRandA[l] = globalRandB[l];
            globalRandB[l] = rng.nextBipolar();
        }
    }
}

//==============================================================================
void Engine::handleMidi (const MidiMessage& m)
{
    if (m.isNoteOn())                       noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())                 noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) allNotesOff (m.isAllSoundOff());
    else if (m.isPitchWheel())              pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
    else if (m.isControllerOfType (1))      modWheel = (float) m.getControllerValue() / 127.0f;
    else if (m.isSustainPedalOn())          sustainPedal = true;
    else if (m.isSustainPedalOff())
    {
        sustainPedal = false;
        for (auto& v : voices)
            if (v.active && v.sustained && ! v.keyDown) { v.sustained = false; v.release(); }
    }
}

Voice* Engine::findVoiceToUse()
{
    for (auto& v : voices)
        if (! v.active) return &v;

    // Steal: prefer the quietest released voice, otherwise the oldest
    Voice* best = nullptr;
    for (auto& v : voices)
        if (! v.keyDown && (best == nullptr || v.env[0].level < best->env[0].level)) best = &v;
    if (best != nullptr) return best;

    for (auto& v : voices)
        if (best == nullptr || v.age < best->age) best = &v;
    return best;
}

void Engine::noteOn (int note, float vel)
{
    const int vm = voiceMode();
    const float glideFrom = lastPitch >= 0.0f ? lastPitch : (float) note;

    if (vm == params::modePoly)
    {
        Voice* v = nullptr;
        for (auto& candidate : voices)
            if (candidate.active && candidate.note == note) { v = &candidate; break; }
        if (v == nullptr) v = findVoiceToUse();

        const bool reuse = v->active && v->note == note;
        if (! reuse) v->kill();
        v->start (*this, note, vel, glideFrom, reuse);
    }
    else
    {
        heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
        heldNotes.push_back (note);
        auto& v = voices[0];
        for (size_t i = 1; i < voices.size(); ++i) voices[i].kill();

        if (vm == params::modeLegato && v.active && v.keyDown)
        {
            v.note = note;
            v.targetPitch = (float) note;
            if (glide.get() <= 0.0001f) v.currentPitch = (float) note;
        }
        else
        {
            v.start (*this, note, vel, v.active ? v.currentPitch : glideFrom, v.active);
        }
    }
    lastPitch = (float) note;
}

void Engine::noteOff (int note)
{
    const int vm = voiceMode();
    if (vm == params::modePoly)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.keyDown)
            {
                v.keyDown = false;
                if (sustainPedal) v.sustained = true;
                else v.release();
            }
        return;
    }

    heldNotes.erase (std::remove (heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    auto& v = voices[0];
    if (! v.active || v.note != note) return;

    if (! heldNotes.empty())
    {
        const int back = heldNotes.back();
        if (vm == params::modeLegato)
        {
            v.note = back;
            v.targetPitch = (float) back;
            if (glide.get() <= 0.0001f) v.currentPitch = (float) back;
        }
        else
            v.start (*this, back, v.velocity, v.currentPitch, true);
        lastPitch = (float) back;
    }
    else
    {
        v.keyDown = false;
        if (sustainPedal) v.sustained = true;
        else v.release();
    }
}

void Engine::allNotesOff (bool hard)
{
    heldNotes.clear();
    for (auto& v : voices)
    {
        if (hard) v.kill();
        else if (v.active) { v.keyDown = false; v.release(); }
    }
}

//==============================================================================
void Voice::start (Engine& e, int n, float vel, float glideFrom, bool retriggerFromCurrent)
{
    const bool wasActive = active && retriggerFromCurrent;
    active = true;
    keyDown = true;
    sustained = false;
    note = n;
    velocity = vel;
    age = ++e.ageCounter;
    targetPitch = (float) n;
    currentPitch = e.glide.get() > 0.0001f ? glideFrom : (float) n;
    randomValue = rng.nextBipolar();

    for (auto& en : env)
    {
        if (! wasActive) en.level = 0.0f;
        en.noteOn();
    }

    for (int l = 0; l < params::kNumLfos; ++l)
    {
        if (e.lfoRetrig[l].get() > 0.5f)
        {
            lfoPhase[l] = 0.0f;
            lfoRandA[l] = rng.nextBipolar();
            lfoRandB[l] = rng.nextBipolar();
        }
    }

    if (! wasActive)
    {
        fresh = true;
        for (int o = 0; o < 2; ++o)
        {
            const int uni = (int) e.oscP[o].uni.get();
            for (int u = 0; u < kMaxUnison; ++u)
                osc[o].phase[u] = uni > 1 ? rng.nextFloat() : 0.0f;
        }
        subPhase = 0.0f;
        noiseLP = 0.0f;
        std::memset (ic1, 0, sizeof (ic1));
        std::memset (ic2, 0, sizeof (ic2));
    }
}

void Voice::release()
{
    for (auto& en : env) en.noteOff();
}

void Voice::kill()
{
    active = false;
    keyDown = false;
    sustained = false;
    note = -1;
    for (auto& en : env) en.kill();
}

void Voice::render (Engine& e, float* outL, float* outR, int numSamples)
{
    const float sr = (float) e.sampleRate;
    const int nd = e.numDests;

    // ---- envelopes -----------------------------------------------------------
    for (int i = 0; i < params::kNumEnvs; ++i)
        env[i].set (e.envA[i].get(), e.envD[i].get(), e.envS[i].get(), e.envR[i].get(), sr);

    // ---- LFOs (rates use last chunk's modulation) -----------------------------
    for (int l = 0; l < params::kNumLfos; ++l)
    {
        if (e.lfoRetrig[l].get() > 0.5f)
        {
            float hz = e.lfoHz[l];
            const int d = dLfoRate0 + l;
            if (e.lfoSync[l].get() < 0.5f && e.destModulated[(size_t) d])
                hz = e.destParams[(size_t) d]->convertFrom0to1 (jlimit (0.0f, 1.0f, modFinal[(size_t) d]));
            lfoPhase[l] += hz * (float) numSamples / sr;
            if (lfoPhase[l] >= 1.0f)
            {
                lfoPhase[l] = wrap01 (lfoPhase[l]);
                lfoRandA[l] = lfoRandB[l];
                lfoRandB[l] = rng.nextBipolar();
            }
            lfoValue[l] = e.lfoSample (l, lfoPhase[l], lfoRandA[l], lfoRandB[l]);
        }
        else
            lfoValue[l] = e.lfoSample (l, e.globalLfoPhase[l], e.globalRandA[l], e.globalRandB[l]);
    }

    // ---- glide ---------------------------------------------------------------
    const float glideTime = e.glide.get();
    if (glideTime > 0.0001f)
    {
        const float coef = 1.0f - std::exp (-(float) numSamples / (glideTime * 0.3f * sr));
        currentPitch += (targetPitch - currentPitch) * coef;
    }
    else
        currentPitch = targetPitch;

    // ---- modulation ----------------------------------------------------------
    float src[mod::kNumSources];
    src[mod::srcEnv1] = env[0].level;
    src[mod::srcEnv2] = env[1].level;
    src[mod::srcEnv3] = env[2].level;
    for (int l = 0; l < params::kNumLfos; ++l) src[mod::srcLfo1 + l] = lfoValue[l];
    src[mod::srcVelocity] = velocity;
    src[mod::srcModWheel] = e.modWheel;
    src[mod::srcNote] = jlimit (-1.0f, 1.0f, (currentPitch - 60.0f) / 24.0f);
    for (int m = 0; m < params::kNumMacros; ++m) src[mod::srcMacro1 + m] = e.macros[m].get();
    src[mod::srcRandom] = randomValue;

    for (int d = 0; d < nd; ++d) modFinal[(size_t) d] = e.baseNorm[(size_t) d];
    for (int i = 0; i < e.numActiveSlots; ++i)
    {
        const auto& s = e.activeSlots[(size_t) i];
        modFinal[(size_t) s.dst] += src[s.src] * s.amt;
    }

    auto actual = [&] (int d) -> float
    {
        if (! e.destModulated[(size_t) d]) return e.baseActual[(size_t) d];
        modFinal[(size_t) d] = jlimit (0.0f, 1.0f, modFinal[(size_t) d]);
        return e.destParams[(size_t) d]->convertFrom0to1 (modFinal[(size_t) d]);
    };

    const float bend = e.pitchBend * e.bendRange.get();
    const float notePitch = currentPitch + bend;

    // ---- oscillator setup ----------------------------------------------------
    struct OscRun
    {
        bool on = false;
        const Wavetable* wt = nullptr;
        int mip = 0, uni = 1, warpMode = 0;
        float warpAmt = 0, warpK = 0;
        float inc[kMaxUnison] {}, gL[kMaxUnison] {}, gR[kMaxUnison] {};
        float pos = 0, posStep = 0, lvl = 0, lvlStep = 0;
    } run[2];

    for (int o = 0; o < 2; ++o)
    {
        auto& p = e.oscP[o];
        auto& r = run[o];
        r.on = p.on.get() > 0.5f;
        if (! r.on) continue;

        r.wt = e.tables[(size_t) o].load (std::memory_order_acquire);
        if (r.wt == nullptr) { r.on = false; continue; }

        const int base = o * 8;
        const float pos   = jlimit (0.0f, 1.0f, actual (base + dPos));
        const float semi  = actual (base + dSemi);
        const float fine  = actual (base + dFine);
        const float det   = actual (base + dDetune);
        const float blend = actual (base + dBlend);
        const float warp  = actual (base + dWarp);
        const float level = actual (base + dLevel);
        const float pan   = actual (base + dPan);
        const float width = p.width.get();

        r.uni = jlimit (1, kMaxUnison, (int) p.uni.get());
        r.warpMode = (int) p.warpMode.get();
        r.warpAmt = warp;
        r.warpK = r.warpMode == params::warpBend ? warp * 6.0f
                : r.warpMode == params::warpCrush ? std::exp2 (10.0f - warp * 8.0f) : 0.0f;

        const float pitch = notePitch + p.oct.get() * 12.0f + semi + fine * 0.01f;
        const float freq = midiToHz (pitch);
        const float spread = 0.7f * std::pow (det, 1.5f);

        float sumSq = 0.0f, maxFreq = freq;
        for (int u = 0; u < r.uni; ++u)
        {
            const float off = r.uni > 1 ? (2.0f * (float) u / (float) (r.uni - 1) - 1.0f) : 0.0f;
            const float f = freq * std::exp2 (off * spread / 12.0f);
            maxFreq = jmax (maxFreq, f);
            r.inc[u] = f / sr;

            const bool centre = r.uni == 1 || std::abs (off) <= 1.0f / (float) (r.uni - 1) + 1.0e-3f;
            const float g = centre ? 1.0f : blend;
            sumSq += g * g;

            const float pp = jlimit (-1.0f, 1.0f, pan + off * width);
            const float ang = (pp + 1.0f) * MathConstants<float>::pi * 0.25f;
            r.gL[u] = g * std::cos (ang);
            r.gR[u] = g * std::sin (ang);
        }
        const float norm = sumSq > 0.0f ? 1.0f / std::sqrt (sumSq) : 0.0f;
        for (int u = 0; u < r.uni; ++u) { r.gL[u] *= norm; r.gR[u] *= norm; }

        float mipFreq = maxFreq;
        if (r.warpMode == params::warpSync)   mipFreq *= 1.0f + warp * 7.0f;
        if (r.warpMode == params::warpMirror) mipFreq *= 1.0f + warp;
        r.mip = Wavetable::mipForFrequency (mipFreq, sr);

        if (fresh) { osc[o].lastPos = pos; osc[o].lastLevel = level; }
        r.pos = osc[o].lastPos;
        r.posStep = (pos - r.pos) / (float) numSamples;
        r.lvl = osc[o].lastLevel;
        r.lvlStep = (level - r.lvl) / (float) numSamples;
        osc[o].lastPos = pos;
        osc[o].lastLevel = level;
    }

    // ---- sub / noise ---------------------------------------------------------
    const bool subOn = e.subOn.get() > 0.5f;
    const float subLvl = subOn ? actual (dSubLevel) : 0.0f;
    const int subWave = (int) e.subWave.get();
    const float subInc = midiToHz (notePitch + 12.0f * e.subOct.get()) / sr;

    const bool noiseOn = e.noiseOn.get() > 0.5f;
    const float noiseLvl = noiseOn ? actual (dNoiseLevel) : 0.0f;
    const float noiseCol = noiseOn ? actual (dNoiseColor) : 0.0f;

    // ---- filter setup --------------------------------------------------------
    const bool fltOn = e.fltOn.get() > 0.5f;
    const int fType = (int) e.fltType.get();
    float a1 = 0, a2 = 0, a3 = 0, k = 2;
    float driveGain = 1.0f, driveNorm = 1.0f;
    bool drive = false;
    if (fltOn)
    {
        float cutoff = actual (dCutoff);
        const float res = jlimit (0.0f, 1.0f, actual (dRes));
        const float envAmt = actual (dFltEnv);
        const float drv = actual (dDrive);
        cutoff *= std::exp2 (env[1].level * envAmt * 7.0f);
        cutoff *= std::exp2 ((currentPitch - 60.0f) / 12.0f * e.fltKey.get());
        cutoff = jlimit (20.0f, jmin (20000.0f, sr * 0.45f), cutoff);

        const float g = std::tan (MathConstants<float>::pi * cutoff / sr);
        k = 2.0f - 1.97f * res;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
        if (drv > 0.001f) { drive = true; driveGain = 1.0f + drv * 6.0f; driveNorm = 1.0f / (1.0f + 0.15f * (driveGain - 1.0f)); }
    }
    const bool twoStage = fType == params::fLP24 || fType == params::fHP24;

    const float velGain = (0.3f + 0.7f * velocity) * kVoiceGain;

    // ---- per-sample loop -----------------------------------------------------
    for (int i = 0; i < numSamples; ++i)
    {
        float L = 0.0f, R = 0.0f;

        for (int o = 0; o < 2; ++o)
        {
            auto& r = run[o];
            if (! r.on) continue;
            float oL = 0.0f, oR = 0.0f;
            auto* ph = osc[o].phase;
            for (int u = 0; u < r.uni; ++u)
            {
                ph[u] += r.inc[u];
                if (ph[u] >= 1.0f) ph[u] -= 1.0f;
                const float p = r.warpMode == params::warpOff || r.warpMode == params::warpFold
                                  ? ph[u] : warpPhase (r.warpMode, r.warpAmt, r.warpK, ph[u]);
                float s = r.wt->lookup (r.mip, r.pos, p);
                if (r.warpMode == params::warpFold) s = foldSample (s, r.warpAmt);
                oL += s * r.gL[u];
                oR += s * r.gR[u];
            }
            L += oL * r.lvl;
            R += oR * r.lvl;
            r.pos += r.posStep;
            r.lvl += r.lvlStep;
        }

        if (subOn)
        {
            subPhase += subInc;
            if (subPhase >= 1.0f) subPhase -= 1.0f;
            float s;
            if (subWave == 0)      s = std::sin (MathConstants<float>::twoPi * subPhase);
            else if (subWave == 1) s = subPhase < 0.5f ? 4.0f * subPhase - 1.0f : 3.0f - 4.0f * subPhase;
            else
            {
                s = subPhase < 0.5f ? 1.0f : -1.0f;
                s += polyBlep (subPhase, subInc);
                s -= polyBlep (wrap01 (subPhase + 0.5f), subInc);
                s *= 0.7f;
            }
            L += s * subLvl * 0.7071f;
            R += s * subLvl * 0.7071f;
        }

        if (noiseOn)
        {
            const float w = rng.nextBipolar();
            noiseLP += 0.08f * (w - noiseLP);
            const float n = noiseCol < 0.0f ? w + (-noiseCol) * (noiseLP * 2.5f - w)
                                            : w + noiseCol * ((w - noiseLP) - w);
            L += n * noiseLvl * 0.5f;
            R += n * noiseLvl * 0.5f;
        }

        if (fltOn)
        {
            float io[2] = { L, R };
            for (int c = 0; c < 2; ++c)
            {
                float x = io[c];
                if (drive) x = std::tanh (x * driveGain) * driveNorm;
                const int stages = twoStage ? 2 : 1;
                for (int st = 0; st < stages; ++st)
                {
                    const float v3 = x - ic2[c][st];
                    const float v1 = a1 * ic1[c][st] + a2 * v3;
                    const float v2 = ic2[c][st] + a2 * ic1[c][st] + a3 * v3;
                    ic1[c][st] = 2.0f * v1 - ic1[c][st];
                    ic2[c][st] = 2.0f * v2 - ic2[c][st];
                    switch (fType)
                    {
                        case params::fLP12: case params::fLP24: x = v2; break;
                        case params::fHP12: case params::fHP24: x = x - k * v1 - v2; break;
                        case params::fBP:   x = v1 * k; break;
                        case params::fNotch: default: x = x - k * v1; break;
                    }
                }
                io[c] = x;
            }
            L = io[0];
            R = io[1];
        }

        const float amp = env[0].next() * velGain;
        env[1].next();
        env[2].next();
        outL[i] += L * amp;
        outR[i] += R * amp;
    }

    fresh = false;
    if (env[0].stage == Envelope::idle)
        kill();

    juce::ignoreUnused (e.currentBpm);
}
}
