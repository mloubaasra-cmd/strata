#include "Wavetable.h"
#include "Parameters.h"

namespace strata
{
using namespace juce;

namespace
{
    constexpr float twoPi = MathConstants<float>::twoPi;
    constexpr int kBankFrames = 32;

    float frac (float x) { return x - std::floor (x); }

    // Additive synthesis of one 2048-sample frame from harmonic amplitudes (index 0 = fundamental)
    void additiveFrame (const std::vector<float>& amps, float* out, dsp::FFT& inverse2048)
    {
        std::vector<float> spec ((size_t) Wavetable::kFrameSize * 2, 0.0f);
        const int maxK = jmin ((int) amps.size(), Wavetable::kMaxHarmonics - 1);
        for (int k = 1; k <= maxK; ++k)
            spec[(size_t) (2 * k + 1)] = -amps[(size_t) (k - 1)];   // pure sine phase
        inverse2048.performRealOnlyInverseTransform (spec.data());
        std::copy (spec.begin(), spec.begin() + Wavetable::kFrameSize, out);
    }

    using TimeFn = std::function<float (float t, float p)>;
    using SpecFn = std::function<std::vector<float> (float t)>;

    std::shared_ptr<Wavetable> makeTimeBank (const String& name, TimeFn fn)
    {
        std::vector<float> frames ((size_t) (kBankFrames * Wavetable::kFrameSize));
        for (int f = 0; f < kBankFrames; ++f)
        {
            const float t = (float) f / (float) (kBankFrames - 1);
            for (int i = 0; i < Wavetable::kFrameSize; ++i)
                frames[(size_t) (f * Wavetable::kFrameSize + i)] = fn (t, (float) i / (float) Wavetable::kFrameSize);
        }
        return Wavetable::fromFrames (std::move (frames), kBankFrames, name);
    }

    std::shared_ptr<Wavetable> makeSpectralBank (const String& name, SpecFn fn)
    {
        dsp::FFT inv (11);
        std::vector<float> frames ((size_t) (kBankFrames * Wavetable::kFrameSize));
        for (int f = 0; f < kBankFrames; ++f)
        {
            const float t = (float) f / (float) (kBankFrames - 1);
            additiveFrame (fn (t), frames.data() + f * Wavetable::kFrameSize, inv);
        }
        return Wavetable::fromFrames (std::move (frames), kBankFrames, name);
    }

    // ---- individual factory banks -------------------------------------------------------
    float shapeSine (float p)   { return std::sin (twoPi * p); }
    float shapeTri (float p)    { return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f); }
    float shapeSaw (float p)    { return p < 0.5f ? 2.0f * p : 2.0f * p - 2.0f; }
    float shapeSquare (float p) { return p < 0.5f ? 1.0f : -1.0f; }

    std::shared_ptr<Wavetable> buildBank (int index)
    {
        const auto name = params::bankChoices()[index];

        switch (index)
        {
            case 0: return makeTimeBank (name, [] (float t, float p)
            {
                const float s = t * 3.0f;
                const int seg = jmin (2, (int) s);
                const float x = s - (float) seg;
                float a = 0, b = 0;
                if (seg == 0)      { a = shapeSine (p);  b = shapeTri (p); }
                else if (seg == 1) { a = shapeTri (p);   b = shapeSaw (p); }
                else               { a = shapeSaw (p);   b = shapeSquare (p); }
                return a + x * (b - a);
            });

            case 1: return makeTimeBank (name, [] (float t, float p)
            {
                const float w = 0.5f - 0.47f * t;
                return p < w ? 1.0f : -1.0f;
            });

            case 2: return makeSpectralBank (name, [] (float t)
            {
                const float n = 1.0f + t * t * 127.0f;
                std::vector<float> a (128, 0.0f);
                for (int k = 1; k <= 128; ++k)
                {
                    const float fade = jlimit (0.0f, 1.0f, n - (float) (k - 1));
                    a[(size_t) (k - 1)] = fade / (float) k;
                }
                return a;
            });

            case 3: return makeSpectralBank (name, [] (float t)
            {
                // A - E - I - O - U formant morph on a 110 Hz voice
                static const float F[5][3] = { { 800, 1150, 2900 }, { 400, 1600, 2700 }, { 270, 2100, 3000 },
                                               { 450, 800, 2830 }, { 325, 700, 2530 } };
                const float s = t * 4.0f;
                const int v = jmin (3, (int) s);
                const float x = s - (float) v;
                float f[3];
                for (int i = 0; i < 3; ++i)
                    f[i] = F[v][i] + x * (F[v + 1][i] - F[v][i]);

                std::vector<float> a (180, 0.0f);
                for (int k = 1; k <= 180; ++k)
                {
                    const float hz = 110.0f * (float) k;
                    float amp = 0.015f / std::sqrt ((float) k);
                    const float gains[3] = { 1.0f, 0.6f, 0.35f };
                    const float bws[3] = { 90.0f, 120.0f, 160.0f };
                    for (int i = 0; i < 3; ++i)
                    {
                        const float d = (hz - f[i]) / bws[i];
                        amp += gains[i] * std::exp (-d * d);
                    }
                    a[(size_t) (k - 1)] = amp;
                }
                return a;
            });

            case 4: return makeTimeBank (name, [] (float t, float p)
            {
                const float i1 = 4.0f * t, i2 = 3.0f * t * t;
                return std::sin (twoPi * p + i1 * std::sin (twoPi * 2.0f * p) + i2 * std::sin (twoPi * 5.0f * p));
            });

            case 5: return makeTimeBank (name, [] (float t, float p)
            {
                const float ratio = 1.0f + t * 7.0f;
                return 2.0f * frac (p * ratio) - 1.0f;
            });

            case 6: return makeTimeBank (name, [] (float t, float p)
            {
                const float x = std::sin (twoPi * p)
                              + 0.6f * std::sin (twoPi * 2.0f * p + t * 3.0f)
                              + 0.4f * std::sin (twoPi * 3.0f * p + t * 5.0f)
                              + 0.25f * std::sin (twoPi * 5.0f * p + t * 2.0f);
                return std::tanh ((1.0f + 5.0f * t) * x);
            });

            case 7: return makeTimeBank (name, [] (float t, float p)
            {
                const float phaseSteps = 256.0f * (1.0f - t) + 4.0f;
                const float pq = std::floor (p * phaseSteps) / phaseSteps;
                const float ampSteps = std::round (64.0f * (1.0f - t) * (1.0f - t)) + 2.0f;
                return std::floor (std::sin (twoPi * pq) * ampSteps) / ampSteps;
            });

            case 8: return makeSpectralBank (name, [] (float t)
            {
                static const int harm[10] = { 1, 2, 3, 4, 5, 6, 8, 10, 12, 16 };
                static const float reg[4][10] = { { 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
                                                  { 1, 0.8f, 0.6f, 0.5f, 0, 0.3f, 0.4f, 0, 0, 0.2f },
                                                  { 1, 0.2f, 0.9f, 0, 0.6f, 0, 0.5f, 0.4f, 0.3f, 0 },
                                                  { 0.8f, 0.8f, 0.8f, 0.8f, 0.6f, 0.6f, 0.6f, 0.5f, 0.5f, 0.5f } };
                const float s = t * 3.0f;
                const int r = jmin (2, (int) s);
                const float x = s - (float) r;
                std::vector<float> a (16, 0.0f);
                for (int i = 0; i < 10; ++i)
                    a[(size_t) (harm[i] - 1)] = reg[r][i] + x * (reg[r + 1][i] - reg[r][i]);
                return a;
            });

            case 9: return makeSpectralBank (name, [] (float t)
            {
                static const std::vector<std::vector<float>> keys = []
                {
                    FastRandom rng (1234);
                    std::vector<std::vector<float>> k (5, std::vector<float> (96));
                    for (auto& spec : k)
                        for (int h = 0; h < 96; ++h)
                            spec[(size_t) h] = rng.nextFloat() * rng.nextFloat() / std::pow ((float) (h + 1), 0.7f);
                    for (auto& spec : k) spec[0] = 1.0f;
                    return k;
                }();
                const float s = t * 4.0f;
                const int r = jmin (3, (int) s);
                const float x = s - (float) r;
                std::vector<float> a (96);
                for (size_t h = 0; h < 96; ++h)
                    a[h] = keys[(size_t) r][h] + x * (keys[(size_t) r + 1][h] - keys[(size_t) r][h]);
                return a;
            });

            case 10: return makeTimeBank (name, [] (float t, float p)
            {
                return std::sin ((2.0f * p - 1.0f) * MathConstants<float>::pi * (0.5f + 3.5f * t));
            });

            case 11: default: return makeSpectralBank (name, [] (float t)
            {
                std::vector<float> a (200);
                for (int k = 1; k <= 200; ++k)
                    a[(size_t) (k - 1)] = std::exp (-(float) (k - 1) * (0.02f + t * 0.6f)) * ((k & 1) ? 1.0f : 0.6f);
                return a;
            });
        }
    }
}

//==============================================================================
std::shared_ptr<Wavetable> Wavetable::fromFrames (std::vector<float> frames, int numFrames, const String& name)
{
    auto wt = std::make_shared<Wavetable>();
    wt->name = name;
    wt->numFrames = jmax (1, numFrames);

    // Remove DC and peak-normalise each frame
    for (int f = 0; f < wt->numFrames; ++f)
    {
        float* fr = frames.data() + f * kFrameSize;
        float mean = 0.0f;
        for (int i = 0; i < kFrameSize; ++i) mean += fr[i];
        mean /= (float) kFrameSize;
        float peak = 0.0f;
        for (int i = 0; i < kFrameSize; ++i) { fr[i] -= mean; peak = jmax (peak, std::abs (fr[i])); }
        if (peak > 1.0e-5f)
            for (int i = 0; i < kFrameSize; ++i) fr[i] /= peak;
    }

    dsp::FFT forward (11);
    std::array<std::unique_ptr<dsp::FFT>, 12> inverses;

    for (int m = 0; m < kNumMips; ++m)
    {
        const int maxHarm = kMaxHarmonics >> m;
        auto& mip = wt->mips[(size_t) m];
        mip.size = jmin (kFrameSize, jmax (64, 4 * maxHarm));
        mip.mask = mip.size - 1;
        mip.data.assign ((size_t) wt->numFrames * (size_t) (mip.size + 1), 0.0f);
        const int order = (int) std::round (std::log2 ((double) mip.size));
        if (inverses[(size_t) order] == nullptr)
            inverses[(size_t) order] = std::make_unique<dsp::FFT> (order);
    }

    std::vector<float> spec ((size_t) kFrameSize * 2);
    std::vector<float> buf ((size_t) kFrameSize * 2);

    for (int f = 0; f < wt->numFrames; ++f)
    {
        std::fill (spec.begin(), spec.end(), 0.0f);
        std::copy (frames.begin() + f * kFrameSize, frames.begin() + (f + 1) * kFrameSize, spec.begin());
        forward.performRealOnlyForwardTransform (spec.data(), true);

        for (int m = 0; m < kNumMips; ++m)
        {
            auto& mip = wt->mips[(size_t) m];
            const int size = mip.size;
            const int limit = jmin (kMaxHarmonics >> m, size / 2 - 1);
            const float scale = (float) size / (float) kFrameSize;

            std::fill (buf.begin(), buf.begin() + 2 * size, 0.0f);
            for (int k = 1; k <= limit; ++k)
            {
                buf[(size_t) (2 * k)]     = spec[(size_t) (2 * k)] * scale;
                buf[(size_t) (2 * k + 1)] = spec[(size_t) (2 * k + 1)] * scale;
            }
            const int order = (int) std::round (std::log2 ((double) size));
            inverses[(size_t) order]->performRealOnlyInverseTransform (buf.data());

            float* dst = mip.data.data() + (size_t) f * (size_t) (size + 1);
            std::copy (buf.begin(), buf.begin() + size, dst);
            dst[size] = dst[0];
        }
    }

    wt->rawFrames = std::move (frames);
    return wt;
}

String Wavetable::toBase64() const
{
    MemoryBlock mb (rawFrames.data(), rawFrames.size() * sizeof (float));
    return mb.toBase64Encoding();
}

std::shared_ptr<Wavetable> Wavetable::fromBase64 (const String& data, int numFrames, const String& name)
{
    MemoryBlock mb;
    if (! mb.fromBase64Encoding (data) || numFrames <= 0)
        return {};
    const size_t needed = (size_t) numFrames * kFrameSize * sizeof (float);
    if (mb.getSize() < needed)
        return {};
    std::vector<float> frames ((size_t) numFrames * kFrameSize);
    std::memcpy (frames.data(), mb.getData(), needed);
    return fromFrames (std::move (frames), numFrames, name);
}

//==============================================================================
WavetableLibrary::WavetableLibrary()
{
    const int numBuiltIn = params::bankChoices().size() - 1;   // last entry is "User"
    for (int i = 0; i < numBuiltIn; ++i)
        banks.push_back (buildBank (i));
}

const Wavetable* WavetableLibrary::getBank (int index) const
{
    return banks[(size_t) jlimit (0, (int) banks.size() - 1, index)].get();
}

std::shared_ptr<Wavetable> WavetableLibrary::getBankShared (int index) const
{
    return banks[(size_t) jlimit (0, (int) banks.size() - 1, index)];
}

std::shared_ptr<Wavetable> WavetableLibrary::loadFromFile (const File& file, String& error)
{
    AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Couldn't read that file. Use a WAV, AIFF or FLAC.";
        return {};
    }

    const int len = (int) std::min<int64> (reader->lengthInSamples, (int64) (1 << 21));
    if (len < 16)
    {
        error = "The file is too short to be a wavetable.";
        return {};
    }

    AudioBuffer<float> audio ((int) reader->numChannels, len);
    reader->read (&audio, 0, len, 0, true, true);
    std::vector<float> mono ((size_t) len, 0.0f);
    for (int c = 0; c < audio.getNumChannels(); ++c)
        for (int i = 0; i < len; ++i)
            mono[(size_t) i] += audio.getSample (c, i) / (float) audio.getNumChannels();

    // Work out where each frame starts and how long it is
    int cycle = 0, count = 0;
    std::vector<int> starts;

    if (len <= 4096)
    {
        cycle = len; count = 1; starts = { 0 };
    }
    else
    {
        for (int cs : { 2048, 1024, 512, 256, 4096 })
            if (len % cs == 0 && len / cs <= 1024) { cycle = cs; count = len / cs; break; }

        if (cycle > 0)
        {
            for (int i = 0; i < count; ++i) starts.push_back (i * cycle);
        }
        else
        {
            // Arbitrary audio: slice into evenly spaced 2048-sample grains
            cycle = 2048;
            count = jlimit (1, 64, len / 2048);
            for (int i = 0; i < count; ++i)
                starts.push_back (count == 1 ? 0 : (int) ((int64) i * (len - cycle) / (count - 1)));
        }
    }

    // Keep at most kMaxFrames, evenly picked
    std::vector<int> picked;
    if ((int) starts.size() > Wavetable::kMaxFrames)
    {
        for (int i = 0; i < Wavetable::kMaxFrames; ++i)
            picked.push_back (starts[(size_t) ((int64) i * ((int) starts.size() - 1) / (Wavetable::kMaxFrames - 1))]);
    }
    else
        picked = starts;

    const int nf = (int) picked.size();
    std::vector<float> frames ((size_t) nf * Wavetable::kFrameSize);
    for (int f = 0; f < nf; ++f)
    {
        const int s = picked[(size_t) f];
        for (int i = 0; i < Wavetable::kFrameSize; ++i)
        {
            const float pos = (float) i * (float) cycle / (float) Wavetable::kFrameSize;
            const int i0 = (int) pos;
            const float fr = pos - (float) i0;
            const float a = mono[(size_t) jmin (len - 1, s + (i0 % cycle))];
            const float b = mono[(size_t) jmin (len - 1, s + ((i0 + 1) % cycle))];
            frames[(size_t) (f * Wavetable::kFrameSize + i)] = a + fr * (b - a);
        }
    }

    return Wavetable::fromFrames (std::move (frames), nf, file.getFileNameWithoutExtension());
}
}
