#pragma once

#include <juce_dsp/juce_dsp.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include "YvParams.h"

// ThaVOICE : algorithmes originaux (aucun code tiers). Tout est en double precision, sans allocation dans process().

namespace yv
{
constexpr double kPi = 3.14159265358979323846;
inline double dbToLin (double d) noexcept { return std::pow (10.0, d / 20.0); }
inline double linToDb (double l) noexcept { return 20.0 * std::log10 (l + 1e-12); }
inline double clampd (double x, double a, double b) noexcept { return x < a ? a : (x > b ? b : x); }

//==============================================================================
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    inline double process (double x) noexcept
    {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
    void raw (double B0, double B1, double B2, double A0, double A1, double A2) noexcept
    {
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    void peaking (double fs, double f, double q, double dB) noexcept
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        raw (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void lowShelf (double fs, double f, double q, double dB) noexcept
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q), t = 2.0 * std::sqrt (A) * al;
        raw (A * ((A + 1) - (A - 1) * c + t), 2 * A * ((A - 1) - (A + 1) * c), A * ((A + 1) - (A - 1) * c - t),
             (A + 1) + (A - 1) * c + t, -2 * ((A - 1) + (A + 1) * c), (A + 1) + (A - 1) * c - t);
    }
    void highShelf (double fs, double f, double q, double dB) noexcept
    {
        const double A = std::pow (10.0, dB / 40.0), w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q), t = 2.0 * std::sqrt (A) * al;
        raw (A * ((A + 1) + (A - 1) * c + t), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - t),
             (A + 1) - (A - 1) * c + t, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - t);
    }
    void highPass (double fs, double f, double q) noexcept
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        raw ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + al, -2 * c, 1 - al);
    }
    void lowPass (double fs, double f, double q) noexcept
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        raw ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + al, -2 * c, 1 - al);
    }
    void bandPass (double fs, double f, double q) noexcept
    {
        const double w = 2.0 * kPi * f / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        raw (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    double magSq (double fs, double f) const noexcept
    {
        const double w = 2.0 * kPi * f / fs, c1 = std::cos (w), c2 = std::cos (2.0 * w);
        const double num = b0 * b0 + b1 * b1 + b2 * b2 + 2.0 * (b0 * b1 + b1 * b2) * c1 + 2.0 * b0 * b2 * c2;
        const double den = 1.0 + a1 * a1 + a2 * a2 + 2.0 * (a1 + a1 * a2) * c1 + 2.0 * a2 * c2;
        return num / std::max (den, 1e-18);
    }
};

struct OnePole
{
    double a = 0.5, z = 0;
    void set (double fc, double fs) noexcept { a = 1.0 - std::exp (-2.0 * kPi * clampd (fc, 1.0, fs * 0.45) / fs); }
    inline double lp (double x) noexcept { z += a * (x - z); return z; }
    inline double hp (double x) noexcept { return x - lp (x); }
    void reset() noexcept { z = 0; }
};

// Ligne a retard (puissance de 2), lecture lineaire. read(d) : echantillon d pas en arriere du dernier ecrit.
struct Delay
{
    std::vector<float> buf;
    int mask = 0, w = 0;
    void init (int maxSamples)
    {
        int n = 8;
        while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.f);
        mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
    inline void write (double x) noexcept { buf[(size_t) w] = (float) x; w = (w + 1) & mask; }
    inline double read (double d) const noexcept
    {
        const double rp = (double) (w - 1) - d;
        const double fl = std::floor (rp);
        const int i0 = (int) fl;
        const double f = rp - fl;
        return (double) buf[(size_t) (i0 & mask)] * (1.0 - f) + (double) buf[(size_t) ((i0 + 1) & mask)] * f;
    }
};

// Passe-tout de Schroeder
struct Allpass
{
    std::vector<float> buf;
    int mask = 0, w = 0, len = 1;
    double g = 0.5;
    void init (int length, double gain)
    {
        len = std::max (1, length); g = gain;
        int n = 8;
        while (n < len + 4) n <<= 1;
        buf.assign ((size_t) n, 0.f);
        mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.f); }
    inline double process (double x) noexcept
    {
        const double z = (double) buf[(size_t) ((w - len) & mask)];
        const double v = x + g * z;
        buf[(size_t) w] = (float) v;
        w = (w + 1) & mask;
        return z - g * v;
    }
};

//==============================================================================
// COMPRESSION : 2 etages (principal + attrape-crete), release dependant du programme, mix parallele.
//==============================================================================
// WARM : triode douce a harmoniques paires. Port du coeur de "Tube2" (Airwindows, (c) Chris Johnson, licence MIT :
// https://github.com/airwindows/airwindows). Ajoute chaleur et densite apres le compresseur sans durcir le son.
class WarmTube
{
public:
    void prepare (double fs)
    {
        hiRate = fs > 1.9 * 44100.0;
        reset();
        // gain petit signal : sert a garder le meme niveau apres la saturation (seuls les pics sont alors arrondis)
        WarmTube t; t.hiRate = hiRate;
        double e0 = 0, e1 = 0;
        for (int i = 0; i < 4096; ++i)
        {
            const double x = 0.02 * std::sin (2.0 * kPi * 997.0 * (double) i / fs);
            const double y = t.shape (0, x);
            if (i > 1024) { e0 += x * x; e1 += y * y; }
        }
        gSmall = e0 > 0 ? std::sqrt (e1 / e0) : 1.0;
        if (gSmall < 0.1) gSmall = 1.0;
    }
    void reset() { for (auto& c : st) c = {}; }

    // x en pleine echelle (+-1) ; renvoie le signal sature, au meme niveau petit signal que l'entree
    // even : part d'harmoniques paires ajoutee (2e harmonique = chaleur), puis coupe-continu 15 Hz
    inline double process (int c, double x, double pre, double even) noexcept
    {
        double y = shape (c, x * pre);
        y += even * y * y;
        auto& s = st[(size_t) c];
        const double o = y - s.dx + 0.9980 * s.dy;
        s.dx = y; s.dy = o;
        return o / (pre * gSmall);
    }

private:
    struct S { double a = 0, b = 0, e = 0, dx = 0, dy = 0; };
    static constexpr int kPow = 5;      // reglage "Tube" a 0.5 : puissance 5
    inline double shape (int c, double x) noexcept
    {
        auto& s = st[(size_t) c];
        const double asym = (double) kPow, gs = 1.0 / (double) (kPow + 1), os = 1.0 + 1.0 / (double) kPow;
        if (hiRate) { const double t = x; x += s.a; s.a = t; x *= 0.5; }
        x = std::clamp (x, -1.0, 1.0);
        x /= asym;
        double sharpen = -x;
        sharpen = sharpen > 0.0 ? 1.0 + std::sqrt (sharpen) : 1.0 - std::sqrt (-sharpen);
        x -= x * std::abs (x) * sharpen * 0.25;
        x *= asym;
        double f = x;
        for (int k = 0; k < kPow; ++k) f *= x;
        if ((kPow % 2 == 1) && x != 0.0) f = (f / x) * std::abs (x);
        f *= gs;
        x -= f;
        x *= os;
        if (hiRate) { const double t = x; x += s.b; s.b = t; x *= 0.5; }
        double slew = s.e - x;
        if (hiRate) { const double t = x; x += s.e; s.e = t; x *= 0.5; } else s.e = x;
        slew = slew > 0.0 ? 1.0 + std::sqrt (slew) * 0.5 : 1.0 - std::sqrt (-slew) * 0.5;
        x -= x * std::abs (x) * slew * gs;
        x = std::clamp (x, -0.52, 0.52);
        return x * 1.923076923076923;
    }
    std::array<S, 2> st;
    bool hiRate = false;
    double gSmall = 1.0;
};

class Comp
{
public:
    std::atomic<float> grDb { 0.f };

    void prepare (double fs) { sr = fs; g1 = g2 = 0; e2 = 0; scF = -1; for (auto& b : sc) b.reset(); rmsC = 1.0 - std::exp (-1.0 / (0.005 * sr)); tube.prepare (fs); }

    void setup (const float* v)
    {
        thr = v[compThr]; ratio = std::max (1.0f, v[compRatio]); knee = v[compKnee];
        atk = std::exp (-1.0 / (std::max (0.05, (double) v[compAtk]) * 0.001 * sr));
        const double rel = std::max (5.0, (double) v[compRel]) * 0.001;
        relSlow = std::exp (-1.0 / (rel * 1.5 * sr));
        relFast = std::exp (-1.0 / (rel * 0.5 * sr));
        makeup = dbToLin (v[compMakeup]);
        mix = clampd (v[compMix] / 100.0, 0.0, 1.0);
        if (std::abs (v[compSC] - scF) > 0.5) { scF = v[compSC]; for (auto& b : sc) b.highPass (sr, scF, 0.7071); }
        warm = clampd (v[compWarm] / 100.0, 0.0, 1.0);
        warmPre = dbToLin (2.0 + 5.0 * warm);
        warmEven = 0.45 * warm;
        stage2 = v[comp2On] > 0.5f;
        amt2 = clampd (v[comp2Amt] / 100.0, 0.0, 1.0);
        ratio2 = 1.0 + amt2 * 5.0;
        thr2 = thr + 6.0;
        atk2 = std::exp (-1.0 / (0.0005 * sr));
        rel2 = std::exp (-1.0 / (0.060 * sr));
    }

    static inline double curve (double lvl, double th, double ra, double kn) noexcept
    {
        const double over = lvl - th;
        if (kn < 0.01) return over > 0 ? (1.0 / ra - 1.0) * over : 0.0;
        if (2.0 * over < -kn) return 0.0;
        if (2.0 * std::abs (over) <= kn) { const double t = over + kn * 0.5; return (1.0 / ra - 1.0) * t * t / (2.0 * kn); }
        return (1.0 / ra - 1.0) * over;
    }

    inline void process (double& l, double& r) noexcept
    {
        // detecteur : filtre passe-haut de sidechain, puis melange pic / RMS (reagit au pic sans pomper sur les graves)
        const double sl = sc[0].process (l), sr_ = sc[1].process (r);
        const double pk0 = std::max (std::abs (sl), std::abs (sr_));
        e2 += (pk0 * pk0 - e2) * rmsC;
        const double lvl = linToDb (0.55 * pk0 + 0.45 * 1.3 * std::sqrt (e2));

        // etage 1 : release rapide quand la reduction est forte, lent sinon
        const double t1 = curve (lvl, thr, ratio, knee);
        const double relC = relSlow + (relFast - relSlow) * clampd (-g1 / 10.0, 0.0, 1.0);
        const double c1 = t1 < g1 ? atk : relC;
        g1 = c1 * g1 + (1.0 - c1) * t1;

        double gTot = g1;
        if (stage2 && amt2 > 0.001)
        {
            const double t2 = curve (lvl + g1, thr2, ratio2, 3.0);
            const double c2 = t2 < g2 ? atk2 : rel2;
            g2 = c2 * g2 + (1.0 - c2) * t2;
            gTot += g2;
        }
        else g2 += (0.0 - g2) * 0.01;

        const double gain = dbToLin (gTot) * makeup;
        double ol = l * gain, orr = r * gain;
        if (warm > 0.001)    // chaleur : saturation douce apres le compresseur, dosee par WARM
        {
            ol += (tube.process (0, ol, warmPre, warmEven) - ol) * warm;
            orr += (tube.process (1, orr, warmPre, warmEven) - orr) * warm;
        }
        l = l * (1.0 - mix) + ol * mix;
        r = r * (1.0 - mix) + orr * mix;
        grMin = std::min (grMin, gTot);
    }

    void publish() { grDb.store ((float) grMin); grMin = 0; }

private:
    double sr = 48000, thr = -22, ratio = 3.5, knee = 6, atk = 0, relSlow = 0, relFast = 0, makeup = 1, mix = 1;
    bool stage2 = true;
    double amt2 = 0.4, ratio2 = 3, thr2 = -16, atk2 = 0, rel2 = 0;
    double g1 = 0, g2 = 0, grMin = 0, e2 = 0, rmsC = 0.01, scF = -1;
    std::array<Biquad, 2> sc;
    WarmTube tube;
    double warm = 0.3, warmPre = 2.0, warmEven = 0.1;
};

//==============================================================================
// EQ : coupe-bas, shelf grave, 2 cloches, shelf aigu, coupe-haut.
class EQ
{
public:
    void prepare (double fs)
    {
        sr = fs;
        for (auto& c : ch) { c.hp.reset(); c.lp.reset(); for (auto& b : c.band) b.reset(); }
        cacheValid = false;
    }

    static void build (const float* v, double fs, Biquad& hp, Biquad& lp, std::array<Biquad, 4>& b, bool& hpAct, bool& lpAct, std::array<bool, 4>& act)
    {
        hpAct = v[hpOn] > 0.5f; lpAct = v[lpFreq] < 19900.0f;
        if (hpAct) hp.highPass (fs, v[hpFreq], 0.7071);
        if (lpAct) lp.lowPass (fs, v[lpFreq], 0.7071);
        act[0] = std::abs (v[lsG]) > 0.01f; act[1] = std::abs (v[lmG]) > 0.01f; act[2] = std::abs (v[hmG]) > 0.01f; act[3] = std::abs (v[hsG]) > 0.01f;
        if (act[0]) b[0].lowShelf (fs, v[lsF], v[lsQ], v[lsG]);
        if (act[1]) b[1].peaking (fs, v[lmF], v[lmQ], v[lmG]);
        if (act[2]) b[2].peaking (fs, v[hmF], v[hmQ], v[hmG]);
        if (act[3]) b[3].highShelf (fs, v[hsF], v[hsQ], v[hsG]);
    }

    void setup (const float* v)
    {
        bool changed = ! cacheValid;
        for (int i = 0; i < 15 && ! changed; ++i) if (v[hpOn + i] != cache[(size_t) i]) changed = true;
        if (! changed) return;
        for (int i = 0; i < 15; ++i) cache[(size_t) i] = v[hpOn + i];
        cacheValid = true;
        Biquad hp, lp; std::array<Biquad, 4> b;
        build (v, sr, hp, lp, b, hpAct, lpAct, act);
        for (auto& c : ch) { c.hp.b0 = hp.b0; c.hp.b1 = hp.b1; c.hp.b2 = hp.b2; c.hp.a1 = hp.a1; c.hp.a2 = hp.a2;
                             c.lp.b0 = lp.b0; c.lp.b1 = lp.b1; c.lp.b2 = lp.b2; c.lp.a1 = lp.a1; c.lp.a2 = lp.a2;
                             for (size_t i = 0; i < 4; ++i) { c.band[i].b0 = b[i].b0; c.band[i].b1 = b[i].b1; c.band[i].b2 = b[i].b2; c.band[i].a1 = b[i].a1; c.band[i].a2 = b[i].a2; } }
    }

    inline void process (double& l, double& r) noexcept
    {
        double* s[2] = { &l, &r };
        for (int k = 0; k < 2; ++k)
        {
            auto& c = ch[(size_t) k];
            double x = *s[k];
            if (hpAct) x = c.hp.process (x);
            for (size_t i = 0; i < 4; ++i) if (act[i]) x = c.band[i].process (x);
            if (lpAct) x = c.lp.process (x);
            *s[k] = x;
        }
    }

    // Reponse en frequence (dB) pour l'affichage
    static double responseDb (const float* v, double fs, double f)
    {
        Biquad hp, lp; std::array<Biquad, 4> b; bool hpA, lpA; std::array<bool, 4> a;
        build (v, fs, hp, lp, b, hpA, lpA, a);
        double m = 1.0;
        if (hpA) m *= hp.magSq (fs, f);
        if (lpA) m *= lp.magSq (fs, f);
        for (size_t i = 0; i < 4; ++i) if (a[i]) m *= b[i].magSq (fs, f);
        return 10.0 * std::log10 (std::max (m, 1e-12));
    }

private:
    struct Chan { Biquad hp, lp; std::array<Biquad, 4> band; };
    std::array<Chan, 2> ch;
    double sr = 48000;
    bool hpAct = false, lpAct = false, cacheValid = false;
    std::array<bool, 4> act {};
    std::array<float, 15> cache {};
};

//==============================================================================
// FX
// Oscilloscope du de-esser : 1 trame ~10 ms (crete de l'entree, crete de la bande de sifflantes, reduction), lue par l'interface.
struct DsScope
{
    static constexpr int N = 256;
    std::array<std::atomic<float>, N> in {}, sib {}, gr {};
    std::atomic<int> pos { 0 };
};

// DE-ESSER (dans l'esprit d'un Pro-DS) :
//  - detection dans la bande [LOW..HIGH] (Butterworth 4e ordre), enveloppe rapide ;
//  - SINGLE VOCAL : seuil RELATIF au niveau de la voix (marche quel que soit le niveau d'enregistrement) ; ALLROUND : seuil absolu ;
//  - SPLIT BAND : une cloche dynamique centree sur la bande (le reste du spectre n'est pas touche) ; WIDE BAND : baisse tout le signal ;
//  - RANGE = reduction maximale ; genou doux ; LISTEN = on entend uniquement la bande detectee.
class DeEsser
{
public:
    DsScope scope;
    std::atomic<float> grDb { 0.f };

    void prepare (double fs)
    {
        sr = fs;
        for (auto& c : ch) c = Chan();
        envB = envT = 0; gdB = 0; fAcc = 0; accIn = accSib = 0; accGr = 0;
        envAtk = 1.0 - std::exp (-1.0 / (0.00006 * sr));
        envRel = std::exp (-1.0 / (0.0045 * sr));
        refAtk = 1.0 - std::exp (-1.0 / (0.020 * sr));
        refRel = std::exp (-1.0 / (0.30 * sr));
        gAtk = 1.0 - std::exp (-1.0 / (0.00035 * sr));
        gRel = 1.0 - std::exp (-1.0 / (0.028 * sr));
        frame = std::max (64, (int) (sr * 0.010));
        for (auto& c : ch) c.refHp.highPass (sr, 160.0, 0.7071);
        f0 = hi0 = -1;
    }
    void setup (const float* v)
    {
        amt = clampd (v[deessAmt] / 100.0, 0.0, 1.0);
        range = clampd (v[deessRange], 0.0, 24.0);
        vocal = v[deessMode] < 0.5f;
        wide = v[deessBand] > 0.5f;
        listen = v[deessListen] > 0.5f;
        const double lo = clampd (v[deessFreq], 1500.0, 0.40 * sr), hi = clampd (std::max ((double) v[deessHigh], lo * 1.25), lo * 1.25, 0.45 * sr);
        // seuils : amt 0 = pas de reduction, amt 100 = tres sensible
        relThr = -4.0 - 0.28 * amt * 100.0 * 0.6;        // SINGLE VOCAL : bande / voix (dB)
        absThr = -amt * 60.0;                            // ALLROUND : niveau absolu de la bande (dBFS)
        if (std::abs (lo - f0) > 0.5 || std::abs (hi - hi0) > 0.5)
        {
            f0 = lo; hi0 = hi;
            for (auto& c : ch)
            {
                c.h1.highPass (sr, lo, 0.5412); c.h2.highPass (sr, lo, 1.3066);
                c.l1.lowPass  (sr, hi, 0.5412); c.l2.lowPass  (sr, hi, 1.3066);
            }
            const double fc = std::sqrt (lo * hi);
            // deux cloches (bas / haut de la bande) pour une reduction plate sur toute la zone [LOW..HIGH]
            const double fa = lo * std::pow (hi / lo, 0.28), fb = lo * std::pow (hi / lo, 0.72);
            q = clampd (0.55 * fc / (hi - lo) + 0.2, 0.45, 1.4);
            w0c = std::cos (2.0 * kPi * fa / sr); alpha = std::sin (2.0 * kPi * fa / sr) / (2.0 * q);
            w1c = std::cos (2.0 * kPi * fb / sr); alpha1 = std::sin (2.0 * kPi * fb / sr) / (2.0 * q);
        }
    }
    inline void process (double& l, double& r) noexcept
    {
        const double x[2] = { l, r };
        double band[2], bpk = 0, tpk = 0;
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[(size_t) c];
            band[c] = s.l2.process (s.l1.process (s.h2.process (s.h1.process (x[c]))));
            bpk = std::max (bpk, std::abs (band[c]));
            tpk = std::max (tpk, std::abs (s.refHp.process (x[c])));
        }
        envB = bpk > envB ? envB + (bpk - envB) * envAtk : bpk + (envB - bpk) * envRel;
        envT = tpk > envT ? envT + (tpk - envT) * refAtk : tpk + (envT - tpk) * refRel;

        double target = 0.0;
        const double bDb = 20.0 * std::log10 (envB + 1e-9), tDb = 20.0 * std::log10 (envT + 1e-9);
        if (amt > 0.001 && range > 0.01 && tDb > -58.0 && bDb > -75.0)
        {
            const double e = vocal ? (bDb - tDb) - relThr : bDb - absThr;
            const double kw = 3.0;                                  // demi-genou
            double gr = e <= -kw ? 0.0 : (e >= kw ? e : (e + kw) * (e + kw) / (4.0 * kw));
            target = -std::min (gr, range);
        }
        gdB += (target - gdB) * (target < gdB ? gAtk : gRel);

        double yl = l, yr = r;
        if (gdB < -0.02)
        {
            if (wide) { const double g = dbToLin (gdB); yl *= g; yr *= g; }
            else
            {
                const double gb = gdB * 0.95;                          // 2 cloches qui se recouvrent
                const double A = std::exp (gb * 0.05756462732485115);   // 10^(dB/40)
                const double c0 = (1 + alpha * A), c1 = -2 * w0c, c2 = 1 - alpha * A, d0 = 1.0 / (1 + alpha / A), d2 = 1 - alpha / A;
                const double e0 = (1 + alpha1 * A), e1 = -2 * w1c, e2 = 1 - alpha1 * A, f0i = 1.0 / (1 + alpha1 / A), f2 = 1 - alpha1 / A;
                for (int c = 0; c < 2; ++c)
                {
                    auto& s = ch[(size_t) c];
                    const double xin = x[c];
                    const double y = (c0 * d0) * xin + s.pz1;
                    s.pz1 = (c1 * d0) * xin - (c1 * d0) * y + s.pz2;
                    s.pz2 = (c2 * d0) * xin - (d2 * d0) * y;
                    const double z = (e0 * f0i) * y + s.qz1;
                    s.qz1 = (e1 * f0i) * y - (e1 * f0i) * z + s.qz2;
                    s.qz2 = (e2 * f0i) * y - (f2 * f0i) * z;
                    (c == 0 ? yl : yr) = z;
                }
            }
        }
        else for (auto& s : ch) s.pz1 = s.pz2 = s.qz1 = s.qz2 = 0;                 // identite : etat remis a zero (pas de residu)
        if (listen) { yl = band[0]; yr = band[1]; }
        l = yl; r = yr;

        // oscilloscope + compteur de reduction
        accIn = std::max (accIn, std::max (std::abs (x[0]), std::abs (x[1])));
        accSib = std::max (accSib, bpk);
        accGr = std::min (accGr, gdB);
        if (++fAcc >= frame)
        {
            const int p = scope.pos.load (std::memory_order_relaxed);
            scope.in[(size_t) p].store ((float) accIn, std::memory_order_relaxed);
            scope.sib[(size_t) p].store ((float) accSib, std::memory_order_relaxed);
            scope.gr[(size_t) p].store ((float) accGr, std::memory_order_relaxed);
            scope.pos.store ((p + 1) % DsScope::N, std::memory_order_release);
            grDb.store ((float) accGr, std::memory_order_relaxed);
            fAcc = 0; accIn = accSib = 0; accGr = 0;
        }
    }
private:
    struct Chan { Biquad h1, h2, l1, l2, refHp; double pz1 = 0, pz2 = 0, qz1 = 0, qz2 = 0; };
    double sr = 48000, f0 = -1, hi0 = -1, amt = 0.5, range = 10, relThr = -10, absThr = -30, q = 1, w0c = 0, alpha = 0.1, w1c = 0, alpha1 = 0.1;
    double envB = 0, envT = 0, envAtk = 0, envRel = 0, refAtk = 0, refRel = 0, gdB = 0, gAtk = 0, gRel = 0;
    double accIn = 0, accSib = 0, accGr = 0;
    int frame = 480, fAcc = 0;
    bool vocal = true, wide = false, listen = false;
    std::array<Chan, 2> ch;
};

// AIR (dans l'esprit d'un exciteur "Mid Air / High Air") : harmoniques generees a partir de la bande 2.5-7 kHz (non-lineaire douce,
// sans repliement car la source est limitee en bande), filtrees vers le haut, plus une legere mise en forme en plateau.
// Les harmoniques sont reduites automatiquement quand les aigus sont deja forts (pas de durete sur les sibilantes).
class Air
{
public:
    void prepare (double fs)
    {
        sr = fs;
        for (auto& c : ch) c.reset();
        env = 0; envRel = std::exp (-1.0 / (0.03 * sr));
        for (auto& c : ch)
        {
            c.srcHp[0].highPass (sr, 2500.0, 0.7071); c.srcHp[1].highPass (sr, 2500.0, 0.7071);
            c.srcLp[0].lowPass  (sr, std::min(7000.0, 0.45 * sr), 0.7071); c.srcLp[1].lowPass (sr, std::min(7000.0, 0.45 * sr), 0.7071);
            c.midHp[0].highPass (sr, 5500.0, 0.7071); c.midHp[1].highPass (sr, 5500.0, 0.7071);
            c.hiHp[0].highPass  (sr, 9000.0, 0.7071); c.hiHp[1].highPass  (sr, 9000.0, 0.7071);
            c.sensHp.highPass (sr, 6000.0, 0.7071);
        }
        shelfMid = shelfHigh = -1e9;
    }
    void setup (const float* v)
    {
        mid = clampd (v[airMid] / 100.0, 0.0, 1.0);
        high = clampd (v[airHigh] / 100.0, 0.0, 1.0);
        if (std::abs (mid - shelfMid) > 1e-4 || std::abs (high - shelfHigh) > 1e-4)
        {
            shelfMid = mid; shelfHigh = high;
            for (auto& c : ch)
            {
                c.peak.peaking (sr, 6500.0, 0.7, 3.5 * mid);
                c.shelf.highShelf (sr, std::min (12000.0, 0.45 * sr), 0.6, 5.0 * high);
            }
        }
    }
    inline void process (double& l, double& r) noexcept
    {
        double x[2] = { l, r }, hf = 0;
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[(size_t) c];
            hf = std::max (hf, std::abs (s.sensHp.process (x[c])));
        }
        env = hf > env ? hf : hf + (env - hf) * envRel;
        const double safe = 1.0 / (1.0 + 5.0 * env);            // moins d'harmoniques si les aigus sont deja presents
        for (int c = 0; c < 2; ++c)
        {
            auto& s = ch[(size_t) c];
            double src = s.srcLp[1].process (s.srcLp[0].process (s.srcHp[1].process (s.srcHp[0].process (x[c]))));
            const double u = src * 4.0;
            const double sat = std::tanh (u) * 0.25;                // impaires
            const double sq = u * u * 0.06 - 0.0;                   // paire (2e harmonique), le continu est retire par les passe-haut
            const double h = sat + sq;
            const double hm = s.midHp[1].process (s.midHp[0].process (h));
            const double hh = s.hiHp[1].process (s.hiHp[0].process (h));
            double y = x[c] + safe * (mid * 1.2 * hm + high * 1.6 * hh);
            if (mid > 0.001) y = s.peak.process (y);
            if (high > 0.001) y = s.shelf.process (y);
            x[c] = y;
        }
        l = x[0]; r = x[1];
    }
private:
    struct Ch
    {
        Biquad srcHp[2], srcLp[2], midHp[2], hiHp[2], sensHp, peak, shelf;
        void reset() { for (auto& b : srcHp) b.reset(); for (auto& b : srcLp) b.reset(); for (auto& b : midHp) b.reset(); for (auto& b : hiHp) b.reset(); sensHp.reset(); peak.reset(); shelf.reset(); }
    };
    double sr = 48000, mid = 0, high = 0, env = 0, envRel = 0, shelfMid = -1e9, shelfHigh = -1e9;
    std::array<Ch, 2> ch;
};

// WIDER : cree de la largeur a partir d'une source mono (voix) en ajoutant un signal lateral decorrele (reseau d'allpass,
// passe-haut ~200 Hz pour garder les graves au centre). Compatible mono : la somme L+R reste egale au signal central.
class Wider
{
public:
    void prepare (double fs)
    {
        sr = fs;
        static const double ms[4] = { 2.9, 4.7, 7.9, 12.3 };
        for (int i = 0; i < 4; ++i) ap[i].init ((int) std::round (ms[i] * 0.001 * fs), i < 2 ? 0.6 : 0.5);
        for (auto& a : ap) a.clear();
        hp[0].highPass (fs, 200.0, 0.7071); hp[1].highPass (fs, 200.0, 0.7071);
        lp.lowPass (fs, 14000.0, 0.7071);
        hp[0].reset(); hp[1].reset(); lp.reset();
    }
    // renvoie le signal lateral decorrele a partir du central m
    inline double side (double m) noexcept
    {
        double s = hp[1].process (hp[0].process (m));
        for (auto& a : ap) s = a.process (s);
        return lp.process (s);
    }
private:
    double sr = 48000;
    Allpass ap[4];
    Biquad hp[2], lp;
};

// Saturation tanh avec anti-aliasing par antiderivee (ADAA 1er ordre) : pas de latence, peu de repliement.
class Saturator
{
public:
    void prepare (double fs)
    {
        sr = fs; x1[0] = x1[1] = 0;
        for (auto& b : pre) b.reset();
        for (auto& b : post) b.reset();
        toneCache = 1e9;
    }
    void setup (const float* v)
    {
        const double p = clampd (v[satDrive] / 100.0, 0.0, 1.0);
        drive = 1.0 + 11.0 * std::pow (p, 1.6);
        norm = 1.0 / std::pow (drive, 0.72);
        mix = clampd (v[satMix] / 100.0, 0.0, 1.0);
        const double tone = v[satTone] / 100.0 * 6.0;
        if (std::abs (tone - toneCache) > 1e-6)
        {
            toneCache = tone;
            for (auto& b : pre) b.highShelf (sr, 2000.0, 0.7, tone);
            for (auto& b : post) b.highShelf (sr, 2000.0, 0.7, -tone);
        }
    }
    static inline double logcosh (double x) noexcept
    {
        const double a = std::abs (x);
        return a + std::log1p (std::exp (-2.0 * a)) - 0.6931471805599453;
    }
    inline double one (int c, double in) noexcept
    {
        const double u = drive * pre[(size_t) c].process (in);
        const double d = u - x1[c];
        double y;
        if (std::abs (d) > 1e-5) y = (logcosh (u) - logcosh (x1[c])) / d;
        else y = std::tanh (0.5 * (u + x1[c]));
        x1[c] = u;
        return post[(size_t) c].process (y * norm);
    }
    inline void process (double& l, double& r) noexcept
    {
        const double wl = one (0, l), wr = one (1, r);
        l = l * (1.0 - mix) + wl * mix;
        r = r * (1.0 - mix) + wr * mix;
    }
private:
    double sr = 48000, drive = 1, norm = 1, mix = 1, toneCache = 1e9;
    double x1[2] { 0, 0 };
    std::array<Biquad, 2> pre, post;
};

class Doubler
{
public:
    void prepare (double fs) { sr = fs; d.init ((int) (0.08 * fs)); ph = 0; }
    void setup (const float* v) { amt = clampd (v[dblAmt] / 100.0, 0.0, 1.0); spread = clampd (v[dblSpread] / 100.0, 0.0, 1.0); }
    inline void process (double& l, double& r) noexcept
    {
        d.write (0.5 * (l + r));
        ph += 1.0 / sr; if (ph > 1000.0) ph -= 1000.0;
        const double s1 = std::sin (2 * kPi * 0.27 * ph) + 0.5 * std::sin (2 * kPi * 0.61 * ph + 1.0);
        const double s2 = std::sin (2 * kPi * 0.21 * ph + 2.0) + 0.5 * std::sin (2 * kPi * 0.53 * ph + 0.3);
        const double s3 = std::sin (2 * kPi * 0.33 * ph + 4.0) + 0.5 * std::sin (2 * kPi * 0.47 * ph + 2.2);
        const double s4 = std::sin (2 * kPi * 0.17 * ph + 5.0) + 0.5 * std::sin (2 * kPi * 0.71 * ph + 3.1);
        const double tl = 0.6 * d.read ((0.0155 + 0.0012 * s1) * sr) + 0.4 * d.read ((0.0275 + 0.0015 * s3) * sr);
        const double tr = 0.6 * d.read ((0.0215 + 0.0012 * s2) * sr) + 0.4 * d.read ((0.0335 + 0.0015 * s4) * sr);
        const double a = amt * 0.8;
        const double outL = tl + (1.0 - spread) * 0.5 * tr, outR = tr + (1.0 - spread) * 0.5 * tl;
        l += a * outL; r += a * outR;
    }
private:
    double sr = 48000, amt = 0.35, spread = 0.6, ph = 0;
    Delay d;
};

class Chorus
{
public:
    void prepare (double fs) { sr = fs; dl.init ((int) (0.05 * fs)); dr.init ((int) (0.05 * fs)); ph = 0; }
    void setup (const float* v)
    {
        rate = v[choRate]; depth = clampd (v[choDepth] / 100.0, 0.0, 1.0); mix = clampd (v[choMix] / 100.0, 0.0, 1.0);
    }
    inline void process (double& l, double& r) noexcept
    {
        dl.write (l); dr.write (r);
        ph += rate / sr; if (ph >= 1.0) ph -= 1.0;
        const double base = 0.011 * sr, dep = 0.006 * sr * depth;
        auto lfo = [&] (double off) { return std::sin (2 * kPi * (ph + off)); };
        const double wl = 0.5 * (dl.read (base + dep * lfo (0.0)) + dl.read (base + dep * lfo (0.5)));
        const double wr = 0.5 * (dr.read (base + dep * lfo (0.25)) + dr.read (base + dep * lfo (0.75)));
        l += mix * wl; r += mix * wr;
    }
private:
    double sr = 48000, rate = 0.8, depth = 0.35, mix = 0.3, ph = 0;
    Delay dl, dr;
};

class Phone
{
public:
    void prepare (double fs)
    {
        sr = fs;
        for (int c = 0; c < 2; ++c) { hp[c].reset(); lp[c].reset(); pk[c].reset(); hp[c].highPass (fs, 380.0, 0.8); lp[c].lowPass (fs, 3300.0, 0.8); pk[c].peaking (fs, 1400.0, 1.0, 4.0); }
    }
    void setup (const float* v) { amt = clampd (v[phoneAmt] / 100.0, 0.0, 1.0); grit = 1.0 + 5.0 * clampd (v[phoneGrit] / 100.0, 0.0, 1.0); }
    inline double one (int c, double x) noexcept
    {
        double y = pk[c].process (lp[c].process (hp[c].process (x)));
        y = std::tanh (grit * y) / std::sqrt (grit);
        return y * 1.3;
    }
    inline void process (double& l, double& r) noexcept
    {
        const double pl = one (0, l), pr = one (1, r);
        l = l * (1.0 - amt) + pl * amt;
        r = r * (1.0 - amt) + pr * amt;
    }
private:
    double sr = 48000, amt = 1, grit = 2;
    Biquad hp[2], lp[2], pk[2];
};

//==============================================================================
// DELAY synchronise au tempo, ping-pong, filtres dans la boucle, ducking pilote par la voix.
class DelayFx
{
public:
    void prepare (double fs)
    {
        sr = fs; dl.init ((int) (4.0 * fs)); dr.init ((int) (4.0 * fs));
        curD = -1; env = 0; duckG = 1; fbL = fbR = 0;
        for (auto& f : hp) f.reset();
        for (auto& f : lp) f.reset();
    }
    void setup (const float* v, double bpm)
    {
        static const double beats[6] = { 1.0, 0.5, 0.75, 0.25, 1.5, 2.0 };
        const bool sync = v[dlySync] > 0.5f;
        double sec = sync ? beats[(int) clampd (std::round (v[dlyNote]), 0, 5)] * 60.0 / std::max (30.0, bpm) : (double) v[dlyTime] * 0.001;
        targetD = clampd (sec * sr, 16.0, 3.9 * sr);
        if (curD < 0) curD = targetD;
        fb = clampd (v[dlyFb] / 100.0, 0.0, 0.92);
        ping = v[dlyPing] > 0.5f;
        mix = clampd (v[dlyMix] / 100.0, 0.0, 1.0);
        for (auto& f : hp) f.set (v[dlyHP], sr);
        for (auto& f : lp) f.set (v[dlyLP], sr);
        duck = clampd (v[dlyDuck] / 100.0, 0.0, 1.0);
        envRel = std::exp (-1.0 / (std::max (10.0, (double) v[dlyDuckRel]) * 0.001 * sr));
        duckSm = 1.0 - std::exp (-1.0 / (0.004 * sr));
    }
    double mixAmount() const noexcept { return mix; }

    // renvoie le signal retarde (wet) : l'appelant l'ajoute a la voix
    inline void process (double l, double r, double& wl, double& wr) noexcept
    {
        curD += (targetD - curD) * 0.0008;
        const double yL = dl.read (curD - 1.0), yR = dr.read (curD - 1.0);
        double fL = yL * fb, fR = yR * fb;
        fL = lp[0].lp (hp[0].hp (fL));
        fR = lp[1].lp (hp[1].hp (fR));
        if (ping) { dl.write (0.5 * (l + r) + fR); dr.write (fL); }
        else      { dl.write (l + fL);             dr.write (r + fR); }

        // ducking : le delay baisse quand la voix est presente, puis revient
        const double lvl = std::max (std::abs (l), std::abs (r));
        env = lvl > env ? lvl : lvl + (env - lvl) * envRel;
        const double e = clampd ((linToDb (env) + 55.0) / 30.0, 0.0, 1.0);
        const double tg = dbToLin (-duck * 24.0 * e);
        duckG += (tg - duckG) * (tg < duckG ? duckSm : 0.002);
        wl = yL * duckG; wr = yR * duckG;
    }
private:
    double sr = 48000, curD = -1, targetD = 24000, fb = 0.3, mix = 0, duck = 0.6, env = 0, envRel = 0, duckG = 1, duckSm = 0.1, fbL = 0, fbR = 0;
    bool ping = true;
    Delay dl, dr;
    OnePole hp[2], lp[2];
};

//==============================================================================
// REVERB : Room / Hall = reseau de retards a rebouclage (FDN 8 lignes, matrice de Hadamard) avec diffuseurs a modulation,
// entrees stereo decorrelees et amortissement par ligne ; Plate = reverb a plaque a deux boucles (topologie classique de
// Dattorro, article public de 1997) ; Spring = ressort (dispersion + boucle). Les modulations lentes evitent le son metallique.
class Reverb
{
public:
    static constexpr int N = 8;
    static constexpr int NAP = 4;
    static constexpr int NSP = 20;

    struct ModAP
    {
        Delay d; double g = 0.5;
        void init (int maxLen) { d.init (maxLen + 4); }
        void clear() { d.clear(); }
        inline double process (double x, double len) noexcept
        {
            const double z = d.read (std::max (1.0, len) - 1.0);
            const double v = x + g * z;
            d.write (v);
            return z - g * v;
        }
    };

    void prepare (double fs)
    {
        sr = fs;
        const double k = fs / 29761.0;
        const int maxLine = (int) (0.30 * fs) + 8;
        for (auto& d : line) d.init (maxLine);
        preL.init ((int) (0.26 * fs)); preR.init ((int) (0.26 * fs));
        er.init ((int) (0.12 * fs));
        static const int apL[NAP] = { 142, 107, 379, 277 }, apR[NAP] = { 151, 113, 397, 283 };
        for (int i = 0; i < NAP; ++i) { inApL[i].init ((int) std::ceil (apL[i] * k * 2.5)); inApR[i].init ((int) std::ceil (apR[i] * k * 2.5)); }
        // plaque
        pLap[0].init ((int) std::ceil (672 * k * 2.2)); pLap[1].init ((int) std::ceil (908 * k * 2.2));
        pLap2[0].init ((int) std::ceil (1800 * k * 2.2)); pLap2[1].init ((int) std::ceil (2656 * k * 2.2));
        pD1[0].init ((int) std::ceil (4453 * k * 2.2)); pD1[1].init ((int) std::ceil (4217 * k * 2.2));
        pD2[0].init ((int) std::ceil (3720 * k * 2.2)); pD2[1].init ((int) std::ceil (3163 * k * 2.2));
        // ressort
        for (int c = 0; c < 2; ++c)
        {
            springD[c].init ((int) (0.12 * fs));
            for (int q = 0; q < NSP; ++q) springAP[c][q].init (std::max (1, (int) std::round ((2 + (q * 7 + c * 3) % 9) * fs / 48000.0)), 0.62);
            springLp[c].reset(); springHp[c].reset();
            hpF[c].reset(); lpF[c].reset();
        }
        for (auto& x : lp) x.reset();
        for (auto& x : lpB) x.reset();
        plow[0] = plow[1] = 0;
        for (int i = 0; i < N; ++i) ph[i] = (double) i / N;
        for (auto& x : plpf) x = 0;
        mode = -1; pendingMode = 1; fade = 0; lfoPh = 0; env = 0; duckG = 1; fbL = fbR = 0; plPh = 0;
    }

    void setup (const float* v)
    {
        pendingMode = (int) clampd (std::round (v[revMode]), 0, 3);
        decay = std::max (0.1, (double) v[revDecay]);
        preS = clampd (v[revPre] * 0.001 * sr, 0.0, 0.25 * sr);
        sizeScale = 0.55 + 0.95 * clampd (v[revSize] / 100.0, 0.0, 1.0);
        damp = clampd (v[revDamp] / 100.0, 0.0, 1.0);
        width = clampd (v[revWidth] / 100.0, 0.0, 1.0);
        mix = clampd (v[revMix] / 100.0, 0.0, 1.0);
        diff = clampd (v[revDiff] / 100.0, 0.0, 1.0);
        bass = clampd (v[revBass] / 100.0, 0.5, 2.0);
        modAmt = clampd (v[revMod] / 100.0, 0.0, 1.0);
        duck = clampd (v[revDuck] / 100.0, 0.0, 1.0);
        envRel = std::exp (-1.0 / (0.25 * sr));
        duckSm = 1.0 - std::exp (-1.0 / (0.006 * sr));
        if (std::abs (v[revLow] - lowC) > 0.5 || std::abs (v[revHigh] - highC) > 0.5)
        {
            lowC = v[revLow]; highC = v[revHigh];
            for (int c = 0; c < 2; ++c) { hpF[c].highPass (sr, lowC, 0.7071); lpF[c].lowPass (sr, highC, 0.7071); }
        }
        applyMode (mode < 0 ? pendingMode : mode);
    }
    double mixAmount() const noexcept { return mix; }

    inline void process (double l, double r, double& wl, double& wr) noexcept
    {
        if (pendingMode != mode)
        {
            fade -= 1.0 / (0.015 * sr);
            if (fade <= 0.0) { fade = 0.0; switchMode (pendingMode); }
        }
        else if (fade < 1.0) fade = std::min (1.0, fade + 1.0 / (0.015 * sr));

        preL.write (l); preR.write (r);
        const double xl = preL.read (std::max (0.0, preS)), xr = preR.read (std::max (0.0, preS));
        double oL = 0, oR = 0;

        lfoPh += 1.0 / sr; if (lfoPh > 4000.0) lfoPh -= 4000.0;

        if (mode == 3)
        {
            const double xin = springHp[0].hp (0.5 * (xl + xr));
            for (int c = 0; c < 2; ++c)
            {
                const double tSec = (c == 0 ? 0.0381 : 0.0467) * sizeScale + 0.0004 * std::sin (2 * kPi * (0.35 * lfoPh + 0.5 * c));
                const double dsamp = std::max (4.0, tSec * sr);
                const double fbk = std::pow (10.0, -3.0 * (dsamp / sr) / std::max (0.1, decay));
                double y = springLp[c].lp (springD[c].read (dsamp - 1.0)) * fbk;
                y += xin * (c == 0 ? 1.0 : 0.9);
                double s = y;
                for (int q = 0; q < NSP; ++q) s = springAP[c][q].process (s);
                springD[c].write (std::tanh (s * 0.9));
                (c == 0 ? oL : oR) = s * 1.1;
            }
        }
        else if (mode == 1)
        {
            // ---- plaque (deux boucles croisees) ----
            double a = xl, b = xr;
            for (int i = 0; i < NAP; ++i) { a = inApL[i].process (a, inLen[i]); b = inApR[i].process (b, inLenR[i]); }
            const double m1 = modDepthP * std::sin (2 * kPi * 0.8 * lfoPh), m2 = modDepthP * std::sin (2 * kPi * 0.63 * lfoPh + 1.7);
            double fb[2] = { fbR, fbL };               // chaque boucle recoit la fin de l'autre
            double endv[2];
            for (int c = 0; c < 2; ++c)
            {
                double t = (c == 0 ? a : b) * 0.6 + fb[c];
                t = pLap[c].process (t, pLen[0][c] + (c == 0 ? m1 : m2));
                pD1[c].write (t);
                double y = pD1[c].read (pLen[1][c] - 1.0);
                plpf[c] += dampA * (y - plpf[c]);
                plow[c] += bassA * (plpf[c] - plow[c]);
                y = (plpf[c] + (bassRatP - 1.0) * plow[c]) * pDecay;
                y = pLap2[c].process (y, pLen[2][c]);
                pD2[c].write (y);
                endv[c] = pD2[c].read (pLen[3][c] - 1.0) * pDecay;
            }
            fbL = endv[0]; fbR = endv[1];
            const double k = sr / 29761.0 * sizeScale;
            oL = pD1[1].read (266 * k) + pD1[1].read (2974 * k) + pD2[1].read (1996 * k) - pD1[0].read (1990 * k) - pD2[0].read (1066 * k) - pD2[1].read (187 * k);
            oR = pD1[0].read (353 * k) + pD1[0].read (3627 * k) + pD2[0].read (2673 * k) - pD1[1].read (2111 * k) - pD2[1].read (1066 * k) - pD2[0].read (121 * k);
            oL *= 0.45; oR *= 0.45;
        }
        else
        {
            // ---- FDN : Room / Hall ----
            double a = xl, b = xr;
            for (int i = 0; i < NAP; ++i) { a = inApL[i].process (a, inLen[i]); b = inApR[i].process (b, inLenR[i]); }
            double d[N], o[N];
            for (int i = 0; i < N; ++i)
            {
                double len = lenS[i];
                if (modDepth > 0) { ph[i] += modRate[i] / sr; if (ph[i] >= 1.0) ph[i] -= 1.0; len += modDepth * std::sin (2 * kPi * ph[i]); }
                d[i] = line[i].read (std::max (2.0, len) - 1.0);
                lp[i].z += dampA * (d[i] - lp[i].z);
                o[i] = lp[i].z;
                lpB[i].z += bassA * (o[i] - lpB[i].z);                 // partie grave : decroissance multipliee par BASS
                d[i] = (o[i] + (bassRat[i] - 1.0) * lpB[i].z) * gain[i];
            }
            hadamard (d);
            for (int i = 0; i < N; ++i) line[i].write (((i & 1) ? b : a) * 0.35 + d[i]);
            oL = 0.5 * (o[0] - o[2] + o[4] - o[6]);
            oR = 0.5 * (o[1] - o[3] + o[5] - o[7]);
            if (erOn)
            {
                er.write (0.5 * (a + b));
                for (int q = 0; q < 6; ++q)
                {
                    const double e = er.read (erT[q] * sr);
                    oL += erG[q] * e * 0.35;
                    oR += erG[5 - q] * e * 0.35;
                }
            }
        }

        const double m = 0.5 * (oL + oR), s = 0.5 * (oL - oR) * width;
        oL = lpF[0].process (hpF[0].process (m + s));
        oR = lpF[1].process (hpF[1].process (m - s));

        const double lvl = std::max (std::abs (l), std::abs (r));
        env = lvl > env ? lvl : lvl + (env - lvl) * envRel;
        const double e = clampd ((linToDb (env) + 50.0) / 30.0, 0.0, 1.0);
        const double tg = dbToLin (-duck * 18.0 * e);
        duckG += (tg - duckG) * (tg < duckG ? duckSm : 0.0008);
        wl = oL * fade * wetScale * duckG; wr = oR * fade * wetScale * duckG;
    }

private:
    static inline void hadamard (double* a) noexcept
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h * 2)
                for (int j = i; j < i + h; ++j) { const double x = a[j], y = a[j + h]; a[j] = x + y; a[j + h] = x - y; }
        for (int i = 0; i < N; ++i) a[i] *= 0.35355339059327373;
    }

    void switchMode (int m)
    {
        mode = m;
        for (auto& d : line) d.clear();
        for (auto& a : inApL) a.clear();
        for (auto& a : inApR) a.clear();
        er.clear();
        for (int c = 0; c < 2; ++c)
        {
            pLap[c].clear(); pLap2[c].clear(); pD1[c].clear(); pD2[c].clear(); plpf[c] = 0;
            springD[c].clear(); for (auto& a : springAP[c]) a.clear(); springLp[c].reset(); springHp[c].reset();
        }
        fbL = fbR = 0;
        for (auto& x : lp) x.reset();
        for (auto& x : lpB) x.reset();
        plow[0] = plow[1] = 0;
        applyMode (m);
    }

    void applyMode (int m)
    {
        static const double baseMs[2][N] = {
            { 9.7, 11.3, 13.9, 15.7, 17.9, 19.9, 22.3, 25.1 },     // Room
            { 27.7, 33.1, 38.9, 46.3, 53.9, 61.3, 68.9, 77.3 } };  // Hall
        const double k = sr / 29761.0;
        bassA = 1.0 - std::exp (-2.0 * kPi * 450.0 / sr);

        // diffuseurs d'entree : gain pilote par DIFFUSION, longueurs et modulation selon le mode
        static const int apL[NAP] = { 142, 107, 379, 277 }, apR[NAP] = { 151, 113, 397, 283 };
        static const double apG[NAP] = { 0.75, 0.75, 0.625, 0.625 };
        const double dscale = clampd (diff / 0.7, 0.0, 1.15);
        for (int i = 0; i < NAP; ++i)
        {
            inApL[i].g = std::min (0.82, apG[i] * dscale); inApR[i].g = inApL[i].g;
            const double lenMul = m == 0 ? 0.7 : (m == 1 ? 1.0 : 1.25);
            inLen[i] = apL[i] * k * lenMul; inLenR[i] = apR[i] * k * lenMul;
        }
        if (m == 3)
        {
            wetScale = 0.9;
            springLp[0].set (4200.0 - 2800.0 * damp, sr); springLp[1].set (4200.0 - 2800.0 * damp, sr);
            springHp[0].set (130.0, sr); springHp[1].set (130.0, sr);
            return;
        }
        if (m == 1)
        {
            const double sc = sizeScale;
            static const double base[4][2] = { { 672, 908 }, { 4453, 4217 }, { 1800, 2656 }, { 3720, 3163 } };
            double tot = 0;
            for (int q = 0; q < 4; ++q) for (int c = 0; c < 2; ++c) { pLen[q][c] = base[q][c] * k * sc; tot += pLen[q][c]; }
            const double T = tot / sr;                                  // duree d'un tour complet des deux boucles
            pDecay = std::min (0.985, std::pow (10.0, -3.0 * T / std::max (0.1, decay) / 4.0));
            bassRatP = std::pow (pDecay, 1.0 / bass - 1.0);
            const double dampCut = 17000.0 - 14000.0 * damp;
            dampA = 1.0 - std::exp (-2.0 * kPi * dampCut / sr);
            for (int c = 0; c < 2; ++c) { pLap[c].g = -0.7 * std::min (1.0, 0.45 + 0.8 * diff); pLap2[c].g = 0.5 * std::min (1.0, 0.45 + 0.8 * diff); }
            modDepthP = modAmt * 0.0012 * sr;
            wetScale = 1.5;
            erOn = false;
            return;
        }
        const int h = m == 0 ? 0 : 1;
        const double sc = sizeScale * (m == 0 ? 0.8 : 1.3);
        modDepth = modAmt * (m == 0 ? 0.0010 : 0.0016) * sr;
        for (int i = 0; i < N; ++i) modRate[i] = 0.31 + 0.137 * (double) i + (m == 2 ? 0.1 : 0.0);
        erOn = (m == 0 || m == 2);
        wetScale = m == 0 ? 1.0 : 1.15;
        const double dampCut = 15000.0 - 12500.0 * damp;
        dampA = 1.0 - std::exp (-2.0 * kPi * dampCut / sr);
        static const double erTms[2][6] = { { 6.1, 11.3, 17.9, 23.3, 31.7, 41.3 }, { 14.3, 25.7, 38.9, 52.1, 67.7, 83.9 } };
        static const double erGs[6] = { 0.9, -0.7, 0.55, -0.45, 0.35, -0.28 };
        for (int i = 0; i < 6; ++i) { erT[i] = erTms[m == 0 ? 0 : 1][i] * 0.001 * (0.7 + 0.5 * sizeScale); erG[i] = erGs[i]; }
        for (int i = 0; i < N; ++i)
        {
            lenS[i] = baseMs[h][i] * 0.001 * sr * sc;
            gain[i] = std::pow (10.0, -3.0 * (lenS[i] / sr) / decay);
            bassRat[i] = std::pow (gain[i], 1.0 / bass - 1.0);
        }
    }

    double bass = 1.0, bassA = 0.05, bassRat[N] {}, bassRatP = 1.0, plow[2] {};
    OnePole lpB[N];
    double sr = 48000, decay = 1.8, preS = 0, sizeScale = 1, damp = 0.5, width = 1, mix = 0, lowC = -1, highC = -1, diff = 0.7, modAmt = 0.4;
    double duck = 0, env = 0, envRel = 0, duckG = 1, duckSm = 0.1;
    double lenS[N] {}, gain[N] {}, ph[N] {}, modRate[N] {}, modDepth = 0, dampA = 0.5, wetScale = 1, fade = 0, lfoPh = 0;
    double erT[6] {}, erG[6] {}, inLen[NAP] {}, inLenR[NAP] {};
    double pLen[4][2] {}, pDecay = 0.8, modDepthP = 0, plPh = 0, fbL = 0, fbR = 0, plpf[2] {};
    bool erOn = false;
    int mode = -1, pendingMode = 1;
    Delay line[N], preL, preR, er, springD[2], pD1[2], pD2[2];
    ModAP inApL[NAP], inApR[NAP], pLap[2], pLap2[2];
    Allpass springAP[2][NSP];
    OnePole lp[N], springLp[2], springHp[2];
    Biquad hpF[2], lpF[2];
};

//==============================================================================
// AUTOTUNE : detection de hauteur (YIN sur signal decime) + correction vers la gamme choisie.
// Le decalage de hauteur est fait par Signalsmith Stretch (MIT, https://github.com/Signalsmith-Audio/signalsmith-stretch),
// un decaleur a vocodeur de phase de haute qualite : pas de clic ni de battement. Latence fixe, declaree a l'hote.
class Tune
{
public:
    std::atomic<float> detMidi { 0.f }, tgtMidi { 0.f };   // pour l'affichage (0 = pas de voix detectee)

    void prepare (double fs)
    {
        sr = fs;
        // decaleur temporel a synchronisation de periode (type PSOLA/harmoniseur) : la forme d'onde de la voix est conservee,
        // seul le pas de lecture change ; on saute d'une periode (raccord correle) quand la lecture derive. Pas de phasiness ni d'effet chorus.
        dNom = std::round (kDelayS * fs);
        lat = (int) dNom;
        for (auto& x : bufL) x = 0.f;
        for (auto& x : bufR) x = 0.f;
        wpos = 0; pA = pB = -dNom; xf = false; xfPos = xfLen = 0; cool = 0; periodS = 0;
        D = std::max (1, (int) std::lround (fs / 12000.0));
        fsd = fs / (double) D;
        std::fill (dec.begin(), dec.end(), 0.f);
        decPos = 0; decCnt = 0; acc = 0; hop = 0;
        corr = corrTarget = 0; voiced = false; hadCenter = false; heldHops = 0; unvoicedHops = 99; lastTarget = -1; nHist = 0; hist[0] = hist[1] = hist[2] = 0;
        ctPos = 0; for (auto& c : ctRing) c = 0;
#ifndef TUNE_CORR_DELAY_S
#define TUNE_CORR_DELAY_S 0.0
#endif
        ctDelay = std::min (200, (int) std::lround (TUNE_CORR_DELAY_S * fs / (double) kChunk));
        detMidi.store (0.f); tgtMidi.store (0.f);
        tauMax = std::min (kTauMax, (int) (fsd / 70.0));
        tauMin = std::max (2, (int) (fsd / 800.0));
    }
    int latency() const noexcept { return lat; }

    void setup (const float* v)
    {
        on = v[tuneOn] > 0.5f;
        key = (int) clampd (std::round (v[tuneKey]), 0, 11);
        const int sc = (int) clampd (std::round (v[tuneScale]), 0, 4);
        static const int masks[5] = { 0xFFF, 0xAB5, 0x5AD, 0x295, 0x4A9 };   // chromatique, majeure, mineure, penta maj, penta min
        mask = masks[sc];
        amount = clampd (v[tuneAmt] / 100.0, 0.0, 1.0);
        tau = std::max (0.0, (double) v[tuneSpeed]) * 0.001;     // Retune Speed en ms : 0 = saut instantane (effet robot), 10-50 = naturel
        human = clampd (v[tuneHuman] / 100.0, 0.0, 1.0);
        flex = clampd (v[tuneFlex] / 100.0, 0.0, 1.0);
        vibAmt = clampd (v[tuneVib] / 100.0, 0.0, 2.0);
        formant = v[tuneFormant] > 0.5f;
    }

    // traite le bloc sur place (float) ; la sortie est retardee de latency() echantillons
    void processBlock (float* l, float* r, int n) noexcept
    {
        for (int start = 0; start < n; start += kChunk)
        {
            const int len = std::min (kChunk, n - start);
            float* pl = l + start; float* pr = r + start;

            if (on)
                for (int i = 0; i < len; ++i)
                {
                    acc += 0.5 * ((double) pl[i] + (double) pr[i]);
                    if (++decCnt >= D)
                    {
                        dec[(size_t) (decPos & (kDec - 1))] = (float) (acc / (double) D);
                        ++decPos; decCnt = 0; acc = 0;
                        if (++hop >= 80) { hop = 0; analyse(); }
                    }
                }
            else corrTarget = 0;

            // Humanize : retune plus lent uniquement sur la partie tenue des notes longues
            const double held = (double) heldHops * (80.0 * D / sr);
            const double tauEff = tau + human * 0.15 * clampd ((held - 0.10) / 0.30, 0.0, 1.0);
            const double a = (on && voiced) ? (tauEff < 1e-4 ? 1.0 : 1.0 - std::exp (-(double) len / (tauEff * sr))) : 1.0 - std::exp (-(double) len / (0.05 * sr));
            ctRing[(size_t) (ctPos & 255)] = corrTarget; ++ctPos;
            const double ctUse = ctRing[(size_t) ((ctPos - 1 - ctDelay) & 255)];
            corr += (ctUse - corr) * a;
            // sans correction, le retard revient tout doucement a sa valeur nominale (au plus 0,7 cent de derive : inaudible)
            const double dErr = (double) wpos - pA - dNom;
            const double rate = std::exp2 (corr / 12.0) * (1.0 + clampd (dErr * 1e-4, -4e-4, 4e-4) * (1.0 - clampd (std::abs (corr) / 0.05, 0.0, 1.0)));

            for (int i = 0; i < len; ++i)
            {
                bufL[(size_t) (wpos & kMask)] = pl[i]; bufR[(size_t) (wpos & kMask)] = pr[i];
                ++wpos;
                double yl = rd (bufL, pA), yr = rd (bufR, pA);
                if (xf)
                {
                    const double g = 0.5 - 0.5 * std::cos (kPi * (double) xfPos / (double) xfLen);
                    yl += (rd (bufL, pB) - yl) * g; yr += (rd (bufR, pB) - yr) * g;
                    pB += rate; pA += rate;
                    if (++xfPos >= xfLen) { pA = pB; xf = false; }
                }
                else pA += rate;
                pl[i] = (float) yl; pr[i] = (float) yr;
                if (! xf && periodS > 0.0) { if (cool > 0) --cool; else maybeSplice(); }
            }
        }
    }

private:
    static constexpr int kDec = 4096, kTauMax = 260, kWin = 512, kChunk = 64, kBuf = 16384, kMask = kBuf - 1;
#ifndef TUNE_DELAY_S
#define TUNE_DELAY_S 0.046
#endif
    static constexpr double kDelayS = TUNE_DELAY_S;

    // lecture interpolee (Hermite) a la position (negative = relative au debut) 
    static inline double rd (const std::array<float, kBuf>& b, double p) noexcept
    {
        const double fl = std::floor (p); const double t = p - fl; const long i = (long) fl;
        const double y0 = b[(size_t) ((i - 1) & kMask)], y1 = b[(size_t) (i & kMask)], y2 = b[(size_t) ((i + 1) & kMask)], y3 = b[(size_t) ((i + 2) & kMask)];
        const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    inline double mono (long i) const noexcept { return 0.5 * ((double) bufL[(size_t) (i & kMask)] + (double) bufR[(size_t) (i & kMask)]); }

    // la lecture derive par rapport a l'ecriture (rapport de lecture != 1) : quand l'ecart au retard nominal depasse une demi-periode,
    // on saute d'une periode (en avant ou en arriere) vers le point le mieux correle, avec un fondu enchaine
    void maybeSplice() noexcept
    {
        const double d = (double) wpos - 1.0 - pA;
        const double T = periodS, half = 0.5 * T;
        if (d >= dNom - half && d <= dNom + half) return;
        const int dir = d < dNom ? 1 : -1;               // +1 : on recule (le retard augmente)
        int L = (int) clampd (T, 96.0, 400.0);
        const long base = (long) std::floor (pA);
        const int dl = (int) std::floor (0.2 * T);
        double bestS = -2; int bestK = 0;
        double ex = 0; for (int k = 0; k < L; ++k) { const double x = mono (base + k); ex += x * x; }
        for (int dk = -dl; dk <= dl; ++dk)
        {
            const long q = base - (long) dir * (long) std::lround (T) + dk;
            if (q + L >= wpos || q < wpos - kBuf + 1024) continue;
            double s = 0, eq = 0;
            for (int k = 0; k < L; ++k) { const double y = mono (q + k); s += mono (base + k) * y; eq += y * y; }
            const double sc = s / std::sqrt (ex * eq + 1e-12);
            if (sc > bestS) { bestS = sc; bestK = dk; }
        }
        if (bestS < 0.3 && std::abs (d - dNom) < 0.9 * T) { cool = 48; return; }
        if (bestS < -1) { cool = 48; return; }
        pB = pA - (double) dir * (double) std::lround (T) + (double) bestK;
        xf = true; xfPos = 0; xfLen = L;
    }

    void analyse() noexcept
    {
        const int n = kWin + tauMax + 1;
        double e = 0;
        for (int i = 0; i < n; ++i)
        {
            const double x = (double) dec[(size_t) ((decPos - n + i) & (kDec - 1))];
            frame[(size_t) i] = x; if (i < kWin) e += x * x;
        }
        e /= kWin;
        double f0 = 0;
        if (e > 2e-6)
        {
            double run = 0;
            cm[0] = 1.0;
            for (int t = 1; t <= tauMax; ++t)
            {
                double s = 0;
                for (int j = 0; j < kWin; ++j) { const double df = frame[(size_t) j] - frame[(size_t) (j + t)]; s += df * df; }
                run += s;
                cm[(size_t) t] = run > 1e-12 ? s * (double) t / run : 1.0;
            }
            int best = -1;
            for (int t = tauMin; t < tauMax; ++t)
                if (cm[(size_t) t] < 0.15 && cm[(size_t) t] <= cm[(size_t) (t + 1)]) { best = t; break; }
            if (best < 0)
            {
                double m = 0.3;
                for (int t = tauMin; t < tauMax; ++t) if (cm[(size_t) t] < m) { m = cm[(size_t) t]; best = t; }
            }
            if (best > 1)
            {
                const double a = cm[(size_t) (best - 1)], b = cm[(size_t) best], c = cm[(size_t) (best + 1)];
                const double den = a - 2.0 * b + c;
                const double tauS = (double) best + (std::abs (den) > 1e-9 ? 0.5 * (a - c) / den : 0.0);
                f0 = fsd / tauS;
            }
        }
        if (f0 > 60.0 && f0 < 900.0)
        {
            double m = 69.0 + 12.0 * std::log2 (f0 / 440.0);
            hist[2] = hist[1]; hist[1] = hist[0]; hist[0] = m; nHist = std::min (3, nHist + 1);
            if (nHist == 3) { const double a = hist[0], b = hist[1], c = hist[2]; m = std::max (std::min (a, b), std::min (std::max (a, b), c)); }
            unvoicedHops = 0; voiced = true;
            int target = (int) std::lround (m);
            double best = 1e9; int bn = target;
            for (int n = target - 3; n <= target + 3; ++n)
            {
                const int deg = (((n - key) % 12) + 12) % 12;
                if (! ((mask >> deg) & 1)) continue;
                const double dist = std::abs (m - (double) n);
                if (dist < best) { best = dist; bn = n; }
            }
            // hysteresis : on reste sur la note courante tant que la voix ne s'en eloigne pas franchement
            if (lastTarget >= 0 && std::abs (m - (double) lastTarget) < best + 0.15)
            {
                const int deg = (((lastTarget - key) % 12) + 12) % 12;
                if ((mask >> deg) & 1) bn = lastTarget;
            }
            // centre de hauteur lent (sans vibrato) : sert a Natural Vibrato
            const bool jumped = ! hadCenter || std::abs (m - centre) > 0.8;
            if (jumped) { centre = m; hadCenter = true; }
            else centre += 0.07 * (m - centre);
            heldHops = (std::abs (m - centre) < 0.6 && ! jumped) ? heldHops + 1 : 0;
            lastTarget = bn;
            // Flex-Tune : on ne tire la note que lorsqu'elle approche de la cible (les glissandos lointains sont preserves)
            const double err = (double) bn - m;
            const double w = flex <= 0.0 ? 1.0 : clampd (1.0 - flex * (std::abs (err) - 0.25) / 0.25, 0.0, 1.0);
            const double errK = clampd ((std::abs (err) - 0.03) / 0.05, 0.0, 1.0);   // zone morte ~3 cents : une voix deja juste n'est pas touchee (pas de derive du decaleur)
            corrTarget = clampd (amount * (w * err * errK + vibAmt * (m - centre)), -6.0, 6.0);
            periodS = sr / f0;
            detMidi.store ((float) m); tgtMidi.store ((float) bn);
        }
        else
        {
            nHist = 0;
            if (++unvoicedHops >= 4) { voiced = false; periodS = 0; hadCenter = false; heldHops = 0; corrTarget = 0; lastTarget = -1; detMidi.store (0.f); tgtMidi.store (0.f); }
        }
    }

    double sr = 48000, fsd = 12000, corr = 0, corrTarget = 0, amount = 1, acc = 0, tau = 0.02, mixT = 0, human = 0, flex = 0, vibAmt = 0, centre = 0;
    int heldHops = 0;
    double gate = 0, absSm = 0; bool gateOn = false;
    double ctRing[256] {}; int ctPos = 0, ctDelay = 0;
    bool formant = true, hadCenter = false;
    int lat = 5760, D = 4, decPos = 0, decCnt = 0, hop = 0, key = 0, mask = 0xFFF, tauMax = 171, tauMin = 15, unvoicedHops = 99, lastTarget = -1, nHist = 0;
    bool on = false, voiced = false;
    double hist[3] {};
    std::array<float, kDec> dec {};
    std::array<double, kWin + kTauMax + 4> frame {};
    std::array<double, kTauMax + 4> cm {};
    std::array<float, kBuf> bufL {}, bufR {};
    long wpos = 0; double pA = 0, pB = 0, dNom = 1056, periodS = 0; bool xf = false; int xfPos = 0, xfLen = 0, cool = 0;
};

//==============================================================================
// LIMITEUR de sortie : anticipation 1,5 ms (garantit de ne jamais depasser le plafond), relachement automatique, stereo lie.
class Limiter
{
public:
    static constexpr int kMaxLa = 512;
    void prepare (double fs)
    {
        la = std::min (kMaxLa - 2, std::max (8, (int) std::lround (0.0015 * fs)));
        bufL.fill (0.0); bufR.fill (0.0);
        reqBuf.fill (1.0); envBuf.fill (1.0);
        pos = 0; env = 1.0; boxSum = (double) (la + 1);
        relC = 1.0 - std::exp (-1.0 / (0.08 * fs));
    }
    int latency() const noexcept { return la; }
    void setCeiling (double dB) { ceil = dbToLin (clampd (dB, -12.0, 0.0)); }
    inline void process (double& l, double& r) noexcept
    {
        const int N = kMaxLa;
        const double peak = std::max (std::abs (l), std::abs (r));
        const double req = peak > ceil ? ceil / peak : 1.0;
        bufL[pos] = l; bufR[pos] = r; reqBuf[pos] = req;
        // minimum de la demande sur la fenetre d'anticipation
        double mn = 1.0;
        for (int k = 0; k <= la; ++k) mn = std::min (mn, reqBuf[(pos - k) & (N - 1)]);
        // attaque instantanee sur le minimum, relachement progressif
        env = mn < env ? mn : env + (mn - env) * relC;
        boxSum += env - envBuf[(pos - la - 1) & (N - 1)];
        envBuf[pos] = env;
        const double g = boxSum / (double) (la + 1);              // lissage de la fenetre : rampe d'attaque sans clic
        const int rd = (pos - la) & (N - 1);
        l = bufL[rd] * g; r = bufR[rd] * g;
        pos = (pos + 1) & (N - 1);
    }
private:
    int la = 72, pos = 0;
    double ceil = 0.89, env = 1.0, relC = 0.001, boxSum = 1.0;
    std::array<double, kMaxLa> bufL {}, bufR {}, reqBuf {}, envBuf {};
};

//==============================================================================
class Engine
{
public:
    const DsScope& dsScope() const { return des.scope; }
    float dsGrNow() const { return des.grDb.load(); }
    std::atomic<float> inPk { 0.f }, outPk { 0.f }, grDb { 0.f };
    int latency() const noexcept { return tune.latency() + lim.latency(); }
    const Tune& tuner() const noexcept { return tune; }

    // Donnees pour l'analyseur de spectre (file mono lock-free : un producteur audio, un lecteur interface)
    static constexpr int kScope = 16384;
    juce::AbstractFifo scopeFifo { kScope };
    std::array<float, kScope> scopeBuf {};

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        tmpL.assign (16384, 0.f); tmpR.assign (16384, 0.f);
        comp.prepare (sr); eq.prepare (sr); des.prepare (sr); air.prepare (sr); wdr.prepare (sr); sat.prepare (sr); dbl.prepare (sr); cho.prepare (sr);
        phn.prepare (sr); dly.prepare (sr); rev.prepare (sr); tune.prepare (sr); lim.prepare (sr);
        dryLine[0].init (tune.latency() + 8); dryLine[1].init (tune.latency() + 8);
        rampStep = 1.0 / (0.02 * sr);
        for (auto& r : ramp) r = -1.0;
        byp = 0; inG = outG = 1.0; first = true;
        scopeFifo.reset();
    }

    void setBpm (double b) noexcept { bpm = b > 20.0 ? b : 120.0; }
    void setParams (const float* v) { for (int i = 0; i < Count; ++i) p[(size_t) i] = v[i]; }

    void process (float* l, float* r, int n)
    {
        const float* v = p.data();
        comp.setup (v); eq.setup (v); des.setup (v); air.setup (v); sat.setup (v); dbl.setup (v); cho.setup (v); phn.setup (v);
        dly.setup (v, bpm); rev.setup (v); tune.setup (v); lim.setCeiling (v[limCeil]);

        const bool on[kRamps] = { v[eqOn] > 0.5f, v[compOn] > 0.5f, v[fxOn] > 0.5f && v[deessOn] > 0.5f, v[fxOn] > 0.5f && v[satOn] > 0.5f, v[airOn] > 0.5f,
                                  v[fxOn] > 0.5f && v[dblOn] > 0.5f, v[fxOn] > 0.5f && v[choOn] > 0.5f, v[fxOn] > 0.5f && v[phoneOn] > 0.5f,
                                  v[fxOn] > 0.5f && v[widthOn] > 0.5f, v[dlyOn] > 0.5f, v[revOn] > 0.5f };
        if (first) { for (int i = 0; i < kRamps; ++i) ramp[(size_t) i] = on[i] ? 1.0 : 0.0; byp = v[bypass] > 0.5f ? 1.0 : 0.0; first = false; }
        const double inTarget = dbToLin (v[inGain]), outTarget = dbToLin (v[outGain]);
        const double dMix = dly.mixAmount(), rMix = rev.mixAmount();
        const double wid = clampd (v[width] / 100.0, 0.0, 2.0);
        const bool bypassOn = v[bypass] > 0.5f;
        double pkIn = 0, pkOut = 0;
        const double ig0 = inG, og0 = outG;
        n = std::min (n, 16384);
        for (int i = 0; i < n; ++i)
        {
            const double ig = ig0 + (inTarget - ig0) * (double) (i + 1) / (double) n;
            tmpL[(size_t) i] = (float) (l[i] * ig); tmpR[(size_t) i] = (float) (r[i] * ig);
        }
        tune.processBlock (tmpL.data(), tmpR.data(), n);

        for (int i = 0; i < n; ++i)
        {
            const double dryL = l[i], dryR = r[i];
            pkIn = std::max (pkIn, std::max (std::abs (dryL), std::abs (dryR)));
            const double f = (double) (i + 1) / (double) n;
            for (int k = 0; k < kRamps; ++k)
            {
                const double t = on[k] ? 1.0 : 0.0;
                double& rr = ramp[(size_t) k];
                rr = rr < t ? std::min (t, rr + rampStep) : std::max (t, rr - rampStep);
            }
            byp = byp < (bypassOn ? 1.0 : 0.0) ? std::min (1.0, byp + rampStep) : std::max (bypassOn ? 1.0 : 0.0, byp - rampStep);

            dryLine[0].write (dryL); dryLine[1].write (dryR);
            const double dlyDryL = dryLine[0].read ((double) tune.latency()), dlyDryR = dryLine[1].read ((double) tune.latency());
            double xl = tmpL[(size_t) i], xr = tmpR[(size_t) i];

            if (ramp[rEq] > 0.0)  { double a = xl, b = xr; eq.process (a, b);   mixIn (xl, xr, a, b, ramp[rEq]); }
            if (ramp[rComp] > 0.0){ double a = xl, b = xr; comp.process (a, b); mixIn (xl, xr, a, b, ramp[rComp]); }
            if (ramp[rDes] > 0.0) { double a = xl, b = xr; des.process (a, b);  mixIn (xl, xr, a, b, ramp[rDes]); }
            if (ramp[rSat] > 0.0) { double a = xl, b = xr; sat.process (a, b);  mixIn (xl, xr, a, b, ramp[rSat]); }
            if (ramp[rAir] > 0.0) { double a = xl, b = xr; air.process (a, b);  mixIn (xl, xr, a, b, ramp[rAir]); }
            if (ramp[rDbl] > 0.0) { double a = xl, b = xr; dbl.process (a, b);  mixIn (xl, xr, a, b, ramp[rDbl]); }
            if (ramp[rCho] > 0.0) { double a = xl, b = xr; cho.process (a, b);  mixIn (xl, xr, a, b, ramp[rCho]); }
            if (ramp[rPhn] > 0.0) { double a = xl, b = xr; phn.process (a, b);  mixIn (xl, xr, a, b, ramp[rPhn]); }
            if (ramp[rWid] > 0.0)
            {
                const double m = 0.5 * (xl + xr);
                double s = 0.5 * (xl - xr) * wid;
                if (wid > 1.0) s += wdr.side (m) * (wid - 1.0) * 0.6;     // largeur creee a partir du centre (source mono)
                else           (void) wdr.side (m);
                mixIn (xl, xr, m + s, m - s, ramp[rWid]);
            }
            if (ramp[rDly] > 0.0) { double wl, wr; dly.process (xl, xr, wl, wr); xl += wl * dMix * ramp[rDly]; xr += wr * dMix * ramp[rDly]; }
            if (ramp[rRev] > 0.0) { double wl, wr; rev.process (xl, xr, wl, wr); xl += wl * rMix * ramp[rRev] * 0.8; xr += wr * rMix * ramp[rRev] * 0.8; }

            const double og = og0 + (outTarget - og0) * f;
            xl *= og; xr *= og;
            xl = xl * (1.0 - byp) + dlyDryL * byp;
            xr = xr * (1.0 - byp) + dlyDryR * byp;
            lim.process (xl, xr);
            xl = guard (xl); xr = guard (xr);
            pkOut = std::max (pkOut, std::max (std::abs (xl), std::abs (xr)));
            l[i] = (float) xl; r[i] = (float) xr;
        }
        inG = inTarget; outG = outTarget;
        comp.publish();

        inPk.store (std::max (inPk.load(), (float) pkIn));
        outPk.store (std::max (outPk.load(), (float) pkOut));
        grDb.store (comp.grDb.load());

        // analyseur : on pousse la sortie mono (si la file est pleine, on ignore)
        int s1, z1, s2, z2;
        scopeFifo.prepareToWrite (n, s1, z1, s2, z2);
        for (int i = 0; i < z1; ++i) scopeBuf[(size_t) (s1 + i)] = 0.5f * (l[i] + r[i]);
        for (int i = 0; i < z2; ++i) scopeBuf[(size_t) (s2 + i)] = 0.5f * (l[z1 + i] + r[z1 + i]);
        scopeFifo.finishedWrite (z1 + z2);
    }

private:
    enum { rEq = 0, rComp, rDes, rSat, rAir, rDbl, rCho, rPhn, rWid, rDly, rRev, kRamps };
    static inline void mixIn (double& xl, double& xr, double a, double b, double t) noexcept
    {
        xl += (a - xl) * t; xr += (b - xr) * t;
    }
    // protection de sortie : transparente jusqu'a -0,9 FS, puis ecretage doux (jamais de clip dur ni de NaN)
    static inline double guard (double x) noexcept
    {
        if (! std::isfinite (x)) return 0.0;
        const double a = std::abs (x);
        if (a <= 0.9) return x;
        const double y = 0.9 + 0.1 * std::tanh ((a - 0.9) / 0.1);
        return x < 0 ? -y : y;
    }

    double sr = 48000, bpm = 120, rampStep = 0.001, byp = 0, inG = 1, outG = 1;
    bool first = true;
    std::array<float, Count> p {};
    std::array<double, kRamps> ramp {};
    Delay dryLine[2];
    std::vector<float> tmpL, tmpR;
    Tune tune; Limiter lim;
    Comp comp; EQ eq; DeEsser des; Air air; Wider wdr; Saturator sat; Doubler dbl; Chorus cho; Phone phn; DelayFx dly; Reverb rev;
};
} // namespace yv
