#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include <functional>
#include <cmath>

namespace strata
{
    // Tiny, fast, deterministic random generator (used by the engine and presets)
    struct FastRandom
    {
        uint32_t state = 0x12345678u;
        explicit FastRandom (uint32_t seed = 0x12345678u) : state (seed ? seed : 1u) {}
        uint32_t next() noexcept { state ^= state << 13; state ^= state >> 17; state ^= state << 5; return state; }
        float nextFloat() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }      // 0..1
        float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }                       // -1..1
        int nextInt (int maxExclusive) noexcept { return maxExclusive <= 0 ? 0 : (int) (next() % (uint32_t) maxExclusive); }
        float range (float lo, float hi) noexcept { return lo + (hi - lo) * nextFloat(); }
    };
}
