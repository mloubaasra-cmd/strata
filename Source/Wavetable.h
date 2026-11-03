#pragma once
#include "Common.h"

namespace strata
{
/**
    A morphable wavetable: N single-cycle frames of 2048 samples, stored as a set of
    band-limited mip levels so high notes don't alias.
*/
struct Wavetable
{
    static constexpr int kFrameSize = 2048;
    static constexpr int kMaxHarmonics = 1024;
    static constexpr int kNumMips = 11;
    static constexpr int kMaxFrames = 256;

    struct Mip
    {
        int size = 0;       // samples per frame (power of two)
        int mask = 0;
        std::vector<float> data;   // numFrames * (size + 1) (last sample = wrap guard)
    };

    juce::String name;
    int numFrames = 0;
    std::array<Mip, kNumMips> mips;
    std::vector<float> rawFrames;   // numFrames * kFrameSize, normalised, for display & saving

    /** Builds a table from time-domain frames (numFrames * 2048 samples). */
    static std::shared_ptr<Wavetable> fromFrames (std::vector<float> frames, int numFrames, const juce::String& name);

    static int mipForFrequency (float freqHz, float sampleRate) noexcept
    {
        const float maxHarm = 0.45f * sampleRate / juce::jmax (1.0f, freqHz);
        if (maxHarm >= (float) kMaxHarmonics) return 0;
        const int level = (int) std::ceil (std::log2 ((float) kMaxHarmonics / juce::jmax (1.0f, maxHarm)));
        return juce::jlimit (0, kNumMips - 1, level);
    }

    /** framePos 0..1 morphs through the frames, phase 0..1 within the cycle. */
    inline float lookup (int mipIndex, float framePos, float phase) const noexcept
    {
        const Mip& m = mips[(size_t) mipIndex];
        const float fp = framePos * (float) (numFrames - 1);
        const int f0 = juce::jlimit (0, numFrames - 1, (int) fp);
        const int f1 = juce::jmin (f0 + 1, numFrames - 1);
        const float ff = fp - (float) f0;

        const float x = phase * (float) m.size;
        const int i = ((int) x) & m.mask;
        const float fr = x - std::floor (x);

        const float* a = m.data.data() + (size_t) f0 * (size_t) (m.size + 1);
        const float* b = m.data.data() + (size_t) f1 * (size_t) (m.size + 1);
        const float sa = a[i] + fr * (a[i + 1] - a[i]);
        const float sb = b[i] + fr * (b[i + 1] - b[i]);
        return sa + ff * (sb - sa);
    }

    /** Serialise the raw frames (for saving user tables in presets). */
    juce::String toBase64() const;
    static std::shared_ptr<Wavetable> fromBase64 (const juce::String& data, int numFrames, const juce::String& name);
};

/** All factory wavetable banks, built once and shared between plug-in instances. */
class WavetableLibrary
{
public:
    WavetableLibrary();
    int getNumBanks() const { return (int) banks.size(); }
    const Wavetable* getBank (int index) const;
    std::shared_ptr<Wavetable> getBankShared (int index) const;

    /** Loads a WAV/AIFF/FLAC as a wavetable. Handles single cycles, 2048/1024/512/256-sample
        frame tables (Serum-style) and arbitrary audio (sliced into frames). */
    static std::shared_ptr<Wavetable> loadFromFile (const juce::File& file, juce::String& error);

private:
    std::vector<std::shared_ptr<Wavetable>> banks;
};
}
