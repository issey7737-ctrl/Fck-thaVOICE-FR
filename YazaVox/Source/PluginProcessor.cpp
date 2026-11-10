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
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),   // aucune entree sidechain : le plugin ne lit QUE sa propre piste
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < Count; ++i) raw[(size_t) i] = apvts.getRawParameterValue (kDefs[i].id);
    for (int i = 0; i < Count; ++i) cur[(size_t) i] = kDefs[i].def;
}

bool YazaVoxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return in == out && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono()) && l.inputBuses.size() == 1;
}

void YazaVoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    for (int i = 0; i < Count; ++i) cur[(size_t) i] = raw[(size_t) i]->load();
    engine.setParams (cur.data());
    engine.prepare (sampleRate, samplesPerBlock);
    analyzer.prepare (sampleRate);
    scratch.setSize (2, std::max (samplesPerBlock, 16384), false, true, false);   // alloue ici, jamais dans le callback
    setLatencySamples (engine.latency());   // 0 ms sans autotune ; avec autotune ~20 ms (le dry du bypass est retarde pareil)
    reportedLat = engine.latency();
}

void YazaVoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int ch = std::max (1, std::min (2, getMainBusNumInputChannels())), n = buffer.getNumSamples();
    if (ch < 1 || n < 1) return;

    for (int i = 0; i < Count; ++i) cur[(size_t) i] = raw[(size_t) i]->load();
    engine.setParams (cur.data());
    if (engine.latency() != reportedLat) { reportedLat = engine.latency(); latUpd.trigger(); }

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
        "Rap Melodique (Autotune)",
        "Chant Pop (Autotune)",
        "Adlibs Wide",
        "Background Vocal"
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
    using VV = std::vector<V>;
    // Socle = chaine mesuree sur des plugins de reference : EQ (shelf de coupe + nettoyage + courbe "Soundgoodizer") ajuste sur 4 bandes,
    // Air (Mid 26 / High 6), comp Vocal, de-esser 7-14 kHz, saturation douce, Wider leger, delay discret (duck), reverb plate courte. Chaque preset = ce socle + ses differences (les entrees suivantes ecrasent les precedentes).
    static const VV base = {
        { "eqOn", 1 }, { "hpOn", 0 }, { "lsF", 82 }, { "lsG", -10.9f }, { "lsQ", 0.83f },
        { "lmF", 384 }, { "lmG", -2.9f }, { "lmQ", 1.44f }, { "hmF", 4836 }, { "hmG", 1.9f }, { "hmQ", 0.6f }, { "hsF", 4000 }, { "hsG", 2.8f }, { "hsQ", 0.3f },
        { "airOn", 1 }, { "airMid", 26 }, { "airHigh", 6 },
        { "compOn", 1 }, { "compThr", -24.6f }, { "compRatio", 3.9f }, { "compAtk", 1.7f }, { "compRel", 10 }, { "compKnee", 23 }, { "compMakeup", 11 },
        { "comp2On", 1 }, { "comp2Amt", 43 }, { "compWarm", 30 },   // reglages mesures sur un compresseur vocal de reference (Pro-C 2, style Vocal)
        { "fxOn", 1 }, { "deessOn", 1 }, { "deessFreq", 7000 }, { "deessHigh", 14000 }, { "deessAmt", 55 }, { "deessRange", 12 }, { "deessMode", 0 }, { "deessBand", 0 },
        { "satOn", 1 }, { "satDrive", 22 }, { "satTone", 8 }, { "satMix", 70 },
        { "widthOn", 1 }, { "width", 129 },
        { "dlyOn", 1 }, { "dlyMix", 8 }, { "dlyNote", 1 }, { "dlyFb", 68 }, { "dlyHP", 20 }, { "dlyLP", 20000 }, { "dlyDuck", 100 }, { "dlyPing", 0 },   // calibre sur un Fruity Delay 3 (1/8, feedback ~68 %, ducking sidechain voix)
        { "revOn", 1 }, { "revMode", 1 }, { "revDecay", 1.07f }, { "revPre", 0 }, { "revSize", 6 }, { "revDiff", 92 }, { "revMod", 0 },
        { "revMix", 20 }, { "revDamp", 65 }, { "revLow", 86 }, { "revHigh", 18400 }, { "revBass", 118 }, { "revDuck", 0 }, { "revWidth", 100 }
    };
    auto mk = [] (VV extra) { VV r = base; r.insert (r.end(), extra.begin(), extra.end()); return r; };
    // autotune : chromatique (cale sur le demi-ton le plus proche, sans connaitre la tonalite) ; AUTO VOIX peut ensuite chercher la cle
    auto tune = [] (float speed, float human, float vib, float flex, float amt) { return VV { { "tuneOn", 1 }, { "tuneScale", 0 }, { "tuneSpeed", speed }, { "tuneAmt", amt }, { "tuneHuman", human }, { "tuneVib", vib }, { "tuneFlex", flex }, { "tuneFormant", 1 } }; };
    auto with = [&] (VV a, const VV& b) { a.insert (a.end(), b.begin(), b.end()); return a; };

    static const std::vector<VV> presets = {
        /* 0 Init */ {},
        /* 1 Rap Lead Clean */ mk ({ { "revMix", 12 }, { "dlyMix", 6 }, { "width", 125 } }),
        /* 2 Rap Lead Aggressive */ mk ({ { "compThr", -31 }, { "compRatio", 6 }, { "compAtk", 4 }, { "compRel", 70 }, { "compMakeup", 16 }, { "comp2Amt", 60 }, { "compWarm", 42 },
                                        { "lmG", -4 }, { "hmG", 4 }, { "satDrive", 40 }, { "deessAmt", 62 }, { "deessRange", 14 }, { "revMix", 6 }, { "dlyMix", 4 }, { "width", 105 } }),
        /* 3 Rap Melodique (Autotune) */ mk (with (tune (5, 70, 30, 0, 95), { { "compWarm", 40 }, { "satDrive", 20 },
                                        { "revMix", 52 }, { "dlyMix", 12 }, { "width", 125 } })),
        /* 4 Chant Pop (Autotune) */ mk (with (tune (22, 80, 80, 20, 90), { { "compThr", -25 }, { "compRatio", 3 }, { "compAtk", 12 }, { "compKnee", 10 }, { "compMakeup", 10 },
                                        { "hmG", 3 }, { "satDrive", 14 }, { "revMix", 52 }, { "dlyMix", 8 }, { "width", 120 } })),
        /* 5 Adlibs Wide */ mk ({ { "hpOn", 1 }, { "hpFreq", 180 }, { "lmG", -3 }, { "dblOn", 1 }, { "dblAmt", 45 }, { "dblSpread", 100 }, { "choOn", 1 }, { "choMix", 25 }, { "width", 170 },
                                        { "dlyMix", 30 }, { "dlyFb", 50 }, { "revMix", 40 }, { "revDecay", 1.8f } }),
        /* 6 Background Vocal */ mk ({ { "hpOn", 1 }, { "hpFreq", 150 }, { "lmG", -3 }, { "hmG", 0 }, { "hsG", 0 }, { "lpFreq", 12000 }, { "dblOn", 1 }, { "dblAmt", 40 }, { "choOn", 1 }, { "choMix", 20 },
                                        { "width", 160 }, { "dlyMix", 12 }, { "revMix", 45 }, { "revDecay", 2.4f } })
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
