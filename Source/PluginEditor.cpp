#include "PluginEditor.h"
#include "Presets.h"

namespace strata
{
using namespace juce;

MainContent::MainContent (StrataProcessor& p, StrataLook& look)
    : ctx { p, look, {}, {} },
      presetBar (ctx),
      synth (ctx), matrix (ctx), fxPage (ctx),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard),
      browser (ctx), tutorial (ctx)
{
    look.globalKnobStyle = jlimit (0, StrataLook::kNumKnobStyles - 1, p.uiSettings.getKnobStyle());
    look.setTheme (p.uiSettings.getAccent());
    setLookAndFeel (&look);

    for (auto* t : { &synthTab, &matrixTab, &fxTab })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (1001);
        addAndMakeVisible (t);
    }
    synthTab.onClick = [this] { setTab (0); };
    matrixTab.onClick = [this] { setTab (1); };
    fxTab.onClick = [this] { setTab (2); };

    const auto styles = StrataLook::knobStyleNames();
    for (int i = 0; i < styles.size(); ++i)
        knobStyleBox.addItem ("Knobs: " + styles[i], i + 1);
    knobStyleBox.setSelectedId (look.globalKnobStyle + 1, dontSendNotification);
    knobStyleBox.setTooltip ("Knob shape for the whole synth. Right-click a knob to override just that one.");
    knobStyleBox.onChange = [this]
    {
        ctx.look.globalKnobStyle = knobStyleBox.getSelectedId() - 1;
        ctx.proc.uiSettings.setKnobStyle (ctx.look.globalKnobStyle);
        knobStylesChanged();
    };

    const auto themeNames = StrataLook::themeNames();
    for (int i = 0; i < themeNames.size(); ++i)
        accentBox.addItem (themeNames[i], i + 1);
    accentBox.setSelectedId (look.getTheme() + 1, dontSendNotification);
    accentBox.setTooltip ("Colour theme");
    accentBox.onChange = [this] { setTheme (accentBox.getSelectedId() - 1); };

    tourButton.setTooltip ("Interactive tour of the synth");
    tourButton.onClick = [this] { startTutorial(); };

    presetBar.onOpenBrowser = [this] { browser.open(); };
    presetBar.onSave = [this] { showSaveDialog(); };
    browser.onClose = [this] { browser.setVisible (false); };
    browser.onPresetLoaded = [this] { presetBar.update(); };

    keyboard.setAvailableRange (24, 108);
    keyboard.setKeyWidth (21.6f);
    keyboard.setLowestVisibleKey (24);
    keyboard.setScrollButtonsVisible (false);

    for (Component* comp : std::initializer_list<Component*> { &presetBar, &knobStyleBox, &accentBox, &tourButton,
                                                               &synth, &keyboard })
        addAndMakeVisible (comp);
    addChildComponent (matrix);
    addChildComponent (fxPage);
    addChildComponent (browser);
    addChildComponent (tutorial);

    tutorial.onFinished = [this] { ctx.proc.uiSettings.setTutorialSeen (true); };
    buildTutorial();

    setTab (0);
    setSize (kWidth, kHeight);
}

MainContent::~MainContent()
{
    setLookAndFeel (nullptr);
}

void MainContent::paint (Graphics& g)
{
    g.fillAll (colours::bg);
    g.setColour (colours::panel.darker (0.2f));
    g.fillRect (0, 0, getWidth(), 52);
    g.setColour (colours::outline);
    g.drawHorizontalLine (52, 0.0f, (float) getWidth());

    const auto accent = ctx.look.getAccent();
    g.setGradientFill (ColourGradient (accent, 16.0f, 0.0f, accent.withRotatedHue (0.12f).brighter (0.3f), 150.0f, 0.0f, false));
    g.setFont (StrataLook::font (24.0f, true));
    g.drawText ("STRATA", 16, 8, 140, 26, Justification::centredLeft);
    g.setColour (colours::textDim);
    g.setFont (StrataLook::font (9.5f, true));
    g.drawText ("WAVETABLE SYNTHESIZER", 17, 32, 160, 12, Justification::centredLeft);
}

void MainContent::resized()
{
    synthTab.setBounds (172, 12, 74, 28);
    matrixTab.setBounds (250, 12, 74, 28);
    fxTab.setBounds (328, 12, 60, 28);
    presetBar.setBounds (402, 12, 400, 28);
    knobStyleBox.setBounds (812, 12, 122, 28);
    accentBox.setBounds (940, 12, 96, 28);
    tourButton.setBounds (1042, 12, 50, 28);

    const Rectangle<int> page (0, 52, kWidth, 578);
    synth.setBounds (page);
    matrix.setBounds (page);
    fxPage.setBounds (page);
    keyboard.setBounds (10, 634, kWidth - 20, 70);
    browser.setBounds (0, 52, kWidth, kHeight - 52);
    tutorial.setBounds (getLocalBounds());
}

void MainContent::setTab (int tab)
{
    currentTab = tab;
    synth.setVisible (tab == 0);
    matrix.setVisible (tab == 1);
    fxPage.setVisible (tab == 2);
    synthTab.setToggleState (tab == 0, dontSendNotification);
    matrixTab.setToggleState (tab == 1, dontSendNotification);
    fxTab.setToggleState (tab == 2, dontSendNotification);
    if (tab == 1) matrix.refresh();
}

void MainContent::setTheme (int index)
{
    ctx.look.setTheme (index);
    ctx.proc.uiSettings.setAccent (ctx.look.getTheme());
    accentBox.setSelectedId (ctx.look.getTheme() + 1, dontSendNotification);
    if (auto* top = getTopLevelComponent()) { top->sendLookAndFeelChange(); top->repaint(); }
    sendLookAndFeelChange();
    repaint();
}

void MainContent::knobStylesChanged()
{
    for (auto* k : ctx.knobs) k->refreshStyle();
    repaint();
}

void MainContent::tick()
{
    const int v = ctx.proc.stateVersion.load();
    if (v != lastVersion)
    {
        lastVersion = v;
        for (auto* k : ctx.knobs) k->refreshMods();
        matrix.refresh();
        synth.lfos.editor.refreshCurve();
        fxPage.orderList.repaint();
    }

    const auto name = ctx.proc.getPresetName();
    if (name != lastPresetName)
    {
        lastPresetName = name;
        presetBar.update();
    }

    for (auto* k : ctx.knobs) k->tick();
    if (synth.isVisible()) synth.tick();
}

void MainContent::showSaveDialog()
{
    auto* w = new AlertWindow ("Save preset", "Name your sound and pick a category.", MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", ctx.proc.getPresetName(), "Name");
    auto cats = ctx.proc.getPresetManager().getCategories();
    cats.removeString ("Init");
    w->addComboBox ("category", cats, "Category");
    if (auto* cb = w->getComboBoxComponent ("category"))
        cb->setText (ctx.proc.getPresetCategory() == "Init" ? String ("User") : ctx.proc.getPresetCategory(), dontSendNotification);
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));

    addAndMakeVisible (w);
    w->centreAroundComponent (this, w->getWidth(), w->getHeight());
    w->setBounds (w->getBounds().withCentre (getLocalBounds().getCentre()));

    Component::SafePointer<MainContent> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (safe == nullptr || result != 1) return;
        const auto name = w->getTextEditorContents ("name");
        auto* cb = w->getComboBoxComponent ("category");
        const auto category = cb != nullptr ? cb->getText() : String ("User");
        if (! safe->ctx.proc.getPresetManager().saveUserPreset (name, category))
            AlertWindow::showMessageBoxAsync (MessageBoxIconType::WarningIcon, "Couldn't save", "Please choose a valid name.");
        safe->presetBar.update();
    }), true);
}

void MainContent::startTutorial()
{
    browser.setVisible (false);
    setTab (0);
    tutorial.start();
}

void MainContent::buildTutorial()
{
    auto& proc = ctx.proc;
    auto startName = std::make_shared<String>();
    auto startValue = std::make_shared<float> (0.0f);
    auto startOrder = std::make_shared<std::array<int, params::kNumFx>>();
    const int cutoffDest = mod::destIndexForParam ("flt_cutoff");

    auto paramValue = [&proc] (const String& id) { return proc.apvts.getParameter (id)->getValue(); };

    std::vector<TutorialStep> s;

    s.push_back ({ "Welcome to Strata",
                   "A quick hands-on tour of the essentials. The highlighted controls stay live, so you can try everything as you go.\n\n"
                   "Play notes with the on-screen keyboard at the bottom, your MIDI keyboard, or your computer keys (A, W, S, E, D...).",
                   {}, {}, {}, {} });

    s.push_back ({ "Presets",
                   "There are " + String ((int) getFactoryPresets().size()) + " factory presets. Click the name to open the browser "
                   "(categories, search, click to audition) or step through them with the arrows.",
                   "load a different preset",
                   [this] { std::vector<Component*> v { &presetBar }; if (browser.isVisible()) v.push_back (&browser); return v; },
                   [this, startName] { setTab (0); *startName = ctx.proc.getPresetName(); },
                   [this, startName] { return ctx.proc.getPresetName() != *startName; } });

    s.push_back ({ "Wavetables",
                   "Each oscillator plays a wavetable: a stack of single-cycle waveforms (frames). Scanning through the frames "
                   "is what gives wavetable synths their movement.",
                   "drag across the display or turn WT POS",
                   [this] { return std::vector<Component*> { &synth.oscA.display, &synth.oscA.pos }; },
                   [this, startValue, paramValue] { browser.setVisible (false); setTab (0); *startValue = paramValue (ids::osc (0, "pos")); },
                   [startValue, paramValue] { return std::abs (paramValue (ids::osc (0, "pos")) - *startValue) > 0.05f; } });

    s.push_back ({ "Load your own",
                   "Pick one of 12 built-in banks here, click LOAD, or simply drop a .wav onto the display. Strata understands "
                   "single cycles, 2048-sample frame tables and even ordinary samples (it slices them into frames). "
                   "The WARP menu bends how the table is read: sync, bend, PWM, mirror, fold and crush.",
                   {},
                   [this] { return std::vector<Component*> { &synth.oscA.bank.box, &synth.oscA.warpMode.box, &synth.oscA.loadButton }; },
                   {}, {} });

    s.push_back ({ "Unison",
                   "Unison stacks up to 8 detuned copies of the oscillator. DETUNE spreads their pitch, BLEND balances the "
                   "outer voices against the centre, WIDTH spreads them across the stereo field.",
                   "set UNISON to 3 or more and play a chord",
                   [this] { return std::vector<Component*> { &synth.oscA.uni, &synth.oscA.detune, &synth.oscA.blend, &synth.oscA.width }; },
                   {},
                   [&proc] { return proc.apvts.getRawParameterValue (ids::osc (0, "uni"))->load() >= 3.0f; } });

    s.push_back ({ "Filter",
                   "The filter shapes the brightness. The graph shows the response. ENV 2 sets how much Envelope 2 opens "
                   "the filter on each note; KEY makes higher notes brighter.",
                   "sweep the CUTOFF knob",
                   [this] { return std::vector<Component*> { &synth.filter }; },
                   [startValue, paramValue] { *startValue = paramValue ("flt_cutoff"); },
                   [startValue, paramValue] { return std::abs (paramValue ("flt_cutoff") - *startValue) > 0.05f; } });

    s.push_back ({ "Knob shapes",
                   "Pick the knob style you like for the whole synth: arc, vintage, hex, chicken head, minimal, neon or square. "
                   "Right-click any knob to give just that one its own shape. The menu next to it switches between 11 colour themes.",
                   "choose another knob shape",
                   [this] { return std::vector<Component*> { &knobStyleBox, &accentBox }; },
                   [startValue, &proc] { *startValue = (float) proc.uiSettings.getKnobStyle(); },
                   [startValue, &proc] { return (float) proc.uiSettings.getKnobStyle() != *startValue; } });

    s.push_back ({ "Drag-and-drop modulation",
                   "Every ENV, LFO, macro and source chip can be dragged onto any knob with a ring. "
                   "That's how you make sounds move.",
                   "drag the LFO 1 chip onto the CUTOFF knob",
                   [this] { return std::vector<Component*> { synth.lfos.chips[0], &synth.filter.cutoff }; },
                   [this] { setTab (0); synth.lfos.select (0); },
                   [&proc, cutoffDest]
                   {
                       for (auto& m : proc.getModulations())
                           if (m.src == mod::srcLfo1 && m.dst == cutoffDest) return true;
                       return false;
                   } });

    s.push_back ({ "Modulation depth",
                   "The orange ring shows how far the source moves the knob, and the white dot shows where it is right now "
                   "while a note plays. Hold Alt and drag the knob to change the depth. Right-click for amount presets, "
                   "invert or remove.",
                   {},
                   [this] { return std::vector<Component*> { &synth.filter.cutoff }; },
                   {}, {} });

    s.push_back ({ "Draw your own LFO",
                   "Pick a shape from the menu, or click the graph to turn it into a custom shape you can draw: drag the "
                   "points, double-click to add or remove them, hold Shift to snap. SYNC locks the LFO to your song's tempo.",
                   "click the LFO graph",
                   [this] { return std::vector<Component*> { &synth.lfos.editor }; },
                   {},
                   [&proc] { return (int) proc.apvts.getRawParameterValue (ids::lfo (0, "shape"))->load() == params::lfoCustom; } });

    s.push_back ({ "Envelopes",
                   "ENV 1 controls volume, ENV 2 drives the filter, ENV 3 is free to modulate anything. "
                   "Drag the handles to set attack, decay, sustain and release, or use the knobs below.",
                   {},
                   [this] { return std::vector<Component*> { &synth.envs.editor }; },
                   {}, {} });

    s.push_back ({ "The matrix",
                   "Every modulation connection is listed here. Change the source, destination or amount precisely, "
                   "or add up to 16 routings without dragging.",
                   {},
                   [this] { return std::vector<Component*> { &matrix }; },
                   [this] { setTab (1); }, {} });

    s.push_back ({ "Effects",
                   "Five effects with their own power buttons. The list on the left is the signal flow: "
                   "drag an effect up or down to change the order (distortion after reverb sounds very different!).",
                   "drag an effect to a new position",
                   [this] { return std::vector<Component*> { &fxPage.orderList }; },
                   [this, startOrder] { setTab (2); *startOrder = ctx.proc.getFxOrder(); },
                   [this, startOrder] { return ctx.proc.getFxOrder() != *startOrder; } });

    s.push_back ({ "Save your sound",
                   "Happy with it? SAVE stores it as a user preset in Documents/Strata/User Presets, under the category you "
                   "choose. Replay this tour any time with the ? TOUR button. Have fun!",
                   {},
                   [this] { return std::vector<Component*> { &presetBar.save, &tourButton }; },
                   [this] { setTab (0); }, {} });

    tutorial.setSteps (std::move (s));
}

//==============================================================================
StrataEditor::StrataEditor (StrataProcessor& p)
    : AudioProcessorEditor (p), proc (p), content (p, look)
{
    look.globalKnobStyle = jlimit (0, StrataLook::kNumKnobStyles - 1, p.uiSettings.getKnobStyle());
    look.setTheme (p.uiSettings.getAccent());
    setLookAndFeel (&look);
    addAndMakeVisible (content);

    setResizable (true, true);
    setResizeLimits (MainContent::kWidth * 7 / 10, MainContent::kHeight * 7 / 10,
                     MainContent::kWidth * 3 / 2, MainContent::kHeight * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) MainContent::kWidth / (double) MainContent::kHeight);
    setSize (MainContent::kWidth, MainContent::kHeight);
    startTimerHz (30);

    if (! p.uiSettings.tutorialSeen())
    {
        Component::SafePointer<StrataEditor> safe (this);
        Timer::callAfterDelay (600, [safe] { if (safe != nullptr) safe->content.startTutorial(); });
    }
}

StrataEditor::~StrataEditor()
{
    setLookAndFeel (nullptr);
}

void StrataEditor::paint (Graphics& g)
{
    g.fillAll (colours::bg);
}

void StrataEditor::resized()
{
    const float scale = (float) getWidth() / (float) MainContent::kWidth;
    content.setBounds (0, 0, MainContent::kWidth, MainContent::kHeight);
    content.setTransform (AffineTransform::scale (scale));
}

void StrataEditor::timerCallback()
{
    content.tick();
}
}
