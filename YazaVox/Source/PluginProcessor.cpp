#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace yv;

juce::AudioProcessorValueTreeState::ParameterLayout YazaVoxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (int i = 0; i < Count; ++i)
    {
        const auto& d = kDefs[i];
        const juce::String name = juce::String (kModuleTitles[d.module]) + (d.group[0] ? " " + juce::String (d.group) : juce::String()) + " " + d.label;
        const juce::ParameterID pid { d.id, 1 };
        if (d.kind == KBool)
            l.add (std::make_unique<juce::AudioParameterBool> (pid, name, d.def > 0.5f));
        else if (d.kind == KChoice)
        {
            juce::StringArray choices; choices.addTokens (d.unit, "|", "");
            l.add (std::make_unique<juce::AudioParameterChoice> (pid, name, choices, (int) d.def));
        }
        else
            l.add (std::make_unique<juce::AudioParameterFloat> (pid, name, juce::NormalisableRange<float> (d.lo, d.hi, d.step, d.skew), d.def));
    }
    return l;
}

YazaVoxProcessor::YazaVoxProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput ("Prod (sidechain)", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < Count; ++i) raw[(size_t) i] = apvts.getRawParameterValue (kDefs[i].id);
    for (int i = 0; i < Count; ++i) cur[(size_t) i] = kDefs[i].def;
}

bool YazaVoxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (! (in == out && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono()))) return false;
    const auto sc = l.inputBuses.size() > 1 ? l.getChannelSet (true, 1) : juce::AudioChannelSet::disabled();      // entree sidechain optionnelle : la prod, pour AUTO VOIX
    return sc.isDisabled() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet::mono();
}

void YazaVoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sampleRate, samplesPerBlock);
    analyzer.prepare (sampleRate);
    scratch.setSize (2, std::max (samplesPerBlock, 16384), false, true, false);   // alloue ici, jamais dans le callback
    setLatencySamples (engine.latency());   // seule l'autotune introduit une latence fixe (le dry du bypass est retarde pareil)
}

void YazaVoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int ch = std::max (1, std::min (2, getMainBusNumInputChannels())), n = buffer.getNumSamples();
    if (ch < 1 || n < 1) return;

    for (int i = 0; i < Count; ++i) cur[(size_t) i] = raw[(size_t) i]->load();
    engine.setParams (cur.data());

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm()) bpm = *b;
    engine.setBpm (bpm);

    if (analyzer.listening())
    {
        const float* pl = nullptr; const float* pr = nullptr;
        if (auto* scBus = getBus (true, 1); scBus != nullptr && scBus->isEnabled() && buffer.getNumChannels() > ch)
        {
            auto sc = getBusBuffer (buffer, true, 1);
            if (sc.getNumChannels() > 0) { pl = sc.getReadPointer (0); pr = sc.getReadPointer (sc.getNumChannels() > 1 ? 1 : 0); }
        }
        analyzer.push (buffer.getReadPointer (0), buffer.getReadPointer (ch >= 2 ? 1 : 0), n, pl, pr);
    }
    if (analyzer.takeDone()) triggerAsyncUpdate();

    if (ch >= 2)
    {
        for (int start = 0; start < n;)
        {
            const int len = std::min (n - start, 16384);
            engine.process (buffer.getWritePointer (0) + start, buffer.getWritePointer (1) + start, len);
            start += len;
        }
    }
    else
    {
        auto* m = buffer.getWritePointer (0);
        auto* a = scratch.getWritePointer (0);
        auto* b = scratch.getWritePointer (1);
        const int cap = std::max (1, std::min (scratch.getNumSamples(), 16384));
        for (int start = 0; start < n;)
        {
            const int len = std::min (n - start, cap);
            juce::FloatVectorOperations::copy (a, m + start, len);
            juce::FloatVectorOperations::copy (b, m + start, len);
            engine.process (a, b, len);
            for (int i = 0; i < len; ++i) m[start + i] = 0.5f * (a[i] + b[i]);
            start += len;
        }
    }
}

//==============================================================================
const juce::StringArray& YazaVoxProcessor::presetNames()
{
    static const juce::StringArray n {
        "Init (neutre)",
        "Rap Lead Clean",
        "Rap Lead Aggressive",
        "Trap Vocal",
        "Drill Vocal",
        "Melodic Rap",
        "Autotune Ready",
        "Dark Vocal",
        "Bright Vocal",
        "Radio Vocal",
        "Vocal Presence",
        "Double Vocal",
        "Adlibs Wide",
        "Adlibs Telephone",
        "Background Vocal",
        "Vocal Bus",
        "Soft Compression",
        "Hard Compression",
        "Intimate Vocal",
        "Airy Vocal",
        "Professional Lead",
        "Hard Tune Vocal",
        "Natural Tune Vocal"
    };
    return n;
}

void YazaVoxProcessor::startAutoMix() { analyzer.start(); }
void YazaVoxProcessor::cancelAutoMix() { analyzer.cancel(); }

juce::String YazaVoxProcessor::takeAutoMessage()
{
    const juce::ScopedLock sl (autoLock);
    auto m = autoMsg; autoMsg = {};
    return m;
}

// thread interface : l'ecoute est terminee, on calcule et on applique
void YazaVoxProcessor::handleAsyncUpdate()
{
    const bool tuneWasOn = raw[(size_t) tuneOn]->load() > 0.5f;
    if (analyzer.state() == 3) { const juce::ScopedLock sl (autoLock); autoMsg = "Pas assez de voix"; return; }
    const auto res = computeAutoSettings (analyzer, tuneWasOn);
    if (res.ok)
    {
        // le mix repart d'une base neutre (sauf entree/sortie/limiteur et autotune), puis recoit les reglages mesures
        static const int keep[] = { bypass, inGain, outGain, limCeil, tuneOn, tuneKey, tuneScale, tuneSpeed, tuneAmt, tuneHuman, tuneFlex, tuneVib, tuneFormant };
        for (int i = 0; i < Count; ++i)
        {
            if (std::find (std::begin (keep), std::end (keep), i) != std::end (keep)) continue;
            if (auto* rp = apvts.getParameter (kDefs[i].id)) rp->setValueNotifyingHost (rp->getDefaultValue());
        }
        for (auto& kv : res.set)
            if (auto* rp = apvts.getParameter (kv.first)) rp->setValueNotifyingHost (rp->convertTo0to1 (kv.second));
        currentPreset = 0;
    }
    const juce::ScopedLock sl (autoLock);
    autoMsg = res.message;
}

void YazaVoxProcessor::resetAll()
{
    for (int i = 0; i < Count; ++i)
        if (auto* rp = apvts.getParameter (kDefs[i].id))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    currentPreset = 0;
}

void YazaVoxProcessor::applyPreset (int index)
{
    struct V { const char* id; float v; };
    static const std::vector<std::vector<V>> presets = {
        {},
        { { "compThr", -24.0f }, { "compRatio", 4.0f }, { "compAtk", 10.0f }, { "compRel", 100.0f }, { "compMakeup", 5.0f }, { "hpFreq", 90.0f }, { "lmG", -2.0f }, { "lmF", 350.0f }, { "hmG", 2.5f }, { "hmF", 3500.0f }, { "hsG", 3.0f }, { "deessAmt", 50.0f }, { "satDrive", 20.0f }, { "dlyMix", 10.0f }, { "dlyNote", 1.0f }, { "revMix", 8.0f }, { "revDecay", 1.2f }, { "revMode", 1.0f }, { "airOn", 1.0f }, { "airHigh", 40.0f } },
        { { "compThr", -28.0f }, { "compRatio", 6.0f }, { "compAtk", 4.0f }, { "compRel", 70.0f }, { "compMakeup", 8.0f }, { "comp2Amt", 60.0f }, { "hpFreq", 100.0f }, { "lsG", -1.0f }, { "lmG", -1.5f }, { "hmG", 4.0f }, { "hmF", 3200.0f }, { "hsG", 3.0f }, { "deessAmt", 55.0f }, { "satDrive", 50.0f }, { "dlyMix", 12.0f }, { "dlyNote", 2.0f }, { "revMix", 6.0f }, { "revDecay", 1.0f }, { "revMode", 0.0f } },
        { { "compThr", -26.0f }, { "compRatio", 5.0f }, { "compAtk", 6.0f }, { "compRel", 90.0f }, { "compMakeup", 7.0f }, { "hpFreq", 95.0f }, { "lsG", 1.5f }, { "lmG", -2.0f }, { "hmG", 3.0f }, { "hsG", 4.0f }, { "deessAmt", 55.0f }, { "satDrive", 35.0f }, { "dblOn", 1.0f }, { "dblAmt", 25.0f }, { "dlyMix", 18.0f }, { "dlyNote", 2.0f }, { "dlyFb", 35.0f }, { "revMix", 12.0f }, { "revDecay", 1.8f }, { "revMode", 1.0f }, { "airOn", 1.0f }, { "airHigh", 40.0f } },
        { { "compThr", -25.0f }, { "compRatio", 5.0f }, { "compAtk", 5.0f }, { "compRel", 80.0f }, { "compMakeup", 6.0f }, { "hpFreq", 110.0f }, { "lsG", -2.0f }, { "lmG", -2.0f }, { "lmF", 300.0f }, { "hmG", 3.5f }, { "hmF", 2800.0f }, { "hsG", 1.5f }, { "deessAmt", 45.0f }, { "satDrive", 40.0f }, { "dlyMix", 6.0f }, { "dlyNote", 3.0f }, { "revMix", 14.0f }, { "revDecay", 2.2f }, { "revMode", 2.0f }, { "revLow", 250.0f } },
        { { "compThr", -22.0f }, { "compRatio", 3.5f }, { "compAtk", 14.0f }, { "compRel", 140.0f }, { "compMakeup", 5.0f }, { "hpFreq", 85.0f }, { "lmG", -1.0f }, { "hmG", 2.0f }, { "hsG", 4.0f }, { "deessAmt", 50.0f }, { "satDrive", 18.0f }, { "choOn", 1.0f }, { "choMix", 15.0f }, { "dlyMix", 20.0f }, { "dlyNote", 2.0f }, { "dlyFb", 40.0f }, { "revMix", 20.0f }, { "revDecay", 2.4f }, { "revMode", 1.0f } },
        { { "compThr", -20.0f }, { "compRatio", 2.5f }, { "compAtk", 20.0f }, { "compRel", 160.0f }, { "compMakeup", 3.0f }, { "comp2On", 0.0f }, { "hpFreq", 90.0f }, { "lmG", -1.5f }, { "hmG", 1.5f }, { "hsG", 1.5f }, { "deessAmt", 35.0f }, { "satDrive", 8.0f }, { "satMix", 60.0f }, { "dlyMix", 0.0f }, { "revMix", 0.0f } },
        { { "compThr", -23.0f }, { "compRatio", 4.0f }, { "compAtk", 12.0f }, { "compRel", 120.0f }, { "compMakeup", 5.0f }, { "hpFreq", 80.0f }, { "lsG", 3.0f }, { "lsF", 150.0f }, { "hmG", -1.5f }, { "hsG", -3.0f }, { "hsF", 8000.0f }, { "lpFreq", 13000.0f }, { "deessAmt", 30.0f }, { "satDrive", 30.0f }, { "dlyMix", 14.0f }, { "dlyNote", 1.0f }, { "dlyLP", 3500.0f }, { "revMix", 18.0f }, { "revDecay", 2.6f }, { "revDamp", 75.0f }, { "revMode", 2.0f } },
        { { "compThr", -22.0f }, { "compRatio", 3.5f }, { "compAtk", 12.0f }, { "compRel", 110.0f }, { "compMakeup", 5.0f }, { "hpFreq", 100.0f }, { "lmG", -1.5f }, { "hmG", 3.0f }, { "hmF", 4000.0f }, { "hsG", 5.0f }, { "hsF", 11000.0f }, { "deessAmt", 60.0f }, { "satDrive", 15.0f }, { "dlyMix", 8.0f }, { "revMix", 10.0f }, { "revDecay", 1.4f }, { "revMode", 1.0f }, { "airOn", 1.0f }, { "airHigh", 40.0f } },
        { { "compThr", -26.0f }, { "compRatio", 5.0f }, { "compAtk", 6.0f }, { "compRel", 80.0f }, { "compMakeup", 8.0f }, { "hpFreq", 120.0f }, { "lsG", -3.0f }, { "lmG", 2.0f }, { "lmF", 900.0f }, { "hmG", 3.0f }, { "hmF", 3000.0f }, { "hsG", 1.0f }, { "lpFreq", 14000.0f }, { "deessAmt", 45.0f }, { "satDrive", 30.0f }, { "dlyMix", 0.0f }, { "revMix", 4.0f }, { "revDecay", 0.8f }, { "revMode", 0.0f } },
        { { "compThr", -22.0f }, { "compRatio", 3.5f }, { "compAtk", 14.0f }, { "compRel", 120.0f }, { "compMakeup", 4.0f }, { "hpFreq", 90.0f }, { "lmG", -1.0f }, { "hmG", 4.0f }, { "hmF", 3500.0f }, { "hmQ", 0.9f }, { "hsG", 2.0f }, { "deessAmt", 50.0f }, { "satDrive", 15.0f }, { "revMix", 6.0f }, { "revDecay", 1.1f } },
        { { "compThr", -22.0f }, { "compRatio", 3.5f }, { "compAtk", 12.0f }, { "compRel", 110.0f }, { "compMakeup", 4.0f }, { "hpFreq", 90.0f }, { "hmG", 2.0f }, { "hsG", 2.0f }, { "deessAmt", 45.0f }, { "dblOn", 1.0f }, { "dblAmt", 55.0f }, { "dblSpread", 80.0f }, { "choOn", 1.0f }, { "choMix", 10.0f }, { "revMix", 10.0f }, { "revDecay", 1.5f } },
        { { "compThr", -24.0f }, { "compRatio", 4.0f }, { "compAtk", 8.0f }, { "compRel", 100.0f }, { "compMakeup", 5.0f }, { "hpFreq", 180.0f }, { "lmG", -2.0f }, { "hmG", 2.0f }, { "hsG", 3.0f }, { "deessAmt", 55.0f }, { "dblOn", 1.0f }, { "dblAmt", 45.0f }, { "dblSpread", 100.0f }, { "choOn", 1.0f }, { "choMix", 25.0f }, { "widthOn", 1.0f }, { "width", 160.0f }, { "dlyMix", 28.0f }, { "dlyNote", 2.0f }, { "dlyFb", 45.0f }, { "revMix", 25.0f }, { "revDecay", 2.5f }, { "revMode", 1.0f } },
        { { "compThr", -26.0f }, { "compRatio", 5.0f }, { "compAtk", 5.0f }, { "compRel", 80.0f }, { "compMakeup", 6.0f }, { "phoneOn", 1.0f }, { "phoneAmt", 100.0f }, { "phoneGrit", 35.0f }, { "hpFreq", 200.0f }, { "dlyMix", 22.0f }, { "dlyNote", 2.0f }, { "dlyFb", 40.0f }, { "dlyLP", 4000.0f }, { "revMix", 14.0f }, { "revDecay", 1.6f }, { "revMode", 0.0f } },
        { { "compThr", -22.0f }, { "compRatio", 4.0f }, { "compAtk", 10.0f }, { "compRel", 120.0f }, { "compMakeup", 4.0f }, { "hpFreq", 150.0f }, { "lmG", -2.0f }, { "hmG", -1.0f }, { "hsG", -1.0f }, { "lpFreq", 12000.0f }, { "deessAmt", 50.0f }, { "dblOn", 1.0f }, { "dblAmt", 40.0f }, { "choOn", 1.0f }, { "choMix", 20.0f }, { "width", 150.0f }, { "dlyMix", 12.0f }, { "dlyNote", 1.0f }, { "revMix", 28.0f }, { "revDecay", 2.8f }, { "revMode", 2.0f } },
        { { "compThr", -20.0f }, { "compRatio", 2.5f }, { "compAtk", 25.0f }, { "compRel", 200.0f }, { "compMakeup", 2.0f }, { "comp2Amt", 20.0f }, { "hpFreq", 70.0f }, { "lmG", -1.0f }, { "hsG", 1.5f }, { "deessAmt", 30.0f }, { "satDrive", 12.0f }, { "dlyMix", 0.0f }, { "revMix", 6.0f }, { "revDecay", 1.2f }, { "revMode", 1.0f } },
        { { "compThr", -18.0f }, { "compRatio", 2.0f }, { "compAtk", 25.0f }, { "compRel", 200.0f }, { "compKnee", 14.0f }, { "compMakeup", 2.0f }, { "comp2On", 0.0f }, { "hpFreq", 80.0f }, { "deessAmt", 30.0f }, { "satDrive", 8.0f }, { "revMix", 8.0f }, { "revDecay", 1.4f }, { "revMode", 1.0f } },
        { { "compThr", -32.0f }, { "compRatio", 10.0f }, { "compAtk", 2.0f }, { "compRel", 60.0f }, { "compKnee", 2.0f }, { "compMakeup", 12.0f }, { "comp2Amt", 80.0f }, { "compMix", 85.0f }, { "hpFreq", 100.0f }, { "hmG", 2.0f }, { "deessAmt", 60.0f }, { "satDrive", 30.0f }, { "revMix", 5.0f }, { "revDecay", 1.0f }, { "revMode", 0.0f } },
        { { "compThr", -20.0f }, { "compRatio", 3.0f }, { "compAtk", 18.0f }, { "compRel", 150.0f }, { "compMakeup", 4.0f }, { "hpFreq", 80.0f }, { "lsG", 2.0f }, { "lsF", 180.0f }, { "hmG", 1.0f }, { "hsG", 2.0f }, { "deessAmt", 45.0f }, { "satDrive", 10.0f }, { "dlyMix", 0.0f }, { "revMix", 8.0f }, { "revDecay", 0.7f }, { "revSize", 25.0f }, { "revMode", 0.0f } },
        { { "compThr", -20.0f }, { "compRatio", 3.0f }, { "compAtk", 16.0f }, { "compRel", 140.0f }, { "compMakeup", 3.0f }, { "hpFreq", 100.0f }, { "lmG", -1.5f }, { "hmG", 1.0f }, { "hsG", 6.0f }, { "hsF", 12000.0f }, { "deessAmt", 60.0f }, { "satDrive", 10.0f }, { "choOn", 1.0f }, { "choMix", 12.0f }, { "dlyMix", 12.0f }, { "dlyNote", 2.0f }, { "dlyLP", 9000.0f }, { "revMix", 22.0f }, { "revDecay", 2.6f }, { "revMode", 1.0f }, { "revHigh", 14000.0f } },
        { { "compThr", -23.0f }, { "compRatio", 4.0f }, { "compAtk", 10.0f }, { "compRel", 110.0f }, { "compMakeup", 5.0f }, { "hpFreq", 85.0f }, { "lmG", -1.5f }, { "lmF", 380.0f }, { "hmG", 2.5f }, { "hmF", 3200.0f }, { "hsG", 3.0f }, { "deessAmt", 50.0f }, { "satDrive", 22.0f }, { "dlyMix", 10.0f }, { "dlyNote", 1.0f }, { "dlyFb", 28.0f }, { "revMix", 11.0f }, { "revDecay", 1.6f }, { "revMode", 1.0f } },
        { { "tuneOn", 1.0f }, { "tuneSpeed", 0.0f }, { "tuneAmt", 100.0f }, { "compThr", -24.0f }, { "compRatio", 4.0f }, { "compMakeup", 5.0f }, { "hpFreq", 90.0f }, { "hmG", 2.0f }, { "hsG", 3.0f }, { "deessAmt", 50.0f }, { "satDrive", 25.0f }, { "dlyMix", 12.0f }, { "dlyNote", 2.0f }, { "revMix", 12.0f }, { "revDecay", 1.6f } },
        { { "tuneOn", 1.0f }, { "tuneSpeed", 35.0f }, { "tuneAmt", 80.0f }, { "compThr", -22.0f }, { "compRatio", 3.0f }, { "compMakeup", 4.0f }, { "hpFreq", 85.0f }, { "hmG", 1.5f }, { "hsG", 2.5f }, { "deessAmt", 45.0f }, { "satDrive", 15.0f }, { "revMix", 8.0f }, { "revDecay", 1.3f } }
    };
    if (index < 0 || index >= (int) presets.size()) return;

    // retour aux valeurs par defaut (sauf bypass et gains E/S), puis application du preset
    for (int i = 0; i < Count; ++i)
    {
        if (i == bypass || i == inGain || i == outGain) continue;
        if (auto* rp = apvts.getParameter (kDefs[i].id)) rp->setValueNotifyingHost (rp->getDefaultValue());
    }
    for (auto& v : presets[(size_t) index])
        if (auto* rp = apvts.getParameter (v.id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (v.v));
    currentPreset = index;
}

//==============================================================================
void YazaVoxProcessor::captureSlot (int s)
{
    for (int i = 0; i < Count; ++i) slots[(size_t) s][(size_t) i] = raw[(size_t) i]->load();
    slotValid[(size_t) s] = true;
}

void YazaVoxProcessor::loadSlot (int s)
{
    for (int i = 0; i < Count; ++i)
        if (auto* rp = apvts.getParameter (kDefs[i].id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (slots[(size_t) s][(size_t) i]));
}

void YazaVoxProcessor::switchAB()
{
    captureSlot (abActive);
    const int other = 1 - abActive;
    if (! slotValid[(size_t) other]) { slots[(size_t) other] = slots[(size_t) abActive]; slotValid[(size_t) other] = true; }
    abActive = other;
    loadSlot (abActive);
}

void YazaVoxProcessor::copyToOther()
{
    captureSlot (abActive);
    slots[(size_t) (1 - abActive)] = slots[(size_t) abActive];
    slotValid[(size_t) (1 - abActive)] = true;
}

juce::AudioProcessorEditor* YazaVoxProcessor::createEditor() { return new YazaVoxEditor (*this); }

void YazaVoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    captureSlot (abActive);
    auto state = apvts.copyState();
    state.setProperty ("ab", abActive, nullptr);
    state.setProperty ("preset", currentPreset, nullptr);
    state.setProperty ("simple", uiSimple.load(), nullptr);
    for (int s = 0; s < 2; ++s)
        if (slotValid[(size_t) s])
        {
            juce::StringArray a;
            for (int i = 0; i < Count; ++i) a.add (juce::String (slots[(size_t) s][(size_t) i], 4));
            state.setProperty ("slot" + juce::String (s), a.joinIntoString (","), nullptr);
        }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void YazaVoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto vt = juce::ValueTree::fromXml (*xml);
        if (! vt.isValid()) return;
        for (int s = 0; s < 2; ++s)
        {
            juce::StringArray a; a.addTokens (vt.getProperty ("slot" + juce::String (s)).toString(), ",", "");
            if (a.size() == Count) { for (int i = 0; i < Count; ++i) slots[(size_t) s][(size_t) i] = a[i].getFloatValue(); slotValid[(size_t) s] = true; }
        }
        uiSimple = (bool) vt.getProperty ("simple", true);
        abActive = juce::jlimit (0, 1, (int) vt.getProperty ("ab", 0));
        currentPreset = juce::jlimit (0, (int) presetNames().size() - 1, (int) vt.getProperty ("preset", 0));
        apvts.replaceState (vt);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new YazaVoxProcessor(); }
