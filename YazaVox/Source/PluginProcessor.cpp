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
        "Rap Melodique (Autotune)",
        "Emo / Trap Melodique (Autotune)",
        "Chant Pop (Autotune)",
        "R&B Smooth (Autotune)",
        "Autotune Hard (T-Pain)",
        "Natural Tune Vocal",
        "Dark Vocal",
        "Bright Vocal",
        "Radio Vocal",
        "Vocal Presence",
        "Double Vocal",
        "Adlibs Wide",
        "Adlibs Telephone",
        "Background Vocal",
        "Vocal Bus",
        "Intimate Vocal",
        "Airy Vocal",
        "Professional Lead",
        "Soft Compression",
        "Hard Compression"
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
    // Socle : low cut ~100 Hz, creux bas-mediums, presence, air discret, comp Vocal, de-esser 7-14 kHz, saturation douce,
    // Wider leger, delay discret (duck), reverb courte. Chaque preset = ce socle + ses differences (les entrees suivantes ecrasent les precedentes).
    static const VV base = {
        { "eqOn", 1 }, { "hpOn", 1 }, { "hpFreq", 100 }, { "lsF", 120 }, { "lsG", -2.0f }, { "lsQ", 0.7f },
        { "lmF", 320 }, { "lmG", -4.5f }, { "lmQ", 1.1f }, { "hmF", 3500 }, { "hmG", 3.5f }, { "hmQ", 0.9f }, { "hsF", 10000 }, { "hsG", 3.5f },
        { "airOn", 1 }, { "airMid", 32 }, { "airHigh", 28 },
        { "compOn", 1 }, { "compThr", -24.6f }, { "compRatio", 3.9f }, { "compAtk", 1.7f }, { "compRel", 10 }, { "compKnee", 23 }, { "compMakeup", 11 },
        { "comp2On", 1 }, { "comp2Amt", 43 }, { "compWarm", 30 },   // reglages mesures sur un compresseur vocal de reference (Pro-C 2, style Vocal)
        { "fxOn", 1 }, { "deessOn", 1 }, { "deessFreq", 7000 }, { "deessHigh", 14000 }, { "deessAmt", 55 }, { "deessRange", 12 }, { "deessMode", 0 }, { "deessBand", 0 },
        { "satOn", 1 }, { "satDrive", 22 }, { "satTone", 8 }, { "satMix", 70 },
        { "widthOn", 1 }, { "width", 118 },
        { "dlyOn", 1 }, { "dlyMix", 8 }, { "dlyNote", 1 }, { "dlyFb", 25 }, { "dlyHP", 250 }, { "dlyLP", 6000 }, { "dlyDuck", 75 }, { "dlyPing", 1 },
        { "revOn", 1 }, { "revMode", 2 }, { "revDecay", 0.7f }, { "revPre", 20 }, { "revSize", 80 }, { "revDiff", 90 }, { "revMod", 38 },
        { "revMix", 5 }, { "revDamp", 65 }, { "revLow", 200 }, { "revHigh", 8000 }, { "revDuck", 50 }
    };
    auto mk = [] (VV extra) { VV r = base; r.insert (r.end(), extra.begin(), extra.end()); return r; };
    // autotune : chromatique (cale sur le demi-ton le plus proche, sans connaitre la tonalite) ; AUTO VOIX peut ensuite chercher la cle
    auto tune = [] (float speed, float human, float vib, float flex, float amt) { return VV { { "tuneOn", 1 }, { "tuneScale", 0 }, { "tuneSpeed", speed }, { "tuneAmt", amt }, { "tuneHuman", human }, { "tuneVib", vib }, { "tuneFlex", flex }, { "tuneFormant", 1 } }; };
    auto with = [&] (VV a, const VV& b) { a.insert (a.end(), b.begin(), b.end()); return a; };

    static const std::vector<VV> presets = {
        /* 0  Init */ {},
        /* 3  Rap Lead Clean */ mk ({ { "revMix", 3 }, { "dlyMix", 6 }, { "width", 112 } }),
        /* 4  Rap Lead Aggressive */ mk ({ { "compThr", -31 }, { "compRatio", 6 }, { "compAtk", 4 }, { "compRel", 70 }, { "compMakeup", 16 }, { "comp2Amt", 60 }, { "compWarm", 42 },
                                        { "lmG", -4 }, { "hmG", 4 }, { "satDrive", 40 }, { "deessAmt", 62 }, { "deessRange", 14 }, { "revMix", 2 }, { "dlyMix", 4 }, { "width", 105 } }),
        /* 5  Trap Vocal */ mk ({ { "hpFreq", 110 }, { "lsG", -3 }, { "lmF", 330 }, { "lmG", -4 }, { "hmF", 3500 }, { "hmG", 3.5f }, { "hsG", 2.5f }, { "airHigh", 12 },
                                  { "compThr", -31 }, { "compRatio", 5 }, { "compMakeup", 16 }, { "comp2Amt", 50 }, { "satDrive", 30 }, { "dlyMix", 12 }, { "dlyFb", 30 },
                                  { "revMix", 6 }, { "revDecay", 0.9f }, { "width", 125 } }),
        /* 6  Drill Vocal */ mk ({ { "hpFreq", 120 }, { "lsG", -3.5f }, { "lmF", 300 }, { "lmG", -3.5f }, { "hmF", 2800 }, { "hmG", 3 }, { "hsG", -1 }, { "lpFreq", 14000 }, { "airOn", 0 },
                                   { "compThr", -31 }, { "compRatio", 5 }, { "compAtk", 5 }, { "compMakeup", 16 }, { "satDrive", 32 }, { "deessAmt", 50 },
                                   { "dlyMix", 0 }, { "revMix", 3 }, { "revMode", 0 }, { "revDecay", 0.6f }, { "width", 105 } }),
        /* 7  Rap Melodique (Autotune) */ mk (with (tune (5, 70, 30, 0, 95), { { "hsG", 2.5f }, { "airHigh", 14 }, { "compWarm", 40 }, { "satDrive", 20 },
                                   { "revMix", 12 }, { "revDecay", 1.0f }, { "revPre", 25 }, { "dlyMix", 12 }, { "width", 125 } })),
        /* 8  Emo / Trap Melodique (Autotune) */ mk (with (tune (0, 60, 20, 0, 100), { { "lsG", -2 }, { "airMid", 32 }, { "airHigh", 16 }, { "hsG", 2.5f },
                                   { "revMix", 16 }, { "revDecay", 1.3f }, { "dlyMix", 14 }, { "dlyNote", 2 }, { "dlyFb", 35 },
                                   { "dblOn", 1 }, { "dblAmt", 25 }, { "choOn", 1 }, { "choMix", 10 }, { "width", 135 } })),
        /* 9  Chant Pop (Autotune) */ mk (with (tune (22, 80, 80, 20, 90), { { "compThr", -25 }, { "compRatio", 3 }, { "compAtk", 12 }, { "compKnee", 10 }, { "compMakeup", 13 },
                                   { "hmG", 2.5f }, { "hsG", 3 }, { "airHigh", 15 }, { "satDrive", 14 }, { "revMix", 14 }, { "revDecay", 1.2f }, { "revMode", 1 }, { "dlyMix", 8 }, { "width", 120 } })),
        /* 10 R&B Smooth (Autotune) */ mk (with (tune (15, 90, 100, 30, 90), { { "lsF", 150 }, { "lsG", -1 }, { "lmG", -2.5f }, { "hmG", 2 }, { "hsG", 2.5f },
                                   { "compThr", -25 }, { "compRatio", 2.8f }, { "compAtk", 15 }, { "compRel", 130 }, { "compMakeup", 12 }, { "satDrive", 12 },
                                   { "revMix", 16 }, { "revDecay", 1.5f }, { "revMode", 1 }, { "dlyMix", 10 }, { "choOn", 1 }, { "choMix", 8 }, { "width", 125 } })),
        /* 11 Autotune Hard (T-Pain) */ mk (with (tune (0, 0, 0, 0, 100), { { "satDrive", 25 }, { "revMix", 8 }, { "dlyMix", 10 }, { "width", 120 } })),
        /* 12 Natural Tune Vocal */ mk (with (tune (35, 100, 60, 0, 80), { { "compThr", -27 }, { "compRatio", 3.5f }, { "revMix", 9 }, { "dlyMix", 8 } })),
        /* 13 Dark Vocal */ mk ({ { "lsG", 1 }, { "lmG", -3 }, { "hmF", 2600 }, { "hmG", 1.5f }, { "hsF", 8000 }, { "hsG", -3 }, { "lpFreq", 13000 }, { "airOn", 0 },
                                  { "satDrive", 28 }, { "dlyMix", 12 }, { "dlyLP", 3500 }, { "revMix", 14 }, { "revDecay", 1.6f }, { "revDamp", 80 }, { "revHigh", 6000 } }),
        /* 14 Bright Vocal */ mk ({ { "hmF", 4000 }, { "hmG", 3.5f }, { "hsF", 11000 }, { "hsG", 4 }, { "airMid", 32 }, { "airHigh", 22 }, { "deessAmt", 62 }, { "deessRange", 14 },
                                    { "satDrive", 15 }, { "revMix", 7 } }),
        /* 15 Radio Vocal */ mk ({ { "hpFreq", 120 }, { "lsG", -3 }, { "lmF", 900 }, { "lmG", 1.5f }, { "hmF", 3000 }, { "hmG", 3 }, { "lpFreq", 14000 }, { "airOn", 0 },
                                   { "compThr", -31 }, { "compRatio", 5 }, { "compMakeup", 16 }, { "revMix", 3 }, { "dlyMix", 0 } }),
        /* 16 Vocal Presence */ mk ({ { "lmG", -3 }, { "hmF", 3500 }, { "hmQ", 0.9f }, { "hmG", 4 }, { "hsG", 2 }, { "satDrive", 20 }, { "revMix", 5 }, { "dlyMix", 6 } }),
        /* 17 Double Vocal */ mk ({ { "dblOn", 1 }, { "dblAmt", 55 }, { "dblSpread", 80 }, { "choOn", 1 }, { "choMix", 10 }, { "width", 130 }, { "revMix", 10 } }),
        /* 18 Adlibs Wide */ mk ({ { "hpFreq", 180 }, { "lmG", -3 }, { "dblOn", 1 }, { "dblAmt", 45 }, { "dblSpread", 100 }, { "choOn", 1 }, { "choMix", 25 }, { "width", 170 },
                                   { "dlyMix", 28 }, { "dlyFb", 45 }, { "revMix", 22 }, { "revDecay", 1.8f } }),
        /* 19 Adlibs Telephone */ mk ({ { "phoneOn", 1 }, { "phoneAmt", 100 }, { "phoneGrit", 35 }, { "hpFreq", 220 }, { "compThr", -31 }, { "compMakeup", 15 },
                                       { "dlyMix", 22 }, { "dlyLP", 4000 }, { "revMix", 12 }, { "revMode", 0 } }),
        /* 20 Background Vocal */ mk ({ { "hpFreq", 150 }, { "lmG", -3 }, { "hmG", -1 }, { "hsG", -1 }, { "lpFreq", 12000 }, { "dblOn", 1 }, { "dblAmt", 40 }, { "choOn", 1 }, { "choMix", 20 },
                                       { "width", 160 }, { "dlyMix", 12 }, { "revMix", 26 }, { "revDecay", 2.4f } }),
        /* 21 Vocal Bus */ mk ({ { "compThr", -25 }, { "compRatio", 2.5f }, { "compAtk", 25 }, { "compRel", 160 }, { "compMakeup", 11 }, { "comp2Amt", 20 }, { "lmG", -1.5f },
                                 { "hsG", 1.5f }, { "satDrive", 10 }, { "dlyMix", 0 }, { "revMix", 4 } }),
        /* 22 Intimate Vocal */ mk ({ { "lsF", 180 }, { "lsG", 2 }, { "lmG", -2.5f }, { "hmG", 1 }, { "hsG", 2 }, { "compThr", -25 }, { "compRatio", 3 }, { "compAtk", 18 }, { "compMakeup", 12 },
                                     { "satDrive", 10 }, { "dlyMix", 0 }, { "revMix", 6 }, { "revMode", 0 }, { "revDecay", 0.7f }, { "width", 105 } }),
        /* 23 Airy Vocal */ mk ({ { "lmG", -3 }, { "hmG", 2 }, { "hsF", 12000 }, { "hsG", 5 }, { "airMid", 34 }, { "airHigh", 28 }, { "deessAmt", 62 }, { "satDrive", 10 },
                                  { "choOn", 1 }, { "choMix", 12 }, { "dlyMix", 14 }, { "revMix", 18 }, { "revDecay", 2.2f }, { "revMode", 1 }, { "width", 130 } }),
        /* 24 Professional Lead */ mk ({ { "revMix", 7 }, { "dlyMix", 8 } }),
        /* 25 Soft Compression */ mk ({ { "compThr", -23 }, { "compRatio", 2 }, { "compAtk", 25 }, { "compRel", 200 }, { "compKnee", 14 }, { "compMakeup", 11 }, { "comp2On", 0 }, { "revMix", 4 } }),
        /* 26 Hard Compression */ mk ({ { "compThr", -36 }, { "compRatio", 10 }, { "compAtk", 2 }, { "compRel", 60 }, { "compKnee", 2 }, { "compMakeup", 21 }, { "comp2Amt", 80 }, { "compMix", 85 }, { "revMix", 3 } })
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
