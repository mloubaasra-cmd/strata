#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace strata
{
using namespace juce;

namespace tree
{
    static const Identifier root { "STRATA" };
    static const Identifier mods { "MODS" };
    static const Identifier slot { "SLOT" };
    static const Identifier curves { "LFOCURVES" };
    static const Identifier lfo { "LFO" };
    static const Identifier fxOrder { "FXORDER" };
    static const Identifier userWt { "USERWT" };
    static const Identifier osc { "OSC" };
    static const Identifier meta { "META" };
}

//==============================================================================
UISettings::UISettings()
{
    PropertiesFile::Options o;
    o.applicationName = "Strata";
    o.filenameSuffix = ".settings";
    o.folderName = "Strata";
    o.osxLibrarySubFolder = "Application Support";
    o.commonToAllUsers = false;
    props = std::make_unique<PropertiesFile> (o);
}

int UISettings::getKnobStyle()             { return props->getIntValue ("knobStyle", 0); }
void UISettings::setKnobStyle (int s)      { props->setValue ("knobStyle", s); props->saveIfNeeded(); }
int UISettings::getAccent()                { return props->getIntValue ("accent", 0); }
void UISettings::setAccent (int a)         { props->setValue ("accent", a); props->saveIfNeeded(); }
bool UISettings::tutorialSeen()            { return props->getBoolValue ("tutorialSeen", false); }
void UISettings::setTutorialSeen (bool b)  { props->setValue ("tutorialSeen", b); props->saveIfNeeded(); }

int UISettings::getKnobOverride (const String& id)
{
    return props->getIntValue ("knob_" + id, -1);
}

void UISettings::setKnobOverride (const String& id, int style)
{
    if (style < 0) props->removeValue ("knob_" + id);
    else props->setValue ("knob_" + id, style);
    props->saveIfNeeded();
}

//==============================================================================
StrataProcessor::StrataProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, tree::root, params::createLayout()),
      engine (apvts),
      fx (apvts)
{
    ensureStateChildren();
    apvts.state.addListener (this);
    for (int o = 0; o < 2; ++o)
        apvts.addParameterListener (ids::osc (o, "bank"), this);

    masterParam = apvts.getRawParameterValue ("master");
    syncAll();

    presetManager = std::make_unique<PresetManager> (*this);
    presetManager->loadFactory (0);   // start on the "Init" patch
}

StrataProcessor::~StrataProcessor()
{
    apvts.state.removeListener (this);
    for (int o = 0; o < 2; ++o)
        apvts.removeParameterListener (ids::osc (o, "bank"), this);
}

bool StrataProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == AudioChannelSet::stereo() || out == AudioChannelSet::mono();
}

void StrataProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    fx.prepare (sampleRate, samplesPerBlock);
    masterGain.reset (sampleRate, 0.05);
    masterGain.setCurrentAndTargetValue (masterParam->load());
    keyboardState.reset();
}

void StrataProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    ScopedNoDenormals noDenormals;

    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;

    engine.render (buffer, midi, bpm);

    if (buffer.getNumChannels() >= 2)
        fx.process (buffer, bpm);

    masterGain.setTargetValue (masterParam->load());
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float g = masterGain.getNextValue();
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            auto* d = buffer.getWritePointer (c);
            // gentle safety clipper so nothing ever explodes the speakers
            d[i] = std::tanh (d[i] * g * 0.9f) / 0.9f;
        }
    }
}

AudioProcessorEditor* StrataProcessor::createEditor()
{
    return new StrataEditor (*this);
}

//==============================================================================
void StrataProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void StrataProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            loadStateTree (ValueTree::fromXml (*xml));
}

void StrataProcessor::loadStateTree (const ValueTree& t)
{
    apvts.replaceState (t.createCopy());
    ensureStateChildren();
    syncAll();
}

ValueTree StrataProcessor::createDefaultState()
{
    ValueTree s (tree::root);
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            ValueTree pt ("PARAM");
            pt.setProperty ("id", rp->getParameterID(), nullptr);
            pt.setProperty ("value", rp->convertFrom0to1 (rp->getDefaultValue()), nullptr);
            s.appendChild (pt, nullptr);
        }
    return s;
}

ValueTree StrataProcessor::child (const Identifier& type)
{
    return apvts.state.getOrCreateChildWithName (type, nullptr);
}

void StrataProcessor::ensureStateChildren()
{
    child (tree::mods);
    auto c = child (tree::curves);
    for (int l = 0; l < params::kNumLfos; ++l)
    {
        auto lt = c.getChildWithProperty ("index", l);
        if (! lt.isValid())
        {
            lt = ValueTree (tree::lfo);
            lt.setProperty ("index", l, nullptr);
            lt.setProperty ("points", curveToString (defaultCurve()), nullptr);
            c.appendChild (lt, nullptr);
        }
    }
    auto f = child (tree::fxOrder);
    if (! f.hasProperty ("order")) f.setProperty ("order", "0,1,2,3,4", nullptr);
    child (tree::userWt);
    auto m = child (tree::meta);
    if (! m.hasProperty ("name")) m.setProperty ("name", "Init", nullptr);
    if (! m.hasProperty ("category")) m.setProperty ("category", "Init", nullptr);
}

//==============================================================================
void StrataProcessor::syncAll()
{
    syncMods();
    syncCurves();
    syncFxOrder();
    syncUserTables();
    for (int o = 0; o < 2; ++o) updateTablePointer (o);
    ++stateVersion;
}

void StrataProcessor::syncMods()
{
    auto m = apvts.state.getChildWithName (tree::mods);
    int n = 0;
    for (auto s : m)
    {
        if (n >= mod::kMaxSlots) break;
        engine.slots[(size_t) n].src.store ((int) s.getProperty ("src", -1));
        engine.slots[(size_t) n].dst.store ((int) s.getProperty ("dst", -1));
        engine.slots[(size_t) n].amount.store ((float) s.getProperty ("amt", 0.0f));
        ++n;
    }
    engine.numSlots.store (n);
    ++stateVersion;
}

void StrataProcessor::syncCurves()
{
    auto c = apvts.state.getChildWithName (tree::curves);
    for (int l = 0; l < params::kNumLfos; ++l)
    {
        auto pts = curveFromString (c.getChildWithProperty ("index", l).getProperty ("points").toString());
        auto& table = engine.lfoCurves[(size_t) l];
        size_t seg = 0;
        for (int i = 0; i < Engine::kCurveSize; ++i)
        {
            const float x = (float) i / (float) (Engine::kCurveSize - 1);
            while (seg + 2 < pts.size() && pts[seg + 1].x < x) ++seg;
            const auto a = pts[seg], b = pts[jmin (seg + 1, pts.size() - 1)];
            const float t = b.x > a.x ? jlimit (0.0f, 1.0f, (x - a.x) / (b.x - a.x)) : 0.0f;
            table[(size_t) i].store (a.y + t * (b.y - a.y));
        }
    }
    ++stateVersion;
}

void StrataProcessor::syncFxOrder()
{
    auto s = apvts.state.getChildWithName (tree::fxOrder).getProperty ("order").toString();
    StringArray tokens;
    tokens.addTokens (s, ",", "");
    std::array<int, params::kNumFx> order { 0, 1, 2, 3, 4 };
    if (tokens.size() == params::kNumFx)
    {
        std::array<bool, params::kNumFx> seen {};
        bool ok = true;
        for (int i = 0; i < params::kNumFx; ++i)
        {
            const int v = tokens[i].getIntValue();
            if (! isPositiveAndBelow (v, params::kNumFx) || seen[(size_t) v]) { ok = false; break; }
            seen[(size_t) v] = true;
            order[(size_t) i] = v;
        }
        if (! ok) order = { 0, 1, 2, 3, 4 };
    }
    for (int i = 0; i < params::kNumFx; ++i)
        fx.order[(size_t) i].store (order[(size_t) i]);
    ++stateVersion;
}

void StrataProcessor::syncUserTables()
{
    auto u = apvts.state.getChildWithName (tree::userWt);
    for (int o = 0; o < 2; ++o)
    {
        auto ot = u.getChildWithProperty ("index", o);
        const String data = ot.getProperty ("data").toString();
        const String sig = ot.getProperty ("name").toString() + ":" + String (data.length()) + ":" + String (data.hashCode64());
        if (sig == userTableSignature[(size_t) o])
            continue;
        userTableSignature[(size_t) o] = sig;

        if (data.isEmpty())
        {
            userTables[(size_t) o].store (nullptr);
            continue;
        }
        if (auto wt = Wavetable::fromBase64 (data, (int) ot.getProperty ("frames", 0), ot.getProperty ("name").toString()))
        {
            keepAlive.push_back (wt);
            userTables[(size_t) o].store (wt.get());
        }
    }
    ++stateVersion;
}

void StrataProcessor::updateTablePointer (int o)
{
    const int bank = (int) apvts.getRawParameterValue (ids::osc (o, "bank"))->load();
    const int userIndex = params::bankChoices().size() - 1;
    const Wavetable* t = nullptr;
    if (bank == userIndex)
        t = userTables[(size_t) o].load();
    if (t == nullptr)
        t = library->getBank (bank == userIndex ? 0 : bank);
    engine.tables[(size_t) o].store (t, std::memory_order_release);
}

void StrataProcessor::parameterChanged (const String& id, float)
{
    // Can be called from the audio thread: only swaps pointers to tables that stay alive.
    for (int o = 0; o < 2; ++o)
        if (id == ids::osc (o, "bank")) updateTablePointer (o);
    ++stateVersion;
}

void StrataProcessor::handleTreeChange (ValueTree& t)
{
    if (! MessageManager::existsAndIsCurrentThread() && MessageManager::getInstanceWithoutCreating() != nullptr)
        return;   // tree edits only happen on the message thread

    for (auto node = t; node.isValid(); node = node.getParent())
    {
        const auto type = node.getType();
        if (type == tree::mods)    { syncMods(); return; }
        if (type == tree::curves)  { syncCurves(); return; }
        if (type == tree::fxOrder) { syncFxOrder(); return; }
        if (type == tree::userWt)  { syncUserTables(); for (int o = 0; o < 2; ++o) updateTablePointer (o); return; }
        if (type == tree::root)    return;
    }
}

void StrataProcessor::valueTreePropertyChanged (ValueTree& t, const Identifier&)  { handleTreeChange (t); }
void StrataProcessor::valueTreeChildAdded (ValueTree& parent, ValueTree&)          { handleTreeChange (parent); }
void StrataProcessor::valueTreeChildRemoved (ValueTree& parent, ValueTree&, int)   { handleTreeChange (parent); }
void StrataProcessor::valueTreeRedirected (ValueTree&)                              { ensureStateChildren(); syncAll(); }

//==============================================================================
int StrataProcessor::addModulation (int src, int dst, float amount)
{
    auto m = child (tree::mods);
    for (int i = 0; i < m.getNumChildren(); ++i)
    {
        auto s = m.getChild (i);
        if ((int) s.getProperty ("src") == src && (int) s.getProperty ("dst") == dst)
        {
            s.setProperty ("amt", amount, nullptr);
            return i;
        }
    }
    if (m.getNumChildren() >= mod::kMaxSlots)
        return -1;

    ValueTree s (tree::slot);
    s.setProperty ("src", src, nullptr);
    s.setProperty ("dst", dst, nullptr);
    s.setProperty ("amt", amount, nullptr);
    m.appendChild (s, nullptr);
    return m.getNumChildren() - 1;
}

void StrataProcessor::setModulation (int index, int src, int dst, float amount)
{
    auto s = child (tree::mods).getChild (index);
    if (! s.isValid()) return;
    s.setProperty ("src", src, nullptr);
    s.setProperty ("dst", dst, nullptr);
    s.setProperty ("amt", jlimit (-1.0f, 1.0f, amount), nullptr);
}

void StrataProcessor::removeModulation (int index)
{
    auto m = child (tree::mods);
    if (isPositiveAndBelow (index, m.getNumChildren()))
        m.removeChild (index, nullptr);
}

void StrataProcessor::clearModulations()
{
    child (tree::mods).removeAllChildren (nullptr);
}

std::vector<ModInfo> StrataProcessor::getModulations() const
{
    std::vector<ModInfo> r;
    for (auto s : apvts.state.getChildWithName (tree::mods))
        r.push_back ({ (int) s.getProperty ("src"), (int) s.getProperty ("dst"), (float) s.getProperty ("amt") });
    return r;
}

//==============================================================================
std::vector<Point<float>> StrataProcessor::defaultCurve()
{
    return { { 0.0f, 0.0f }, { 0.25f, 1.0f }, { 0.5f, 0.35f }, { 0.75f, 0.8f }, { 1.0f, 0.0f } };
}

String StrataProcessor::curveToString (const std::vector<Point<float>>& pts)
{
    StringArray parts;
    for (auto& p : pts)
        parts.add (String (p.x, 4) + "," + String (p.y, 4));
    return parts.joinIntoString (";");
}

std::vector<Point<float>> StrataProcessor::curveFromString (const String& s)
{
    std::vector<Point<float>> pts;
    StringArray parts;
    parts.addTokens (s, ";", "");
    for (auto& part : parts)
    {
        StringArray xy;
        xy.addTokens (part, ",", "");
        if (xy.size() == 2)
            pts.push_back ({ jlimit (0.0f, 1.0f, xy[0].getFloatValue()), jlimit (0.0f, 1.0f, xy[1].getFloatValue()) });
    }
    std::sort (pts.begin(), pts.end(), [] (auto a, auto b) { return a.x < b.x; });
    if (pts.size() < 2) pts = defaultCurve();
    pts.front().x = 0.0f;
    pts.back().x = 1.0f;
    return pts;
}

std::vector<Point<float>> StrataProcessor::getLfoCurve (int l) const
{
    return curveFromString (apvts.state.getChildWithName (tree::curves)
                                .getChildWithProperty ("index", l).getProperty ("points").toString());
}

void StrataProcessor::setLfoCurve (int l, const std::vector<Point<float>>& pts)
{
    auto c = child (tree::curves).getChildWithProperty ("index", l);
    if (c.isValid())
        c.setProperty ("points", curveToString (pts), nullptr);
}

//==============================================================================
std::array<int, params::kNumFx> StrataProcessor::getFxOrder() const
{
    std::array<int, params::kNumFx> o {};
    for (int i = 0; i < params::kNumFx; ++i)
        o[(size_t) i] = fx.order[(size_t) i].load();
    return o;
}

void StrataProcessor::setFxOrder (const std::array<int, params::kNumFx>& order)
{
    StringArray s;
    for (auto v : order) s.add (String (v));
    child (tree::fxOrder).setProperty ("order", s.joinIntoString (","), nullptr);
}

//==============================================================================
bool StrataProcessor::loadUserWavetable (int o, const File& file, String& error)
{
    auto wt = WavetableLibrary::loadFromFile (file, error);
    if (wt == nullptr) return false;

    auto u = child (tree::userWt);
    auto ot = u.getChildWithProperty ("index", o);
    if (! ot.isValid())
    {
        ot = ValueTree (tree::osc);
        ot.setProperty ("index", o, nullptr);
        u.appendChild (ot, nullptr);
    }
    ot.setProperty ("name", wt->name, nullptr);
    ot.setProperty ("frames", wt->numFrames, nullptr);
    ot.setProperty ("data", wt->toBase64(), nullptr);   // triggers syncUserTables

    if (auto* p = apvts.getParameter (ids::osc (o, "bank")))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (params::bankChoices().size() - 1)));
    updateTablePointer (o);
    return true;
}

//==============================================================================
String StrataProcessor::getPresetName() const
{
    return apvts.state.getChildWithName (tree::meta).getProperty ("name", "Init").toString();
}

String StrataProcessor::getPresetCategory() const
{
    return apvts.state.getChildWithName (tree::meta).getProperty ("category", "Init").toString();
}

void StrataProcessor::setPresetMeta (const String& name, const String& category)
{
    auto m = child (tree::meta);
    m.setProperty ("name", name, nullptr);
    m.setProperty ("category", category, nullptr);
    ++stateVersion;
}
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new strata::StrataProcessor();
}
