#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "YvDsp.h"
#include "YvAuto.h"

class YazaVoxProcessor : public juce::AudioProcessor, private juce::AsyncUpdater
{
public:
    YazaVoxProcessor();
    ~YazaVoxProcessor() override { cancelPendingUpdate(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Fck ThaVOICE"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return (int) presetNames().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override { return presetNames()[i]; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static const juce::StringArray& presetNames();
    void applyPreset (int index);   // thread interface / hote
    void resetAll();                // tout remettre aux valeurs par defaut (gains inclus)

    // A/B
    int  getAB() const noexcept { return abActive; }
    void switchAB();                // memorise le reglage courant dans le slot actif, charge l'autre
    void copyToOther();             // copie le reglage courant vers l'autre slot

    int getCurrentPreset() const noexcept { return currentPreset; }
    double getSampleRate_() const noexcept { return sr; }
    float getParam (int idx) const { return raw[(size_t) idx]->load(); }

    // AUTO VOIX : ecoute ~10 s (la voix de CETTE piste uniquement) puis regle le mix, et la tonalite si l'autotune est active
    void startAutoMix();
    void cancelAutoMix();
    int  getAutoState() const noexcept { return analyzer.state(); }     // 0 repos, 1 ecoute, 2 calcul, 3 pas assez de voix
    float getAutoProgress() const noexcept { return analyzer.progress(); }
    juce::String takeAutoMessage();                                      // message du dernier resultat (une seule fois)

    juce::AudioProcessorValueTreeState apvts;
    yv::Engine engine;
    std::atomic<bool> uiSimple { true };

private:
    void handleAsyncUpdate() override;
    yv::VoiceAnalyzer analyzer;
    juce::CriticalSection autoLock;
    juce::String autoMsg;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void captureSlot (int s);
    void loadSlot (int s);

    std::array<std::atomic<float>*, yv::Count> raw {};
    std::array<float, yv::Count> cur {};
    std::array<std::array<float, yv::Count>, 2> slots {};
    std::array<bool, 2> slotValid { false, false };
    int abActive = 0, currentPreset = 0;
    double sr = 48000;
    juce::AudioBuffer<float> scratch;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YazaVoxProcessor)
};
