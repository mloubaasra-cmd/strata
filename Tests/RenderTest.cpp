// Offline test: checks wavetable mip levels, renders every factory preset and reports
// silence / NaN / clipping, then snapshots the editor pages to PNG.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <chrono>

using namespace juce;
using namespace strata;

static void checkWavetables()
{
    SharedResourcePointer<WavetableLibrary> lib;
    for (int b = 0; b < lib->getNumBanks(); ++b)
    {
        const auto* wt = lib->getBank (b);
        float worst = 0.0f;
        for (int m = 0; m < Wavetable::kNumMips; ++m)
        {
            float peak = 0.0f;
            for (int i = 0; i < 512; ++i)
                peak = jmax (peak, std::abs (wt->lookup (m, 0.0f, (float) i / 512.0f)));
            if (m == 0) worst = peak;
            if (b == 0 && (m == 0 || m == 10))
                std::printf ("  bank %-15s mip %2d peak %.3f\n", wt->name.toRawUTF8(), m, peak);
        }
        std::printf ("  bank %-15s frames %d  mip0 peak %.3f\n", wt->name.toRawUTF8(), wt->numFrames, worst);
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    std::printf ("Wavetables:\n");
    checkWavetables();

    StrataProcessor proc;
    const double sr = 48000.0;
    const int block = 512;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    const auto& presets = getFactoryPresets();
    std::printf ("\nRendering %d factory presets...\n", (int) presets.size());
    int problems = 0;
    double totalRenderSec = 0.0, totalAudioSec = 0.0;

    for (int p = 0; p < (int) presets.size(); ++p)
    {
        proc.getPresetManager().loadFactory (p);
        proc.prepareToPlay (sr, block);   // resets voices / fx between presets

        const auto cat = presets[(size_t) p].category;
        std::vector<int> notes;
        if (cat == "Bass" || cat == "Growl" || cat == "Sub & 808") notes = { 36 };
        else if (cat == "Lead" || cat == "FX") notes = { 72 };
        else notes = { 60, 64, 67, 71 };

        AudioBuffer<float> buf (2, block);
        const int holdBlocks = (int) (2.0 * sr / block), releaseBlocks = (int) (1.5 * sr / block);
        float peak = 0.0f; double sumSq = 0.0; long n = 0; bool nan = false;

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int b = 0; b < holdBlocks + releaseBlocks; ++b)
        {
            MidiBuffer midi;
            if (b == 0) for (int nn : notes) midi.addEvent (MidiMessage::noteOn (1, nn, (uint8) 100), 0);
            if (b == holdBlocks) for (int nn : notes) midi.addEvent (MidiMessage::noteOff (1, nn), 0);
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) nan = true;
                    peak = jmax (peak, std::abs (v));
                    sumSq += (double) v * v; ++n;
                }
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        totalRenderSec += std::chrono::duration<double> (t1 - t0).count();
        totalAudioSec += (holdBlocks + releaseBlocks) * block / sr;

        const double rms = std::sqrt (sumSq / (double) jmax (1L, n));
        const bool bad = nan || peak < 0.01f || rms < 0.002;
        if (bad) ++problems;
        std::printf ("%3d %-20s %-16s peak %6.3f  rms %6.4f  mods %d %s\n", p, presets[(size_t) p].name.toRawUTF8(), cat.toRawUTF8(),
                     peak, rms, (int) proc.getModulations().size(), nan ? "NaN!" : (bad ? "<-- CHECK" : ""));
    }
    std::printf ("\nProblems: %d   Render speed: %.1fx realtime (all presets, single thread)\n", problems, totalAudioSec / totalRenderSec);

    // Wavetable import round trip
    {
        File tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("strata_test_table.wav");
        tmp.deleteFile();
        {
            WavAudioFormat wav;
            auto stream = tmp.createOutputStream();
            std::unique_ptr<AudioFormatWriter> writer (wav.createWriterFor (stream.get(), 44100.0, 1, 16, {}, 0));
            if (writer != nullptr)
            {
                stream.release();   // writer owns it now
                AudioBuffer<float> t (1, 2048 * 16);
                for (int f = 0; f < 16; ++f)
                    for (int i = 0; i < 2048; ++i)
                        t.setSample (0, f * 2048 + i, std::sin (MathConstants<float>::twoPi * (float) i / 2048.0f * (float) (1 + f)) * 0.8f);
                writer->writeFromAudioSampleBuffer (t, 0, t.getNumSamples());
            }
        }
        String err;
        const bool ok = proc.loadUserWavetable (0, tmp, err);
        std::printf ("User wavetable import: %s (%s) frames=%d\n", ok ? "OK" : "FAILED", err.toRawUTF8(),
                     proc.getOscTable (0) ? proc.getOscTable (0)->numFrames : -1);
        MemoryBlock state;
        proc.getStateInformation (state);
        proc.getPresetManager().loadFactory (0);
        proc.setStateInformation (state.getData(), (int) state.getSize());
        std::printf ("State restore keeps user table: %s\n", proc.getOscTable (0) && proc.getOscTable (0)->numFrames == 16 ? "OK" : "FAILED");
        tmp.deleteFile();
    }

    // Editor snapshots
    if (argc > 1)
    {
        const File outDir (argv[1]);
        outDir.createDirectory();
        proc.getPresetManager().loadFactory (40);
        proc.addModulation (mod::srcLfo1, mod::destIndexForParam ("flt_cutoff"), 0.3f);
        std::unique_ptr<AudioProcessorEditor> ed (proc.createEditor());
        auto* content = dynamic_cast<MainContent*> (ed->getChildComponent (0));
        ed->setSize (1100, 712);
        for (int tab = 0; tab < 3; ++tab)
        {
            content->setTab (tab);
            content->tick();
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            FileOutputStream fos (outDir.getChildFile ("page" + String (tab) + ".png"));
            fos.setPosition (0); fos.truncate();
            PNGImageFormat().writeImageToStream (img, fos);
        }
        // theme gallery
        {
            const auto names = StrataLook::themeNames();
            const int cols = 3, tw = 550, th = 356;
            const int rows = (names.size() + cols - 1) / cols;
            Image gallery (Image::RGB, cols * tw, rows * (th + 26), true);
            Graphics gg (gallery);
            gg.fillAll (Colour (0xff202020));
            content->setTab (0);
            for (int t = 0; t < names.size(); ++t)
            {
                content->setTheme (t);
                content->tick();
                auto shot = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
                FileOutputStream f (outDir.getChildFile ("theme" + String (t) + ".png"));
                f.setPosition (0); f.truncate();
                PNGImageFormat().writeImageToStream (shot, f);
                const int x = (t % cols) * tw, y = (t / cols) * (th + 26);
                gg.drawImage (shot, x, y + 26, tw, th, 0, 0, shot.getWidth(), shot.getHeight());
                gg.setColour (Colours::white);
                gg.setFont (16.0f);
                gg.drawText (String (t + 1) + ". " + names[t], x + 8, y, tw - 16, 26, Justification::centredLeft);
            }
            FileOutputStream f (outDir.getChildFile ("themes.png"));
            f.setPosition (0); f.truncate();
            PNGImageFormat().writeImageToStream (gallery, f);
            content->setTheme (0);
        }

        // knob styles sheet
        {
            StrataLook look;
            Image sheet (Image::ARGB, 7 * 90, 2 * 90, true);
            Graphics g (sheet);
            g.fillAll (colours::panel);
            for (int st = 0; st < StrataLook::kNumKnobStyles; ++st)
                for (int row = 0; row < 2; ++row)
                    look.drawKnob (g, Rectangle<float> ((float) st * 90.0f + 15.0f, (float) row * 90.0f + 15.0f, 60.0f, 60.0f),
                                   row == 0 ? 0.7f : 0.25f, MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f,
                                   st, row == 1, true, false);
            FileOutputStream fos (outDir.getChildFile ("knobs.png"));
            fos.setPosition (0); fos.truncate();
            PNGImageFormat().writeImageToStream (sheet, fos);
        }

        content->startTutorial();
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        {
            FileOutputStream fos (outDir.getChildFile ("tutorial.png"));
            fos.setPosition (0); fos.truncate();
            PNGImageFormat().writeImageToStream (img, fos);
        }
        content->getTutorial().goToStep (7);
        auto img2 = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        {
            FileOutputStream fos (outDir.getChildFile ("tutorial8.png"));
            fos.setPosition (0); fos.truncate();
            PNGImageFormat().writeImageToStream (img2, fos);
        }
        std::printf ("Snapshots written to %s\n", outDir.getFullPathName().toRawUTF8());
    }
    return problems == 0 ? 0 : 1;
}
