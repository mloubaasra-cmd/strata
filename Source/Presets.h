#pragma once
#include "Common.h"
#include "Parameters.h"

namespace strata
{
class StrataProcessor;

/** Collects parameter values, modulations, curves and FX order for a factory patch. */
struct PresetBuilder
{
    std::map<juce::String, float> values;
    struct Mod { int src; juce::String dst; float amount; };
    std::vector<Mod> mods;
    std::map<int, std::vector<juce::Point<float>>> curves;
    std::array<int, params::kNumFx> fxOrder { 0, 1, 2, 3, 4 };

    PresetBuilder& set (const juce::String& id, float value) { values[id] = value; return *this; }
    PresetBuilder& modulate (int src, const juce::String& dstParam, float amount) { mods.push_back ({ src, dstParam, amount }); return *this; }
    PresetBuilder& curve (int lfo, std::vector<juce::Point<float>> pts) { curves[lfo] = std::move (pts); return *this; }
};

struct FactoryPreset
{
    juce::String name, category;
    std::function<void (PresetBuilder&)> build;
};

/** All factory presets (defined in FactoryPresets.cpp). */
const std::vector<FactoryPreset>& getFactoryPresets();
juce::StringArray getFactoryCategories();

struct PresetInfo
{
    juce::String name, category;
    bool isFactory = true;
    int factoryIndex = -1;
    juce::File file;
};

class PresetManager
{
public:
    explicit PresetManager (StrataProcessor&);

    static juce::File getUserPresetFolder();
    static constexpr const char* extension = ".strata";

    void refresh();
    const std::vector<PresetInfo>& getAll() const { return all; }
    juce::StringArray getCategories() const;

    void load (int index);
    void loadFactory (int factoryIndex);
    void next();
    void previous();
    int getCurrentIndex() const { return current; }

    bool saveUserPreset (const juce::String& name, const juce::String& category);
    bool deletePreset (int index);

    juce::ValueTree buildFactoryState (int factoryIndex);

private:
    StrataProcessor& proc;
    std::vector<PresetInfo> all;
    int current = 0;
};
}
