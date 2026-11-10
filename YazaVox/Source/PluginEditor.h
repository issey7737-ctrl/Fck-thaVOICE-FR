#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"

namespace yvui
{
using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
using ButtonAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

namespace col
{
    const juce::Colour bg        { 0xff060606 };
    const juce::Colour panel     { 0xff101010 };
    const juce::Colour panelHi   { 0xff171212 };
    const juce::Colour edge      { 0xff2b0d10 };
    const juce::Colour redDark   { 0xff6b0f1a };
    const juce::Colour red       { 0xffd4152a };
    const juce::Colour redBright { 0xffff3b4e };
    const juce::Colour text      { 0xffe9e3e3 };
    const juce::Colour textDim   { 0xff8d7f80 };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int h) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

class KnobCell : public juce::Component
{
public:
    KnobCell (YazaVoxProcessor&, int paramIndex);
    void resized() override;
    juce::Label name;
    juce::Slider slider;
private:
    std::unique_ptr<SliderAtt> att;
};

class ToggleCell : public juce::Component
{
public:
    ToggleCell (YazaVoxProcessor&, int paramIndex);
    void resized() override;
    juce::ToggleButton btn;
private:
    std::unique_ptr<ButtonAtt> att;
};

class ChoiceCell : public juce::Component
{
public:
    ChoiceCell (YazaVoxProcessor&, int paramIndex);
    void resized() override;
    juce::Label name;
    juce::ComboBox box;
private:
    std::unique_ptr<ComboAtt> att;
};

class LevelMeter : public juce::Component
{
public:
    explicit LevelMeter (juce::String t) : title (std::move (t)) {}
    void push (float peakLin);
    void paint (juce::Graphics&) override;
private:
    juce::String title;
    float level = 0.f, hold = 0.f, shownLevel = -1.f, shownHold = -1.f; int holdCount = 0, clipCount = 0; bool shownClip = false;
};

class GrMeter : public juce::Component
{
public:
    void push (float grDb);
    void paint (juce::Graphics&) override;
private:
    float gr = 0.f, shownGr = -1.f;
};

class NoteDisplay : public juce::Component
{
public:
    explicit NoteDisplay (YazaVoxProcessor& p) : proc (p) {}
    void update();
    void paint (juce::Graphics&) override;
    static juce::String noteName (float midi);
private:
    YazaVoxProcessor& proc;
    float det = 0.f, tgt = 0.f;
};

class EqView : public juce::Component
{
public:
    // cut = false : EQ principal (4 bandes) ; cut = true : EQ de coupe (low cut / high cut) avec menu de reglages rapides
    EqView (YazaVoxProcessor&, bool cutMode, const EqView* spectrumSource = nullptr);
    void update();   // 30 Hz : lit l'analyseur
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverBand = -1; repaint(); }
private:
    static constexpr int kOrder = 12, kN = 1 << kOrder;
    struct BandIdx { int f, g, q; juce::Colour colour; int num; };
    juce::Point<float> nodePos (int i) const;
    float xOf (double f) const;
    double fOf (float x) const;
    float yOfDb (double db) const;
    int hit (juce::Point<float>) const;
    void setNorm (int idx, float real, bool gesture);
    void applyCutPreset (int item);
    void syncCutBox();

    YazaVoxProcessor& proc;
    const bool cutMode;
    const EqView* specSrc;
    int topPad = 0;
    juce::ComboBox cutBox;
    juce::dsp::FFT fft { kOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) kN, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, kN> ring {};
    std::array<float, 2 * kN> work {};
    std::array<float, kN / 2> spec {};
    int wpos = 0;
    int dragBand = -1, hoverBand = -1;
    std::vector<BandIdx> bands;
};

class ModulePanel : public juce::Component
{
public:
    ModulePanel (YazaVoxProcessor&, int module, int rowCells, std::vector<juce::String> skipGroups = {});
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();

    juce::Component* extraTop = nullptr;
    float extraFrac = 0.f;
    juce::Component* extraLeft = nullptr;      // bloc a gauche (ex. DE-ESSER), pleine hauteur
    float extraLeftFrac = 0.f;
    juce::Component* headerExtra = nullptr;

private:
    struct Group
    {
        juce::String name;
        int toggleIdx = -1;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<ButtonAtt> toggleAtt;
        std::vector<std::unique_ptr<juce::Component>> cells;
        juce::Rectangle<int> header;
    };
    YazaVoxProcessor& proc;
    int module, rowCells, onIdx = -1;
    std::unique_ptr<juce::ToggleButton> power;
    std::unique_ptr<ButtonAtt> powerAtt;
    std::vector<std::unique_ptr<Group>> groups;
};


//==============================================================================
// Vue SIMPLE : grand bouton COMP + quelques reglages par rangee
class AmberLAF : public LookAndFeel
{
public:
    AmberLAF();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int, int, int, int, juce::ComboBox&) override;
};

class AmberKnob : public juce::Component
{
public:
    AmberKnob (YazaVoxProcessor&, int paramIndex, juce::String label, std::vector<int> gates = {}, std::function<void (float)> onChange = {}, bool showLabel = true);
    void resized() override;
    void refresh();
    juce::Slider slider;
    juce::Label name;
private:
    YazaVoxProcessor& proc;
    std::vector<int> gates;
    int idx;
    std::unique_ptr<SliderAtt> att;
};


//==============================================================================
// Vue DE-ESSER plein ecran (dans l'esprit d'un Pro-DS) : forme d'onde en temps reel avec les sibilances reduites en jaune/vert
class DsBandBar : public juce::Component
{
public:
    explicit DsBandBar (YazaVoxProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
private:
    float xOf (double f) const;
    double fOf (float x) const;
    YazaVoxProcessor& proc;
    int drag = -1;            // 0 = LOW, 1 = HIGH, 2 = les deux
    double grabLo = 0, grabHi = 0; float grabX = 0;
};

class DsSeg : public juce::Component
{
public:
    DsSeg (YazaVoxProcessor& p, int paramIdx, juce::String a, juce::String b) : proc (p), idx (paramIdx), n0 (std::move (a)), n1 (std::move (b)) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    YazaVoxProcessor& proc; int idx; juce::String n0, n1;
};

class DsListen : public juce::Component
{
public:
    explicit DsListen (YazaVoxProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    YazaVoxProcessor& proc;
};


// Bloc DE-ESSER integre au panneau FX : ecran forme d'onde (sibilances visibles + correction en temps reel), THRESHOLD / RANGE, vu-metres, bande de frequences
class DsMini : public juce::Component, private juce::Timer
{
public:
    explicit DsMini (YazaVoxProcessor&);
    ~DsMini() override { stopTimer(); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override { if (isShowing()) repaint (screen.getUnion (meters)); }
    YazaVoxProcessor& proc;
    juce::ToggleButton power;
    std::unique_ptr<ButtonAtt> powerAtt;
    std::unique_ptr<KnobCell> thr, rng;
    DsSeg mode, band;
    DsBandBar bar;
    DsListen listen;
    juce::Rectangle<int> titleR, screen, meters;
    float grSmooth = 0.f;
};

class SimpleView : public juce::Component
{
public:
    SimpleView (YazaVoxProcessor&, std::function<void()> onPro);
    ~SimpleView() override { setLookAndFeel (nullptr); }
    void pushLevels (float inPk, float outPk);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Row { juce::String title; int onIdx; std::vector<juce::Component*> items; juce::Rectangle<int> band; };
    void setComp (float m01);
    void selectPreset (int i);

    YazaVoxProcessor& proc;
    AmberLAF laf;
    std::function<void()> onPro;

    juce::Slider compMacro;
    juce::Label compLabel;
    juce::ToggleButton lowCut { "Low cut" };
    std::unique_ptr<ButtonAtt> lowCutAtt;
    juce::TextButton autoIn { "Auto input" }, autoVoice { "AUTO VOIX" }, prevBtn { "<" }, nextBtn { ">" }, proBtn { "PRO" };
    juce::ComboBox presets;
    int autoShow = 0;

    std::vector<std::unique_ptr<juce::Component>> owned;
    AmberKnob *inKnob = nullptr, *outKnob = nullptr, *limKnob = nullptr;
    std::vector<Row> rows;

    float inLvl = 0.f, outLvl = 0.f, phase = 0.f, inHold = 0.f, shownOut = -1.f;
    bool syncing = false, measuring = false;
    int measureFrames = 0;
    float measurePk = 0.f;
    juce::Rectangle<int> inBar, outBar, orbArea, titleArea;
    juce::Image bg;
};

class YazaVoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit YazaVoxEditor (YazaVoxProcessor&);
    ~YazaVoxEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;
    void selectPreset (int i);

    YazaVoxProcessor& proc;
    LookAndFeel laf;

    juce::ComboBox presets;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, abBtn { "A" }, copyBtn { "A>B" }, resetBtn { "RESET" }, simpleBtn { "SIMPLE" }, autoBtn { "AUTO VOIX" };
    int autoShow = 0;
    std::unique_ptr<SimpleView> simple;
    std::unique_ptr<DsMini> dsMini;
    void applyMode();
    juce::ToggleButton bypassBtn { "BYPASS" };
    std::unique_ptr<ButtonAtt> bypassAtt;
    KnobCell inKnob, outKnob, limKnob;
    LevelMeter inMeter { "IN" }, outMeter { "OUT" };

    GrMeter grMeter;
    EqView eqView, cutView;
    NoteDisplay noteDisp;
    ModulePanel comp, eq, fx, dly, rev, tune;
    juce::Rectangle<int> logoRect, headerRect;
    int tick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YazaVoxEditor)
};
} // namespace yvui

using YazaVoxEditor = yvui::YazaVoxEditor;
