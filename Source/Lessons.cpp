// Interactive sound design lessons. Each lesson starts from the Init patch and walks the
// user through building one type of sound, checking each move before moving on.

#include "PluginEditor.h"
#include "Presets.h"

namespace strata
{
using namespace juce;

void MainContent::buildLessons()
{
    auto& proc = ctx.proc;
    auto P = [&proc] (const String& id) { return proc.apvts.getRawParameterValue (id)->load(); };
    auto hasMod = [&proc] (int src, const String& dstId)
    {
        const int d = mod::destIndexForParam (dstId);
        for (auto& m : proc.getModulations())
            if (m.src == src && m.dst == d) return true;
        return false;
    };

    using Targets = std::function<std::vector<Component*>()>;
    using Check = std::function<bool()>;
    using Enter = std::function<void()>;
    auto step = [] (const String& title, const String& body, const String& tryIt, Targets t, Enter enter = {}, Check done = {})
    {
        return TutorialStep { title, body, tryIt, std::move (t), std::move (enter), std::move (done) };
    };

    // Common opening: explain, then load Init once (going BACK won't wipe your work).
    auto opening = [this, step] (std::vector<TutorialStep>& s, const String& title, const String& body)
    {
        auto loaded = std::make_shared<bool> (false);
        s.push_back (step (title, body + "\n\nThis lesson starts from a blank Init patch. Save your current sound first if you want to keep it, then press NEXT.",
                           {}, {}, [loaded] { *loaded = false; }));
        s.push_back (step ("Start from scratch",
                           "This is Init: one plain saw wave. Play a few notes on the keyboard below so you know where you're starting from.",
                           {}, [this] { return std::vector<Component*> { &keyboard }; },
                           [this, loaded]
                           {
                               if (! *loaded) { ctx.proc.getPresetManager().loadFactory (0); *loaded = true; }
                               setTab (0); synth.envs.select (0); synth.lfos.select (0);
                           }));
    };
    auto finish = [this, step] (std::vector<TutorialStep>& s, const String& body)
    {
        s.push_back (step ("You made it!", body + "\n\nPress SAVE to keep it as your own preset.", {},
                           [this] { return std::vector<Component*> { &presetBar.save }; },
                           [this] { setTab (0); }));
    };
    auto fxStep = [this, step, P] (std::vector<TutorialStep>& s, int fx, const String& title, const String& body, const String& powerId)
    {
        s.push_back (step (title, body, "switch on " + params::fxNames()[fx].toUpperCase(),
                           [this, fx] { return std::vector<Component*> { fxPage.panels[fx] }; },
                           [this] { setTab (2); },
                           [P, powerId] { return P (powerId) > 0.5f; }));
    };
    auto showEnv = [this] (int e) { return [this, e] { setTab (0); synth.envs.select (e); }; };
    auto showSynth = [this] { return [this] { setTab (0); synth.lfos.select (0); }; };

    lessons.clear();

    //==========================================================================
    {
        std::vector<TutorialStep> s;
        opening (s, "Lesson: Reese Bass",
                 "A Reese is two saw waves slightly out of tune, slowly phasing against each other. "
                 "It's the classic drum & bass and dark-techno bass.");
        s.push_back (step ("Add a second oscillator", "Osc B is another saw. Turn it on.", "click Osc B's power button",
                           [this] { return std::vector<Component*> { &synth.oscB.power.button }; }, showSynth(),
                           [P] { return P ("oscB_on") > 0.5f; }));
        s.push_back (step ("Detune it", "Push Osc B's FINE to about +10 cents. Hold a low note: hear the slow 'whoosh'? That phasing is the Reese.",
                           "set Osc B FINE to +8 cents or more",
                           [this] { return std::vector<Component*> { &synth.oscB.fine }; }, {},
                           [P] { return std::abs (P ("oscB_fine")) >= 8.0f; }));
        s.push_back (step ("Thicken with unison", "Unison stacks detuned copies of Osc A. Two or three is plenty for a bass.",
                           "set Osc A UNISON to 2 or more",
                           [this] { return std::vector<Component*> { &synth.oscA.uni, &synth.oscA.detune }; }, {},
                           [P] { return P ("oscA_uni") >= 2.0f; }));
        s.push_back (step ("Darken it", "Basses live low. Pull the filter CUTOFF down so the fizz goes away.",
                           "set CUTOFF below 1.5 kHz",
                           [this] { return std::vector<Component*> { &synth.filter.cutoff }; }, {},
                           [P] { return P ("flt_cutoff") < 1500.0f; }));
        s.push_back (step ("Add weight", "The sub oscillator adds a clean sine an octave down: the part you feel on big speakers.",
                           "turn on SUB",
                           [this] { return std::vector<Component*> { &synth.subNoise.subPower.button, &synth.subNoise.subLevel }; }, {},
                           [P] { return P ("sub_on") > 0.5f; }));
        s.push_back (step ("Play it mono", "Basslines are one note at a time. LEGATO also makes overlapping notes slide into each other.",
                           "set MODE to Mono or Legato",
                           [this] { return std::vector<Component*> { &synth.global.voiceMode.box }; }, {},
                           [P] { return P ("voice_mode") >= 1.0f; }));
        s.push_back (step ("Make it move", "Modulation brings sounds to life. Drag the LFO 1 chip and drop it on CUTOFF.",
                           "drag LFO 1 onto CUTOFF",
                           [this] { return std::vector<Component*> { synth.lfos.chips[0], &synth.filter.cutoff }; }, showSynth(),
                           [hasMod] { return hasMod (mod::srcLfo1, "flt_cutoff"); }));
        s.push_back (step ("Slow it down", "A slow LFO gives a gentle sweep instead of a wobble.",
                           "turn LFO 1 RATE below 1 Hz",
                           [this] { return std::vector<Component*> { synth.lfos.rates[0] }; }, {},
                           [P] { return P ("lfo1_rate") < 1.0f; }));
        fxStep (s, params::fxDistortion, "Add grit", "A little saturation helps a bass cut through on small speakers.", "dist_on");
        finish (s, "That's a Reese. Next ideas: swap Osc A to the Growl wavetable, or route LFO 1 to Osc B FINE for wider phasing.");
        lessons.push_back ({ "Reese Bass", std::move (s) });
    }

    //==========================================================================
    {
        std::vector<TutorialStep> s;
        opening (s, "Lesson: Supersaw Lead",
                 "The supersaw is a wall of detuned saws, the sound of trance and festival EDM.");
        s.push_back (step ("Stack the saws", "Turn UNISON up. Each step adds another detuned copy.",
                           "set Osc A UNISON to 7 or 8",
                           [this] { return std::vector<Component*> { &synth.oscA.uni }; }, showSynth(),
                           [P] { return P ("oscA_uni") >= 7.0f; }));
        s.push_back (step ("Spread the pitch", "DETUNE sets how far apart the copies are. More = bigger and wider.",
                           "set DETUNE to 35% or more",
                           [this] { return std::vector<Component*> { &synth.oscA.detune, &synth.oscA.blend }; }, {},
                           [P] { return P ("oscA_detune") >= 0.35f; }));
        s.push_back (step ("Go wide", "WIDTH pans the unison voices across the stereo field.",
                           "set WIDTH to 90% or more",
                           [this] { return std::vector<Component*> { &synth.oscA.width }; }, {},
                           [P] { return P ("oscA_width") >= 0.9f; }));
        s.push_back (step ("Layer an octave", "Turn on Osc B and raise its OCTAVE by one. Octave layers make leads brighter and bigger.",
                           "Osc B on, OCTAVE +1",
                           [this] { return std::vector<Component*> { &synth.oscB.power.button, &synth.oscB.oct }; }, {},
                           [P] { return P ("oscB_on") > 0.5f && P ("oscB_oct") >= 1.0f; }));
        s.push_back (step ("Let notes ring", "A longer RELEASE on ENV 1 stops notes cutting off abruptly.",
                           "set ENV 1 RELEASE to 250 ms or more",
                           [this] { return std::vector<Component*> { synth.envs.knobs[3] }; }, showEnv (0),
                           [P] { return P ("env1_rel") >= 0.25f; }));
        fxStep (s, params::fxDelay, "Add echoes", "Tempo-synced delay fills the gaps between notes.", "dly_on");
        fxStep (s, params::fxReverb, "Add space", "Reverb puts the lead in a room.", "rev_on");
        finish (s, "That's a supersaw lead. Try drawing an LFO onto FINE for vibrato, or the Sync Sweep wavetable for a sharper edge.");
        lessons.push_back ({ "Supersaw Lead", std::move (s) });
    }

    //==========================================================================
    {
        std::vector<TutorialStep> s;
        opening (s, "Lesson: Pluck",
                 "A pluck is short and percussive: it starts bright and closes quickly. Great for melodies and arps.");
        s.push_back (step ("No sustain", "With SUSTAIN at zero, the note fades out even while you hold the key.",
                           "set ENV 1 SUSTAIN to 0",
                           [this] { return std::vector<Component*> { synth.envs.knobs[2] }; }, showEnv (0),
                           [P] { return P ("env1_sus") < 0.05f; }));
        s.push_back (step ("Short decay", "DECAY now sets the length of the pluck.",
                           "set ENV 1 DECAY below 450 ms",
                           [this] { return std::vector<Component*> { synth.envs.knobs[1] }; }, {},
                           [P] { return P ("env1_dec") <= 0.45f; }));
        s.push_back (step ("Close the filter", "Pull CUTOFF down. The envelope will open it again on each note.",
                           "set CUTOFF below 1 kHz",
                           [this] { return std::vector<Component*> { &synth.filter.cutoff }; }, showSynth(),
                           [P] { return P ("flt_cutoff") < 1000.0f; }));
        s.push_back (step ("Let ENV 2 open it", "ENV 2 controls the filter. Turn its amount up and the cutoff jumps open at the start of every note.",
                           "set the ENV 2 knob to 40% or more",
                           [this] { return std::vector<Component*> { &synth.filter.envAmt }; }, {},
                           [P] { return P ("flt_env") > 0.4f; }));
        s.push_back (step ("Snap", "Now shorten ENV 2's DECAY. The faster the filter closes, the snappier the pluck.",
                           "set ENV 2 DECAY below 300 ms",
                           [this] { return std::vector<Component*> { synth.envs.knobs[5], &synth.envs.editor }; }, showEnv (1),
                           [P] { return P ("env2_dec") < 0.3f; }));
        fxStep (s, params::fxDelay, "Echo", "Delay turns a single pluck into a rhythm.", "dly_on");
        finish (s, "That's a pluck. Try the FM Metal or Pluck Spectra wavetables, or add UNISON 3 for a wider pluck.");
        lessons.push_back ({ "Pluck", std::move (s) });
    }

    //==========================================================================
    {
        std::vector<TutorialStep> s;
        opening (s, "Lesson: Evolving Pad",
                 "Pads fade in slowly, last a long time and keep changing. They fill the background of a track.");
        s.push_back (step ("Slow attack", "A slow ATTACK makes notes swell in instead of starting hard.",
                           "set ENV 1 ATTACK to 400 ms or more",
                           [this] { return std::vector<Component*> { synth.envs.knobs[0] }; }, showEnv (0),
                           [P] { return P ("env1_atk") >= 0.4f; }));
        s.push_back (step ("Long release", "A long RELEASE lets chords blend into each other.",
                           "set ENV 1 RELEASE to 1.5 s or more",
                           [this] { return std::vector<Component*> { synth.envs.knobs[3] }; }, {},
                           [P] { return P ("env1_rel") >= 1.5f; }));
        s.push_back (step ("Pick a richer wavetable", "Choose a different wavetable bank. Spectral, Formant and Harmonic Sweep are lovely for pads.",
                           "change Osc A's wavetable",
                           [this] { return std::vector<Component*> { &synth.oscA.bank.box, &synth.oscA.display }; }, showSynth(),
                           [P] { return P ("oscA_bank") > 0.5f; }));
        s.push_back (step ("Thicken", "Pads love unison.", "set Osc A UNISON to 5 or more",
                           [this] { return std::vector<Component*> { &synth.oscA.uni, &synth.oscA.detune }; }, {},
                           [P] { return P ("oscA_uni") >= 5.0f; }));
        s.push_back (step ("Keep it moving", "Drop LFO 1 on WT POS: the waveform will slowly morph while you hold the chord.",
                           "drag LFO 1 onto WT POS",
                           [this] { return std::vector<Component*> { synth.lfos.chips[0], &synth.oscA.pos }; }, {},
                           [hasMod] { return hasMod (mod::srcLfo1, "oscA_pos"); }));
        s.push_back (step ("Slower", "For pads, slow modulation sounds natural.", "set LFO 1 RATE below 0.5 Hz",
                           [this] { return std::vector<Component*> { synth.lfos.rates[0], &synth.lfos.editor }; }, {},
                           [P] { return P ("lfo1_rate") < 0.5f; }));
        fxStep (s, params::fxChorus, "Shimmer", "Chorus widens and softens the pad.", "cho_on");
        s.push_back (step ("Big space", "Turn reverb on and raise its MIX for a lush tail.", "reverb on, MIX 35% or more",
                           [this] { return std::vector<Component*> { fxPage.panels[params::fxReverb] }; }, [this] { setTab (2); },
                           [P] { return P ("rev_on") > 0.5f && P ("rev_mix") >= 0.35f; }));
        finish (s, "That's an evolving pad. Try a second LFO on CUTOFF at a different rate so the movement never repeats.");
        lessons.push_back ({ "Evolving Pad", std::move (s) });
    }

    //==========================================================================
    {
        std::vector<TutorialStep> s;
        opening (s, "Lesson: Wobble Bass",
                 "A wobble bass is an LFO opening and closing the sound in time with the song. The core of dubstep.");
        s.push_back (step ("Growl wavetable", "Choose the Growl wavetable bank for Osc A.", "set Osc A's wavetable to Growl",
                           [this] { return std::vector<Component*> { &synth.oscA.bank.box }; }, showSynth(),
                           [P] { return (int) P ("oscA_bank") == 6; }));
        s.push_back (step ("Mono", "Bass, so one note at a time.", "set MODE to Mono or Legato",
                           [this] { return std::vector<Component*> { &synth.global.voiceMode.box }; }, {},
                           [P] { return P ("voice_mode") >= 1.0f; }));
        s.push_back (step ("Lock to tempo", "SYNC makes LFO 1 follow your project's BPM.", "turn on LFO 1 SYNC",
                           [this] { return std::vector<Component*> { &synth.lfos.syncs[0]->button }; }, {},
                           [P] { return P ("lfo1_sync") > 0.5f; }));
        s.push_back (step ("Pick the rhythm", "The division is the wobble speed. 1/8 is classic; 1/16 is faster; triplets feel swung.",
                           "choose 1/8 or faster",
                           [this] { return std::vector<Component*> { &synth.lfos.divs[0]->box }; }, {},
                           [P] { return P ("lfo1_div") >= 8.0f; }));
        s.push_back (step ("Wobble the filter", "Drag LFO 1 onto CUTOFF.", "drag LFO 1 onto CUTOFF",
                           [this] { return std::vector<Component*> { synth.lfos.chips[0], &synth.filter.cutoff }; }, {},
                           [hasMod] { return hasMod (mod::srcLfo1, "flt_cutoff"); }));
        s.push_back (step ("Wobble the wavetable", "Also drop LFO 1 on WT POS so the tone changes, not just the brightness.",
                           "drag LFO 1 onto WT POS",
                           [this] { return std::vector<Component*> { synth.lfos.chips[0], &synth.oscA.pos }; }, {},
                           [hasMod] { return hasMod (mod::srcLfo1, "oscA_pos"); }));
        s.push_back (step ("Add bite", "Resonance makes the filter 'talk'.", "set RES to 35% or more",
                           [this] { return std::vector<Component*> { &synth.filter.res }; }, {},
                           [P] { return P ("flt_res") >= 0.35f; }));
        s.push_back (step ("Sub", "Keep the low end solid while the top wobbles.", "turn on SUB",
                           [this] { return std::vector<Component*> { &synth.subNoise.subPower.button }; }, {},
                           [P] { return P ("sub_on") > 0.5f; }));
        fxStep (s, params::fxDistortion, "Make it angry", "Distortion is half of a dubstep bass. Try the Fold type too.", "dist_on");
        finish (s, "That's a wobble. Change LFO 1's shape to Square for a riddim feel, or automate the division in your song.");
        lessons.push_back ({ "Wobble Bass", std::move (s) });
    }

    //==========================================================================
    {
        std::vector<TutorialStep> s;
        opening (s, "Lesson: Riser FX",
                 "A riser builds tension before a drop: it climbs in pitch and brightness over several seconds.");
        s.push_back (step ("Add noise", "Noise gives risers their 'whoosh'.", "turn on NOISE",
                           [this] { return std::vector<Component*> { &synth.subNoise.noisePower.button, &synth.subNoise.noiseLevel }; }, showSynth(),
                           [P] { return P ("noise_on") > 0.5f; }));
        s.push_back (step ("A slow envelope", "ENV 3 is free to control anything. Give it a long ATTACK: that's the length of the rise.",
                           "set ENV 3 ATTACK to 2 s or more",
                           [this] { return std::vector<Component*> { synth.envs.knobs[8], &synth.envs.editor }; }, showEnv (2),
                           [P] { return P ("env3_atk") >= 2.0f; }));
        s.push_back (step ("Raise its sustain", "Turn ENV 3 SUSTAIN up so the riser holds at the top instead of falling back.",
                           "set ENV 3 SUSTAIN to 80% or more",
                           [this] { return std::vector<Component*> { synth.envs.knobs[10] }; }, {},
                           [P] { return P ("env3_sus") >= 0.8f; }));
        s.push_back (step ("Pitch up", "Drag the ENV 3 chip onto Osc A SEMI. The pitch now climbs as ENV 3 rises.",
                           "drag ENV 3 onto SEMI",
                           [this] { return std::vector<Component*> { synth.envs.chips[2], &synth.oscA.semi }; }, {},
                           [hasMod] { return hasMod (mod::srcEnv3, "oscA_semi"); }));
        s.push_back (step ("Open up", "Drop ENV 3 on CUTOFF too, so it gets brighter as it rises.", "drag ENV 3 onto CUTOFF",
                           [this] { return std::vector<Component*> { synth.envs.chips[2], &synth.filter.cutoff }; }, {},
                           [hasMod] { return hasMod (mod::srcEnv3, "flt_cutoff"); }));
        s.push_back (step ("Start dark", "Lower CUTOFF so there's room to open.", "set CUTOFF below 1 kHz",
                           [this] { return std::vector<Component*> { &synth.filter.cutoff }; }, {},
                           [P] { return P ("flt_cutoff") < 1000.0f; }));
        fxStep (s, params::fxReverb, "Wash", "A big reverb makes it feel huge.", "rev_on");
        finish (s, "Hold a note for a few seconds and listen to it climb. Add Alt-drag depth to SEMI for a bigger jump.");
        lessons.push_back ({ "Riser FX", std::move (s) });
    }

    //==========================================================================
    {
        std::vector<TutorialStep> s;
        opening (s, "Lesson: Inventing new sounds with warp",
                 "Warp bends how the wavetable is read, and modulating it is how you discover sounds nobody else has.");
        s.push_back (step ("A metallic table", "Choose FM Metal for Osc A.", "set Osc A's wavetable to FM Metal",
                           [this] { return std::vector<Component*> { &synth.oscA.bank.box }; }, showSynth(),
                           [P] { return (int) P ("oscA_bank") == 4; }));
        s.push_back (step ("Pick a warp mode", "Each mode bends the wave differently. Try a few while holding a note: Sync and Fold are wild, Bend is subtle.",
                           "choose any warp mode",
                           [this] { return std::vector<Component*> { &synth.oscA.warpMode.box }; }, {},
                           [P] { return P ("oscA_warpMode") > 0.5f; }));
        s.push_back (step ("Dial it in", "Turn WARP up to hear it.", "set WARP to 30% or more",
                           [this] { return std::vector<Component*> { &synth.oscA.warp }; }, {},
                           [P] { return P ("oscA_warp") >= 0.3f; }));
        s.push_back (step ("Animate the warp", "Drop ENV 3 on WARP so every note starts warped and settles.",
                           "drag ENV 3 onto WARP",
                           [this] { return std::vector<Component*> { synth.envs.chips[2], &synth.oscA.warp }; }, showEnv (2),
                           [hasMod] { return hasMod (mod::srcEnv3, "oscA_warp"); }));
        s.push_back (step ("One knob to rule it", "Drag the M1 macro chip onto WT POS. Now MACRO 1 sweeps the timbre, and you can automate it in your DAW.",
                           "drag M1 onto WT POS",
                           [this] { return std::vector<Component*> { synth.global.macroChips[0], &synth.oscA.pos }; }, {},
                           [hasMod] { return hasMod (mod::srcMacro1, "oscA_pos"); }));
        fxStep (s, params::fxDistortion, "Push it", "Distortion exaggerates everything warp did.", "dist_on");
        finish (s, "You've invented a sound. Repeat the recipe with any wavetable, including your own: drop a .wav on the display.");
        lessons.push_back ({ "Inventing with Warp", std::move (s) });
    }
}

void MainContent::showLearnMenu()
{
    PopupMenu m;
    m.addItem ("Quick tour of Strata", [this] { startTutorial(); });
    m.addSeparator();
    m.addSectionHeader ("Sound design lessons (hands-on)");
    for (int i = 0; i < (int) lessons.size(); ++i)
        m.addItem (String (i + 1) + ".  " + lessons[(size_t) i].first, [this, i] { startLesson (i); });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&tourButton));
}

void MainContent::startLesson (int index)
{
    if (! isPositiveAndBelow (index, (int) lessons.size())) return;
    browser.setVisible (false);
    setTab (0);
    tutorial.setSteps (lessons[(size_t) index].second);
    tutorial.start();
}
}
