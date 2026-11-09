#include "PluginEditor.h"

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
EqView::EqView (YazaVoxProcessor& p) : proc (p)
{
    bands = { BandIdx { lsF, lsG, lsQ }, BandIdx { lmF, lmG, lmQ }, BandIdx { hmF, hmG, hmQ }, BandIdx { hsF, hsG, hsQ } };
    spec.fill (-120.f);
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
    return (float) getHeight() * (0.5f - (float) db / 36.f);   // +-18 dB
}

void EqView::update()
{
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
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff130b0c), 0, 0, juce::Colour (0xff070505), 0, r.getBottom(), false));
    g.fillRoundedRectangle (r, 6.f);

    // grille
    g.setFont (plain (std::clamp (r.getHeight() * 0.075f, 8.f, 11.f)));
    for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
    {
        const float x = xOf (f);
        g.setColour (juce::Colour (0xff241517));
        g.drawVerticalLine ((int) x, 0.f, r.getBottom());
        g.setColour (col::textDim.withAlpha (0.7f));
        g.drawText (f >= 1000 ? juce::String (f / 1000.0, f >= 10000 ? 0 : (f == 1000.0 || f == 2000.0 || f == 5000.0 ? 0 : 1)) + "k" : juce::String ((int) f),
                    (int) x + 2, (int) r.getBottom() - 13, 30, 12, juce::Justification::left);
    }
    for (int db : { -12, -6, 0, 6, 12 })
    {
        g.setColour (db == 0 ? juce::Colour (0xff3a1a1e) : juce::Colour (0xff1c1112));
        g.drawHorizontalLine ((int) yOfDb (db), 0.f, r.getRight());
    }

    // spectre (reference : 0 dBFS en haut, -90 en bas)
    const double sr = proc.getSampleRate_();
    juce::Path sp;
    const int W = getWidth();
    bool started = false;
    for (int px = 0; px < W; px += 2)
    {
        const double f0 = fOf ((float) px), f1 = fOf ((float) px + 2.f);
        const int b0 = std::max (1, (int) (f0 / sr * kN)), b1 = std::max (b0 + 1, std::min (kN / 2 - 1, (int) (f1 / sr * kN) + 1));
        float m = -120.f;
        for (int b = b0; b < b1; ++b) m = std::max (m, spec[(size_t) b]);
        const float y = r.getHeight() * (1.f - juce::jlimit (0.f, 1.f, (m + 90.f) / 90.f));
        if (! started) { sp.startNewSubPath (0.f, r.getBottom()); sp.lineTo ((float) px, y); started = true; }
        else sp.lineTo ((float) px, y);
    }
    if (started)
    {
        sp.lineTo ((float) W, r.getBottom()); sp.closeSubPath();
        g.setGradientFill (juce::ColourGradient (col::red.withAlpha (0.55f), 0, 0, col::redDark.withAlpha (0.05f), 0, r.getBottom(), false));
        g.fillPath (sp);
    }

    // courbe de reponse
    float v[Count];
    for (int i = 0; i < Count; ++i) v[i] = proc.getParam (i);
    const bool eqActive = v[eqOn] > 0.5f;
    juce::Path curve;
    for (int px = 0; px <= W; px += 2)
    {
        const double db = EQ::responseDb (v, sr, fOf ((float) px));
        const float y = yOfDb (juce::jlimit (-18.0, 18.0, db));
        if (px == 0) curve.startNewSubPath (0.f, y); else curve.lineTo ((float) px, y);
    }
    g.setColour (col::redBright.withAlpha (eqActive ? 0.35f : 0.12f));
    g.strokePath (curve, juce::PathStrokeType (5.f));
    g.setColour (eqActive ? juce::Colours::white : col::textDim);
    g.strokePath (curve, juce::PathStrokeType (1.8f));

    // poignees
    for (size_t i = 0; i < bands.size(); ++i)
    {
        const auto& b = bands[i];
        const juce::Point<float> c (xOf (v[b.f]), yOfDb (juce::jlimit (-18.0, 18.0, (double) v[b.g])));
        const bool on = std::abs (v[b.g]) > 0.01f;
        g.setColour (col::red.withAlpha (on ? 0.3f : 0.12f));
        g.fillEllipse (c.x - 11.f, c.y - 11.f, 22.f, 22.f);
        g.setColour (on || dragBand == (int) i ? col::redBright : col::redDark);
        g.fillEllipse (c.x - 5.f, c.y - 5.f, 10.f, 10.f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawEllipse (c.x - 5.f, c.y - 5.f, 10.f, 10.f, 1.2f);
    }
    g.setColour (col::edge);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.f, 1.f);
}

int EqView::hit (juce::Point<float> p) const
{
    int best = -1; float bd = 18.f;
    for (size_t i = 0; i < bands.size(); ++i)
    {
        const juce::Point<float> c (xOf (proc.getParam (bands[i].f)), yOfDb (juce::jlimit (-18.0, 18.0, (double) proc.getParam (bands[i].g))));
        const float d = c.getDistanceFrom (p);
        if (d < bd) { bd = d; best = (int) i; }
    }
    return best;
}

void EqView::setNorm (int idx, float real, bool)
{
    if (auto* p = proc.apvts.getParameter (kDefs[idx].id))
        p->setValueNotifyingHost (p->convertTo0to1 (real));
}

void EqView::mouseDown (const juce::MouseEvent& e)
{
    dragBand = hit (e.position);
    if (dragBand >= 0)
    {
        proc.apvts.getParameter (kDefs[bands[(size_t) dragBand].f].id)->beginChangeGesture();
        proc.apvts.getParameter (kDefs[bands[(size_t) dragBand].g].id)->beginChangeGesture();
    }
}
void EqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragBand < 0) return;
    const auto& b = bands[(size_t) dragBand];
    const auto& df = kDefs[b.f]; const auto& dg = kDefs[b.g];
    const float f = juce::jlimit (df.lo, df.hi, (float) fOf (e.position.x));
    const float db = juce::jlimit (dg.lo, dg.hi, (0.5f - e.position.y / (float) getHeight()) * 36.f);
    setNorm (b.f, f, false);
    setNorm (b.g, std::round (db * 10.f) / 10.f, false);
}
void EqView::mouseUp (const juce::MouseEvent&)
{
    if (dragBand >= 0)
    {
        proc.apvts.getParameter (kDefs[bands[(size_t) dragBand].f].id)->endChangeGesture();
        proc.apvts.getParameter (kDefs[bands[(size_t) dragBand].g].id)->endChangeGesture();
    }
    dragBand = -1;
}
void EqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hit (e.position);
    if (h >= 0) setNorm (bands[(size_t) h].g, 0.f, false);
}
void EqView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int h = hit (e.position);
    if (h < 0) return;
    const auto& q = kDefs[bands[(size_t) h].q];
    const float nq = juce::jlimit (q.lo, q.hi, proc.getParam (bands[(size_t) h].q) * (1.f + w.deltaY * 0.5f));
    if (auto* p = proc.apvts.getParameter (q.id)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (nq)); p->endChangeGesture(); }
}

//==============================================================================
ModulePanel::ModulePanel (YazaVoxProcessor& p, int m, int rc, const char* skipGroup) : proc (p), module (m), rowCells (rc)
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
        if (skipGroup != nullptr && juce::String (d.group) == skipGroup) continue;
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
    if (extraLeft) extraLeft->setBounds (area.removeFromLeft ((int) ((float) area.getWidth() * extraLeftFrac)).reduced (2, 2));
    if (extraTop) extraTop->setBounds (area.removeFromTop ((int) ((float) area.getHeight() * extraFrac)).reduced (0, 2));
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
SimpleView::SimpleView (YazaVoxProcessor& p, std::function<void()> pro) : proc (p), onPro (std::move (pro))
{
    setLookAndFeel (&laf);
    auto add = [this] (auto* c) { owned.emplace_back (c); addAndMakeVisible (*owned.back()); return c; };

    // grand bouton COMP (macro : seuil, ratio, gain de compensation, etage 2)
    compMacro.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    compMacro.setRotaryParameters (juce::MathConstants<float>::pi * 1.15f, juce::MathConstants<float>::pi * 2.85f, true);
    compMacro.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    compMacro.setRange (0.0, 100.0, 1.0);
    compMacro.setDoubleClickReturnValue (true, 50.0);
    compMacro.setPopupDisplayEnabled (true, true, nullptr);
    compMacro.setTextValueSuffix (" %");
    compMacro.getProperties().set ("big", true);
    compMacro.setMouseDragSensitivity (260);
    compMacro.onValueChange = [this] { if (! syncing) setComp ((float) compMacro.getValue() / 100.f); };
    addAndMakeVisible (compMacro);
    compLabel.setText ("COMP", juce::dontSendNotification);
    compLabel.setJustificationType (juce::Justification::centred);
    compLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (compLabel);

    lowCut.setClickingTogglesState (true);
    addAndMakeVisible (lowCut);
    lowCutAtt = std::make_unique<ButtonAtt> (p.apvts, "hpOn", lowCut);
    autoIn.onClick = [this] { measuring = true; measureFrames = 90; measurePk = 0.f; autoIn.setButtonText ("Listening..."); };
    prevBtn.onClick = [this] { const int n = YazaVoxProcessor::presetNames().size(); selectPreset ((proc.getCurrentPreset() + n - 1) % n); };
    nextBtn.onClick = [this] { selectPreset ((proc.getCurrentPreset() + 1) % YazaVoxProcessor::presetNames().size()); };
    proBtn.onClick = [this] { if (onPro) onPro(); };
    autoVoice.onClick = [this] { autoClick (proc); };
    for (auto* b : { &autoIn, &autoVoice, &prevBtn, &nextBtn, &proBtn }) addAndMakeVisible (*b);
    presets.addItemList (YazaVoxProcessor::presetNames(), 1);
    presets.setSelectedItemIndex (proc.getCurrentPreset(), juce::dontSendNotification);
    presets.setJustificationType (juce::Justification::centred);
    presets.onChange = [this] { selectPreset (presets.getSelectedItemIndex()); };
    addAndMakeVisible (presets);

    auto on1 = [&p] (std::vector<int> ids) { return [&p, ids] (float v) { for (int id : ids) setReal (p, id, v > 0.0f ? 1.f : 0.f); }; };
    auto onAlways = [&p] (int id) { return [&p, id] (float) { setReal (p, id, 1.f); }; };
    auto onFx = [&p] (int id) { return [&p, id] (float v) { setReal (p, id, v > 0.f ? 1.f : 0.f); if (v > 0.f) setReal (p, fxOn, 1.f); }; };
    (void) on1;

    // EQ
    rows.push_back ({ "EQ", eqOn, {}, {} });
    rows.back().items = { add (new AmberKnob (p, lsG, "Low", { eqOn }, onAlways (eqOn))), add (new AmberKnob (p, lmG, "Mid", { eqOn }, onAlways (eqOn))),
                          add (new AmberKnob (p, hmG, "Hi", { eqOn }, onAlways (eqOn))), add (new AmberKnob (p, hsG, "Air", { eqOn }, onAlways (eqOn))),
                          add (new AmberKnob (p, airHigh, "Exciter", { airOn }, [&p] (float v) { setReal (p, airOn, v > 0.f ? 1.f : 0.f); })) };
    // TUNE
    rows.push_back ({ "TUNE", tuneOn, {}, {} });
    rows.back().items = { add (new ToggleCell (p, tuneOn)), add (new ChoiceCell (p, tuneKey)), add (new ChoiceCell (p, tuneScale)),
                          add (new AmberKnob (p, tuneSpeed, "Retune", { tuneOn }, onAlways (tuneOn))), add (new AmberKnob (p, tuneHuman, "Humanize", { tuneOn }, onAlways (tuneOn))) };
    // FX
    rows.push_back ({ "FX", fxOn, {}, {} });
    rows.back().items = { add (new AmberKnob (p, deessAmt, "De-ess", { fxOn, deessOn }, onFx (deessOn))), add (new AmberKnob (p, satDrive, "Drive", { fxOn, satOn }, onFx (satOn))),
                          add (new AmberKnob (p, phoneAmt, "Phone", { fxOn, phoneOn }, onFx (phoneOn))), add (new AmberKnob (p, dblAmt, "Doubler", { fxOn, dblOn }, onFx (dblOn))),
                          add (new AmberKnob (p, choMix, "Chorus", { fxOn, choOn }, onFx (choOn))) };
    // DLY
    rows.push_back ({ "DLY", dlyOn, {}, {} });
    rows.back().items = { add (new AmberKnob (p, dlyMix, "Delay", { dlyOn }, [&p] (float v) { setReal (p, dlyOn, v > 0.f ? 1.f : 0.f); })),
                          add (new AmberKnob (p, dlyFb, "Feed", { dlyOn })), add (new AmberKnob (p, dlyLP, "Filter", { dlyOn })),
                          add (new ChoiceCell (p, dlyNote)), add (new ToggleCell (p, dlyPing)) };
    // REV
    rows.push_back ({ "REV", revOn, {}, {} });
    rows.back().items = { add (new AmberKnob (p, revMix, "Mix", { revOn }, [&p] (float v) { setReal (p, revOn, v > 0.f ? 1.f : 0.f); })),
                          add (new AmberKnob (p, revDecay, "Decay", { revOn })), add (new AmberKnob (p, revSize, "Size", { revOn })),
                          add (new AmberKnob (p, revDuck, "Duck", { revOn })), add (new ChoiceCell (p, revMode)) };

    inKnob = add (new AmberKnob (p, inGain, "IN", {}, {}, false));
    outKnob = add (new AmberKnob (p, outGain, "OUT", {}, {}, false));
    limKnob = add (new AmberKnob (p, limCeil, "LIMIT", {}, {}, true));
    refresh();
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
    auto sm = [] (float& l, float pk) { const float v = juce::jlimit (0.f, 1.f, ((float) linToDb (pk) + 60.f) / 60.f); l = v > l ? v : l - (l - v) * 0.15f; };
    sm (inLvl, inPk); sm (outLvl, outPk);
    phase += 0.09f;
    inHold = std::max (inLvl, inHold - 0.004f);   // pic recent : sert a dire si l'entree est dans la zone cible
    if (measuring)
    {
        measurePk = std::max (measurePk, inPk);
        if (--measureFrames <= 0)
        {
            measuring = false;
            autoIn.setButtonText ("Auto input");
            if (measurePk > 1e-4f)
                setReal (proc, inGain, juce::jlimit (-18.f, 18.f, -9.f - (float) linToDb (measurePk)));   // pic cale a -9 dBFS
        }
    }
    autoTick (proc, autoVoice, presets, autoShow);
    repaint (orbArea.getUnion (inBar.getUnion (outBar)).expanded (50));
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
        for (auto* c : r.items) c->setAlpha (on ? 1.f : 0.55f);
    }
    repaint (titleArea.getUnion (orbArea));
}

void SimpleView::resized()
{
    const int W = getWidth(), H = getHeight();
    auto fx_ = [&] (float f) { return (int) std::round ((float) W * f); };
    auto fy_ = [&] (float f) { return (int) std::round ((float) H * f); };

    // barre du haut
    const int ty = fy_ (0.018f), th = fy_ (0.07f);
    int x = fx_ (0.432f);
    auto put = [&] (juce::Component& c, float w) { c.setBounds (x, ty, fx_ (w), th); x += fx_ (w + 0.006f); };
    put (lowCut, 0.06f); put (autoIn, 0.075f); put (autoVoice, 0.1f); put (prevBtn, 0.03f); put (presets, 0.14f); put (nextBtn, 0.03f); put (proBtn, 0.055f);

    // bandes de droite
    const int bx = fx_ (0.47f), bw = fx_ (0.52f);
    const float ys[5][2] = { { 0.115f, 0.255f }, { 0.27f, 0.41f }, { 0.425f, 0.565f }, { 0.58f, 0.72f }, { 0.735f, 0.875f } };
    const int ri[5] = { 0, 1, 2, 3, 4 };
    for (int k = 0; k < 5; ++k)
    {
        auto& r = rows[(size_t) ri[k]];
        r.band = { bx, fy_ (ys[k][0]), bw, fy_ (ys[k][1] - ys[k][0]) };
        auto area = r.band.reduced (6, 4);
        area.removeFromLeft ((int) ((float) r.band.getWidth() * 0.21f));
        const int n = (int) r.items.size();
        const int cw = area.getWidth() / n;
        for (int i = 0; i < n; ++i) r.items[(size_t) i]->setBounds (area.getX() + i * cw + 4, area.getY(), cw - 8, area.getHeight());
    }
    titleArea = { fx_ (0.03f), fy_ (0.02f), fx_ (0.40f), fy_ (0.15f) };

    // COMP + halo
    const int d = (int) ((float) std::min (W * 0.30f, H * 0.52f));
    const juce::Point<int> c { fx_ (0.235f), fy_ (0.485f) };
    compMacro.setBounds (c.x - d / 2, c.y - d / 2, d, d);
    compLabel.setFont (juce::Font (juce::FontOptions ((float) H * 0.07f, juce::Font::bold)));
    compLabel.setBounds (c.x - d / 2, c.y + d / 2 + fy_ (0.01f), d, fy_ (0.09f));
    orbArea = { c.x - d, c.y - d, d * 2, d * 2 };

    // barres IN / OUT
    const int by = fy_ (0.945f), bh = fy_ (0.03f), kd = fy_ (0.07f);
    inBar = { fx_ (0.012f), by, fx_ (0.29f), bh };
    outBar = { fx_ (0.70f), by, fx_ (0.285f), bh };
    inKnob->setBounds (fx_ (0.315f), by + bh / 2 - kd / 2, kd, kd);
    outKnob->setBounds (fx_ (0.655f) - kd, by + bh / 2 - kd / 2, kd, kd);
    limKnob->setBounds (fx_ (0.475f) - kd / 2, by + bh / 2 - kd / 2, kd, kd);

    // fond mis en cache
    bg = juce::Image (juce::Image::ARGB, std::max (1, W), std::max (1, H), true);
    juce::Graphics g (bg);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff86141b), 0, 0, juce::Colour (0xff560b10), 0, (float) H, false));
    g.fillAll();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0x33ff5a30), (float) W * 0.3f, (float) H * 0.45f, juce::Colours::transparentBlack, (float) W * 0.8f, (float) H * 0.9f, true));
    g.fillAll();
    g.setColour (juce::Colours::black.withAlpha (0.10f));
    const int step = std::max (6, W / 190);
    for (int yy = 0; yy < H; yy += step)
        for (int xx = ((yy / step) & 1) * step / 2; xx < W; xx += step) g.fillRect (xx, yy, 2, 2);
    // bandes
    for (auto& r : rows)
    {
        g.setColour (juce::Colour (0xff3d080c).withAlpha (0.78f));
        juce::Path p; p.addRoundedRectangle (r.band.toFloat(), 14.f);
        g.fillPath (p);
        g.setColour (juce::Colour (0xffb0262c).withAlpha (0.22f));
        for (int w = 0; w < 4; ++w)
        {
            juce::Path s;
            const float yb = (float) r.band.getY() + (float) r.band.getHeight() * (0.2f + 0.2f * (float) w);
            for (int xx = r.band.getX() + 10; xx < r.band.getRight() - 10; xx += 6)
            {
                const float yy = yb + 5.f * std::sin ((float) xx * 0.018f + (float) w * 1.3f);
                if (xx == r.band.getX() + 10) s.startNewSubPath ((float) xx, yy); else s.lineTo ((float) xx, yy);
            }
            g.strokePath (s, juce::PathStrokeType (1.4f));
        }
    }
}

void SimpleView::paint (juce::Graphics& g)
{
    g.drawImageAt (bg, 0, 0);
    const int W = getWidth(), H = getHeight();

    // orbe vocal anime (original) autour du bouton COMP : barres radiales pilotees par le niveau de sortie
    const auto c = orbArea.getCentre().toFloat();
    const float kr = (float) compMacro.getWidth() * 0.5f;
    const int N = 72;
    for (int i = 0; i < N; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * (float) i / (float) N;
        const float wob = 0.5f + 0.5f * std::sin (a * 5.f + phase) * std::sin (a * 3.f - phase * 0.7f);
        const float len = kr * (0.04f + 0.28f * outLvl * (0.35f + wob));
        const float r0 = kr * 1.08f, r1 = r0 + len;
        g.setColour (amberHi.withAlpha (0.25f + 0.6f * outLvl * wob));
        g.drawLine (c.x + std::sin (a) * r0, c.y - std::cos (a) * r0, c.x + std::sin (a) * r1, c.y - std::cos (a) * r1, std::max (1.5f, (float) W * 0.0025f));
    }

    // titres de rangees
    const float tfs = (float) H * 0.075f;
    for (auto& r : rows)
    {
        const bool on = proc.getParam (r.onIdx) > 0.5f;
        g.setColour (on ? juce::Colours::white : juce::Colours::white.withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (tfs, juce::Font::bold)));
        g.drawFittedText (r.title, r.band.withWidth ((int) ((float) r.band.getWidth() * 0.20f)).translated (10, 0).reduced (0, 4), juce::Justification::centredLeft, 1, 0.55f);
        if (r.title == "TUNE" && on)
        {
            const float d = proc.engine.tuner().detMidi.load(), t = proc.engine.tuner().tgtMidi.load();
            g.setFont (juce::Font (juce::FontOptions (tfs * 0.42f, juce::Font::bold)));
            g.setColour (amberHi);
            g.drawText (NoteDisplay::noteName (d) + " > " + NoteDisplay::noteName (t), r.band.withTrimmedTop (r.band.getHeight() * 3 / 4).withWidth ((int) ((float) r.band.getWidth() * 0.19f)).translated (10, 0), juce::Justification::centredLeft);
        }
    }

    // titre central
    g.setFont (juce::Font (juce::FontOptions ((float) H * 0.10f, juce::Font::bold | juce::Font::italic)));
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawFittedText ("Fck ThaVOICE", titleArea.translated (3, 4), juce::Justification::centred, 1, 0.6f);
    g.setGradientFill (juce::ColourGradient (amberHi, 0, (float) titleArea.getY(), amberDark, 0, (float) titleArea.getBottom(), false));
    g.drawFittedText ("Fck ThaVOICE", titleArea, juce::Justification::centred, 1, 0.6f);
    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions ((float) H * 0.028f)));
    g.drawText ("VOCAL SUITE", titleArea.withTrimmedTop ((int) ((float) titleArea.getHeight() * 0.78f)), juce::Justification::centred);

    // barres IN / OUT + repere de niveau conseille
    // zone cible de l'entree : -18 .. -9 dB (la barre va de -60 a 0 dB)
    const float zLo = 42.f / 60.f, zHi = 51.f / 60.f;
    const bool silent = inHold < 0.03f;
    const int status = silent ? 0 : (inHold < zLo ? 1 : (inHold <= zHi + 0.01f ? 2 : 3));   // 0 rien, 1 trop bas, 2 OK, 3 trop fort
    const juce::Colour okG { 0xff4cd964 }, lowC { 0xfff6b53c }, hotC { 0xffff4a3d };
    auto bar = [&] (juce::Rectangle<int> b, float lvl, bool right, juce::Colour c0, juce::Colour c1)
    {
        const auto f = b.toFloat();
        g.setColour (wineDark.withAlpha (0.85f)); g.fillRoundedRectangle (f, f.getHeight() * 0.5f);
        const float w = f.getWidth() * lvl;
        g.setGradientFill (juce::ColourGradient (c0, f.getX(), 0, c1, f.getRight(), 0, false));
        if (right) g.fillRoundedRectangle (f.getRight() - w, f.getY(), w, f.getHeight(), f.getHeight() * 0.5f);
        else g.fillRoundedRectangle (f.getX(), f.getY(), w, f.getHeight(), f.getHeight() * 0.5f);
    };
    {
        const juce::Colour c = status == 2 ? okG : (status == 3 ? hotC : amberHi);
        bar (inBar, inLvl, false, status == 2 ? okG.darker (0.4f) : (status == 3 ? hotC.darker (0.4f) : amberDark), c);
    }
    bar (outBar, outLvl, true, amberDark, amberHi);
    {
        const float bx0 = (float) inBar.getX() + (float) inBar.getWidth() * zLo, bx1 = (float) inBar.getX() + (float) inBar.getWidth() * zHi;
        const float by0 = (float) inBar.getY() - 3.f, bh0 = (float) inBar.getHeight() + 6.f;
        // la zone cible : cadre bien visible autour de la partie de la barre a atteindre
        g.setColour (okG.withAlpha (status == 2 ? 0.35f : 0.18f));
        g.fillRoundedRectangle (bx0, by0, bx1 - bx0, bh0, 5.f);
        g.setColour (status == 2 ? okG : juce::Colours::white.withAlpha (0.85f));
        g.drawRoundedRectangle (bx0, by0, bx1 - bx0, bh0, 5.f, 2.f);
        const char* msg = status == 0 ? "TARGET" : (status == 1 ? "TOO LOW" : (status == 2 ? "TARGET OK" : "TOO LOUD"));
        g.setColour (status == 0 ? juce::Colours::white.withAlpha (0.85f) : (status == 1 ? lowC : (status == 2 ? okG : hotC)));
        g.setFont (juce::Font (juce::FontOptions ((float) H * 0.03f, juce::Font::bold)));
        g.drawText (msg, (int) bx0 - 70, inBar.getY() - (int) ((float) H * 0.045f), (int) (bx1 - bx0) + 140, (int) ((float) H * 0.035f), juce::Justification::centred);
    }
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions ((float) H * 0.05f, juce::Font::bold)));
    g.drawText ("IN", inKnob->getRight() + 4, inBar.getY() - 8, 104, inBar.getHeight() + 16, juce::Justification::centredLeft);
    g.drawText ("OUT", outKnob->getX() - 110, outBar.getY() - 8, 104, outBar.getHeight() + 16, juce::Justification::centredRight);
}


//==============================================================================
// DE-ESSER plein ecran
static const juce::Colour dsYellow { 0xffffd23c }, dsGreen { 0xff8be04a }, dsBg1 { 0xff15110d }, dsBg2 { 0xff070605 };

float DsBandBar::xOf (double f) const
{
    const float m = 26.f, w = (float) getWidth() - 2.f * m;
    return m + w * (float) (std::log (f / 2000.0) / std::log (18000.0 / 2000.0));
}
double DsBandBar::fOf (float x) const
{
    const float m = 26.f, w = (float) getWidth() - 2.f * m;
    return 2000.0 * std::pow (18000.0 / 2000.0, juce::jlimit (0.f, 1.f, (x - m) / w));
}
void DsBandBar::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float ty = r.getY() + r.getHeight() * 0.40f, th = std::max (8.f, r.getHeight() * 0.22f);
    g.setColour (juce::Colour (0xff0b0907));
    g.fillRoundedRectangle (r.getX() + 12, ty, r.getWidth() - 24, th, 3.f);
    g.setColour (col::edge.brighter (0.3f));
    g.drawRoundedRectangle (r.getX() + 12, ty, r.getWidth() - 24, th, 3.f, 1.f);
    // graduations
    g.setFont (plain (std::clamp (r.getHeight() * 0.22f, 8.f, 11.f)));
    for (double f : { 2000.0, 3000.0, 4000.0, 6000.0, 8000.0, 10000.0, 14000.0, 18000.0 })
    {
        const float x = xOf (f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawVerticalLine ((int) x, ty + 2, ty + th - 2);
        g.setColour (col::textDim);
        g.drawText (f >= 10000 ? juce::String (f / 1000.0, 0) + "k" : juce::String (f / 1000.0, f == 3000.0 ? 0 : 0) + "k", (int) x - 16, (int) (ty + th + 1), 32, 12, juce::Justification::centred);
    }
    const double lo = proc.getParam (deessFreq), hi = std::max ((double) proc.getParam (deessHigh), lo * 1.25);
    const float xl = xOf (lo), xh = xOf (hi);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffb81c).withAlpha (0.55f), 0, ty, juce::Colour (0xffd98a00).withAlpha (0.30f), 0, ty + th, false));
    g.fillRect (xl, ty + 1, xh - xl, th - 2);
    for (int k = 0; k < 2; ++k)
    {
        const float x = k == 0 ? xl : xh;
        juce::Path tri;
        const float s = std::clamp (th * 0.55f, 8.f, 14.f);
        tri.addTriangle (x - s * (k == 0 ? 0.0f : 1.0f), ty - s, x + s * (k == 0 ? 1.0f : 0.0f), ty - s, x, ty + th);
        g.setColour (juce::Colour (0xffe9e3d8)); g.fillPath (tri);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.strokePath (tri, juce::PathStrokeType (1.f));
    }
    g.setColour (dsYellow);
    g.setFont (bold (std::clamp (r.getHeight() * 0.28f, 10.f, 14.f)));
    g.drawText (juce::String (lo / 1000.0, 1) + " kHz", 4, 0, 90, (int) (ty - 2), juce::Justification::centredLeft);
    g.drawText (juce::String (hi / 1000.0, 1) + " kHz", getWidth() - 94, 0, 90, (int) (ty - 2), juce::Justification::centredRight);
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
    : AudioProcessorEditor (&p), proc (p), inKnob (p, inGain), outKnob (p, outGain), limKnob (p, limCeil), eqView (p), noteDisp (p),
      comp (p, MComp, 6), eq (p, MEq, 9), fx (p, MFx, 6, "DE-ESSER"), dly (p, MDly, 7), rev (p, MRev, 7), tune (p, MTune, 4)
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
    eq.extraTop = &eqView; eq.addAndMakeVisible (eqView); eq.extraFrac = 0.42f;
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
    if (! simple->isVisible()) { eqView.update(); noteDisp.update(); }

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
