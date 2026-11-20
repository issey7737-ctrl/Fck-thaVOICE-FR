#include "PluginEditor.h"
#include <BinaryData.h>

using namespace yv;

namespace yvui
{
// AUTO VOIX : texte du bouton pendant l'ecoute, resultat affiche quelques secondes dans la zone des presets
static void autoTick (YazaVoxProcessor& p, juce::TextButton& b, juce::ComboBox& presets, int& showTicks)
{
    if (p.getAutoState() == 1) b.setButtonText ("ECOUTE " + juce::String ((int) (p.getAutoProgress() * 100.f)) + "%");
    else if (b.getButtonText() != "AUTO VOIX") b.setButtonText ("AUTO VOIX");
    const auto m = p.takeAutoMessage();
    if (m.isNotEmpty()) { presets.setText (m, juce::dontSendNotification); showTicks = 150; }
    if (showTicks > 0) --showTicks;
}
static void autoClick (YazaVoxProcessor& p) { if (p.getAutoState() == 1) p.cancelAutoMix(); else p.startAutoMix(); }
}

namespace yvui
{
//==============================================================================
static juce::String idOf (int i) { return kDefs[i].id; }
static bool isOnId (int i) { return juce::String (kDefs[i].id).endsWith ("On"); }
static juce::Font bold (float h) { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }
static juce::Font plain (float h) { return juce::Font (juce::FontOptions (h)); }

LookAndFeel::LookAndFeel()
{
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::ComboBox::outlineColourId, col::edge);
    setColour (juce::ComboBox::backgroundColourId, col::panelHi);
    setColour (juce::ComboBox::arrowColourId, col::red);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff120a0b));
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::redDark);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, col::redDark);
    setColour (juce::Label::textColourId, col::textDim);
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1c0f11));
    setColour (juce::TextEditor::textColourId, col::text);
    setColour (juce::TextEditor::highlightColourId, col::redDark);
    setColour (juce::CaretComponent::caretColourId, col::redBright);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float d = std::min (b.getWidth(), b.getHeight()) - 4.f;
    if (d < 8.f) return;
    const auto c = b.getCentre();
    const float r = d * 0.5f, track = std::max (2.5f, d * 0.075f);
    const float ang = a0 + pos * (a1 - a0);
    const bool en = s.isEnabled();

    // halo
    if (en && pos > 0.001f)
    {
        juce::ColourGradient halo (col::red.withAlpha (0.18f), c.x, c.y, juce::Colours::transparentBlack, c.x, c.y - r * 1.25f, true);
        g.setGradientFill (halo);
        g.fillEllipse (c.x - r * 1.25f, c.y - r * 1.25f, r * 2.5f, r * 2.5f);
    }
    // piste
    juce::Path bgArc;
    bgArc.addCentredArc (c.x, c.y, r - track * 0.5f, r - track * 0.5f, 0.f, a0, a1, true);
    g.setColour (juce::Colour (0xff2a1a1c));
    g.strokePath (bgArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // valeur : depuis le centre pour les plages bipolaires
    const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
    const float zero = bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.f;
    const float aZero = a0 + zero * (a1 - a0);
    juce::Path vArc;
    vArc.addCentredArc (c.x, c.y, r - track * 0.5f, r - track * 0.5f, 0.f, bipolar ? aZero : a0, ang, true);
    g.setGradientFill (juce::ColourGradient (col::redDark, c.x - r, c.y + r, col::redBright, c.x + r, c.y - r, false));
    g.strokePath (vArc, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // corps
    const float rb = r - track * 1.7f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2224), c.x, c.y - rb, juce::Colour (0xff0d0909), c.x, c.y + rb, false));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f);
    g.setColour (juce::Colour (0xff3a1c20));
    g.drawEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f, 1.f);
    // index
    juce::Path p;
    p.addRoundedRectangle (-1.6f, -rb + 2.f, 3.2f, rb * 0.42f, 1.4f);
    g.setColour (en ? col::redBright : col::textDim);
    g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    if (b.getButtonText().isEmpty())
    {
        // petit interrupteur lumineux (ON de module / de groupe)
        const float d = std::min (r.getWidth(), r.getHeight()) - 2.f;
        const auto c = r.getCentre();
        if (on)
        {
            g.setColour (col::red.withAlpha (0.25f));
            g.fillEllipse (c.x - d * 0.85f, c.y - d * 0.85f, d * 1.7f, d * 1.7f);
        }
        g.setGradientFill (juce::ColourGradient (on ? col::redBright : juce::Colour (0xff2b2022), c.x, c.y - d / 2, on ? col::redDark : juce::Colour (0xff120c0d), c.x, c.y + d / 2, false));
        g.fillEllipse (c.x - d / 2, c.y - d / 2, d, d);
        g.setColour (over ? col::redBright : col::edge.brighter (0.3f));
        g.drawEllipse (c.x - d / 2, c.y - d / 2, d, d, 1.2f);
        return;
    }
    const auto pill = r.reduced (2.f);
    g.setColour (on ? col::redDark : juce::Colour (0xff1a1213));
    g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
    g.setColour (on ? col::redBright : (over ? col::textDim : col::edge.brighter (0.3f)));
    g.drawRoundedRectangle (pill, pill.getHeight() * 0.5f, 1.2f);
    g.setColour (on ? juce::Colours::white : col::textDim);
    g.setFont (bold (std::clamp (pill.getHeight() * 0.5f, 9.f, 15.f)));
    g.drawText (b.getButtonText(), pill.toNearestInt(), juce::Justification::centred);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.f);
    const bool hot = b.getToggleState() || down;
    g.setGradientFill (juce::ColourGradient (hot ? col::red : (over ? juce::Colour (0xff2a1b1d) : juce::Colour (0xff1d1415)), 0, r.getY(),
                                             hot ? col::redDark : juce::Colour (0xff130d0e), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (over || hot ? col::redBright.withAlpha (0.8f) : col::edge.brighter (0.4f));
    g.drawRoundedRectangle (r, 6.f, 1.f);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.f);
    g.setColour (juce::Colour (0xff170f10));
    g.fillRoundedRectangle (r, 6.f);
    g.setColour (box.hasKeyboardFocus (false) ? col::redBright : col::edge.brighter (0.4f));
    g.drawRoundedRectangle (r, 6.f, 1.f);
    juce::Path arrow;
    const float ax = (float) w - 14.f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4.f, ay - 2.f, ax + 4.f, ay - 2.f, ax, ay + 3.f);
    g.setColour (col::red);
    g.fillPath (arrow);
}
juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& b) { return bold (std::clamp ((float) b.getHeight() * 0.5f, 10.f, 16.f)); }
juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int h) { return bold (std::clamp ((float) h * 0.42f, 10.f, 15.f)); }

//==============================================================================
static void initLabel (juce::Label& l, const juce::String& t)
{
    l.setText (t, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.setInterceptsMouseClicks (false, false);
    l.setMinimumHorizontalScale (0.6f);
}

KnobCell::KnobCell (YazaVoxProcessor& p, int idx)
{
    const auto& d = kDefs[idx];
    initLabel (name, d.label);
    addAndMakeVisible (name);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    slider.setTextValueSuffix (d.unit);
    slider.setNumDecimalPlacesToDisplay (d.step >= 1.f ? 0 : (d.step >= 0.1f ? 1 : 2));
    slider.setDoubleClickReturnValue (true, d.def);
    slider.setMouseDragSensitivity (220);
    addAndMakeVisible (slider);
    att = std::make_unique<SliderAtt> (p.apvts, idOf (idx), slider);
}
void KnobCell::resized()
{
    auto r = getLocalBounds();
    const int lh = std::clamp (r.getHeight() / 7, 11, 18);
    name.setFont (bold ((float) lh * 0.78f));
    name.setBounds (r.removeFromTop (lh));
    const int th = std::clamp (r.getHeight() / 6, 13, 20);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, std::max (40, r.getWidth() - 2), th);
    slider.getLookAndFeel().setColour (juce::Slider::textBoxTextColourId, col::text);
    slider.setBounds (r);
}

ToggleCell::ToggleCell (YazaVoxProcessor& p, int idx)
{
    btn.setButtonText (kDefs[idx].label);
    addAndMakeVisible (btn);
    att = std::make_unique<ButtonAtt> (p.apvts, idOf (idx), btn);
}
void ToggleCell::resized()
{
    auto r = getLocalBounds();
    const int h = std::clamp (r.getHeight() / 3, 20, 32);
    btn.setBounds (r.withSizeKeepingCentre (r.getWidth() - 4, h));
}

ChoiceCell::ChoiceCell (YazaVoxProcessor& p, int idx)
{
    initLabel (name, kDefs[idx].label);
    addAndMakeVisible (name);
    juce::StringArray items; items.addTokens (kDefs[idx].unit, "|", "");
    box.addItemList (items, 1);
    addAndMakeVisible (box);
    att = std::make_unique<ComboAtt> (p.apvts, idOf (idx), box);
}
void ChoiceCell::resized()
{
    auto r = getLocalBounds();
    const int lh = std::clamp (r.getHeight() / 7, 11, 18);
    name.setFont (bold ((float) lh * 0.78f));
    name.setBounds (r.removeFromTop (lh));
    const int h = std::clamp (r.getHeight() / 3, 22, 32);
    box.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, h).translated (0, -r.getHeight() / 8));
}

//==============================================================================
void LevelMeter::push (float pk)
{
    const float db = (float) linToDb (pk);
    const float v = juce::jlimit (0.f, 1.f, (db + 60.f) / 63.f);
    level = v > level ? v : level - (level - v) * 0.18f;
    if (v >= hold) { hold = v; holdCount = 45; } else if (--holdCount < 0) hold = std::max (0.f, hold - 0.02f);
    if (pk >= 0.995f) clipCount = 60; else if (clipCount > 0) --clipCount;
    repaint();
}
void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::textDim);
    g.setFont (bold (std::clamp (r.getWidth() * 0.9f, 9.f, 12.f)));
    g.drawText (title, r.removeFromBottom (13.f).toNearestInt(), juce::Justification::centred);
    r.reduce (1.f, 2.f);
    g.setColour (juce::Colour (0xff0b0707));
    g.fillRoundedRectangle (r, 3.f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 3.f, 1.f);
    auto in = r.reduced (2.f);
    const float hgt = in.getHeight() * level;
    juce::Rectangle<float> bar (in.getX(), in.getBottom() - hgt, in.getWidth(), hgt);
    g.setGradientFill (juce::ColourGradient (col::redBright, in.getX(), in.getY(), col::redDark, in.getX(), in.getBottom(), false));
    g.fillRoundedRectangle (bar, 2.f);
    // graduations
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    for (int d : { -48, -36, -24, -12, -6, 0 })
    {
        const float y = in.getBottom() - in.getHeight() * ((float) d + 60.f) / 63.f;
        g.drawHorizontalLine ((int) y, in.getX(), in.getRight());
    }
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.fillRect (in.getX(), in.getBottom() - in.getHeight() * hold - 1.f, in.getWidth(), 2.f);
    if (clipCount > 0) { g.setColour (juce::Colours::white); g.fillRect (in.getX(), in.getY(), in.getWidth(), 3.f); }
}

void GrMeter::push (float grDb)
{
    const float target = juce::jlimit (0.f, 24.f, -grDb);
    gr = target > gr ? target : gr - (gr - target) * 0.2f;
    repaint();
}
void GrMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (col::textDim);
    g.setFont (bold (std::clamp (r.getHeight() * 0.5f, 9.f, 12.f)));
    auto lab = r.removeFromLeft (26.f);
    g.drawText ("GR", lab.toNearestInt(), juce::Justification::centredLeft);
    auto txt = r.removeFromRight (46.f);
    g.setColour (col::text);
    g.drawText (juce::String (-gr, 1) + " dB", txt.toNearestInt(), juce::Justification::centredRight);
    r = r.reduced (0.f, r.getHeight() * 0.22f);
    g.setColour (juce::Colour (0xff0b0707));
    g.fillRoundedRectangle (r, 3.f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 3.f, 1.f);
    const float w = (r.getWidth() - 4.f) * (gr / 24.f);
    g.setGradientFill (juce::ColourGradient (col::redDark, r.getX(), 0, col::redBright, r.getRight(), 0, false));
    g.fillRoundedRectangle (r.getRight() - 2.f - w, r.getY() + 2.f, w, r.getHeight() - 4.f, 2.f);   // la barre se remplit de la droite vers la gauche
}

//==============================================================================

juce::String NoteDisplay::noteName (float midi)
{
    if (midi <= 0.f) return "--";
    static const char* n[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int m = (int) std::lround (midi);
    return juce::String (n[((m % 12) + 12) % 12]) + juce::String (m / 12 - 1);
}
void NoteDisplay::update()
{
    const float d = proc.engine.tuner().detMidi.load(), t = proc.engine.tuner().tgtMidi.load();
    if (d != det || t != tgt) { det = d; tgt = t; repaint(); }
}
void NoteDisplay::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff0b0707));
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 5.f, 1.f);
    g.setFont (bold (std::clamp (r.getHeight() * 0.58f, 10.f, 18.f)));
    g.setColour (col::textDim);
    g.drawText (noteName (det), r.withTrimmedRight (r.getWidth() * 0.55f).toNearestInt(), juce::Justification::centred);
    g.setColour (col::redBright);
    g.drawText (noteName (tgt), r.withTrimmedLeft (r.getWidth() * 0.55f).toNearestInt(), juce::Justification::centred);
    g.setColour (col::textDim);
    g.drawText (">", r.toNearestInt(), juce::Justification::centred);
}

//==============================================================================
static const juce::Colour eqNodeCol[6] = { juce::Colour (0xffa17be8), juce::Colour (0xffe76fb0), juce::Colour (0xffee8d4c), juce::Colour (0xffe9d54e), juce::Colour (0xff62cf72), juce::Colour (0xff5fa3ea) };

struct CutPreset { const char* name; float hp, lp; bool hpOn; };
static const CutPreset kCutPresets[] = {
    { "20 Hz + 18 kHz cut", 20.f, 18000.f, true },  { "30 Hz + 18 kHz cut", 30.f, 18000.f, true },  { "40 Hz + 18 kHz cut", 40.f, 18000.f, true },
    { "40 Hz cut", 40.f, 20000.f, true },           { "60 Hz + 16 kHz cut", 60.f, 16000.f, true },  { "80 Hz + 16 kHz cut", 80.f, 16000.f, true },
    { "100 Hz + 14 kHz cut", 100.f, 14000.f, true }, { "120 Hz cut", 120.f, 20000.f, true },         { "Off", 80.f, 20000.f, false } };

EqView::EqView (YazaVoxProcessor& p, bool cut, const EqView* src) : proc (p), cutMode (cut), specSrc (src)
{
    if (cutMode)
    {
        bands = { BandIdx { hpFreq, -1, -1, eqNodeCol[0], 1 }, BandIdx { lpFreq, -1, -1, eqNodeCol[5], 2 } };
        topPad = 20;
        int id = 1;
        for (auto& pr : kCutPresets) cutBox.addItem (pr.name, id++);
        cutBox.setTextWhenNothingSelected ("Custom");
        cutBox.onChange = [this] { applyCutPreset (cutBox.getSelectedItemIndex()); };
        addAndMakeVisible (cutBox);
    }
    else
        bands = { BandIdx { lsF, lsG, lsQ, eqNodeCol[1], 1 }, BandIdx { lmF, lmG, lmQ, eqNodeCol[2], 2 }, BandIdx { hmF, hmG, hmQ, eqNodeCol[3], 3 }, BandIdx { hsF, hsG, hsQ, eqNodeCol[4], 4 } };
    spec.fill (-120.f);
}

void EqView::resized()
{
    if (cutMode) cutBox.setBounds (getWidth() - 168, 2, 164, 17);
}

void EqView::applyCutPreset (int item)
{
    if (item < 0 || item >= (int) (sizeof (kCutPresets) / sizeof (kCutPresets[0]))) return;
    const auto& pr = kCutPresets[item];
    setNorm (hpFreq, pr.hp, false); setNorm (lpFreq, pr.lp, false); setNorm (hpOn, pr.hpOn ? 1.f : 0.f, false);
}

void EqView::syncCutBox()
{
    int match = -1;
    const float hp = proc.getParam (hpFreq), lp = proc.getParam (lpFreq);
    const bool on = proc.getParam (hpOn) > 0.5f;
    for (int i = 0; i < (int) (sizeof (kCutPresets) / sizeof (kCutPresets[0])); ++i)
    {
        const auto& pr = kCutPresets[i];
        if (pr.hpOn != on) continue;
        if (on && std::abs (pr.hp - hp) > 1.5f) continue;
        if (std::abs (pr.lp - lp) > 15.f) continue;
        match = i; break;
    }
    if (match != cutBox.getSelectedItemIndex() && ! cutBox.isPopupActive())
        cutBox.setSelectedItemIndex (match, juce::dontSendNotification);
}

float EqView::xOf (double f) const
{
    return (float) getWidth() * (float) ((std::log10 (std::max (20.0, f)) - std::log10 (20.0)) / 3.0);
}
double EqView::fOf (float x) const
{
    return std::pow (10.0, std::log10 (20.0) + 3.0 * (double) x / std::max (1, getWidth()));
}
float EqView::yOfDb (double db) const
{
    return (float) topPad + (float) (getHeight() - topPad) * (0.5f - (float) db / 36.f);   // +-18 dB
}

juce::Point<float> EqView::nodePos (int i) const
{
    const auto& b = bands[(size_t) i];
    const float m = std::clamp ((float) getHeight() * 0.09f, 7.f, 12.f);
    const float x = std::clamp (xOf (proc.getParam (b.f)), m, (float) getWidth() - m);
    const float y = b.g < 0 ? yOfDb (0.0) : yOfDb (juce::jlimit (-18.0, 18.0, (double) proc.getParam (b.g)));
    return { x, y };
}

void EqView::update()
{
    if (specSrc != nullptr) { syncCutBox(); repaint(); return; }
    auto& e = proc.engine;
    int s1, z1, s2, z2;
    const int ready = e.scopeFifo.getNumReady();
    if (ready <= 0) return;
    e.scopeFifo.prepareToRead (ready, s1, z1, s2, z2);
    auto push = [&] (int start, int n) { for (int i = 0; i < n; ++i) { ring[(size_t) wpos] = e.scopeBuf[(size_t) (start + i)]; wpos = (wpos + 1) & (kN - 1); } };
    push (s1, z1); push (s2, z2);
    e.scopeFifo.finishedRead (z1 + z2);

    for (int i = 0; i < kN; ++i) work[(size_t) i] = ring[(size_t) ((wpos + i) & (kN - 1))];
    std::fill (work.begin() + kN, work.end(), 0.f);
    window.multiplyWithWindowingTable (work.data(), (size_t) kN);
    fft.performFrequencyOnlyForwardTransform (work.data());
    for (int b = 1; b < kN / 2; ++b)
    {
        const float db = 20.f * std::log10 (work[(size_t) b] / (kN * 0.25f) + 1e-7f);
        float& s = spec[(size_t) b];
        s = db > s ? s + (db - s) * 0.5f : s + (db - s) * 0.12f;
    }
    repaint();
}

void EqView::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float gt = (float) topPad, gh = r.getHeight() - gt;
    const float cy = yOfDb (0.0);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff142027), 0, gt, juce::Colour (0xff1c2d36), 0, cy, false));
    g.fillRoundedRectangle (r, 7.f);
    g.saveState();
    juce::Path clip; clip.addRoundedRectangle (r, 7.f);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a3d48), 0, cy, juce::Colour (0xff22323c), 0, r.getBottom(), false));
    g.fillRect (r.withTrimmedTop (cy));
    if (cutMode)   // barre de titre facon Fruity
    {
        g.setColour (juce::Colour (0xff0f1a20)); g.fillRect (r.withHeight (gt));
        g.setColour (juce::Colour (0xff3d5766)); g.drawHorizontalLine ((int) gt, 0.f, r.getRight());
        g.setColour (juce::Colour (0xffb7ccd6)); g.setFont (bold (10.5f));
        g.drawText ("CUT EQ", 8, 0, getWidth() - 180, topPad, juce::Justification::centredLeft);
    }

    g.setFont (plain (std::clamp (gh * 0.075f, 8.f, 11.f)));
    for (double dec : { 10.0, 100.0, 1000.0 })
        for (int k = 1; k < 10; ++k)
        {
            const double f = dec * k;
            if (f < 20.0 || f > 20000.0) continue;
            const float x = xOf (f);
            const bool major = (k == 1 || k == 2 || k == 5);
            g.setColour (juce::Colour (0xff5d7f90).withAlpha (major ? 0.20f : 0.09f));
            g.drawVerticalLine ((int) x, gt, r.getBottom());
            if (major && f >= 50.0)
            {
                g.setColour (juce::Colour (0xff9db6c2).withAlpha (0.65f));
                g.drawText (f >= 1000 ? juce::String (f / 1000.0, 0) + "k" : juce::String ((int) f), (int) x + 2, (int) r.getBottom() - 13, 30, 12, juce::Justification::left);
            }
        }
    for (int db : { -12, -6, 6, 12 })
    {
        g.setColour (juce::Colour (0xff5d7f90).withAlpha (0.12f));
        g.drawHorizontalLine ((int) yOfDb (db), 0.f, r.getRight());
        g.setColour (juce::Colour (0xff9db6c2).withAlpha (0.5f));
        g.drawText (juce::String (db > 0 ? "+" : "") + juce::String (db), (int) r.getRight() - 26, (int) yOfDb (db) - 11, 24, 11, juce::Justification::right);
    }

    // spectre en temps reel
    const auto& S = specSrc != nullptr ? specSrc->spec : spec;
    const double sr = proc.getSampleRate_();
    juce::Path sp, spLine;
    const int W = getWidth();
    bool started = false;
    for (int px = 0; px < W; px += 2)
    {
        const double f0 = fOf ((float) px), f1 = fOf ((float) px + 2.f);
        const int b0 = std::max (1, (int) (f0 / sr * kN)), b1 = std::max (b0 + 1, std::min (kN / 2 - 1, (int) (f1 / sr * kN) + 1));
        float m = -120.f;
        for (int b = b0; b < b1; ++b) m = std::max (m, S[(size_t) b]);
        const float y = gt + gh * (1.f - juce::jlimit (0.f, 1.f, (m + 90.f) / 90.f));
        if (! started) { sp.startNewSubPath (0.f, r.getBottom()); sp.lineTo ((float) px, y); spLine.startNewSubPath ((float) px, y); started = true; }
        else { sp.lineTo ((float) px, y); spLine.lineTo ((float) px, y); }
    }
    if (started)
    {
        sp.lineTo ((float) W, r.getBottom()); sp.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffcfe3ec).withAlpha (0.42f), 0, gt, juce::Colour (0xff8fb0c0).withAlpha (0.10f), 0, r.getBottom(), false));
        g.fillPath (sp);
        g.setColour (juce::Colour (0xffdcecf3).withAlpha (0.55f));
        g.strokePath (spLine, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved));
    }

    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawHorizontalLine ((int) cy, 0.f, r.getRight());

    // courbe de reponse : EQ principal = 4 bandes seulement ; EQ de coupe = filtres seulement
    float v[Count];
    for (int i = 0; i < Count; ++i) v[i] = proc.getParam (i);
    const bool eqActive = v[eqOn] > 0.5f;
    float vv[Count];
    std::copy (v, v + Count, vv);
    if (cutMode) vv[lsG] = vv[lmG] = vv[hmG] = vv[hsG] = 0.f;
    else { vv[hpOn] = 0.f; vv[lpFreq] = 20000.f; }
    juce::Path curve, area;
    for (int px = 0; px <= W; px += 2)
    {
        const double db = EQ::responseDb (vv, sr, fOf ((float) px));
        const float y = yOfDb (juce::jlimit (-18.0, 18.0, db));
        if (px == 0) { curve.startNewSubPath (0.f, y); area.startNewSubPath (0.f, cy); area.lineTo (0.f, y); }
        else { curve.lineTo ((float) px, y); area.lineTo ((float) px, y); }
    }
    area.lineTo ((float) W, cy); area.closeSubPath();
    g.setColour (juce::Colours::white.withAlpha (eqActive ? 0.13f : 0.05f));
    g.fillPath (area);
    g.setColour (juce::Colours::white.withAlpha (eqActive ? 0.22f : 0.08f));
    g.strokePath (curve, juce::PathStrokeType (5.f));
    g.setColour (eqActive ? juce::Colours::white : juce::Colour (0xff8fa3ad));
    g.strokePath (curve, juce::PathStrokeType (1.7f));

    // noeuds numerotes
    const float rad = std::clamp (gh * 0.075f, 6.f, 11.f);
    for (int i = 0; i < (int) bands.size(); ++i)
    {
        const auto& b = bands[(size_t) i];
        const auto c = nodePos (i);
        const bool act = b.g < 0 ? (b.f == hpFreq ? v[hpOn] > 0.5f : v[lpFreq] < 19900.f) : std::abs (v[b.g]) > 0.01f;
        const bool hot = dragBand == i || hoverBand == i;
        const auto col_ = b.colour;
        if (act || hot) { g.setColour (col_.withAlpha (hot ? 0.38f : 0.22f)); g.fillEllipse (c.x - rad * 1.7f, c.y - rad * 1.7f, rad * 3.4f, rad * 3.4f); }
        g.setGradientFill (juce::ColourGradient (col_.darker (0.25f).withAlpha (0.85f), c.x, c.y - rad, col_.darker (0.75f), c.x, c.y + rad, false));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f);
        g.setColour (col_.withAlpha (act || hot ? 1.f : 0.65f));
        g.drawEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f, hot ? 2.2f : 1.6f);
        g.setFont (bold (rad * 1.05f));
        g.setColour (juce::Colours::white.withAlpha (act || hot ? 1.f : 0.75f));
        g.drawText (juce::String (b.num), (int) (c.x - rad), (int) (c.y - rad), (int) (rad * 2.f), (int) (rad * 2.f), juce::Justification::centred);
    }
    const int hb = dragBand >= 0 ? dragBand : hoverBand;
    if (hb >= 0)
    {
        const auto& b = bands[(size_t) hb];
        const auto c = nodePos (hb);
        const double f = proc.getParam (b.f);
        juce::String t = (f >= 1000.0 ? juce::String (f / 1000.0, 2) + " kHz" : juce::String ((int) f) + " Hz");
        if (b.g >= 0) t << "   " << (proc.getParam (b.g) > 0 ? "+" : "") << juce::String (proc.getParam (b.g), 1) << " dB   Q " << juce::String (proc.getParam (b.q), 2);
        g.setFont (bold (std::clamp (gh * 0.085f, 9.f, 12.f)));
        const float tw = g.getCurrentFont().getStringWidthFloat (t) + 12.f, th = g.getCurrentFont().getHeight() + 6.f;
        juce::Rectangle<float> tb (std::clamp (c.x - tw / 2, 3.f, r.getRight() - tw - 3.f), c.y < gt + gh * 0.4f ? c.y + rad * 1.5f : c.y - rad * 1.5f - th, tw, th);
        g.setColour (juce::Colour (0xff0d171c).withAlpha (0.88f)); g.fillRoundedRectangle (tb, 4.f);
        g.setColour (b.colour); g.drawRoundedRectangle (tb, 4.f, 1.f);
        g.setColour (juce::Colours::white); g.drawText (t, tb.toNearestInt(), juce::Justification::centred);
    }
    g.restoreState();
    g.setColour (juce::Colour (0xff3d5766));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.f, 1.2f);
}

int EqView::hit (juce::Point<float> p) const
{
    int best = -1; float bd = std::max (16.f, (float) getHeight() * 0.12f);
    for (int i = 0; i < (int) bands.size(); ++i)
    {
        const float d = nodePos (i).getDistanceFrom (p);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

void EqView::setNorm (int idx, float real, bool)
{
    if (auto* p = proc.apvts.getParameter (kDefs[idx].id))
        p->setValueNotifyingHost (p->convertTo0to1 (real));
}

void EqView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h != hoverBand) { hoverBand = h; repaint(); }
}
void EqView::mouseDown (const juce::MouseEvent& e)
{
    dragBand = hit (e.position);
    if (dragBand >= 0)
    {
        const auto& b = bands[(size_t) dragBand];
        proc.apvts.getParameter (kDefs[b.f].id)->beginChangeGesture();
        if (b.g >= 0) proc.apvts.getParameter (kDefs[b.g].id)->beginChangeGesture();
        if (b.f == hpFreq) setNorm (hpOn, 1.f, false);
    }
}
void EqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;
    const auto& b = bands[(size_t) dragBand];
    const auto& df = kDefs[b.f];
    const float f = juce::jlimit (df.lo, df.hi, (float) fOf (e.position.x));
    setNorm (b.f, b.f == hpFreq || b.f == lpFreq ? std::round (f) : f, false);
    if (b.g >= 0)
    {
        const auto& dg = kDefs[b.g];
        const float db = juce::jlimit (dg.lo, dg.hi, (0.5f - (e.position.y - (float) topPad) / (float) (getHeight() - topPad)) * 36.f);
        setNorm (b.g, std::round (db * 10.f) / 10.f, false);
    }
}
void EqView::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
    {
        const auto& b = bands[(size_t) dragBand];
        proc.apvts.getParameter (kDefs[b.f].id)->endChangeGesture();
        if (b.g >= 0) proc.apvts.getParameter (kDefs[b.g].id)->endChangeGesture();
    }
    dragBand = -1;
}
void EqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h < 0) return;
    const auto& b = bands[(size_t) h];
    if (b.g >= 0) setNorm (b.g, 0.f, false);
    else if (b.f == hpFreq) setNorm (hpOn, proc.getParam (hpOn) > 0.5f ? 0.f : 1.f, false);     // double-clic : coupe le low cut / le remet
    else setNorm (lpFreq, 20000.f, false);                                                         // high cut : retour a "ouvert"
}
void EqView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int h = hit (e.position);
    if (h < 0 || bands[(size_t) h].q < 0) return;
    const auto& q = kDefs[bands[(size_t) h].q];
    const float nq = juce::jlimit (q.lo, q.hi, proc.getParam (bands[(size_t) h].q) * (1.f + w.deltaY * 0.5f));
    if (auto* p = proc.apvts.getParameter (q.id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (nq)); p->endChangeGesture(); }
}

//==============================================================================
ModulePanel::ModulePanel (YazaVoxProcessor& p, int m, int rc, std::vector<juce::String> skipGroups) : proc (p), module (m), rowCells (rc)
{
    for (int i = 0; i < Count; ++i)
        if (kDefs[i].module == m && kDefs[i].group[0] == 0 && isOnId (i)) onIdx = i;
    power = std::make_unique<juce::ToggleButton>();
    addAndMakeVisible (*power);
    if (onIdx >= 0) powerAtt = std::make_unique<ButtonAtt> (p.apvts, idOf (onIdx), *power);

    for (int i = 0; i < Count; ++i)
    {
        const auto& d = kDefs[i];
        if (d.module != m || d.group[0] == 0) continue;
        if (skipGroups.end() != std::find (skipGroups.begin(), skipGroups.end(), juce::String (d.group))) continue;
        Group* grp = nullptr;
        for (auto& g : groups) if (g->name == d.group) grp = g.get();
        if (grp == nullptr)
        {
            groups.push_back (std::make_unique<Group>());
            grp = groups.back().get();
            grp->name = d.group;
        }
        if (isOnId (i) && d.kind == KBool)
        {
            grp->toggleIdx = i;
            grp->toggle = std::make_unique<juce::ToggleButton>();
            addAndMakeVisible (*grp->toggle);
            grp->toggleAtt = std::make_unique<ButtonAtt> (p.apvts, idOf (i), *grp->toggle);
        }
        else
        {
            std::unique_ptr<juce::Component> c;
            if (d.kind == KBool) c = std::make_unique<ToggleCell> (p, i);
            else if (d.kind == KChoice) c = std::make_unique<ChoiceCell> (p, i);
            else c = std::make_unique<KnobCell> (p, i);
            addAndMakeVisible (*c);
            grp->cells.push_back (std::move (c));
        }
    }
}

void ModulePanel::refresh()
{
    const bool modOn = onIdx < 0 || proc.getParam (onIdx) > 0.5f;
    for (auto& g : groups)
    {
        const bool gOn = g->toggleIdx < 0 || proc.getParam (g->toggleIdx) > 0.5f;
        const float a = (modOn ? 1.f : 0.35f) * (gOn ? 1.f : 0.5f);
        for (auto& c : g->cells) c->setAlpha (a);
        if (g->toggle) g->toggle->setAlpha (modOn ? 1.f : 0.5f);
    }
}

void ModulePanel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.f);
    g.setGradientFill (juce::ColourGradient (col::panelHi, 0, r.getY(), col::panel, 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.f);
    g.setColour (col::edge);
    g.drawRoundedRectangle (r, 10.f, 1.2f);
    // liseré rouge en haut
    g.setGradientFill (juce::ColourGradient (col::red.withAlpha (0.9f), r.getX() + 12, 0, juce::Colours::transparentBlack, r.getRight() - 12, 0, false));
    g.fillRect (r.getX() + 12.f, r.getY() + 0.5f, r.getWidth() - 24.f, 1.5f);

    const int th = std::clamp ((int) (r.getHeight() * 0.1f), 22, 34);
    g.setColour (col::text);
    g.setFont (bold ((float) th * 0.62f));
    g.drawText (kModuleTitles[module], 36, 4, 120, th, juce::Justification::centredLeft);

    g.setFont (bold (std::clamp ((float) th * 0.4f, 9.f, 12.f)));
    for (auto& grp : groups)
    {
        g.setColour (col::redBright.withAlpha (0.85f));
        auto h = grp->header;
        if (grp->toggle) h.removeFromLeft (h.getHeight() + 2);
        g.drawText (grp->name, h, juce::Justification::centredLeft);
        g.setColour (col::edge);
        g.drawHorizontalLine (grp->header.getBottom(), (float) grp->header.getX(), (float) grp->header.getRight());
    }
}

void ModulePanel::resized()
{
    auto area = getLocalBounds().reduced (8, 6);
    const int th = std::clamp ((int) (getHeight() * 0.1f), 22, 34);
    auto top = area.removeFromTop (th);
    power->setBounds (top.removeFromLeft (th).reduced (th / 5));
    if (headerExtra) headerExtra->setBounds (top.removeFromRight (std::min (top.getWidth(), 240)).reduced (0, 2));
    if (extraTop) extraTop->setBounds (area.removeFromTop ((int) ((float) area.getHeight() * extraFrac)).reduced (0, 2));
    if (extraLeft) extraLeft->setBounds (area.removeFromLeft ((int) ((float) area.getWidth() * extraLeftFrac)).reduced (0, 2).withTrimmedRight (4));
    area.removeFromTop (2);

    // repartition en rangees
    std::vector<std::vector<Group*>> rows (1);
    int used = 0;
    for (auto& g : groups)
    {
        const int n = std::max (1, (int) g->cells.size());
        if (used + n > rowCells && used > 0) { rows.emplace_back(); used = 0; }
        rows.back().push_back (g.get());
        used += n;
    }
    const float cellW = (float) area.getWidth() / (float) rowCells;
    const int rowH = area.getHeight() / (int) rows.size();
    const int hh = std::clamp (rowH / 8, 14, 20);
    for (size_t ri = 0; ri < rows.size(); ++ri)
    {
        float x = (float) area.getX();
        const int y = area.getY() + (int) ri * rowH;
        for (auto* g : rows[ri])
        {
            const int n = std::max (1, (int) g->cells.size());
            const int gw = (int) std::round (cellW * (float) n);
            g->header = { (int) x + 3, y, gw - 8, hh };
            if (g->toggle) g->toggle->setBounds (g->header.getX(), g->header.getY() + 1, hh - 2, hh - 2);
            for (size_t k = 0; k < g->cells.size(); ++k)
                g->cells[k]->setBounds ((int) std::round (x + cellW * (float) k) + 1, y + hh + 3, (int) std::round (cellW) - 2, rowH - hh - 6);
            x += cellW * (float) n;
        }
    }
    repaint();
}


//==============================================================================
static void setReal (YazaVoxProcessor& p, int idx, float real)
{
    if (auto* a = p.apvts.getParameter (kDefs[idx].id))
    {
        const float n = a->convertTo0to1 (real);
        if (std::abs (a->getValue() - n) < 1e-5f) return;
        a->beginChangeGesture(); a->setValueNotifyingHost (n); a->endChangeGesture();
    }
}

static const juce::Colour amber { 0xffe8890a }, amberHi { 0xfff6b53c }, amberDark { 0xff9a5a05 }, wine { 0xff5a0c11 }, wineDark { 0xff3a070b };

AmberLAF::AmberLAF()
{
    setColour (juce::Label::textColourId, juce::Colours::white);
    setColour (juce::ComboBox::textColourId, amberHi);
    setColour (juce::ComboBox::arrowColourId, amber);
    setColour (juce::PopupMenu::backgroundColourId, wineDark);
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, amberDark);
    setColour (juce::TextButton::textColourOffId, amberHi);
    setColour (juce::TextButton::textColourOnId, wineDark);
}

void AmberLAF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float d = std::min (b.getWidth(), b.getHeight()) - 2.f;
    if (d < 10.f) return;
    const auto c = b.getCentre();
    const float r = d * 0.5f;
    const bool active = (bool) s.getProperties().getWithDefault ("active", true);
    const bool big = s.getProperties().contains ("big");
    const float ang = a0 + pos * (a1 - a0);

    // couronne de graduations
    const int ticks = big ? 60 : 22;
    for (int i = 0; i <= ticks; ++i)
    {
        const float t = (float) i / (float) ticks, a = a0 + t * (a1 - a0);
        const float r0 = r * (big ? 0.84f : 0.80f), r1 = r * (big ? 0.97f : 0.92f) * (i % 5 == 0 && big ? 1.03f : 1.f);
        const bool lit = big && active && t <= pos;
        g.setColour (lit ? amberHi : juce::Colours::white.withAlpha (big ? 0.35f : 0.28f));
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, big ? 2.f : 1.f);
    }
    const float rb = r * (big ? 0.72f : 0.64f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - rb, c.y - rb + 3.f, rb * 2.f, rb * 2.f);
    const auto top = active ? amberHi : juce::Colour (0xffa6a1a1), bot = active ? amberDark : juce::Colour (0xff5d5858);
    g.setGradientFill (juce::ColourGradient (top, c.x, c.y - rb, bot, c.x, c.y + rb, false));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2.f, rb * 2.f);
    g.setGradientFill (juce::ColourGradient (active ? amber : juce::Colour (0xff8a8585), c.x, c.y - rb * 0.8f, active ? amberDark.darker (0.2f) : juce::Colour (0xff6a6565), c.x, c.y + rb * 0.8f, false));
    g.fillEllipse (c.x - rb * 0.84f, c.y - rb * 0.84f, rb * 1.68f, rb * 1.68f);
    if (big)   // trait indicateur
    {
        juce::Path p; p.addRoundedRectangle (-2.5f, -rb * 0.82f, 5.f, rb * 0.38f, 2.f);
        g.setColour (wineDark);
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
    else    // petit triangle
    {
        juce::Path p; const float k = rb * 0.2f;
        p.addTriangle (0.f, -rb * 0.72f, -k, -rb * 0.72f + k * 1.5f, k, -rb * 0.72f + k * 1.5f);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.fillPath (p, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
    }
}

void AmberLAF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (2.f);
    const bool on = b.getToggleState();
    g.setColour (on ? amber : wineDark.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (over ? amberHi : amberDark);
    g.drawRoundedRectangle (r, 7.f, 1.4f);
    g.setColour (on ? wineDark : amberHi);
    g.setFont (bold (std::clamp (r.getHeight() * 0.5f, 9.f, 17.f)));
    g.drawText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred);
}

void AmberLAF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (1.f);
    g.setColour (down ? amber : (over ? wine.brighter (0.25f) : wineDark.withAlpha (0.9f)));
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (over ? amberHi : amberDark);
    g.drawRoundedRectangle (r, 7.f, 1.2f);
}

void AmberLAF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.f);
    g.setColour (wineDark.withAlpha (0.9f));
    g.fillRoundedRectangle (r, 7.f);
    g.setColour (amberDark);
    g.drawRoundedRectangle (r, 7.f, 1.2f);
    juce::Path a; const float ax = (float) w - 13.f, ay = (float) h * 0.5f;
    a.addTriangle (ax - 4.f, ay - 2.f, ax + 4.f, ay - 2.f, ax, ay + 3.f);
    g.setColour (amber);
    g.fillPath (a);
}

AmberKnob::AmberKnob (YazaVoxProcessor& p, int i, juce::String label, std::vector<int> gs, std::function<void (float)> onChange, bool showLabel)
    : proc (p), gates (std::move (gs)), idx (i)
{
    const auto& d = kDefs[i];
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setTextValueSuffix (d.unit);
    slider.setNumDecimalPlacesToDisplay (d.step >= 1.f ? 0 : 1);
    slider.setDoubleClickReturnValue (true, d.def);
    slider.setPopupDisplayEnabled (true, true, nullptr);
    slider.setMouseDragSensitivity (200);
    addAndMakeVisible (slider);
    att = std::make_unique<SliderAtt> (p.apvts, idOf (i), slider);
    if (onChange) slider.onValueChange = [this, onChange] { onChange ((float) slider.getValue()); };
    name.setText (label, juce::dontSendNotification);
    name.setJustificationType (juce::Justification::centred);
    name.setInterceptsMouseClicks (false, false);
    name.setVisible (showLabel);
    addAndMakeVisible (name);
}
void AmberKnob::resized()
{
    auto r = getLocalBounds();
    if (name.isVisible())
    {
        const int lh = std::clamp ((int) (r.getHeight() * 0.24f), 12, 40);
        name.setFont (juce::Font (juce::FontOptions ((float) lh * 0.78f)));
        name.setBounds (r.removeFromBottom (lh));
    }
    slider.setBounds (r);
}
void AmberKnob::refresh()
{
    bool act = true;
    for (int g : gates) if (proc.getParam (g) < 0.5f) act = false;
    const auto& d = kDefs[idx];
    if (d.lo >= 0.f && idx != tuneSpeed && proc.getParam (idx) <= d.lo + 1e-4f) act = false;
    if ((bool) slider.getProperties().getWithDefault ("active", true) != act)
    {
        slider.getProperties().set ("active", act);
        slider.repaint();
    }
}

//==============================================================================
// Vue SIMPLE : fond colline (image fixe) + vrais boutons animes dessines par le code
static const juce::Colour kPlate { 0xffb4b4b4 }, kLime { 0xff00ff24 }, kOrange { 0xffffa800 }, kHot { 0xffff3a24 };
static constexpr float kDW = 1654.f, kDH = 1040.f;                            // taille du design
static constexpr float kRingCx = 393.f, kRingCy = 498.f, kRingR = 223.f;     // anneau COMP
static constexpr float kBarX0 = 40.f, kBarX1 = 515.f;                        // barre IN
static constexpr float kDbMin = 48.f, kZoneLo = 30.f / 48.f, kZoneHi = 41.6f / 48.f;   // echelle -48..0 dB, zone TARGET -18..-6.4 dB

static juce::Font plateFont (float h)
{
    static const bool impact = juce::Font::findAllTypefaceNames().contains ("Impact");
    return impact ? juce::Font (juce::FontOptions ("Impact", h, juce::Font::plain)) : juce::Font (juce::FontOptions (h, juce::Font::bold));
}

// texte blanc cerne de noir (comme la maquette), reduit pour tenir dans la largeur
static void inkText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float h, juce::Colour fill = juce::Colours::white,
                     juce::Justification j = juce::Justification::centred)
{
    if (t.isEmpty() || r.getWidth() < 4.f || h < 4.f) return;
    juce::GlyphArrangement ga;
    ga.addLineOfText (plateFont (h), t, 0.f, 0.f);
    const float w = ga.getBoundingBox (0, -1, true).getWidth() + h * 0.16f;
    if (w > r.getWidth()) h *= r.getWidth() / w;
    if (h < 4.f) return;
    const float o = std::max (1.2f, h * 0.075f);
    g.setFont (plateFont (h));
    g.setColour (juce::Colours::black);
    static const float dx[8] = { 1.f, 0.7f, 0.f, -0.7f, -1.f, -0.7f, 0.f, 0.7f }, dy[8] = { 0.f, 0.7f, 1.f, 0.7f, 0.f, -0.7f, -1.f, -0.7f };
    for (int i = 0; i < 8; ++i) g.drawText (t, r.translated (dx[i] * o, dy[i] * o), j, false);
    g.setColour (fill);
    g.drawText (t, r, j, false);
}

static void arrowDown (juce::Graphics& g, float cx, float y, float w, float s)
{
    juce::Path p; p.addTriangle (cx - w * 0.5f, y, cx + w * 0.5f, y, cx, y + w * 0.42f);
    g.setColour (juce::Colours::white); g.fillPath (p);
    g.setColour (juce::Colours::black); g.strokePath (p, juce::PathStrokeType (std::max (1.5f, 3.f * s), juce::PathStrokeType::mitered));
}

static void plateRect (juce::Graphics& g, juce::Rectangle<float> r, float lw, float alpha = 1.f)
{
    g.setColour (kPlate.withAlpha (alpha)); g.fillRect (r);
    g.setColour (juce::Colours::black); g.drawRect (r, lw);
}

static void trapezoid (juce::Graphics& g, juce::Rectangle<float> r, float inTop, float inBot, float lw)
{
    juce::Path p;
    p.startNewSubPath (r.getX() + inTop, r.getY()); p.lineTo (r.getRight() - inTop, r.getY());
    p.lineTo (r.getRight() - inBot, r.getBottom()); p.lineTo (r.getX() + inBot, r.getBottom()); p.closeSubPath();
    g.setColour (kPlate); g.fillPath (p);
    g.setColour (juce::Colours::black); g.strokePath (p, juce::PathStrokeType (lw, juce::PathStrokeType::mitered));
}

XpLAF::XpLAF()
{
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff2b343b));
    setColour (juce::PopupMenu::textColourId, juce::Colours::white);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kOrange);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
    setColour (juce::ComboBox::textColourId, juce::Colours::transparentBlack);   // le texte est dessine par la vue
}

void XpLAF::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s)
{
    if (s.getProperties().contains ("big")) return;     // grand bouton COMP : la vue dessine l'anneau et le dossier
    const auto b = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto c = b.getCentre();
    const float R0 = std::min (b.getWidth(), b.getHeight()) * 0.5f * 0.894f;
    if (R0 < 5.f) return;
    const float ang = a0 + pos * (a1 - a0);
    const bool active = (bool) s.getProperties().getWithDefault ("active", true);
    const bool hot = s.isMouseOverOrDragging();
    const float R = R0 * (s.isMouseButtonDown() ? 1.06f : (hot ? 1.03f : 1.f));    // petit gonflement au survol / a la prise

    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillEllipse (c.x - R, c.y - R + R * 0.12f, R * 2.f, R * 2.f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff6c6c6c), c.x, c.y - R, juce::Colour (0xffd6d6d6), c.x, c.y + R, false));
    g.fillEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f, std::max (1.4f, R * 0.07f));
    const float fr = R * 0.80f, fy = c.y - R * 0.10f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (active ? 0xffffffff : 0xffd2d2d2), c.x, fy - fr, juce::Colour (active ? 0xffeeeeee : 0xffb0b0b0), c.x, fy + fr, false));
    g.fillEllipse (c.x - fr, fy - fr, fr * 2.f, fr * 2.f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - fr, fy - fr, fr * 2.f, fr * 2.f, std::max (1.2f, R * 0.055f));
    juce::Path line; line.addRoundedRectangle (-fr * 0.05f, -fr * 0.84f, fr * 0.10f, fr * 0.52f, fr * 0.05f);
    g.setColour (juce::Colour (active ? 0xffe3151d : 0xff9c6a6a));
    g.fillPath (line, juce::AffineTransform::rotation (ang).translated (c.x, fy));
    if (hot) { g.setColour (kOrange.withAlpha (0.85f)); g.drawEllipse (c.x - R * 1.09f, c.y - R * 1.09f, R * 2.18f, R * 2.18f, std::max (1.5f, R * 0.08f)); }
}

void XpLAF::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat();
    const bool on = b.getToggleState();
    const auto mode = b.getProperties().getWithDefault ("mode", "title").toString();
    if (mode == "lever")     // interrupteur ON / OFF de TUNE
    {
        const auto o = r.reduced (r.getWidth() * 0.04f);
        const float lw = std::max (1.6f, o.getWidth() * 0.05f);
        g.setColour (kPlate); g.fillRoundedRectangle (o, lw * 1.6f);
        g.setColour (juce::Colours::black); g.drawRoundedRectangle (o, lw * 1.6f, lw * 1.7f);
        auto in = o.reduced (o.getWidth() * 0.10f, o.getHeight() * 0.06f);
        const float half = in.getHeight() * 0.5f;
        auto top = in.removeFromTop (half).reduced (0.f, 1.f), bot = in.reduced (0.f, 1.f);
        const auto sel = on ? top : bot;
        g.setColour (juce::Colours::white); g.fillRect (sel);
        g.setColour (juce::Colours::black); g.drawRect (sel, lw);
        inkText (g, "ON", top, top.getHeight() * 0.92f);
        inkText (g, "OFF", bot, bot.getHeight() * 0.92f);
        if (over) { g.setColour (juce::Colours::white.withAlpha (0.14f)); g.fillRoundedRectangle (o, lw * 1.6f); }
        return;
    }
    if (mode == "plate")    // plaque a deux etats (PONG, LOW CUT)
    {
        const float lw = std::max (2.f, r.getHeight() * 0.06f);
        const auto o = r.reduced (lw * 0.5f);
        plateRect (g, o, lw, on ? 1.f : 0.55f);
        inkText (g, b.getButtonText(), o.reduced (o.getWidth() * 0.04f, 0.f), o.getHeight() * 0.70f, on ? juce::Colours::white : juce::Colour (0xff6e6e6e));
        if (down) { g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRect (o); }
        else if (over) { g.setColour (juce::Colours::white.withAlpha (0.20f)); g.fillRect (o); }
        return;
    }
    if (over) { g.setColour (juce::Colours::white.withAlpha (0.10f)); g.fillRect (r); }   // titre de rangee : on s'eclaire au survol
}

void XpLAF::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat();
    if (b.getProperties().contains ("plate"))
    {
        const float lw = std::max (2.f, r.getHeight() * (b.getProperties().contains ("thick") ? 0.085f : 0.06f));
        const auto o = r.reduced (lw * 0.5f);
        g.setColour (down ? kPlate.darker (0.25f) : (over ? kPlate.brighter (0.15f) : kPlate));
        g.fillRect (o);
        g.setColour (juce::Colours::black); g.drawRect (o, lw);
        if (b.getProperties().contains ("arrow"))
        {
            const float dir = (float) (int) b.getProperties().getWithDefault ("arrow", 1);
            juce::Path p; const auto c = o.getCentre(); const float a = o.getWidth() * 0.30f;
            p.addTriangle (c.x - dir * a * 0.8f, c.y - a * 1.1f, c.x - dir * a * 0.8f, c.y + a * 1.1f, c.x + dir * a * 0.8f, c.y);
            g.setColour (juce::Colours::white); g.fillPath (p);
            g.setColour (juce::Colours::black); g.strokePath (p, juce::PathStrokeType (std::max (1.5f, lw * 0.8f)));
        }
    }
}

void XpLAF::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    if (b.getButtonText().isEmpty() || b.getProperties().contains ("arrow")) return;
    auto r = b.getLocalBounds().toFloat().reduced (b.getWidth() * 0.04f, 0.f);
    if (down) r.translate (0.f, 1.f);
    inkText (g, b.getButtonText(), r, b.getHeight() * 0.74f);
}

void XpLAF::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& b)
{
    if (b.isMouseOver (true)) { g.setColour (juce::Colours::white.withAlpha (0.14f)); g.fillRect (0, 0, w, h); }
}

juce::Rectangle<int> SimpleView::M (const juce::Rectangle<float>& r) const
{
    return juce::Rectangle<float> (offX + r.getX() * scale, offY + r.getY() * scale, r.getWidth() * scale, r.getHeight() * scale).toNearestInt();
}

juce::ComboBox* SimpleView::addBox (int idx, juce::Rectangle<float> r, const char* tip)
{
    auto* box = new juce::ComboBox();
    juce::StringArray items; items.addTokens (kDefs[idx].unit, "|", "");
    box->addItemList (items, 1);
    box->setColour (juce::ComboBox::textColourId, juce::Colours::transparentBlack);
    box->setColour (juce::ComboBox::arrowColourId, juce::Colours::transparentBlack);
    box->onChange = [this] { repaint(); };
    owned.emplace_back (box);
    addAndMakeVisible (*box);
    boxAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, idOf (idx), *box));
    places.push_back ({ box, r, tip });
    return box;
}

juce::ToggleButton* SimpleView::addToggle (int idx, const char* mode, juce::Rectangle<float> r, const char* tip, const char* text)
{
    auto* t = new juce::ToggleButton (text);
    t->setClickingTogglesState (true);
    t->getProperties().set ("mode", mode);
    owned.emplace_back (t);
    addAndMakeVisible (*t);
    btnAtts.push_back (std::make_unique<ButtonAtt> (proc.apvts, idOf (idx), *t));
    places.push_back ({ t, r, tip });
    return t;
}

SimpleView::SimpleView (YazaVoxProcessor& p, std::function<void()> pro) : proc (p), onPro (std::move (pro))
{
    setLookAndFeel (&laf);
    setOpaque (true);
    src = juce::ImageCache::getFromMemory (BinaryData::ui_bg_png, BinaryData::ui_bg_pngSize);
    folder = juce::ImageCache::getFromMemory (BinaryData::ui_folder_png, BinaryData::ui_folder_pngSize);

    // grand bouton COMP (macro : seuil, ratio, gain de compensation, etage 2) : zone de glisse sur l'anneau
    compMacro.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    compMacro.setRotaryParameters (juce::MathConstants<float>::pi * 1.15f, juce::MathConstants<float>::pi * 2.85f, true);
    compMacro.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    compMacro.setRange (0.0, 100.0, 1.0);
    compMacro.setDoubleClickReturnValue (true, 50.0);
    compMacro.setPopupDisplayEnabled (false, false, nullptr);
    compMacro.getProperties().set ("big", true);
    compMacro.setMouseDragSensitivity (260);
    compMacro.onValueChange = [this] { if (! syncing) setComp ((float) compMacro.getValue() / 100.f); };
    addAndMakeVisible (compMacro);
    places.push_back ({ &compMacro, { kRingCx - 235.f, kRingCy - 235.f, 470.f, 470.f }, "THA COMP" });

    // barre du haut (ciel) : AUTO VOIX, AUTO IN, LOW CUT, preset avec fleches, PRO!
    autoIn.onClick = [this] { measuring = true; measureFrames = 45; measurePk = 0.f; autoIn.setButtonText ("..."); };
    prevBtn.onClick = [this] { const int n = YazaVoxProcessor::presetNames().size(); selectPreset ((proc.getCurrentPreset() + n - 1) % n); };
    nextBtn.onClick = [this] { selectPreset ((proc.getCurrentPreset() + 1) % YazaVoxProcessor::presetNames().size()); };
    proBtn.onClick = [this] { if (onPro) onPro(); };
    autoVoice.onClick = [this] { autoClick (proc); };
    for (auto* b : { &autoIn, &autoVoice, &prevBtn, &nextBtn, &proBtn }) { b->getProperties().set ("plate", true); addAndMakeVisible (*b); }
    proBtn.getProperties().set ("thick", true);
    prevBtn.getProperties().set ("arrow", -1);
    nextBtn.getProperties().set ("arrow", 1);
    places.push_back ({ &autoVoice, { 262.f, 24.f, 200.f, 56.f }, "AUTO VOIX" });
    places.push_back ({ &autoIn, { 478.f, 24.f, 150.f, 56.f }, "AUTO INPUT" });
    places.push_back ({ &prevBtn, { 966.f, 26.f, 36.f, 54.f }, "" });
    places.push_back ({ &nextBtn, { 1292.f, 26.f, 36.f, 54.f }, "" });
    places.push_back ({ &proBtn, { 1376.f, 12.f, 225.f, 76.f }, "MODE PRO" });
    addToggle (hpOn, "plate", { 644.f, 24.f, 150.f, 56.f }, "LOW CUT", "LOW CUT");
    presets.addItemList (YazaVoxProcessor::presetNames(), 1);
    presets.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
    presets.onChange = [this] { selectPreset (presets.getSelectedItemIndex()); };
    addAndMakeVisible (presets);
    places.push_back ({ &presets, { 1008.f, 14.f, 278.f, 78.f }, "PRESET" });

    auto onAlways = [&p] (int id) { return [&p, id] (float) { setReal (p, id, 1.f); }; };
    auto onFx = [&p] (int id) { return [&p, id] (float v) { setReal (p, id, v > 0.f ? 1.f : 0.f); if (v > 0.f) setReal (p, fxOn, 1.f); }; };
    auto onMix = [&p] (int id) { return [&p, id] (float v) { setReal (p, id, v > 0.f ? 1.f : 0.f); }; };
    auto knob = [&] (int idx, std::vector<int> gates, std::function<void (float)> cb, float cx, float cy, float r, const char* tip)
    {
        auto* k = new AmberKnob (p, idx, "", std::move (gates), std::move (cb), false);
        owned.emplace_back (k);
        addAndMakeVisible (*k);
        const float h = r / 0.894f;
        places.push_back ({ k, { cx - h, cy - h, 2.f * h, 2.f * h }, tip });
        return k;
    };

    // EQ
    rows.push_back ({ eqOn, { 806.f, 218.f, 802.f, 127.f } });
    addToggle (eqOn, "title", { 816.f, 222.f, 255.f, 118.f }, "EQ");
    knob (lsG, { eqOn }, onAlways (eqOn), 1122, 285, 41, "EQ GRAVES"); knob (lmG, { eqOn }, onAlways (eqOn), 1248, 285, 41, "EQ BAS-MEDIUMS");
    knob (hmG, { eqOn }, onAlways (eqOn), 1376, 285, 41, "EQ HAUT-MEDIUMS"); knob (hsG, { eqOn }, onAlways (eqOn), 1530, 285, 41, "EQ AIGUS");
    // TUNE
    rows.push_back ({ tuneOn, { 806.f, 357.f, 802.f, 126.f } });
    addToggle (tuneOn, "title", { 816.f, 360.f, 265.f, 118.f }, "TUNE");
    tuneLever = addToggle (tuneOn, "lever", { 1086.f, 362.f, 84.f, 118.f }, "TUNE");
    keyBox = addBox (tuneKey, { 1178.f, 366.f, 88.f, 112.f }, "TONALITE");
    scaleBox = addBox (tuneScale, { 1264.f, 366.f, 92.f, 112.f }, "GAMME");
    knob (tuneSpeed, { tuneOn }, onAlways (tuneOn), 1418, 415, 41, "VITESSE"); knob (tuneHuman, { tuneOn }, onAlways (tuneOn), 1530, 415, 41, "HUMANISER");
    // FX
    rows.push_back ({ fxOn, { 806.f, 495.f, 802.f, 126.f } });
    addToggle (fxOn, "title", { 816.f, 498.f, 205.f, 118.f }, "FX");
    knob (deessAmt, { fxOn, deessOn }, onFx (deessOn), 1073, 552, 41, "DE-ESSER"); knob (satDrive, { fxOn, satOn }, onFx (satOn), 1192, 552, 41, "SATURATION");
    knob (phoneAmt, { fxOn, phoneOn }, onFx (phoneOn), 1305, 552, 41, "TELEPHONE"); knob (dblAmt, { fxOn, dblOn }, onFx (dblOn), 1417, 552, 41, "DOUBLEUR");
    knob (choMix, { fxOn, choOn }, onFx (choOn), 1530, 552, 41, "CHORUS");
    // DLY
    rows.push_back ({ dlyOn, { 806.f, 633.f, 802.f, 126.f } });
    addToggle (dlyOn, "title", { 816.f, 636.f, 205.f, 118.f }, "DELAY");
    knob (dlyMix, { dlyOn }, onMix (dlyOn), 1073, 692, 41, "DELAY MIX"); knob (dlyFb, { dlyOn }, {}, 1194, 692, 41, "REPETITIONS"); knob (dlyLP, { dlyOn }, {}, 1305, 692, 41, "TON");
    noteBox = addBox (dlyNote, { 1376.f, 658.f, 84.f, 118.f }, "NOTE DU DELAY");
    pongBtn = addToggle (dlyPing, "plate", { 1476.f, 660.f, 116.f, 72.f }, "PING-PONG", "PONG");
    // REV
    rows.push_back ({ revOn, { 806.f, 771.f, 802.f, 126.f } });
    addToggle (revOn, "title", { 816.f, 774.f, 205.f, 118.f }, "REVERB");
    knob (revMix, { revOn }, onMix (revOn), 1073, 830, 41, "REVERB MIX"); knob (revDecay, { revOn }, {}, 1193, 830, 41, "DUREE");
    knob (revSize, { revOn }, {}, 1305, 830, 41, "TAILLE"); knob (revDuck, { revOn }, {}, 1417, 828, 41, "DUCK");
    modeBox = addBox (revMode, { 1474.f, 796.f, 112.f, 104.f }, "TYPE DE REVERB");

    inKnob = knob (inGain, {}, {}, 538, 988, 32, "INPUT");
    outKnob = knob (outGain, {}, {}, 839, 988, 32, "OUTPUT");
    knob (limCeil, {}, {}, 695, 942, 22, "LIMITEUR");
    refresh();
    for (auto& pl : places)
    {
        pl.c->addMouseListener (this, false);
        if (auto* k = dynamic_cast<AmberKnob*> (pl.c)) k->slider.addMouseListener (this, false);
    }
}

SimpleView::~SimpleView()
{
    for (auto& pl : places)
    {
        pl.c->removeMouseListener (this);
        if (auto* k = dynamic_cast<AmberKnob*> (pl.c)) k->slider.removeMouseListener (this);
    }
    setLookAndFeel (nullptr);
}

void SimpleView::setHover (const juce::String& t)
{
    if (t == hover) return;
    hover = t;
    repaint (titleArea);
}

void SimpleView::hoverFrom (const juce::MouseEvent& e, bool leaving)
{
    juce::String t;
    if (! leaving)
        for (auto& pl : places)
        {
            auto* k = dynamic_cast<AmberKnob*> (pl.c);
            if (e.eventComponent != pl.c && ! (k != nullptr && e.eventComponent == &k->slider)) continue;
            t = pl.tip;
            if (t.isEmpty()) break;
            if (k != nullptr) t << "   " << k->slider.getTextFromValue (k->slider.getValue());
            else if (pl.c == &compMacro) t << "   " << juce::roundToInt (compMacro.getValue()) << " %";
            else if (auto* cb = dynamic_cast<juce::ComboBox*> (pl.c)) t << "   " << cb->getText();
            else if (auto* tb = dynamic_cast<juce::ToggleButton*> (pl.c)) t << (tb->getToggleState() ? "   ON" : "   OFF");
            break;
        }
    setHover (t);
}

void SimpleView::selectPreset (int i)
{
    proc.applyPreset (i);
    presets.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
}

void SimpleView::setComp (float m)
{
    const float thr = -8.f - 26.f * m, ratio = 1.5f + 4.5f * m;
    const float makeup = std::min (14.f, 0.45f * std::abs (thr) * (1.f - 1.f / ratio));
    setReal (proc, compOn, m > 0.001f ? 1.f : 0.f);
    setReal (proc, compThr, thr);
    setReal (proc, compRatio, ratio);
    setReal (proc, compMakeup, makeup);
    setReal (proc, comp2On, 1.f);
    setReal (proc, comp2Amt, 20.f + 50.f * m);
}

void SimpleView::pushLevels (float inPk, float outPk)
{
    // niveau IN mesure APRES le bouton IN (la zone TARGET se regle avec IN)
    const float pkIn = inPk * (float) std::pow (10.0, proc.getParam (inGain) / 20.0);
    auto sm = [] (float& l, float pk) { const float v = juce::jlimit (0.f, 1.f, ((float) linToDb (pk) + kDbMin) / kDbMin); l = v > l ? v : l - (l - v) * 0.33f; };
    sm (inLvl, pkIn); sm (outLvl, outPk);
    if (outLvl > 0.02f) phase += 0.18f;
    inHold = std::max (inLvl, inHold - 0.008f);
    if (measuring)
    {
        measurePk = std::max (measurePk, pkIn);
        if (--measureFrames <= 0)
        {
            measuring = false;
            autoIn.setButtonText ("AUTO IN");
            if (measurePk > 1e-4f)
                setReal (proc, inGain, juce::jlimit (-18.f, 18.f, proc.getParam (inGain) + (-13.5f - (float) linToDb (measurePk))));
        }
    }
    autoTick (proc, autoVoice, presets, autoShow);

    // l'anneau suit doucement la valeur du bouton COMP
    const float target = (float) compMacro.getValue() / 100.f;
    if (std::abs (arcShown - target) > 0.0015f) arcShown += (target - arcShown) * 0.40f; else arcShown = target;

    // ne redessine que ce qui a change (une etape sur deux : 15 images/s)
    if ((++orbTick & 1) != 0) return;
    const bool dIn = std::abs (inLvl - shownIn) > 0.004f, dOut = std::abs (outLvl - shownOut) > 0.004f;
    const float hold = inHold;
    const int status = hold < 0.03f ? 0 : (hold < kZoneLo ? 1 : (hold <= kZoneHi + 0.01f ? 2 : 3));
    if (dIn) repaint (inBar);
    if (status != shownStatus) { repaint (targetArea); shownStatus = status; }
    if (dOut) repaint (outBar);
    if (dOut || std::abs (phase - shownPhase) > 0.01f || std::abs (arcShown - shownArc) > 0.0005f) repaint (orbArea);
    if (dOut) repaint (sunArea);
    shownIn = inLvl; shownOut = outLvl; shownPhase = phase; shownArc = arcShown;
}

void SimpleView::refresh()
{
    if (autoShow <= 0 && presets.getSelectedItemIndex() != proc.getCurrentPreset())
        presets.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
    if (! compMacro.isMouseButtonDown())
    {
        const float m = proc.getParam (compOn) > 0.5f ? juce::jlimit (0.f, 1.f, (-8.f - proc.getParam (compThr)) / 26.f) : 0.f;
        syncing = true; compMacro.setValue (m * 100.0, juce::dontSendNotification); syncing = false;
    }
    compMacro.getProperties().set ("active", proc.getParam (compOn) > 0.5f);
    for (auto& o : owned) if (auto* k = dynamic_cast<AmberKnob*> (o.get())) k->refresh();
    for (auto& r : rows)
    {
        const bool on = proc.getParam (r.onIdx) > 0.5f;
        for (auto& pl : places)
            if (auto* k = dynamic_cast<AmberKnob*> (pl.c)) if (r.dimR.contains (pl.r.getCentre())) k->setAlpha (on ? 1.f : 0.6f);
    }
    const juce::String now[5] = { keyBox->getText(), scaleBox->getText(), noteBox->getText(), modeBox->getText(), presets.getText() };
    for (int i = 0; i < 5; ++i) if (shown[i] != now[i]) { shown[i] = now[i]; repaint(); }
    if (proc.getParam (tuneOn) > 0.5f && (++noteTick & 3) == 0) repaint (noteArea);
}

void SimpleView::resized()
{
    const float W = (float) getWidth(), H = (float) getHeight();
    scale = std::min (W / kDW, H / kDH);
    offX = (W - kDW * scale) * 0.5f; offY = (H - kDH * scale) * 0.5f;
    for (auto& pl : places) pl.c->setBounds (M (pl.r));
    orbArea = M (kRingCx - 262.f, kRingCy - 262.f, 524.f, 590.f).getIntersection (getLocalBounds());
    inBar = M (32.f, 945.f, 492.f, 88.f);
    outBar = M (862.f, 945.f, 488.f, 88.f);
    targetArea = M (290.f, 915.f, 230.f, 42.f);
    sunArea = M (0.f, 0.f, 330.f, 330.f);
    noteArea = M (1370.f, 128.f, 238.f, 60.f);
    titleArea = M (930.f, 128.f, 440.f, 60.f);

    // fond mis en cache a la taille de la fenetre (le design est centre, marges noires comme le cadre)
    bg = juce::Image (juce::Image::RGB, std::max (1, getWidth()), std::max (1, getHeight()), false);
    juce::Graphics g (bg);
    g.fillAll (juce::Colours::black);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (src, (int) std::round (offX), (int) std::round (offY), (int) std::round (kDW * scale), (int) std::round (kDH * scale),
                 0, 0, src.getWidth(), src.getHeight());
    if (folder.isValid())
        folderScaled = folder.rescaled (std::max (1, (int) std::round ((float) folder.getWidth() * scale * kDW / (float) src.getWidth())),
                                        std::max (1, (int) std::round ((float) folder.getHeight() * scale * kDH / (float) src.getHeight())),
                                        juce::Graphics::highResamplingQuality);
}

void SimpleView::paint (juce::Graphics& g)
{
    g.drawImageAt (bg, 0, 0);
    const float s = scale;
    const auto clip = g.getClipBounds();
    auto vis = [&] (juce::Rectangle<float> d) { return clip.intersects (M (d).expanded (3)); };
    const float lw = std::max (1.5f, 3.f * s);

    // soleil : halo qui respire avec la voix
    if (outLvl > 0.03f && vis ({ 0.f, 0.f, 330.f, 330.f }))
    {
        const auto c = juce::Point<float> (offX + 95.f * s, offY + 95.f * s);
        const float rad = (190.f + 90.f * outLvl) * s;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff2a0).withAlpha (0.55f * outLvl), c.x, c.y, juce::Colours::transparentBlack, c.x + rad, c.y, true));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2.f, rad * 2.f);
    }

    // anneau COMP + dossier
    if (vis ({ kRingCx - 262.f, kRingCy - 262.f, 524.f, 590.f }))
    {
        const auto c = juce::Point<float> (offX + kRingCx * s, offY + kRingCy * s);
        const float R = kRingR * s, th = 15.f * s;
        if (outLvl > 0.03f)      // onde autour de l'anneau
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f * outLvl));
            const float rr = R + th * (0.9f + 1.6f * outLvl);
            g.drawEllipse (c.x - rr, c.y - rr, rr * 2.f, rr * 2.f, std::max (1.5f, 4.f * s));
        }
        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - R, c.y - R, R * 2.f, R * 2.f, th);
        const float m = juce::jlimit (0.f, 1.f, arcShown), sweep = juce::degreesToRadians (300.f);
        const float a1 = juce::MathConstants<float>::pi, a0 = a1 - m * sweep;     // le vert part du bas (minimum) et monte par la droite
        if (m > 0.003f)
        {
            juce::Path arc; arc.addCentredArc (c.x, c.y, R, R, 0.f, a0, a1, true);
            g.setColour (kLime);
            g.strokePath (arc, juce::PathStrokeType (th, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
        {   // repere rouge fixe en bas = minimum
            g.setColour (juce::Colour (0xffff1010));
            g.fillRect (c.x - 4.5f * s, c.y + (kRingR - 20.f) * s, 9.f * s, 40.f * s);
        }
        if (folderScaled.isValid())     // le dossier EST le bouton : droit a 0 %, il tourne avec la valeur
        {
            const float grow = 1.f + 0.05f * outLvl, bob = std::sin (phase * 1.7f) * 4.f * s * outLvl;
            const float fw = (float) folderScaled.getWidth(), fh = (float) folderScaled.getHeight();
            if (m < 0.003f && outLvl < 0.02f) g.drawImageAt (folderScaled, juce::roundToInt (c.x - fw * 0.5f), juce::roundToInt (c.y - fh * 0.5f));
            else
            {
                g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
                g.drawImageTransformed (folderScaled, juce::AffineTransform::translation (-fw * 0.5f, -fh * 0.5f).scaled (grow).rotated (-m * sweep).translated (c.x, c.y + bob));
            }
        }
        inkText (g, juce::String (juce::roundToInt (arcShown * 100.f)) + "%", Rf (kRingCx - 90.f, 748.f, 180.f, 50.f), 44.f * s);
    }

    // rangees eteintes : on assombrit
    for (auto& r : rows)
        if (proc.getParam (r.onIdx) < 0.5f && vis (r.dimR))
        {
            g.setColour (juce::Colour (0xff0b1014).withAlpha (0.42f));
            g.fillRect (Rf (r.dimR.getX(), r.dimR.getY(), r.dimR.getWidth(), r.dimR.getHeight()));
        }

    // barre de titre de la fenetre : nom du reglage survole + note detectee
    if (vis ({ 930.f, 128.f, 680.f, 60.f }))
    {
        g.setFont (juce::Font (juce::FontOptions (30.f * s, juce::Font::bold)));
        g.setColour (hover.isEmpty() ? juce::Colour (0xff8fa0ab) : juce::Colour (0xffffffff));
        g.drawText (hover.isEmpty() ? juce::String ("Fck Tha VOICE") : hover, titleArea.toFloat(), juce::Justification::centredLeft, true);
        if (proc.getParam (tuneOn) > 0.5f && proc.engine.tuner().detMidi.load() > 1.f)
        {
            const float d = proc.engine.tuner().detMidi.load(), t = proc.engine.tuner().tgtMidi.load();
            g.setColour (kOrange);
            g.drawText (NoteDisplay::noteName (d) + " > " + NoteDisplay::noteName (t), noteArea.toFloat(), juce::Justification::centredRight, true);
        }
    }

    // petits noms sous les boutons du panneau (meme police que le reste)
    if (vis ({ 1030.f, 300.f, 560.f, 600.f }))
    {
        struct L { float x, y; const char* t; };
        static const L ls[] = { { 1122, 285, "LOW" }, { 1248, 285, "LOW MID" }, { 1376, 285, "HI MID" }, { 1530, 285, "HIGH" },
                                { 1418, 415, "SPEED" }, { 1530, 415, "HUMAN" },
                                { 1073, 552, "DE-ESS" }, { 1192, 552, "SAT" }, { 1305, 552, "PHONE" }, { 1417, 552, "DOUBLE" }, { 1530, 552, "CHORUS" },
                                { 1073, 692, "MIX" }, { 1194, 692, "REPEAT" }, { 1305, 692, "TONE" },
                                { 1073, 830, "MIX" }, { 1193, 830, "DECAY" }, { 1305, 830, "SIZE" }, { 1417, 828, "DUCK" } };
        for (auto& l : ls) inkText (g, l.t, Rf (l.x - 42.f, l.y + 41.f, 84.f, 19.f), 18.f * s);
    }

    // plaques du haut (le texte est a nous : le nom du preset change)
    if (vis ({ 1000.f, 8.f, 300.f, 90.f }))
    {
        const auto pr = Rf (1008.f, 14.f, 278.f, 78.f);
        plateRect (g, pr.reduced (lw * 0.5f), lw);
        const auto name = presets.getText().toUpperCase();
        const auto inner = pr.reduced (12.f * s, 4.f * s);
        juce::GlyphArrangement ga; ga.addLineOfText (plateFont (54.f * s), name, 0.f, 0.f);
        const int sp = name.length() / 2;
        int cut = -1;     // nom trop long : deux lignes, coupees sur l'espace le plus proche du milieu
        if (ga.getBoundingBox (0, -1, true).getWidth() > inner.getWidth())
            for (int d = 0; d < name.length() && cut < 0; ++d) { if (name[sp + d] == ' ') cut = sp + d; else if (sp - d >= 0 && name[sp - d] == ' ') cut = sp - d; }
        if (cut > 0)
        {
            inkText (g, name.substring (0, cut), inner.withTrimmedBottom (inner.getHeight() * 0.5f), 36.f * s);
            inkText (g, name.substring (cut + 1), inner.withTrimmedTop (inner.getHeight() * 0.5f), 36.f * s);
        }
        else inkText (g, name, inner, 54.f * s);
    }

    // plaques de la rangee TUNE / DLY / REV
    if (vis ({ 1176.f, 364.f, 190.f, 116.f }))
    {
        trapezoid (g, Rf (1182.f, 372.f, 80.f, 80.f), 0.f, 12.f * s, lw);
        inkText (g, keyBox->getText(), Rf (1190.f, 372.f, 64.f, 80.f), 66.f * s);
        arrowDown (g, offX + 1222.f * s, offY + 460.f * s, 52.f * s, s);
        static const char* shortScale[5] = { "Chr", "Maj", "Min", "PMj", "PMn" };
        trapezoid (g, Rf (1268.f, 372.f, 84.f, 72.f), 12.f * s, 0.f, lw);
        inkText (g, shortScale[juce::jlimit (0, 4, scaleBox->getSelectedItemIndex())], Rf (1274.f, 374.f, 72.f, 68.f), 40.f * s);
        arrowDown (g, offX + 1310.f * s, offY + 458.f * s, 52.f * s, s);
    }
    if (vis ({ 1376.f, 658.f, 90.f, 118.f }))
    {
        plateRect (g, Rf (1382.f, 662.f, 72.f, 68.f).reduced (lw * 0.5f), lw);
        inkText (g, noteBox->getText(), Rf (1384.f, 664.f, 68.f, 64.f), 46.f * s);
        arrowDown (g, offX + 1418.f * s, offY + 746.f * s, 42.f * s, s);
    }
    if (vis ({ 1474.f, 796.f, 112.f, 104.f }))
    {
        plateRect (g, Rf (1480.f, 800.f, 100.f, 68.f).reduced (lw * 0.5f), lw);
        inkText (g, modeBox->getText().toUpperCase(), Rf (1482.f, 802.f, 96.f, 64.f), 44.f * s);
        arrowDown (g, offX + 1530.f * s, offY + 880.f * s, 42.f * s, s);
    }

    // barres IN / OUT : parallelogrammes comme le design
    auto bar = [&] (float xl, float xr, float ylt, float yrt, float ylb, float yrb, float lvl, juce::Colour fill, bool outline)
    {
        juce::Path tr;
        tr.startNewSubPath (offX + xl * s, offY + ylt * s); tr.lineTo (offX + xr * s, offY + yrt * s);
        tr.lineTo (offX + xr * s, offY + yrb * s); tr.lineTo (offX + xl * s, offY + ylb * s); tr.closeSubPath();
        g.setColour (juce::Colours::black); g.fillPath (tr);
        if (lvl > 0.004f)
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (tr);
            const float xf = offX + (xl + (xr - xl) * lvl) * s;
            g.setGradientFill (juce::ColourGradient (fill.brighter (0.15f), 0.f, offY + yrt * s, fill.darker (0.35f), 0.f, offY + ylb * s, false));
            g.fillRect (juce::Rectangle<float> (offX + xl * s, offY + std::min (ylt, yrt) * s, xf - (offX + xl * s), (std::max (ylb, yrb) - std::min (ylt, yrt)) * s));
        }
        if (outline) { g.setColour (juce::Colours::black); g.strokePath (tr, juce::PathStrokeType (lw)); }
    };
    const float hold = inHold;
    const int status = hold < 0.03f ? 0 : (hold < kZoneLo ? 1 : (hold <= kZoneHi + 0.01f ? 2 : 3));
    if (clip.intersects (inBar))
    {
        bar (kBarX0, kBarX1, 957.f, 965.f, 1026.f, 1016.f, inLvl, status == 3 ? kHot : kLime, true);
        for (float z : { kZoneLo, kZoneHi })      // reperes rouges de la zone TARGET
        {
            const float x = offX + (kBarX0 + (kBarX1 - kBarX0) * z) * s;
            g.setColour (juce::Colour (0xffff1010)); g.fillRect (x - 3.5f * s, offY + 950.f * s, 7.f * s, 76.f * s);
        }
        if (hold > 0.03f)     // maintien du pic
        {
            const float x = offX + (kBarX0 + (kBarX1 - kBarX0) * hold) * s;
            g.setColour (juce::Colours::white.withAlpha (0.9f)); g.fillRect (x - 1.5f * s, offY + 960.f * s, 3.f * s, 58.f * s);
        }
    }
    if (clip.intersects (targetArea))
    {
        static const char* msg[4] = { "TARGET", "TOO LOW", "TARGET OK", "TOO LOUD" };
        const juce::Colour col[4] = { kLime, kOrange, kLime, kHot };
        const float zc = kBarX0 + (kBarX1 - kBarX0) * (kZoneLo + kZoneHi) * 0.5f;
        inkText (g, msg[status], Rf (zc - 100.f, 918.f, 200.f, 36.f), 32.f * s, col[status]);
    }
    if (clip.intersects (outBar))
        bar (872.f, 1340.f, 961.f, 955.f, 1016.f, 1024.f, outLvl, outLvl > 0.97f ? kHot : kOrange, true);
}


//==============================================================================
// DE-ESSER plein ecran
static const juce::Colour dsYellow { 0xffffd23c }, dsGreen { 0xff8be04a }, dsBg1 { 0xff15110d }, dsBg2 { 0xff070605 };

float DsBandBar::xOf (double f) const
{
    const float m = 12.f, w = (float) getWidth() - 2.f * m;
    return m + w * (float) (std::log (f / 2000.0) / std::log (18000.0 / 2000.0));
}
double DsBandBar::fOf (float x) const
{
    const float m = 12.f, w = (float) getWidth() - 2.f * m;
    return 2000.0 * std::pow (18000.0 / 2000.0, juce::jlimit (0.f, 1.f, (x - m) / w));
}
void DsBandBar::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float H = r.getHeight();
    const float txtH = std::clamp (H * 0.30f, 9.f, 12.f);
    const float s = std::clamp (H * 0.17f, 5.f, 8.f);                 // taille des poignees
    const float th = std::clamp (H * 0.22f, 6.f, 11.f);               // epaisseur de la piste
    const float ty = s + 2.f;
    const float x0 = 12.f, x1 = r.getWidth() - 12.f;
    g.setColour (juce::Colour (0xff0b0907));
    g.fillRoundedRectangle (x0, ty, x1 - x0, th, 2.f);
    g.setColour (col::edge.brighter (0.3f));
    g.drawRoundedRectangle (x0, ty, x1 - x0, th, 2.f, 1.f);
    for (double f : { 2000.0, 3000.0, 4000.0, 6000.0, 8000.0, 10000.0, 14000.0 })     // graduations (traits seuls)
    {
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        g.drawVerticalLine ((int) xOf (f), ty + 1, ty + th - 1);
    }
    const double lo = proc.getParam (deessFreq), hi = std::max ((double) proc.getParam (deessHigh), lo * 1.25);
    const float xl = xOf (lo), xh = xOf (hi);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb81c).withAlpha (0.60f), 0, ty, juce::Colour (0xffd98a00).withAlpha (0.35f), 0, ty + th, false));
    g.fillRect (xl, ty + 1, xh - xl, th - 2);
    // poignees : triangles qui pointent vers la bande depuis l'EXTERIEUR (jamais l'un sur l'autre)
    for (int k = 0; k < 2; ++k)
    {
        const float x = k == 0 ? xl : xh, dir = k == 0 ? -1.f : 1.f;
        juce::Path tri;
        tri.addTriangle (x, ty + th + 1.f, x, ty - s, x + dir * s * 1.2f, ty - s);
        g.setColour (juce::Colour (0xffe9e3d8)); g.fillPath (tri);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.strokePath (tri, juce::PathStrokeType (0.8f));
        g.setColour (juce::Colour (0xffffd23c)); g.drawVerticalLine ((int) x, ty, ty + th);
    }
    g.setColour (juce::Colour (0xffffd23c));
    g.setFont (bold (txtH));
    const float yt = ty + th + 3.f;
    g.drawText (juce::String (lo / 1000.0, 1) + " kHz", 2, (int) yt, 70, (int) txtH + 2, juce::Justification::centredLeft);
    g.drawText (juce::String (hi / 1000.0, 1) + " kHz", getWidth() - 72, (int) yt, 70, (int) txtH + 2, juce::Justification::centredRight);
}
void DsBandBar::mouseDown (const juce::MouseEvent& e)
{
    const double lo = proc.getParam (deessFreq), hi = std::max ((double) proc.getParam (deessHigh), lo * 1.25);
    const float x = e.position.x, xl = xOf (lo), xh = xOf (hi);
    drag = std::abs (x - xl) < 14.f && std::abs (x - xl) <= std::abs (x - xh) ? 0 : (std::abs (x - xh) < 14.f ? 1 : (x > xl && x < xh ? 2 : (std::abs (x - xl) < std::abs (x - xh) ? 0 : 1)));
    grabLo = lo; grabHi = hi; grabX = x;
    for (int id : { (int) deessFreq, (int) deessHigh }) proc.apvts.getParameter (kDefs[id].id)->beginChangeGesture();
}
void DsBandBar::mouseDrag (const juce::MouseEvent& e)
{
    if (drag < 0) return;
    const auto& dl = kDefs[deessFreq]; const auto& dh = kDefs[deessHigh];
    double lo = grabLo, hi = grabHi;
    if (drag == 2)
    {
        const double ratio = fOf (e.position.x) / fOf (grabX);
        lo = grabLo * ratio; hi = grabHi * ratio;
        if (lo < dl.lo) { hi *= dl.lo / lo; lo = dl.lo; }
        if (hi > dh.hi) { lo *= dh.hi / hi; hi = dh.hi; }
    }
    else if (drag == 0) lo = fOf (e.position.x);
    else hi = fOf (e.position.x);
    lo = juce::jlimit ((double) dl.lo, (double) dl.hi, lo);
    hi = juce::jlimit ((double) dh.lo, (double) dh.hi, hi);
    if (drag == 0) lo = std::min (lo, hi / 1.25);
    if (drag == 1) hi = std::max (hi, lo * 1.25);
    for (auto [id, v] : { std::pair<int, double> { (int) deessFreq, lo }, std::pair<int, double> { (int) deessHigh, hi } })
        if (auto* p = proc.apvts.getParameter (kDefs[id].id)) p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    repaint();
}
void DsBandBar::mouseUp (const juce::MouseEvent&)
{
    if (drag < 0) return;
    for (int id : { (int) deessFreq, (int) deessHigh }) proc.apvts.getParameter (kDefs[id].id)->endChangeGesture();
    drag = -1;
}

void DsSeg::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.f);
    const int sel = proc.getParam (idx) > 0.5f ? 1 : 0;
    for (int k = 0; k < 2; ++k)
    {
        auto b = k == 0 ? r.withWidth (r.getWidth() / 2) : r.withTrimmedLeft (r.getWidth() / 2);
        const bool on = k == sel;
        g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff4a3a14) : juce::Colour (0xff1e1a15), 0, b.getY(), on ? juce::Colour (0xff2b200a) : juce::Colour (0xff100d0a), 0, b.getBottom(), false));
        g.fillRoundedRectangle (b.reduced (1.f), 4.f);
        g.setColour (on ? dsYellow.withAlpha (0.9f) : col::edge.brighter (0.4f));
        g.drawRoundedRectangle (b.reduced (1.f), 4.f, 1.1f);
        g.setColour (on ? dsYellow : juce::Colours::white.withAlpha (0.55f));
        g.setFont (bold (std::clamp (r.getHeight() * 0.32f, 9.f, 13.f)));
        g.drawFittedText ((k == 0 ? n0 : n1).toUpperCase(), b.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}
void DsSeg::mouseDown (const juce::MouseEvent& e)
{
    setReal (proc, idx, e.position.x > (float) getWidth() * 0.5f ? 1.f : 0.f);
    repaint();
}

void DsListen::paint (juce::Graphics& g)
{
    const bool on = proc.getParam (deessListen) > 0.5f;
    const auto r = getLocalBounds().toFloat().reduced (3.f);
    const float d = std::min (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const auto circle = juce::Rectangle<float> (d, d).withCentre (c);
    if (on) { g.setColour (dsYellow.withAlpha (0.22f)); g.fillEllipse (circle.expanded (d * 0.12f)); }
    g.setGradientFill (juce::ColourGradient (on ? juce::Colour (0xff5b4512) : juce::Colour (0xff2a2520), c.x, circle.getY(), on ? juce::Colour (0xff2b200a) : juce::Colour (0xff0f0d0b), c.x, circle.getBottom(), false));
    g.fillEllipse (circle);
    g.setColour (on ? dsYellow : col::edge.brighter (0.5f));
    g.drawEllipse (circle, 1.6f);
    // casque
    g.setColour (on ? dsYellow : juce::Colours::white.withAlpha (0.65f));
    juce::Path hp;
    const float rr = d * 0.26f;
    hp.addArc (c.x - rr, c.y - rr, rr * 2, rr * 2, -1.62f, 1.62f, true);
    g.strokePath (hp, juce::PathStrokeType (d * 0.055f));
    g.fillRoundedRectangle (c.x - rr - d * 0.06f, c.y - d * 0.02f, d * 0.12f, d * 0.24f, d * 0.04f);
    g.fillRoundedRectangle (c.x + rr - d * 0.06f, c.y - d * 0.02f, d * 0.12f, d * 0.24f, d * 0.04f);
}
void DsListen::mouseDown (const juce::MouseEvent&)
{
    setReal (proc, deessListen, proc.getParam (deessListen) > 0.5f ? 0.f : 1.f);
    repaint();
}

DsMini::DsMini (YazaVoxProcessor& p)
    : proc (p), mode (p, deessMode, "Vocal", "Allround"), band (p, deessBand, "Split", "Wide"), bar (p), listen (p)
{
    power.setClickingTogglesState (true);
    addAndMakeVisible (power);
    powerAtt = std::make_unique<ButtonAtt> (p.apvts, "deessOn", power);
    power.onClick = [this] { if (power.getToggleState()) setReal (proc, fxOn, 1.f); };
    thr = std::make_unique<KnobCell> (p, deessAmt);
    rng = std::make_unique<KnobCell> (p, deessRange);
    for (auto* c : { (juce::Component*) thr.get(), (juce::Component*) rng.get(), (juce::Component*) &mode, (juce::Component*) &band, (juce::Component*) &bar, (juce::Component*) &listen }) addAndMakeVisible (*c);
    startTimerHz (30);
}

void DsMini::resized()
{
    auto r = getLocalBounds().reduced (3, 2);
    const int H = r.getHeight();
    auto top = r.removeFromTop (std::clamp (H / 13, 16, 22));
    power.setBounds (top.removeFromLeft (top.getHeight()).reduced (top.getHeight() / 5));
    titleR = top;
    r.removeFromTop (2);
    screen = r.removeFromTop ((int) ((float) H * 0.30f));
    r.removeFromTop (3);
    auto bot = r.removeFromBottom ((int) ((float) H * 0.19f));          // bande de frequences
    bar.setBounds (bot);
    r.removeFromBottom (2);
    auto segs = r.removeFromBottom (std::clamp (H / 11, 15, 22));       // modes
    const int sw = segs.getWidth() / 2;
    mode.setBounds (segs.removeFromLeft (sw).reduced (1, 0));
    band.setBounds (segs.reduced (1, 0));
    r.removeFromBottom (2);
    // knobs + vu-metres + ecoute
    const int mw = std::clamp (r.getWidth() / 6, 14, 22);
    meters = r.removeFromRight (mw * 2 + 6);
    const int lw = std::clamp (r.getWidth() / 5, 22, 40);
    listen.setBounds (r.removeFromLeft (lw).withSizeKeepingCentre (lw, lw).translated (0, 0));
    const int kw = r.getWidth() / 2;
    thr->setBounds (r.removeFromLeft (kw));
    rng->setBounds (r);
}

void DsMini::paint (juce::Graphics& g)
{
    g.setColour (col::redBright.withAlpha (0.9f));
    g.setFont (bold (std::clamp ((float) titleR.getHeight() * 0.6f, 9.f, 12.f)));
    g.drawText ("DE-ESSER", titleR, juce::Justification::centredLeft);

    // ---- ecran forme d'onde ----
    const auto dr = screen.toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1010), 0, dr.getY(), juce::Colour (0xff060404), 0, dr.getBottom(), false));
    g.fillRoundedRectangle (dr, 4.f);
    g.saveState();
    g.reduceClipRegion (screen.reduced (1));
    const float cy = dr.getCentreY(), hh = dr.getHeight() * 0.5f - 3.f;
    const auto& sc = proc.engine.dsScope();
    constexpr int N = DsScope::N;
    const int pos = sc.pos.load (std::memory_order_acquire);
    const float bw = (dr.getWidth() - 4.f) / (float) N;
    auto hOf = [hh] (float lin) { return hh * std::sqrt (juce::jlimit (0.f, 1.f, lin)); };
    // ligne de seuil (ligne rouge du croquis) : niveau d'entree a partir duquel ca corrige
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawHorizontalLine ((int) cy, dr.getX() + 2, dr.getRight() - 2);
    juce::Path top, bot;
    for (int k = 0; k < N; ++k)
    {
        const int i = (pos + k) % N;
        const float x = dr.getX() + 2.f + (float) k * bw;
        const float h = hOf (sc.in[(size_t) i].load (std::memory_order_relaxed));
        if (k == 0) { top.startNewSubPath (x, cy - h); bot.startNewSubPath (x, cy + h); }
        else { top.lineTo (x, cy - h); bot.lineTo (x, cy + h); }
    }
    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.strokePath (top, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
    g.strokePath (bot, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved));
    // sibilances corrigees : portion retiree en jaune, portion conservee en vert
    for (int k = 0; k < N; ++k)
    {
        const int i = (pos + k) % N;
        const float gr = sc.gr[(size_t) i].load (std::memory_order_relaxed);
        if (gr > -0.25f) continue;
        const float in = sc.in[(size_t) i].load (std::memory_order_relaxed);
        const float x = dr.getX() + 2.f + (float) k * bw;
        const float hIn = hOf (in), hOut = hOf (in * (float) dbToLin (gr));
        g.setColour (juce::Colour (0xffffe03a));
        g.fillRect (x, cy - hIn, bw + 0.4f, hIn - hOut); g.fillRect (x, cy + hOut, bw + 0.4f, hIn - hOut);
        g.setColour (juce::Colour (0xff6fe04a));
        g.fillRect (x, cy - hOut, bw + 0.4f, std::max (1.5f, hOut * 0.35f)); g.fillRect (x, cy + hOut - std::max (1.5f, hOut * 0.35f), bw + 0.4f, std::max (1.5f, hOut * 0.35f));
    }
    g.restoreState();
    g.setColour (col::redBright.withAlpha (0.85f));
    g.drawRoundedRectangle (dr.reduced (0.5f), 4.f, 1.3f);

    // ---- vu-metres (reduction + sortie) ----
    const float mw = (float) meters.getWidth() * 0.5f - 2.f;
    const auto mb = meters.toFloat().reduced (0.f, 4.f);
    const float grNow = -proc.engine.dsGrNow();
    grSmooth = grNow > grSmooth ? grNow : grSmooth + (grNow - grSmooth) * 0.18f;
    const float outLin = proc.engine.outPk.load();
    const float fo = juce::jlimit (0.f, 1.f, (20.f * std::log10 (outLin + 1e-6f) + 48.f) / 48.f);
    for (int k = 0; k < 2; ++k)
    {
        const juce::Rectangle<float> m (mb.getX() + (float) k * (mw + 4.f), mb.getY() + 10.f, mw, mb.getHeight() - 10.f);
        g.setColour (juce::Colour (0xff0b0807)); g.fillRect (m);
        const float f = k == 0 ? juce::jlimit (0.f, 1.f, grSmooth / 24.f) : fo;
        if (k == 0) g.setColour (juce::Colour (0xffffd23c)); else g.setColour (juce::Colour (0xff6fe04a));
        if (k == 0) g.fillRect (m.withHeight (m.getHeight() * f));
        else g.fillRect (m.withTop (m.getBottom() - m.getHeight() * f));
        g.setColour (juce::Colour (0xffffd23c).withAlpha (0.9f));
        g.drawRect (m, 1.f);
    }
    g.setColour (col::textDim);
    g.setFont (bold (8.f));
    g.drawText ("GR", (int) mb.getX(), (int) mb.getY(), (int) mw + 2, 10, juce::Justification::centred);
    g.drawText ("OUT", (int) (mb.getX() + mw + 3), (int) mb.getY(), (int) mw + 4, 10, juce::Justification::centred);
}

//==============================================================================
YazaVoxEditor::YazaVoxEditor (YazaVoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), inKnob (p, inGain), outKnob (p, outGain), limKnob (p, limCeil), eqView (p, false), cutView (p, true, &eqView), noteDisp (p),
      comp (p, MComp, 6), eq (p, MEq, 3, { "FILTERS", "LOW SHELF", "LOW MID", "HIGH MID", "HIGH SHELF" }), fx (p, MFx, 6, { "DE-ESSER" }), dly (p, MDly, 7), rev (p, MRev, 7), tune (p, MTune, 4)
{
    setLookAndFeel (&laf);

    presets.addItemList (YazaVoxProcessor::presetNames(), 1);
    presets.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
    presets.onChange = [this] { selectPreset (presets.getSelectedItemIndex()); };
    addAndMakeVisible (presets);

    prevBtn.onClick = [this] { const int n = YazaVoxProcessor::presetNames().size(); selectPreset ((proc.getCurrentPreset() + n - 1) % n); };
    nextBtn.onClick = [this] { selectPreset ((proc.getCurrentPreset() + 1) % YazaVoxProcessor::presetNames().size()); };
    abBtn.onClick = [this] { proc.switchAB(); };
    copyBtn.onClick = [this] { proc.copyToOther(); };
    resetBtn.onClick = [this] { proc.resetAll(); };
    simpleBtn.onClick = [this] { proc.uiSimple = true; applyMode(); };
    autoBtn.onClick = [this] { autoClick (proc); };
    for (auto* b : { &prevBtn, &nextBtn, &abBtn, &copyBtn, &resetBtn, &simpleBtn, &autoBtn }) addAndMakeVisible (*b);

    bypassBtn.setClickingTogglesState (true);
    addAndMakeVisible (bypassBtn);
    bypassAtt = std::make_unique<ButtonAtt> (proc.apvts, "bypass", bypassBtn);

    addAndMakeVisible (inKnob); addAndMakeVisible (outKnob); addAndMakeVisible (limKnob);
    addAndMakeVisible (inMeter); addAndMakeVisible (outMeter);

    comp.headerExtra = &grMeter; comp.addAndMakeVisible (grMeter);
    dsMini = std::make_unique<DsMini> (proc);
    fx.extraLeft = dsMini.get(); fx.addAndMakeVisible (*dsMini); fx.extraLeftFrac = 0.43f;
    eq.extraTop = &eqView; eq.addAndMakeVisible (eqView); eq.extraFrac = 0.52f;
    eq.extraLeft = &cutView; eq.addAndMakeVisible (cutView); eq.extraLeftFrac = 0.62f;
    tune.headerExtra = &noteDisp; tune.addAndMakeVisible (noteDisp);
    for (auto* m : { &comp, &eq, &fx, &dly, &rev, &tune }) addAndMakeVisible (*m);

    simple = std::make_unique<SimpleView> (proc, [this] { proc.uiSimple = false; applyMode(); });
    addAndMakeVisible (*simple);

    applyMode();

    setResizable (true, true);
    setResizeLimits (900, 600, 1900, 1200);
    getConstrainer()->setFixedAspectRatio (0.0);
    setSize (1180, 740);
    startTimerHz (30);
}

YazaVoxEditor::~YazaVoxEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void YazaVoxEditor::applyMode()
{
    const bool s = proc.uiSimple.load();
    simple->setVisible (s);
    simple->toFront (false);
    if (s) simple->refresh();
}

void YazaVoxEditor::selectPreset (int i)
{
    proc.applyPreset (i);
    presets.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
}

void YazaVoxEditor::timerCallback()
{
    const float inP = proc.engine.inPk.exchange (0.f), outP = proc.engine.outPk.exchange (0.f);
    inMeter.push (inP);
    outMeter.push (outP);
    if (simple->isVisible()) simple->pushLevels (inP, outP);
    else autoTick (proc, autoBtn, presets, autoShow);
    grMeter.push (proc.engine.grDb.load());
    if (! simple->isVisible()) { eqView.update(); cutView.update(); noteDisp.update(); }

    if ((tick++ & 3) == 0)
    {
        if (simple->isVisible()) simple->refresh();
        for (auto* m : { &comp, &eq, &fx, &dly, &rev, &tune }) m->refresh();
        if (autoShow <= 0 && presets.getSelectedItemIndex() != proc.getCurrentPreset())
            presets.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
        const juce::String ab = proc.getAB() == 0 ? "A" : "B";
        if (abBtn.getButtonText() != ab) abBtn.setButtonText (ab);
        const juce::String cp = proc.getAB() == 0 ? "A>B" : "B>A";
        if (copyBtn.getButtonText() != cp) copyBtn.setButtonText (cp);
    }
}

void YazaVoxEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // lueur de fond
    g.setGradientFill (juce::ColourGradient (col::red.withAlpha (0.10f), (float) getWidth() * 0.15f, 0.f,
                                             juce::Colours::transparentBlack, (float) getWidth() * 0.55f, (float) getHeight() * 0.5f, true));
    g.fillRect (getLocalBounds());
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff160b0c), 0, 0, col::bg, 0, (float) headerRect.getBottom(), false));
    g.fillRect (headerRect);
    g.setColour (col::red);
    g.fillRect (0, headerRect.getBottom() - 1, getWidth(), 2);

    g.setColour (col::text);
    g.setFont (bold ((float) logoRect.getHeight() * 0.40f));
    g.drawFittedText ("Fck Tha", logoRect.withWidth (logoRect.getWidth() * 47 / 100), juce::Justification::centredRight, 1, 0.7f);
    g.setColour (col::redBright);
    g.drawFittedText ("VOICE", logoRect.withTrimmedLeft (logoRect.getWidth() * 47 / 100 + 2), juce::Justification::centredLeft, 1, 0.7f);
    g.setColour (col::textDim);
    g.setFont (plain (std::clamp ((float) logoRect.getHeight() * 0.17f, 8.f, 11.f)));
    g.drawText ("VOCAL SUITE", logoRect.removeFromBottom (14), juce::Justification::centred);
}

void YazaVoxEditor::resized()
{
    simple->setBounds (getLocalBounds());
    auto r = getLocalBounds();
    const int W = r.getWidth(), H = r.getHeight();
    const int hh = std::clamp ((int) (H * 0.135f), 82, 120);
    headerRect = r.removeFromTop (hh);
    auto h = headerRect.reduced (14, 6);

    logoRect = h.removeFromLeft (std::clamp (W / 6, 150, 230));

    // droite : IN knob, meters, OUT knob, bypass
    auto right = h.removeFromRight (std::clamp (W * 40 / 100, 330, 520));
    const int bw = std::clamp (right.getWidth() / 5, 62, 96);
    bypassBtn.setBounds (right.removeFromRight (bw).withSizeKeepingCentre (bw, std::clamp (hh / 3, 26, 36)));
    right.removeFromRight (8);
    const int kw = std::clamp (right.getWidth() / 4, 64, 100);
    outKnob.setBounds (right.removeFromRight (kw));
    outMeter.setBounds (right.removeFromRight (right.getHeight() > 0 ? 22 : 0).reduced (0, 0));
    right.removeFromRight (4);
    inMeter.setBounds (right.removeFromRight (22));
    right.removeFromRight (4);
    inKnob.setBounds (right.removeFromRight (kw));
    right.removeFromRight (4);
    limKnob.setBounds (right.removeFromRight (kw));

    // centre : presets, A/B, copy, reset
    h.removeFromRight (10);
    const int rowH = std::clamp (hh / 3, 26, 34);
    auto c = h.withSizeKeepingCentre (h.getWidth(), rowH * 2 + 6);
    auto row1 = c.removeFromTop (rowH);
    c.removeFromTop (6);
    auto row2 = c;
    prevBtn.setBounds (row1.removeFromLeft (rowH));
    nextBtn.setBounds (row1.removeFromRight (rowH));
    presets.setBounds (row1.reduced (6, 0));
    const int q = row2.getWidth() / 6;
    abBtn.setBounds (row2.removeFromLeft (q).reduced (3, 0));
    copyBtn.setBounds (row2.removeFromLeft (q).reduced (3, 0));
    resetBtn.setBounds (row2.removeFromLeft (q).reduced (3, 0));
    autoBtn.setBounds (row2.removeFromLeft (q * 2).reduced (3, 0));
    simpleBtn.setBounds (row2.reduced (3, 0));

    // corps
    auto body = r.reduced (10, 8);
    const int rowA = (int) ((float) body.getHeight() * 0.50f);
    auto a = body.removeFromTop (rowA);
    body.removeFromTop (8);
    auto b = body;
    const int gap = 8;
    const int aw = a.getWidth();
    comp.setBounds (a.removeFromLeft ((int) ((float) aw * 0.26f)));
    a.removeFromLeft (gap);
    eq.setBounds (a.removeFromLeft ((int) ((float) aw * 0.42f)));
    a.removeFromLeft (gap);
    tune.setBounds (a);
    fx.setBounds (b.removeFromLeft ((int) ((float) b.getWidth() * 0.43f)));
    b.removeFromLeft (gap);
    dly.setBounds (b.removeFromLeft ((int) ((float) b.getWidth() * 0.55f)));
    b.removeFromLeft (gap);
    rev.setBounds (b);
}
} // namespace yvui
