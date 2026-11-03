#pragma once
#include "Common.h"
#include "Parameters.h"
#include "Wavetable.h"
#include "Engine.h"
#include "Effects.h"

namespace strata
{
class PresetManager;

struct ModInfo
{
    int src = 0, dst = 0;
    float amount = 0.0f;
};

/** User-interface preferences that are not part of a preset (knob shapes, colours, tutorial). */
class UISettings
{
public:
    UISettings();
    juce::PropertiesFile& file() { return *props; }

    int getKnobStyle();
    void setKnobStyle (int style);
    int getKnobOverride (const juce::String& paramID);     // -1 = use global
    void setKnobOverride (const juce::String& paramID, int style);
    int getAccent();
    void setAccent (int accent);
    bool tutorialSeen();
    void setTutorialSeen (bool);

private:
    std::unique_ptr<juce::PropertiesFile> props;
};

class StrataProcessor : public juce::AudioProcessor,
                        private juce::ValueTree::Listener,
                        private juce::AudioProcessorValueTreeState::Listener
{
public:
    StrataProcessor();
    ~StrataProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Strata"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return getPresetName(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- modulation matrix (message thread) -----------------------------------
    int addModulation (int src, int dst, float amount);
    void setModulation (int index, int src, int dst, float amount);
    void removeModulation (int index);
    void clearModulations();
    std::vector<ModInfo> getModulations() const;

    // ---- LFO curves (custom shape) --------------------------------------------
    std::vector<juce::Point<float>> getLfoCurve (int lfo) const;
    void setLfoCurve (int lfo, const std::vector<juce::Point<float>>& points);
    static std::vector<juce::Point<float>> defaultCurve();
    static juce::String curveToString (const std::vector<juce::Point<float>>&);
    static std::vector<juce::Point<float>> curveFromString (const juce::String&);

    // ---- FX order ---------------------------------------------------------------
    std::array<int, params::kNumFx> getFxOrder() const;
    void setFxOrder (const std::array<int, params::kNumFx>& order);

    // ---- wavetables -------------------------------------------------------------
    bool loadUserWavetable (int osc, const juce::File& file, juce::String& error);
    const Wavetable* getOscTable (int osc) const { return engine.tables[(size_t) osc].load(); }

    // ---- presets ----------------------------------------------------------------
    void loadStateTree (const juce::ValueTree& tree);
    juce::ValueTree createDefaultState();
    juce::String getPresetName() const;
    juce::String getPresetCategory() const;
    void setPresetMeta (const juce::String& name, const juce::String& category);
    PresetManager& getPresetManager() { return *presetManager; }

    /** Bumped whenever mods, curves, FX order or user tables change (UI polls this). */
    std::atomic<int> stateVersion { 0 };

    juce::AudioProcessorValueTreeState apvts;
    Engine engine;
    FxChain fx;
    juce::MidiKeyboardState keyboardState;
    UISettings uiSettings;

private:
    juce::ValueTree child (const juce::Identifier& type);
    void ensureStateChildren();
    void syncAll();
    void syncMods();
    void syncCurves();
    void syncFxOrder();
    void syncUserTables();
    void updateTablePointer (int osc);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleTreeChange (juce::ValueTree& tree);

    juce::SharedResourcePointer<WavetableLibrary> library;
    std::vector<std::shared_ptr<Wavetable>> keepAlive;
    std::array<std::atomic<Wavetable*>, 2> userTables {};
    std::array<juce::String, 2> userTableSignature;
    std::unique_ptr<PresetManager> presetManager;
    juce::SmoothedValue<float> masterGain;
    std::atomic<float>* masterParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StrataProcessor)
};
}
