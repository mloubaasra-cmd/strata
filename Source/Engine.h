#pragma once
#include "Common.h"
#include "Parameters.h"
#include "Wavetable.h"

namespace strata
{
struct ModSlot
{
    std::atomic<int> src { -1 };
    std::atomic<int> dst { -1 };
    std::atomic<float> amount { 0.0f };
};

/** ADSR with linear attack and exponential decay / release. */
struct Envelope
{
    enum Stage { idle, attack, decay, sustain, release };
    Stage stage = idle;
    float level = 0.0f;
    float atkInc = 0.01f, decCoef = 0.999f, sus = 1.0f, relCoef = 0.999f;

    void set (float a, float d, float s, float r, float sr) noexcept
    {
        atkInc  = 1.0f / (juce::jmax (0.0005f, a) * sr);
        decCoef = std::exp (-4.6f / (juce::jmax (0.0005f, d) * sr));
        sus     = s;
        relCoef = std::exp (-4.6f / (juce::jmax (0.0005f, r) * sr));
    }
    void noteOn() noexcept   { stage = attack; }
    void noteOff() noexcept  { if (stage != idle) stage = release; }
    void kill() noexcept     { stage = idle; level = 0.0f; }

    inline float next() noexcept
    {
        switch (stage)
        {
            case attack:
                level += atkInc;
                if (level >= 1.0f) { level = 1.0f; stage = decay; }
                break;
            case decay:
                level = sus + (level - sus) * decCoef;
                if (std::abs (level - sus) < 1.0e-4f)
                {
                    level = sus;
                    stage = sus < 1.0e-4f ? idle : sustain;
                }
                break;
            case sustain: level = sus; break;
            case release:
                level *= relCoef;
                if (level < 1.0e-4f) { level = 0.0f; stage = idle; }
                break;
            case idle: default: break;
        }
        return level;
    }
};

class Engine;

struct Voice
{
    static constexpr int kMaxUnison = 8;

    bool active = false, keyDown = false, sustained = false, fresh = true;
    int note = -1;
    float velocity = 0.0f;
    uint64_t age = 0;
    float currentPitch = 60.0f, targetPitch = 60.0f;

    Envelope env[params::kNumEnvs];
    float lfoPhase[params::kNumLfos] {};
    float lfoValue[params::kNumLfos] {};
    float lfoRandA[params::kNumLfos] {}, lfoRandB[params::kNumLfos] {};
    float randomValue = 0.0f;

    struct OscState
    {
        float phase[kMaxUnison] {};
        float lastPos = 0.0f, lastLevel = 0.0f;
    } osc[2];

    float subPhase = 0.0f, noiseLP = 0.0f;
    float ic1[2][2] {}, ic2[2][2] {};   // [channel][stage]
    FastRandom rng;

    std::array<float, 32> modFinal {};   // normalised destination values after modulation

    void start (Engine& e, int note, float vel, float glideFrom, bool retriggerFromCurrent);
    void release();
    void kill();
    void render (Engine& e, float* outL, float* outR, int numSamples);
};

/**
    The synth engine: voice management, modulation and per-voice DSP.
    Lives inside the processor; the UI reads the display atomics.
*/
class Engine
{
public:
    static constexpr int kMaxVoices = 16;
    static constexpr int kChunk = 32;
    static constexpr int kCurveSize = 256;
    static constexpr int kMaxDests = 32;

    explicit Engine (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate);
    void reset();
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, double bpm);

    // ---- shared state written by the message thread ---------------------------------
    std::array<ModSlot, mod::kMaxSlots> slots;
    std::atomic<int> numSlots { 0 };
    std::array<std::array<std::atomic<float>, kCurveSize>, params::kNumLfos> lfoCurves;
    std::array<std::atomic<const Wavetable*>, 2> tables {};

    // ---- display values read by the UI ----------------------------------------------
    std::array<std::atomic<float>, kMaxDests> displayValue {};
    std::atomic<int> activeVoices { 0 };
    std::array<std::atomic<float>, params::kNumLfos> displayLfoPhase {};
    std::array<std::atomic<float>, params::kNumEnvs> displayEnvLevel {};

private:
    friend struct Voice;

    void handleMidi (const juce::MidiMessage& m);
    void noteOn (int note, float velocity);
    void noteOff (int note);
    void allNotesOff (bool hard);
    void updateChunkGlobals (int numSamples, double bpm);
    Voice* findVoiceToUse();
    int voiceMode() const;
    float lfoSample (int lfo, float phase, float randA, float randB) const;

    juce::AudioProcessorValueTreeState& apvts;
    double sampleRate = 44100.0;
    std::array<Voice, kMaxVoices> voices;
    uint64_t ageCounter = 0;

    std::vector<int> heldNotes;
    bool sustainPedal = false;
    float modWheel = 0.0f, pitchBend = 0.0f;
    float lastPitch = -1.0f;

    // Cached parameter pointers
    struct Raw { std::atomic<float>* p = nullptr; float get() const { return p->load (std::memory_order_relaxed); } };
    Raw get (const juce::String& id) { return { apvts.getRawParameterValue (id) }; }

    struct OscParams { Raw on, bank, pos, oct, semi, fine, uni, detune, blend, width, warpMode, warp, level, pan; } oscP[2];
    Raw subOn, subWave, subOct, subLevel, noiseOn, noiseLevel, noiseColor;
    Raw fltOn, fltType, fltCutoff, fltRes, fltDrive, fltEnv, fltKey;
    Raw envA[params::kNumEnvs], envD[params::kNumEnvs], envS[params::kNumEnvs], envR[params::kNumEnvs];
    Raw lfoRate[params::kNumLfos], lfoSync[params::kNumLfos], lfoDiv[params::kNumLfos], lfoShape[params::kNumLfos], lfoRetrig[params::kNumLfos];
    Raw macros[params::kNumMacros];
    Raw mode, glide, bendRange;

    std::array<juce::RangedAudioParameter*, kMaxDests> destParams {};
    int numDests = 0;

    // Per-chunk globals
    std::array<float, kMaxDests> baseNorm {}, baseActual {};
    std::array<bool, kMaxDests> destModulated {};
    struct ActiveSlot { int src, dst; float amt; };
    std::array<ActiveSlot, mod::kMaxSlots> activeSlots {};
    int numActiveSlots = 0;
    float globalLfoPhase[params::kNumLfos] {};
    float globalRandA[params::kNumLfos] {}, globalRandB[params::kNumLfos] {};
    float lfoHz[params::kNumLfos] {};
    double currentBpm = 120.0;
    FastRandom rng { 987654321u };
};
}
