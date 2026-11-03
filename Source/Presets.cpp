#include "Presets.h"
#include "PluginProcessor.h"

namespace strata
{
using namespace juce;

PresetManager::PresetManager (StrataProcessor& p) : proc (p)
{
    refresh();
}

File PresetManager::getUserPresetFolder()
{
    auto dir = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("Strata").getChildFile ("User Presets");
    if (! dir.exists()) dir.createDirectory();
    return dir;
}

void PresetManager::refresh()
{
    const auto currentName = proc.getPresetName();
    all.clear();

    const auto& factory = getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        all.push_back ({ factory[(size_t) i].name, factory[(size_t) i].category, true, i, {} });

    auto files = getUserPresetFolder().findChildFiles (File::findFiles, false, String ("*") + extension);
    files.sort();
    for (auto& f : files)
    {
        String category = "User";
        if (auto xml = XmlDocument::parse (f))
            if (auto* meta = xml->getChildByName ("META"))
                category = meta->getStringAttribute ("category", "User");
        if (category.isEmpty()) category = "User";
        all.push_back ({ f.getFileNameWithoutExtension(), category, false, -1, f });
    }

    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].name == currentName) { current = i; break; }
}

StringArray PresetManager::getCategories() const
{
    StringArray c = getFactoryCategories();
    for (auto& p : all)
        if (! p.isFactory) c.addIfNotAlreadyThere (p.category);
    c.addIfNotAlreadyThere ("User");
    return c;
}

ValueTree PresetManager::buildFactoryState (int index)
{
    const auto& preset = getFactoryPresets()[(size_t) index];
    PresetBuilder b;
    if (preset.build) preset.build (b);

    auto state = proc.createDefaultState();
    for (auto& [id, value] : b.values)
    {
        auto pt = state.getChildWithProperty ("id", id);
        jassert (pt.isValid());   // unknown parameter id in a factory preset
        if (pt.isValid()) pt.setProperty ("value", value, nullptr);
    }

    ValueTree mods ("MODS");
    for (auto& m : b.mods)
    {
        const int d = mod::destIndexForParam (m.dst);
        jassert (d >= 0);
        if (d < 0 || mods.getNumChildren() >= mod::kMaxSlots) continue;
        ValueTree s ("SLOT");
        s.setProperty ("src", m.src, nullptr);
        s.setProperty ("dst", d, nullptr);
        s.setProperty ("amt", m.amount, nullptr);
        mods.appendChild (s, nullptr);
    }
    state.appendChild (mods, nullptr);

    ValueTree curves ("LFOCURVES");
    for (int l = 0; l < params::kNumLfos; ++l)
    {
        ValueTree lt ("LFO");
        lt.setProperty ("index", l, nullptr);
        auto it = b.curves.find (l);
        lt.setProperty ("points", StrataProcessor::curveToString (it != b.curves.end() ? it->second : StrataProcessor::defaultCurve()), nullptr);
        curves.appendChild (lt, nullptr);
    }
    state.appendChild (curves, nullptr);

    StringArray order;
    for (auto v : b.fxOrder) order.add (String (v));
    ValueTree fx ("FXORDER");
    fx.setProperty ("order", order.joinIntoString (","), nullptr);
    state.appendChild (fx, nullptr);

    state.appendChild (ValueTree ("USERWT"), nullptr);

    ValueTree meta ("META");
    meta.setProperty ("name", preset.name, nullptr);
    meta.setProperty ("category", preset.category, nullptr);
    state.appendChild (meta, nullptr);
    return state;
}

void PresetManager::loadFactory (int factoryIndex)
{
    if (! isPositiveAndBelow (factoryIndex, (int) getFactoryPresets().size())) return;
    proc.loadStateTree (buildFactoryState (factoryIndex));
    for (int i = 0; i < (int) all.size(); ++i)
        if (all[(size_t) i].isFactory && all[(size_t) i].factoryIndex == factoryIndex) { current = i; break; }
}

void PresetManager::load (int index)
{
    if (! isPositiveAndBelow (index, (int) all.size())) return;
    const auto& info = all[(size_t) index];
    if (info.isFactory)
    {
        loadFactory (info.factoryIndex);
    }
    else if (auto xml = XmlDocument::parse (info.file))
    {
        auto t = ValueTree::fromXml (*xml);
        if (t.isValid())
        {
            proc.loadStateTree (t);
            proc.setPresetMeta (info.name, info.category);
        }
    }
    current = index;
}

void PresetManager::next()     { if (! all.empty()) load ((current + 1) % (int) all.size()); }
void PresetManager::previous() { if (! all.empty()) load ((current - 1 + (int) all.size()) % (int) all.size()); }

bool PresetManager::saveUserPreset (const String& rawName, const String& category)
{
    const auto name = File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;

    proc.setPresetMeta (name, category.isNotEmpty() ? category : String ("User"));
    auto state = proc.apvts.copyState();
    auto xml = state.createXml();
    if (xml == nullptr) return false;

    const auto file = getUserPresetFolder().getChildFile (name + extension);
    if (! xml->writeTo (file)) return false;

    refresh();
    for (int i = 0; i < (int) all.size(); ++i)
        if (! all[(size_t) i].isFactory && all[(size_t) i].file == file) { current = i; break; }
    return true;
}

bool PresetManager::deletePreset (int index)
{
    if (! isPositiveAndBelow (index, (int) all.size()) || all[(size_t) index].isFactory) return false;
    const bool ok = all[(size_t) index].file.moveToTrash() || all[(size_t) index].file.deleteFile();
    refresh();
    return ok;
}
}
