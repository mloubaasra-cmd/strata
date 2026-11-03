#include "Pages.h"
#include <complex>

namespace strata
{
using namespace juce;

namespace
{
    void placeKnobRow (std::initializer_list<Component*> knobs, int x, int y, int totalWidth, int w, int h)
    {
        const int n = (int) knobs.size();
        const float step = (float) totalWidth / (float) n;
        int i = 0;
        for (auto* k : knobs)
        {
            const int cx = x + roundToInt (step * ((float) i + 0.5f));
            k->setBounds (cx - w / 2, y, w, h);
            ++i;
        }
    }
}

//==============================================================================
OscPanel::OscPanel (UIContext& c, int o)
    : Panel (o == 0 ? "OSC A" : "OSC B"),
      display (c, o),
      pos (c, ids::osc (o, "pos"), "WT POS"), warp (c, ids::osc (o, "warp"), "WARP"),
      oct (c, ids::osc (o, "oct"), "OCTAVE"), semi (c, ids::osc (o, "semi"), "SEMI"),
      fine (c, ids::osc (o, "fine"), "FINE"), level (c, ids::osc (o, "level"), "LEVEL"),
      uni (c, ids::osc (o, "uni"), "UNISON"), detune (c, ids::osc (o, "detune"), "DETUNE"),
      blend (c, ids::osc (o, "blend"), "BLEND"), width (c, ids::osc (o, "width"), "WIDTH"),
      pan (c, ids::osc (o, "pan"), "PAN"),
      ctx (c), osc (o)
{
    power.attach (c.proc, ids::osc (o, "on"));
    bank.attach (c.proc, ids::osc (o, "bank"));
    warpMode.attach (c.proc, ids::osc (o, "warpMode"));
    bank.box.setTooltip ("Wavetable bank");
    warpMode.box.setTooltip ("Warp mode: bends the read-out of the wavetable");
    loadButton.setTooltip ("Load your own wavetable (.wav, .aif, .flac)");
    loadButton.onClick = [this]
    {
        chooser = std::make_unique<FileChooser> ("Load a wavetable", File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                              [this] (const FileChooser& fc)
                              {
                                  const auto f = fc.getResult();
                                  if (f.existsAsFile()) WavetableDisplay::loadFile (ctx, osc, f);
                              });
    };

    for (Component* comp : std::initializer_list<Component*> { &power.button, &bank.box, &warpMode.box, &loadButton, &display,
                                                               &pos, &warp, &oct, &semi, &fine, &level, &uni, &detune, &blend, &width, &pan })
        addAndMakeVisible (comp);
}

void OscPanel::resized()
{
    power.button.setBounds (8, 6, 20, 20);
    bank.box.setBounds (82, 5, 128, 22);
    warpMode.box.setBounds (214, 5, 80, 22);
    loadButton.setBounds (298, 5, 54, 22);
    display.setBounds (8, 32, getWidth() - 16, 92);
    placeKnobRow ({ &pos, &warp, &oct, &semi, &fine, &level }, 8, 128, getWidth() - 16, 58, 64);
    placeKnobRow ({ &uni, &detune, &blend, &width, &pan }, 36, 194, getWidth() - 72, 58, 64);
}

//==============================================================================
SubNoisePanel::SubNoisePanel (UIContext& c)
    : subOct (c, "sub_oct", "OCTAVE"), subLevel (c, "sub_level", "LEVEL"),
      noiseColor (c, "noise_color", "COLOR"), noiseLevel (c, "noise_level", "LEVEL")
{
    subPower.attach (c.proc, "sub_on");
    noisePower.attach (c.proc, "noise_on");
    subWave.attach (c.proc, "sub_wave");
    for (Component* comp : std::initializer_list<Component*> { &subPower.button, &noisePower.button, &subWave.box,
                                                               &subOct, &subLevel, &noiseColor, &noiseLevel })
        addAndMakeVisible (comp);
}

void SubNoisePanel::paint (Graphics& g)
{
    Panel::paint (g);
    g.setColour (colours::text);
    g.setFont (StrataLook::font (12.5f, true));
    g.drawText ("SUB", 34, 6, 60, 18, Justification::centredLeft);
    g.drawText ("NOISE", getWidth() / 2 + 34, 6, 60, 18, Justification::centredLeft);
    g.setColour (colours::outline);
    g.drawVerticalLine (getWidth() / 2, 8.0f, (float) getHeight() - 8.0f);
}

void SubNoisePanel::resized()
{
    const int half = getWidth() / 2;
    subPower.button.setBounds (8, 6, 20, 20);
    subWave.box.setBounds (70, 5, half - 78, 22);
    noisePower.button.setBounds (half + 8, 6, 20, 20);
    placeKnobRow ({ &subOct, &subLevel }, 8, 34, half - 16, 60, 64);
    placeKnobRow ({ &noiseColor, &noiseLevel }, half + 8, 34, half - 16, 60, 64);
}

//==============================================================================
FilterPanel::FilterPanel (UIContext& c)
    : Panel ("FILTER"),
      cutoff (c, "flt_cutoff", "CUTOFF"), res (c, "flt_res", "RES"), drive (c, "flt_drive", "DRIVE"),
      envAmt (c, "flt_env", "ENV 2"), keyTrack (c, "flt_key", "KEY"), ctx (c)
{
    power.attach (c.proc, "flt_on");
    type.attach (c.proc, "flt_type");
    for (Component* comp : std::initializer_list<Component*> { &power.button, &type.box, &cutoff, &res, &drive, &envAmt, &keyTrack })
        addAndMakeVisible (comp);
}

void FilterPanel::resized()
{
    power.button.setBounds (8, 6, 20, 20);
    type.box.setBounds (getWidth() - 108, 5, 100, 22);
    graphArea = { 8, 32, getWidth() - 16, 48 };
    placeKnobRow ({ &cutoff, &res, &drive, &envAmt, &keyTrack }, 8, 84, getWidth() - 16, 62, 64);
}

float FilterPanel::currentCutoffNorm() const
{
    const int d = cutoff.dest;
    auto& e = ctx.proc.engine;
    if (e.activeVoices.load() > 0 && d >= 0 && cutoff.hasModulation())
        return e.displayValue[(size_t) d].load();
    return ctx.proc.apvts.getParameter ("flt_cutoff")->getValue();
}

void FilterPanel::paint (Graphics& g)
{
    Panel::paint (g);
    auto r = graphArea.toFloat();
    g.setColour (Colour (0xff0a0d12));
    g.fillRoundedRectangle (r, 4.0f);

    auto* cut = ctx.proc.apvts.getParameter ("flt_cutoff");
    const float fc = cut->convertFrom0to1 (jlimit (0.0f, 1.0f, currentCutoffNorm()));
    const float resv = ctx.proc.apvts.getRawParameterValue ("flt_res")->load();
    const int t = (int) ctx.proc.apvts.getRawParameterValue ("flt_type")->load();
    const bool on = ctx.proc.apvts.getRawParameterValue ("flt_on")->load() > 0.5f;
    const float k = 2.0f - 1.97f * resv;

    Path p;
    const int n = 120;
    for (int i = 0; i <= n; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, (float) i / (float) n);
        const float w = f / fc;
        const std::complex<float> den (1.0f - w * w, k * w);
        std::complex<float> num;
        switch (t)
        {
            case params::fLP12: case params::fLP24: num = 1.0f; break;
            case params::fHP12: case params::fHP24: num = -w * w; break;
            case params::fBP:                       num = std::complex<float> (0.0f, k * w); break;
            default:                                num = 1.0f - w * w; break;
        }
        float mag = std::abs (num / den);
        if (t == params::fLP24 || t == params::fHP24) mag *= mag;
        const float db = jlimit (-36.0f, 18.0f, Decibels::gainToDecibels (mag, -60.0f));
        const float x = r.getX() + r.getWidth() * (float) i / (float) n;
        const float y = jmap (db, -36.0f, 18.0f, r.getBottom() - 2.0f, r.getY() + 2.0f);
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    const auto accent = on ? ctx.look.getAccent() : colours::textDim;
    Path fill (p);
    fill.lineTo (r.getRight(), r.getBottom());
    fill.lineTo (r.getX(), r.getBottom());
    fill.closeSubPath();
    g.setColour (accent.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (accent);
    g.strokePath (p, PathStrokeType (1.6f));
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (10.0f));
    g.drawText (cut->getText (cut->convertTo0to1 (fc), 12), r.reduced (6.0f, 2.0f), Justification::topRight);
}

void FilterPanel::tick()
{
    const float c = currentCutoffNorm();
    const float r = ctx.proc.apvts.getRawParameterValue ("flt_res")->load();
    const int t = (int) ctx.proc.apvts.getRawParameterValue ("flt_type")->load();
    const bool on = ctx.proc.apvts.getRawParameterValue ("flt_on")->load() > 0.5f;
    if (std::abs (c - lastCut) > 0.001f || r != lastRes || t != lastType || on != lastOn)
    {
        lastCut = c; lastRes = r; lastType = t; lastOn = on;
        repaint (graphArea);
    }
}

//==============================================================================
EnvPanel::EnvPanel (UIContext& c) : Panel ("ENVELOPES"), editor (c, 0)
{
    titleIndent = 10;
    for (int e = 0; e < params::kNumEnvs; ++e)
    {
        auto* chip = chips.add (new SourceChip (mod::srcEnv1 + e, "ENV " + String (e + 1)));
        chip->onClick = [this, e] { select (e); };
        addAndMakeVisible (chip);

        knobs.add (new ModKnob (c, ids::env (e, "atk"), "ATTACK"));
        knobs.add (new ModKnob (c, ids::env (e, "dec"), "DECAY"));
        knobs.add (new ModKnob (c, ids::env (e, "sus"), "SUSTAIN"));
        knobs.add (new ModKnob (c, ids::env (e, "rel"), "RELEASE"));
    }
    for (auto* k : knobs) addChildComponent (k);
    addAndMakeVisible (editor);
    select (0);
}

void EnvPanel::select (int e)
{
    selected = e;
    for (int i = 0; i < chips.size(); ++i) { chips[i]->selected = i == e; chips[i]->repaint(); }
    for (int i = 0; i < knobs.size(); ++i) knobs[i]->setVisible (i / 4 == e);
    editor.setEnvelope (e);
}

void EnvPanel::resized()
{
    for (int i = 0; i < chips.size(); ++i)
        chips[i]->setBounds (112 + i * 76, 5, 70, 22);
    editor.setBounds (8, 32, getWidth() - 16, 150);
    for (int e = 0; e < params::kNumEnvs; ++e)
        placeKnobRow ({ knobs[e * 4], knobs[e * 4 + 1], knobs[e * 4 + 2], knobs[e * 4 + 3] }, 8, 192, getWidth() - 16, 66, 70);
}

//==============================================================================
LfoPanel::LfoPanel (UIContext& c) : Panel ("LFO"), editor (c, 0)
{
    titleIndent = 10;
    for (int l = 0; l < params::kNumLfos; ++l)
    {
        auto* chip = chips.add (new SourceChip (mod::srcLfo1 + l, "LFO " + String (l + 1)));
        chip->onClick = [this, l] { select (l); };
        addAndMakeVisible (chip);

        addChildComponent (rates.add (new ModKnob (c, ids::lfo (l, "rate"), "RATE")));
        auto* sh = shapes.add (new ParamCombo());
        sh->attach (c.proc, ids::lfo (l, "shape"));
        addChildComponent (sh->box);
        auto* dv = divs.add (new ParamCombo());
        dv->attach (c.proc, ids::lfo (l, "div"));
        addChildComponent (dv->box);
        auto* sy = syncs.add (new ParamToggle());
        sy->attach (c.proc, ids::lfo (l, "sync"), "SYNC");
        addChildComponent (sy->button);
        auto* rt = retrigs.add (new ParamToggle());
        rt->attach (c.proc, ids::lfo (l, "retrig"), "RETRIG");
        rt->button.setTooltip ("On: restarts on every note. Off: free-running for all voices.");
        addChildComponent (rt->button);
    }
    addAndMakeVisible (editor);
    select (0);
}

void LfoPanel::select (int l)
{
    selected = l;
    for (int i = 0; i < chips.size(); ++i) { chips[i]->selected = i == l; chips[i]->repaint(); }
    for (int i = 0; i < params::kNumLfos; ++i)
    {
        const bool v = i == l;
        rates[i]->setVisible (v);
        shapes[i]->box.setVisible (v);
        divs[i]->box.setVisible (v);
        syncs[i]->button.setVisible (v);
        retrigs[i]->button.setVisible (v);
    }
    editor.setLfo (l);
}

void LfoPanel::paint (Graphics& g)
{
    Panel::paint (g);
    drawSectionLabel (g, { 96, 188, 100, 14 }, "SHAPE");
    drawSectionLabel (g, { 232, 188, 100, 14 }, "SYNC DIVISION");
}

void LfoPanel::resized()
{
    for (int i = 0; i < chips.size(); ++i)
        chips[i]->setBounds (60 + i * 72, 5, 66, 22);
    editor.setBounds (8, 32, getWidth() - 16, 150);
    for (int l = 0; l < params::kNumLfos; ++l)
    {
        rates[l]->setBounds (14, 190, 70, 76);
        shapes[l]->box.setBounds (96, 204, 124, 24);
        divs[l]->box.setBounds (232, 204, 96, 24);
        syncs[l]->button.setBounds (336, 204, 82, 24);
        retrigs[l]->button.setBounds (96, 238, 124, 24);
    }
}

//==============================================================================
GlobalPanel::GlobalPanel (UIContext& c)
    : Panel ("MACROS & VOICE"),
      glide (c, "glide", "GLIDE"), bend (c, "bend_range", "BEND"), master (c, "master", "MASTER")
{
    titleIndent = 10;
    for (int m = 0; m < params::kNumMacros; ++m)
    {
        auto* chip = macroChips.add (new SourceChip (mod::srcMacro1 + m, "M" + String (m + 1)));
        addAndMakeVisible (chip);
        addAndMakeVisible (macros.add (new ModKnob (c, ids::macro (m), "MACRO " + String (m + 1))));
    }
    const int others[] = { mod::srcVelocity, mod::srcModWheel, mod::srcNote, mod::srcRandom };
    for (int s : others)
        addAndMakeVisible (otherChips.add (new SourceChip (s, mod::sourceShortNames()[s])));

    voiceMode.attach (c.proc, "voice_mode");
    for (Component* comp : std::initializer_list<Component*> { &voiceMode.box, &glide, &bend, &master })
        addAndMakeVisible (comp);
}

void GlobalPanel::paint (Graphics& g)
{
    Panel::paint (g);
    drawSectionLabel (g, { 10, 124, 200, 14 }, "MORE SOURCES  (drag onto knobs)");
    drawSectionLabel (g, { 10, 172, 200, 14 }, "VOICE");
    drawSectionLabel (g, { 10, 194, 80, 14 }, "MODE");
}

void GlobalPanel::resized()
{
    const int w = (getWidth() - 16) / 4;
    for (int m = 0; m < params::kNumMacros; ++m)
    {
        macroChips[m]->setBounds (8 + m * w + 3, 30, w - 6, 20);
        macros[m]->setBounds (8 + m * w, 52, w, 68);
    }
    for (int i = 0; i < otherChips.size(); ++i)
        otherChips[i]->setBounds (8 + i * w + 3, 142, w - 6, 20);
    voiceMode.box.setBounds (10, 210, 76, 24);
    placeKnobRow ({ &glide, &bend, &master }, 92, 190, getWidth() - 100, 62, 70);
}

//==============================================================================
SynthPage::SynthPage (UIContext& c)
    : oscA (c, 0), oscB (c, 1), subNoise (c), filter (c), envs (c), lfos (c), global (c)
{
    for (Component* comp : std::initializer_list<Component*> { &oscA, &oscB, &subNoise, &filter, &envs, &lfos, &global })
        addAndMakeVisible (comp);
}

void SynthPage::resized()
{
    oscA.setBounds (10, 8, 360, 264);
    oscB.setBounds (378, 8, 360, 264);
    subNoise.setBounds (746, 8, 344, 104);
    filter.setBounds (746, 120, 344, 152);
    envs.setBounds (10, 280, 350, 290);
    lfos.setBounds (368, 280, 430, 290);
    global.setBounds (806, 280, 284, 290);
}

void SynthPage::tick()
{
    oscA.display.tick();
    oscB.display.tick();
    filter.tick();
    envs.tick();
    lfos.tick();
}

//==============================================================================
MatrixPage::MatrixPage (UIContext& c) : ctx (c)
{
    for (int i = 0; i < mod::kMaxSlots; ++i)
    {
        auto* row = rows.add (new Row());
        row->index.setText (String (i + 1), dontSendNotification);
        row->index.setJustificationType (Justification::centred);
        row->index.setColour (Label::textColourId, colours::textDim);
        row->src.addItemList (mod::sourceNames(), 1);
        for (int d = 0; d < mod::numDests(); ++d)
            row->dst.addItem (mod::destName (d), d + 1);
        row->amount.setSliderStyle (Slider::LinearHorizontal);
        row->amount.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        row->amount.setRange (-1.0, 1.0, 0.0);
        row->amount.setDoubleClickReturnValue (true, 0.0);
        row->value.setJustificationType (Justification::centredRight);
        row->value.setColour (Label::textColourId, colours::mod);

        auto push = [this, i]
        {
            auto* r = rows[i];
            ctx.proc.setModulation (i, r->src.getSelectedId() - 1, r->dst.getSelectedId() - 1, (float) r->amount.getValue());
        };
        row->src.onChange = push;
        row->dst.onChange = push;
        row->amount.onValueChange = [this, i, push]
        {
            push();
            rows[i]->value.setText ((rows[i]->amount.getValue() > 0 ? "+" : "") + String (roundToInt (rows[i]->amount.getValue() * 100)) + "%",
                                    dontSendNotification);
        };
        row->remove.onClick = [this, i] { ctx.proc.removeModulation (i); };
        for (Component* comp : std::initializer_list<Component*> { &row->index, &row->src, &row->dst, &row->amount, &row->value, &row->remove })
            row->addAndMakeVisible (comp);
        addChildComponent (row);
    }

    addButton.onClick = [this]
    {
        const auto mods = ctx.proc.getModulations();
        for (int s = mod::srcLfo1; s < mod::kNumSources; ++s)
            for (int d = 0; d < mod::numDests(); ++d)
            {
                bool used = false;
                for (auto& m : mods) used |= (m.src == s && m.dst == d);
                if (! used) { ctx.proc.addModulation (s, d, 0.25f); return; }
            }
    };
    clearButton.onClick = [this] { ctx.proc.clearModulations(); };
    addAndMakeVisible (addButton);
    addAndMakeVisible (clearButton);
    refresh();
}

void MatrixPage::Row::resized()
{
    index.setBounds (0, 0, 30, getHeight());
    src.setBounds (40, 3, 200, getHeight() - 6);
    dst.setBounds (270, 3, 240, getHeight() - 6);
    amount.setBounds (530, 3, 380, getHeight() - 6);
    value.setBounds (916, 3, 70, getHeight() - 6);
    remove.setBounds (1010, 4, 28, getHeight() - 8);
}

void MatrixPage::Row::paint (Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (14.0f, true));
    g.drawText (">", 244, 0, 22, getHeight(), Justification::centred);
}

void MatrixPage::paint (Graphics& g)
{
    g.setColour (colours::text);
    g.setFont (StrataLook::font (16.0f, true));
    g.drawText ("MODULATION MATRIX", 20, 10, 300, 26, Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (12.0f));
    g.drawText ("Tip: drag any ENV / LFO / macro chip straight onto a knob. Alt-drag a knob to change depth.",
                250, 10, 620, 26, Justification::centredLeft);
    g.setFont (StrataLook::font (10.5f, true));
    g.drawText ("SOURCE", 60, 46, 200, 16, Justification::centredLeft);
    g.drawText ("DESTINATION", 290, 46, 200, 16, Justification::centredLeft);
    g.drawText ("AMOUNT", 550, 46, 200, 16, Justification::centredLeft);

    if (numVisible == 0)
    {
        g.setFont (StrataLook::font (14.0f));
        g.drawText ("No modulation yet. Drag a source chip onto a knob, or press + ADD SLOT.",
                    getLocalBounds().withTrimmedTop (60).removeFromTop (80), Justification::centred);
    }
}

void MatrixPage::resized()
{
    addButton.setBounds (getWidth() - 220, 10, 100, 26);
    clearButton.setBounds (getWidth() - 112, 10, 92, 26);
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (20, 66 + i * 31, getWidth() - 40, 28);
}

void MatrixPage::refresh()
{
    const auto mods = ctx.proc.getModulations();
    numVisible = (int) mods.size();
    for (int i = 0; i < rows.size(); ++i)
    {
        auto* r = rows[i];
        const bool v = i < numVisible;
        r->setVisible (v);
        if (! v) continue;
        const auto& m = mods[(size_t) i];
        if (r->src.getSelectedId() != m.src + 1) r->src.setSelectedId (m.src + 1, dontSendNotification);
        if (r->dst.getSelectedId() != m.dst + 1) r->dst.setSelectedId (m.dst + 1, dontSendNotification);
        if (! r->amount.isMouseButtonDown() && std::abs (r->amount.getValue() - m.amount) > 1.0e-4)
            r->amount.setValue (m.amount, dontSendNotification);
        r->value.setText ((m.amount > 0 ? "+" : "") + String (roundToInt (m.amount * 100)) + "%", dontSendNotification);
    }
    repaint();
}

//==============================================================================
FxOrderList::FxOrderList (UIContext& c) : ctx (c) {}

Rectangle<int> FxOrderList::rowBounds (int i) const { return { 10, 44 + i * 64, getWidth() - 20, 56 }; }

int FxOrderList::rowAt (int y) const { return jlimit (0, params::kNumFx - 1, (y - 44 + 32) / 64); }

void FxOrderList::paint (Graphics& g)
{
    g.setColour (colours::panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (StrataLook::font (12.5f, true));
    g.drawText ("SIGNAL FLOW", 12, 10, 200, 20, Justification::centredLeft);

    static const char* onIds[] = { "dist_on", "cho_on", "pha_on", "dly_on", "rev_on" };
    const auto order = ctx.proc.getFxOrder();
    const auto accent = ctx.look.getAccent();

    for (int i = 0; i < params::kNumFx; ++i)
    {
        const int fx = order[(size_t) i];
        auto r = rowBounds (i).toFloat();
        const bool on = ctx.proc.apvts.getRawParameterValue (onIds[fx])->load() > 0.5f;
        g.setColour (i == dragFrom ? colours::panelLight.brighter (0.15f) : colours::panelLight);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (on ? accent.withAlpha (0.6f) : colours::outline);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);

        g.setColour (colours::textDim);
        for (int a = 0; a < 3; ++a)
            for (int b = 0; b < 2; ++b)
                g.fillEllipse (r.getX() + 10.0f + (float) b * 4.0f, r.getCentreY() - 6.0f + (float) a * 4.5f, 2.5f, 2.5f);

        g.setFont (StrataLook::font (11.0f, true));
        g.drawText (String (i + 1), (int) r.getX() + 24, (int) r.getY(), 20, (int) r.getHeight(), Justification::centred);
        g.setColour (on ? colours::text : colours::textDim);
        g.setFont (StrataLook::font (14.0f, true));
        g.drawText (params::fxNames()[fx], (int) r.getX() + 50, (int) r.getY(), 120, (int) r.getHeight(), Justification::centredLeft);
        g.setColour (on ? accent : colours::outline);
        g.fillEllipse (r.getRight() - 20.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
    }

    if (dragFrom >= 0 && dropAt >= 0 && dropAt != dragFrom)
    {
        const int y = rowBounds (dropAt).getY() + (dropAt > dragFrom ? 60 : -4);
        g.setColour (accent);
        g.fillRect (10, y, getWidth() - 20, 3);
    }

    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (11.0f));
    g.drawFittedText ("Drag effects to change the order they process in. Top runs first.",
                      Rectangle<int> (12, 44 + 5 * 64 + 4, getWidth() - 24, 40), Justification::topLeft, 3);
}

void FxOrderList::mouseDown (const MouseEvent& e)
{
    for (int i = 0; i < params::kNumFx; ++i)
        if (rowBounds (i).contains (e.getPosition())) { dragFrom = i; dropAt = i; break; }
    repaint();
}

void FxOrderList::mouseDrag (const MouseEvent& e)
{
    if (dragFrom < 0) return;
    setMouseCursor (MouseCursor::DraggingHandCursor);
    dropAt = rowAt (e.y);
    repaint();
}

void FxOrderList::mouseUp (const MouseEvent&)
{
    setMouseCursor (MouseCursor::NormalCursor);
    if (dragFrom >= 0 && dropAt >= 0 && dropAt != dragFrom)
    {
        auto order = ctx.proc.getFxOrder();
        std::vector<int> v (order.begin(), order.end());
        const int moved = v[(size_t) dragFrom];
        v.erase (v.begin() + dragFrom);
        v.insert (v.begin() + dropAt, moved);
        for (int i = 0; i < params::kNumFx; ++i) order[(size_t) i] = v[(size_t) i];
        ctx.proc.setFxOrder (order);
    }
    dragFrom = dropAt = -1;
    repaint();
}

//==============================================================================
FxPage::FxPanel::FxPanel (UIContext& c, const String& t, const String& powerId, const String& b)
    : Panel (t), blurb (b), ctx (c)
{
    power.attach (c.proc, powerId);
    addAndMakeVisible (power.button);
}

ModKnob& FxPage::FxPanel::knob (const String& id, const String& label)
{
    auto* k = knobs.add (new ModKnob (ctx, id, label));
    addAndMakeVisible (k);
    return *k;
}

ParamCombo& FxPage::FxPanel::combo (const String& id)
{
    auto* cb = combos.add (new ParamCombo());
    cb->attach (ctx.proc, id);
    addAndMakeVisible (cb->box);
    return *cb;
}

ParamToggle& FxPage::FxPanel::toggle (const String& id, const String& text)
{
    auto* t = toggles.add (new ParamToggle());
    t->attach (ctx.proc, id, text);
    addAndMakeVisible (t->button);
    return *t;
}

void FxPage::FxPanel::paint (Graphics& g)
{
    Panel::paint (g);
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (11.5f));
    g.drawFittedText (blurb, getLocalBounds().reduced (14, 0).removeFromBottom (54), Justification::centredLeft, 3);
}

void FxPage::FxPanel::resized()
{
    power.button.setBounds (8, 6, 20, 20);
    int x = getWidth() - 8;
    for (int i = toggles.size(); --i >= 0;)
    {
        x -= 64;
        toggles[i]->button.setBounds (x, 5, 60, 24);
    }
    for (int i = combos.size(); --i >= 0;)
    {
        x -= 104;
        combos[i]->box.setBounds (x, 5, 100, 24);
    }

    const int n = knobs.size();
    const int kw = jmin (90, (getWidth() - 20) / jmax (1, n));
    const int total = kw * n;
    for (int i = 0; i < n; ++i)
        knobs[i]->setBounds ((getWidth() - total) / 2 + i * kw, 64, kw, 92);
}

FxPage::FxPage (UIContext& c) : orderList (c)
{
    addAndMakeVisible (orderList);

    auto* d = panels.add (new FxPanel (c, "DISTORTION", "dist_on", "Soft and hard clipping, wave folding, bit reduction and sample-rate reduction."));
    d->combo ("dist_type"); d->knob ("dist_drive", "DRIVE"); d->knob ("dist_mix", "MIX");

    auto* ch = panels.add (new FxPanel (c, "CHORUS", "cho_on", "Thickens and widens with modulated short delays."));
    ch->knob ("cho_rate", "RATE"); ch->knob ("cho_depth", "DEPTH"); ch->knob ("cho_mix", "MIX");

    auto* ph = panels.add (new FxPanel (c, "PHASER", "pha_on", "Sweeping notches for movement. Negative feedback gives a hollow tone."));
    ph->knob ("pha_rate", "RATE"); ph->knob ("pha_depth", "DEPTH"); ph->knob ("pha_fb", "FEEDBACK"); ph->knob ("pha_mix", "MIX");

    auto* dl = panels.add (new FxPanel (c, "DELAY", "dly_on", "Tempo-synced or free echo with ping-pong stereo and a tone control in the feedback path."));
    dl->combo ("dly_div"); dl->toggle ("dly_sync", "SYNC"); dl->toggle ("dly_pp", "PING");
    dl->knob ("dly_time", "TIME"); dl->knob ("dly_fb", "FEEDBACK"); dl->knob ("dly_tone", "TONE"); dl->knob ("dly_mix", "MIX");

    auto* rv = panels.add (new FxPanel (c, "REVERB", "rev_on", "Room to hall-sized space. Damping darkens the tail."));
    rv->knob ("rev_size", "SIZE"); rv->knob ("rev_damp", "DAMP"); rv->knob ("rev_width", "WIDTH"); rv->knob ("rev_mix", "MIX");

    for (auto* p : panels) addAndMakeVisible (p);
}

void FxPage::resized()
{
    orderList.setBounds (10, 8, 220, getHeight() - 16);
    const int x0 = 240, w = getWidth() - x0 - 10;
    const int h = (getHeight() - 24) / 2;
    const int w3 = (w - 16) / 3;
    for (int i = 0; i < 3; ++i)
        panels[i]->setBounds (x0 + i * (w3 + 8), 8, w3, h);
    const int w2 = (w - 8) / 2;
    panels[3]->setBounds (x0, 16 + h, w2, h);
    panels[4]->setBounds (x0 + w2 + 8, 16 + h, w2, h);
}
}
