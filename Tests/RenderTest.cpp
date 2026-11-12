// Offline test: checks wavetable mip levels, renders every factory preset and reports
// silence / NaN / clipping, then snapshots the editor pages to PNG.

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include <chrono>
#include <set>

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

static constexpr double kTargetDb = -15.0;

static std::vector<int> notesFor (const String& cat, const String& name)
{
    auto has = [&name] (const char* w) { return name.containsIgnoreCase (w); };
    if (cat == "Bass" || cat == "Growl" || cat == "Sub & 808" || cat == "Dubstep & Riddim"
        || has ("808") || has ("Bass") || has ("Rumble") || has ("Acid") || has ("303") || ((has ("Wobble") || has ("Wub")) && ! has ("Chords")))
        return { 36 };
    if (cat == "Lead" || cat == "FX" || has ("Lead") || has ("Flute") || has ("Screech") || has ("Sequence") || has ("Blip"))
        return { 72 };
    return { 60, 64, 67, 71 };
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
    struct Loud { String name; double db; float peak; };
    std::vector<Loud> loudness;
    std::set<String> names;
    std::printf ("\nRendering %d factory presets...\n", (int) presets.size());
    int problems = 0;
    double totalRenderSec = 0.0, totalAudioSec = 0.0;

    for (int p = 0; p < (int) presets.size(); ++p)
    {
        proc.getPresetManager().loadFactory (p);
        proc.prepareToPlay (sr, block);   // resets voices / fx between presets

        const auto cat = presets[(size_t) p].category;
        const auto& nm = presets[(size_t) p].name;
        std::vector<int> notes = notesFor (cat, nm);

        AudioBuffer<float> buf (2, block);
        const int holdBlocks = (int) (2.0 * sr / block), releaseBlocks = (int) (1.5 * sr / block);
        float peak = 0.0f; double sumSq = 0.0; long n = 0; bool nan = false;
        const int win = (int) (0.1 * sr); double winSum = 0.0; int winN = 0; double maxWin = 0.0;

        const auto t0 = std::chrono::high_resolution_clock::now();
        for (int b = 0; b < holdBlocks + releaseBlocks; ++b)
        {
            MidiBuffer midi;
            if (b == 0) for (int nn : notes) midi.addEvent (MidiMessage::noteOn (1, nn, (uint8) 100), 0);
            if (b == holdBlocks) for (int nn : notes) midi.addEvent (MidiMessage::noteOff (1, nn), 0);
            proc.processBlock (buf, midi);
            for (int i = 0; i < block; ++i)
            {
                for (int c = 0; c < 2; ++c)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) nan = true;
                    peak = jmax (peak, std::abs (v));
                    sumSq += (double) v * v; ++n;
                    winSum += (double) v * v * 0.5;
                }
                if (++winN >= win) { maxWin = jmax (maxWin, winSum / winN); winSum = 0.0; winN = 0; }
            }
        }
        const auto t1 = std::chrono::high_resolution_clock::now();
        totalRenderSec += std::chrono::duration<double> (t1 - t0).count();
        totalAudioSec += (holdBlocks + releaseBlocks) * block / sr;

        const double rms = std::sqrt (sumSq / (double) jmax (1L, n));
        const double loudDb = 10.0 * std::log10 (jmax (1.0e-12, maxWin));
        const bool bad = nan || peak < 0.01f || rms < 0.002;
        if (bad) ++problems;
        if (names.count (nm)) { std::printf ("DUPLICATE NAME: %s\n", nm.toRawUTF8()); ++problems; }
        names.insert (nm);
        loudness.push_back ({ nm, loudDb, peak });
        std::printf ("%3d %-20s %-16s peak %6.3f  loud %6.1f dB  mods %d %s\n", p, nm.toRawUTF8(), cat.toRawUTF8(),
                     peak, loudDb, (int) proc.getModulations().size(), nan ? "NaN!" : (bad ? "<-- CHECK" : ""));
    }

    {
        std::vector<double> v;
        for (auto& l : loudness) v.push_back (l.db);
        std::sort (v.begin(), v.end());
        std::printf ("\nLoudness (max 100 ms RMS): min %.1f  median %.1f  max %.1f dB, spread %.1f dB\n",
                     v.front(), v[v.size() / 2], v.back(), v.back() - v.front());
    }

    // --levels <PresetLevels.inc>: rewrite per-preset trims towards the target loudness
    if (argc > 2 && String (argv[1]) == "--levels")
    {
        const File f (argv[2]);
        std::map<String, float> old;
        StringArray lines;
        f.readLines (lines);
        for (auto& line : lines)
        {
            const auto name = line.fromFirstOccurrenceOf ("\"", false, false).upToLastOccurrenceOf ("\"", false, false);
            const auto value = line.fromLastOccurrenceOf ("\",", false, false).trim().upToFirstOccurrenceOf ("f", false, false).getFloatValue();
            if (name.isNotEmpty() && value > 0.0f) old[name] = value;
        }
        String out;
        for (auto& l : loudness)
        {
            const float prev = old.count (l.name) ? old[l.name] : 1.0f;
            const float trim = jlimit (0.1f, 8.0f, prev * (float) std::pow (10.0, (kTargetDb - l.db) / 20.0));
            out << "            { \"" << l.name << "\", " << String (trim, 4) << "f },\n";
        }
        f.replaceWithText (out);
        std::printf ("Wrote %d trims to %s\n", (int) loudness.size(), f.getFullPathName().toRawUTF8());
        return 0;
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
    if (argc > 1 && String (argv[1]) != "--levels")
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

        // Lessons: no step may already be "done" on the Init patch, or it would skip itself.
        {
            int steps = 0, preDone = 0;
            for (auto& [name, lesson] : content->getLessons())
            {
                proc.getPresetManager().loadFactory (0);
                for (auto& st : lesson)
                    if (st.isDone)
                    {
                        ++steps;
                        if (st.isDone()) { ++preDone; std::printf ("  lesson '%s' step '%s' is already complete on Init!\n", name.toRawUTF8(), st.title.toRawUTF8()); }
                    }
            }
            std::printf ("Lessons: %d lessons, %d checked steps, %d pre-completed\n", (int) content->getLessons().size(), steps, preDone);
            content->startLessonForTest (4);
            content->getTutorial().goToStep (6);
            auto li = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            FileOutputStream lf (outDir.getChildFile ("lesson.png"));
            lf.setPosition (0); lf.truncate();
            PNGImageFormat().writeImageToStream (li, lf);
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
