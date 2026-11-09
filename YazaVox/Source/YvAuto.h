#pragma once

// ThaVOICE : AUTO VOIX. Le plugin ecoute la voix pendant ~10 s de chant/rap (VoiceAnalyzer, tourne dans le callback audio),
// puis calcule des reglages de mix (computeAutoSettings, sur le thread interface) : niveau d'entree, compresseur, EQ, de-esser,
// air, saturation, delay/reverb et, si l'autotune est active, la tonalite (Key/Scale) trouvee dans la melodie.
// Tout est mesure sur la voix elle-meme ; les reglages sont bornes pour ne jamais partir trop loin.

#include "YvDsp.h"
#include <juce_dsp/juce_dsp.h>

namespace yv
{
// Analyse d'un canal (voix ou prod) : anneau, FFT, niveau, bandes tiers d'octave
class SpecChan
{
public:
    static constexpr int kN = 2048, kBands = 22;
    SpecChan() : fft (11)
    {
        for (int i = 0; i < kN; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * 3.14159265f * (float) i / (float) kN);
    }
    void prepare (double fs)
    {
        sr = fs;
        for (int k = 0; k < kBands; ++k)
        {
            const double fc = 100.0 * std::pow (2.0, k / 3.0);
            bandLo[(size_t) k] = std::max (1, (int) std::floor (fc * std::pow (2.0, -1.0 / 6.0) * kN / fs));
            bandHi[(size_t) k] = std::min (kN / 2 - 1, (int) std::floor (fc * std::pow (2.0, 1.0 / 6.0) * kN / fs));
            bandFc[(size_t) k] = fc;
        }
        sibLo = std::max (1, (int) (5000.0 * kN / fs)); sibHi = std::min (kN / 2 - 1, (int) (10000.0 * kN / fs));
        totLo = std::max (1, (int) (80.0 * kN / fs)); totHi = std::min (kN / 2 - 1, (int) (16000.0 * kN / fs));
        chLo = std::max (2, (int) (110.0 * kN / fs)); chHi = std::min (kN / 2 - 2, (int) (1800.0 * kN / fs));
    }
    void reset() { std::fill (ring.begin(), ring.end(), 0.f); fill = 0; std::fill (chroma.begin(), chroma.end(), 0.0); }
    inline void add (float x) noexcept { ring[(size_t) (fill & (kN - 1))] = x; ++fill; }
    bool ready() const noexcept { return fill >= kN; }
    // calcule niveau (dB RMS), bandes (dB), sibilance ; withChroma : accumule le chromagramme (hauteurs dominantes)
    void frame (float& level, float* bands, float& sib, float& sibCent, bool withChroma) noexcept
    {
        double ms = 0;
        for (int i = 0; i < kN; ++i)
        {
            const float x = ring[(size_t) ((fill - kN + i) & (kN - 1))];
            ms += (double) x * x; fftData[(size_t) i] = x * win[(size_t) i];
        }
        std::fill (fftData.begin() + kN, fftData.end(), 0.f);
        ms /= kN;
        level = (float) (10.0 * std::log10 (ms + 1e-12));
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        double tot = 1e-30, sb = 0, sc = 0;
        for (int b = totLo; b <= totHi; ++b) tot += (double) fftData[(size_t) b] * fftData[(size_t) b];
        for (int b = sibLo; b <= sibHi; ++b) { const double p = (double) fftData[(size_t) b] * fftData[(size_t) b]; sb += p; sc += p * (double) b * sr / (double) kN; }
        sib = (float) (sb / tot);
        sibCent = sb > 1e-30 ? (float) (sc / sb) : 0.f;
        for (int k = 0; k < kBands; ++k)
        {
            double e = 1e-30;
            for (int b = bandLo[(size_t) k]; b <= bandHi[(size_t) k]; ++b) e += (double) fftData[(size_t) b] * fftData[(size_t) b];
            bands[k] = (float) (10.0 * std::log10 (e));
        }
        if (withChroma && level > -50.f)
        {
            float mx = 0; for (int b = chLo; b <= chHi; ++b) mx = std::max (mx, fftData[(size_t) b]);
            const float thr = mx * 0.06f;      // -24 dB sous le pic de la trame
            for (int b = chLo; b <= chHi; ++b)
            {
                const float m = fftData[(size_t) b];
                if (m < thr || m < fftData[(size_t) (b - 1)] || m < fftData[(size_t) (b + 1)]) continue;
                const double a = fftData[(size_t) (b - 1)], c = fftData[(size_t) (b + 1)], d = a - 2.0 * (double) m + c;
                const double bf = (double) b + (std::abs (d) > 1e-12 ? 0.5 * (a - c) / d : 0.0);
                const double midiN = 69.0 + 12.0 * std::log2 (bf * sr / (double) kN / 440.0);
                const int pc = ((((int) std::lround (midiN)) % 12) + 12) % 12;
                chroma[(size_t) pc] += (double) m;
            }
        }
    }
    double bandCentre (int k) const noexcept { return bandFc[(size_t) k]; }
    std::array<double, 12> chroma {};
private:
    juce::dsp::FFT fft;
    std::array<float, kN> win {}, ring {};
    std::array<float, 2 * kN> fftData {};
    std::array<int, kBands> bandLo {}, bandHi {};
    std::array<double, kBands> bandFc {};
    int sibLo = 1, sibHi = 1, totLo = 1, totHi = 1, chLo = 2, chHi = 3;
    double sr = 48000;
    long long fill = 0;
};

class VoiceAnalyzer
{
public:
    static constexpr int kN = SpecChan::kN, kH = 1024, kBands = SpecChan::kBands, kMaxFrames = 3600;
    static constexpr double kNeedActiveSec = 10.0;

    VoiceAnalyzer()
    {
        level.resize (kMaxFrames); sib.resize (kMaxFrames); sibCent.resize (kMaxFrames); midi.resize (kMaxFrames); levelP.resize (kMaxFrames);
        bands.resize ((size_t) kMaxFrames * kBands); bandsP.resize ((size_t) kMaxFrames * kBands);
    }

    void prepare (double fs)
    {
        sr = fs; voice.prepare (fs); prod.prepare (fs);
        D = std::max (1, (int) std::lround (fs / 12000.0)); fsd = fs / (double) D;
        tauMax = std::min (kTauMax, (int) (fsd / 70.0)); tauMin = std::max (2, (int) (fsd / 800.0));
        cancel();
    }

    //---- commandes (thread interface)
    void start()
    {
        nFrames = 0; sinceHop = 0; decAcc = 0; decCnt = 0; decPos = 0; activeFrames = 0; prodFrames = 0; sawProd = false;
        voice.reset(); prod.reset();
        std::fill (dec.begin(), dec.end(), 0.f);
        progressV.store (0.f);
        st.store (1);
    }
    void cancel() { st.store (0); progressV.store (0.f); }

    bool listening() const noexcept { return st.load (std::memory_order_relaxed) == 1; }
    int  state() const noexcept { return st.load(); }           // 0 repos, 1 ecoute, 2 pret, 3 pas assez de voix
    float progress() const noexcept { return progressV.load(); }
    bool takeDone() noexcept { return doneFlag.exchange (false); }

    //---- thread audio : voix (l, r) et, si le bus sidechain est actif, la prod (pl, pr)
    void push (const float* l, const float* r, int n, const float* pl = nullptr, const float* pr = nullptr) noexcept
    {
        if (st.load (std::memory_order_relaxed) != 1) return;
        if (pl != nullptr) sawProd = true;
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            voice.add (m);
            prod.add (pl != nullptr ? 0.5f * (pl[i] + (pr != nullptr ? pr[i] : pl[i])) : 0.f);
            decAcc += m;
            if (++decCnt >= D) { dec[(size_t) (decPos & (kDec - 1))] = decAcc / (float) D; ++decPos; decCnt = 0; decAcc = 0; }
            if (++sinceHop >= kH)
            {
                sinceHop = 0;
                if (voice.ready()) frame();
                if (st.load (std::memory_order_relaxed) != 1) return;
            }
        }
    }

    //---- donnees (valides quand state() != 1)
    int frames() const noexcept { return nFrames; }
    float lev (int f) const noexcept { return level[(size_t) f]; }
    float levProd (int f) const noexcept { return levelP[(size_t) f]; }
    float sibRatio (int f) const noexcept { return sib[(size_t) f]; }
    float sibCentroid (int f) const noexcept { return sibCent[(size_t) f]; }
    float pitch (int f) const noexcept { return midi[(size_t) f]; }
    float band (int f, int k) const noexcept { return bands[(size_t) f * kBands + (size_t) k]; }
    float bandProd (int f, int k) const noexcept { return bandsP[(size_t) f * kBands + (size_t) k]; }
    double bandCentre (int k) const noexcept { return voice.bandCentre (k); }
    double hopSeconds() const noexcept { return (double) kH / sr; }
    bool prodPresent() const noexcept { return sawProd && nFrames > 0 && prodFrames * 2 >= nFrames; }   // la prod joue dans au moins la moitie du temps
    const std::array<double, 12>& prodChroma() const noexcept { return prod.chroma; }

private:
    static constexpr int kDec = 4096, kWin = 512, kTauMax = 260;

    void frame() noexcept
    {
        const int f = nFrames;
        float sb, sc, lvl;
        voice.frame (lvl, &bands[(size_t) f * kBands], sb, sc, false);
        level[(size_t) f] = lvl; sib[(size_t) f] = sb; sibCent[(size_t) f] = sc;
        float lp = -120.f, s2, c2;
        if (sawProd) prod.frame (lp, &bandsP[(size_t) f * kBands], s2, c2, lvl > -40.f);   // la tonalite de la prod est lue quand la voix joue
        levelP[(size_t) f] = lp;
        if (lp > -55.f) ++prodFrames;
        midi[(size_t) f] = lvl > -50.f ? detectPitch() : 0.f;

        ++nFrames;
        if (lvl > -40.f) ++activeFrames;
        const double activeSec = (double) activeFrames * hopSeconds();
        progressV.store ((float) std::min (1.0, activeSec / kNeedActiveSec));
        if (activeSec >= kNeedActiveSec) { st.store (2); doneFlag.store (true); }
        else if (nFrames >= kMaxFrames) { st.store (activeSec >= 3.0 ? 2 : 3); doneFlag.store (true); }
    }

    // YIN (meme principe que l'autotune) : renvoie la note MIDI fractionnaire ou 0
    float detectPitch() noexcept
    {
        const int n = kWin + tauMax + 1;
        double e = 0;
        for (int i = 0; i < n; ++i)
        {
            const double x = (double) dec[(size_t) ((decPos - n + i) & (kDec - 1))];
            yinFrame[(size_t) i] = x; if (i < kWin) e += x * x;
        }
        if (e / kWin < 2e-6) return 0.f;
        double run = 0; yinCm[0] = 1.0;
        for (int t = 1; t <= tauMax; ++t)
        {
            double s = 0;
            for (int j = 0; j < kWin; ++j) { const double df = yinFrame[(size_t) j] - yinFrame[(size_t) (j + t)]; s += df * df; }
            run += s;
            yinCm[(size_t) t] = run > 1e-12 ? s * (double) t / run : 1.0;
        }
        int best = -1;
        for (int t = tauMin; t < tauMax; ++t)
            if (yinCm[(size_t) t] < 0.12 && yinCm[(size_t) t] <= yinCm[(size_t) (t + 1)]) { best = t; break; }
        if (best < 2) return 0.f;
        const double a = yinCm[(size_t) (best - 1)], b = yinCm[(size_t) best], c = yinCm[(size_t) (best + 1)];
        const double den = a - 2.0 * b + c;
        const double tauS = (double) best + (std::abs (den) > 1e-9 ? 0.5 * (a - c) / den : 0.0);
        const double f0 = fsd / tauS;
        if (f0 < 65.0 || f0 > 900.0) return 0.f;
        return (float) (69.0 + 12.0 * std::log2 (f0 / 440.0));
    }

    SpecChan voice, prod;
    std::array<float, kDec> dec {};
    std::array<double, kWin + kTauMax + 4> yinFrame {};
    std::array<double, kTauMax + 4> yinCm {};
    std::vector<float> level, levelP, sib, sibCent, midi, bands, bandsP;
    double sr = 48000, fsd = 12000;
    int D = 4, tauMax = 171, tauMin = 15;
    long long decPos = 0;
    int sinceHop = 0, decCnt = 0, nFrames = 0, activeFrames = 0, prodFrames = 0;
    bool sawProd = false;
    float decAcc = 0;
    std::atomic<int> st { 0 };
    std::atomic<float> progressV { 0.f };
    std::atomic<bool> doneFlag { false };
};

//==============================================================================
struct AutoResult
{
    bool ok = false;
    juce::String message;
    std::vector<std::pair<const char*, float>> set;   // (id du parametre, valeur reelle)
    // diagnostic
    double p80 = 0, spread = 0, voicedSec = 0, sibRel = 0, devMud = 0, devPres = 0, devAir = 0, f0 = 0, keyCorr = 0;
    int key = -1, scale = -1;
    bool prod = false;                       // la prod (sidechain) a ete ecoutee
    double prodOffs = 0, outG = 0, mudCut = 0, presBoost = 0, mudMask = 0;
};

namespace autodetail
{
inline double percentile (std::vector<double> v, double q)
{
    if (v.empty()) return 0;
    std::sort (v.begin(), v.end());
    return v[(size_t) std::min ((double) v.size() - 1.0, std::floor (q * (double) (v.size() - 1) + 0.5))];
}
inline const char* noteName (int k)
{
    static const char* n[12] = { "Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si" };
    return n[((k % 12) + 12) % 12];
}
// Krumhansl-Schmuckler : tonalite la plus probable d'un histogramme de classes de hauteur
inline void detectKey (const double* h, int& key, bool& minor, double& corr, double& second)
{
    static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
    double mh = 0; for (int i = 0; i < 12; ++i) mh += h[i]; mh /= 12.0;
    double sh = 0; for (int i = 0; i < 12; ++i) sh += (h[i] - mh) * (h[i] - mh);
    double best = -2, sec = -2; key = 0; minor = false;
    for (int t = 0; t < 12; ++t)
        for (int m = 0; m < 2; ++m)
        {
            const double* pr = m ? mnr : maj;
            double mp = 0; for (int i = 0; i < 12; ++i) mp += pr[i]; mp /= 12.0;
            double sp = 0, cv = 0;
            for (int i = 0; i < 12; ++i) { const double a = h[(i + t) % 12] - mh, b = pr[i] - mp; cv += a * b; sp += b * b; }
            const double r = cv / std::sqrt (sh * sp + 1e-12);
            if (r > best) { sec = best; best = r; key = t; minor = m == 1; }
            else if (r > sec) sec = r;
        }
    corr = best; second = sec;
}
} // namespace autodetail

inline AutoResult computeAutoSettings (const VoiceAnalyzer& an, bool tuneOn)
{
    using namespace autodetail;
    constexpr int NB = VoiceAnalyzer::kBands;
    AutoResult R;
    const int nf = an.frames();
    if (nf < 20) { R.message = "Pas assez de voix"; return R; }

    // --- trames "voix" : on garde les trames fortes (relatif au niveau haut, pour ignorer silences et bruit de fond)
    std::vector<double> all;
    for (int f = 0; f < nf; ++f) if (an.lev (f) > -60.f) all.push_back (an.lev (f));
    if (all.size() < 20) { R.message = "Pas assez de voix"; return R; }
    const double top = percentile (all, 0.95);
    const double gate = std::max (-55.0, top - 26.0);
    std::vector<int> vf;
    for (int f = 0; f < nf; ++f) if (an.lev (f) > gate) vf.push_back (f);
    R.voicedSec = (double) vf.size() * an.hopSeconds();
    if (R.voicedSec < 2.5) { R.message = "Pas assez de voix"; return R; }
    const bool hasProd = an.prodPresent();
    R.prod = hasProd;

    std::vector<double> lv; for (int f : vf) lv.push_back (an.lev (f));
    const double p30 = percentile (lv, 0.30), p70 = percentile (lv, 0.70), p80 = percentile (lv, 0.80), p95 = percentile (lv, 0.95);
    R.p80 = p80; R.spread = p95 - p30;
    const double voicedFrac = (double) vf.size() / (double) nf;

    // --- niveau d'entree : les trames fortes a -20 dBFS (RMS)
    const double inG = std::clamp (-20.0 - p80, -18.0, 18.0);
    const double g = inG;
    const double thr = std::clamp (p70 + g - 2.0, -36.0, -12.0);
    const double ratio = R.spread > 16 ? 5.0 : (R.spread > 12 ? 4.0 : (R.spread > 8 ? 3.2 : 2.5));
    auto grAt = [&] (double frameLevel) { const double x = frameLevel + g + 6.0; return x > thr ? (x - thr) * (1.0 - 1.0 / ratio) : 0.0; };
    double gr = 0; for (double l : lv) gr += grAt (l);
    gr /= std::max<size_t> (1, lv.size());
    const double makeup = std::clamp (0.75 * gr + 1.0, 0.0, 12.0);
    const double rel = voicedFrac > 0.8 ? 90.0 : 120.0;

    // --- spectre moyen voix (puissance moyenne des trames fortes) ; prod : meme chose sur les memes trames
    double S[NB], PS[NB] = {};
    for (int k = 0; k < NB; ++k)
    {
        double e = 0, ep = 0;
        for (int f : vf) { e += std::pow (10.0, an.band (f, k) / 10.0); if (hasProd) ep += std::pow (10.0, an.bandProd (f, k) / 10.0); }
        S[k] = 10.0 * std::log10 (e / (double) vf.size() + 1e-30);
        if (hasProd) PS[k] = 10.0 * std::log10 (ep / (double) vf.size() + 1e-30);
    }
    // courbe de reference d'une voix (relative a 1 kHz) ; centres 100 ... 12800 Hz
    static const double T[NB] = { 3, 5, 7, 8, 8, 7, 5, 4, 2, 1, 0, -1, -2, -3, -4, -5, -6.5, -8, -9.5, -11.5, -14, -17 };
    auto meanS = [&] (const double* X, double lo, double hi)
    {
        double a = 0; int n = 0;
        for (int k = 0; k < NB; ++k) if (an.bandCentre (k) >= lo && an.bandCentre (k) <= hi) { a += X[k]; ++n; }
        return n ? a / n : 0.0;
    };
    const double ref = meanS (S, 800, 1300);
    auto devOf = [&] (double lo, double hi, int* am = nullptr)
    {
        double a = 0, mx = -1e9; int n = 0, bk = -1;
        for (int k = 0; k < NB; ++k)
            if (an.bandCentre (k) >= lo && an.bandCentre (k) <= hi) { const double d = (S[k] - ref) - T[k]; a += d; ++n; if (d > mx) { mx = d; bk = k; } }
        if (am) *am = bk;
        return n ? a / n : 0.0;
    };
    int kMud = 4, kBox = 8, kHarsh = 15;
    const double devMud = devOf (200, 500, &kMud), devBox = devOf (500, 800, &kBox), devLow = devOf (100, 160), devPres = devOf (2400, 5100, &kHarsh), devAir = devOf (8000, 12800);
    R.devMud = devMud; R.devPres = devPres; R.devAir = devAir;

    // --- la prod : ou elle masque la voix (equilibre vise : voix 4 dB au-dessus de la prod dans le medium)
    double mudMask = 0, presMask = 0, airMask = 0, outG = 0;
    int kPres = 15;
    if (hasProd)
    {
        const double offs = meanS (S, 500, 4100) - meanS (PS, 500, 4100) - 4.0;
        R.prodOffs = offs;
        auto maskMean = [&] (double lo, double hi, int* am = nullptr)
        {
            double a = 0, mx = -1e9; int n = 0, bk = -1;
            for (int k = 0; k < NB; ++k)
                if (an.bandCentre (k) >= lo && an.bandCentre (k) <= hi) { const double m = (PS[k] + offs) - S[k]; a += m; ++n; if (m > mx) { mx = m; bk = k; } }
            if (am) *am = bk;
            return n ? a / n : 0.0;
        };
        mudMask = maskMean (200, 500); presMask = maskMean (2000, 5100, &kPres); airMask = maskMean (8000, 12800);
        // niveau voix/prod : voix traitee ~ +3,5 dB au-dessus de la prod (RMS), mesure sur les memes trames
        double lp = 0; for (int f : vf) lp += std::pow (10.0, an.levProd (f) / 10.0);
        const double Lprod = 10.0 * std::log10 (lp / (double) vf.size() + 1e-30);
        const double voiceOut = p80 + g + makeup - grAt (p80);
        outG = std::clamp ((Lprod + 3.5) - voiceOut, -9.0, 9.0);
        R.outG = outG;
    }

    // --- hauteur : f0 median, histogramme de classes de hauteur
    std::vector<double> mp;
    for (int f : vf) if (an.pitch (f) > 0.f) mp.push_back ((double) an.pitch (f));
    R.f0 = mp.empty() ? 0.0 : 440.0 * std::pow (2.0, (percentile (mp, 0.5) - 69.0) / 12.0);

    // --- EQ : le "muddy" est traite en priorite (bas-mediums + boite 500-800 + proximite), avec la prod en plus
    const double hp = (R.f0 > 0 && R.f0 > 220.0 ? 115.0 : (R.f0 > 0 && R.f0 < 150.0 ? 90.0 : 100.0)) + (devLow > 5.0 ? 20.0 : 0.0);   // low cut ~100 Hz comme ta chaine
    double mudCut = 0.0;
    if (devMud > 0.5) mudCut = 0.85 * devMud;
    if (devBox > 1.5) mudCut = std::max (mudCut, 0.7 * devBox);
    if (hasProd) mudCut += std::clamp (0.35 * (mudMask + 8.0), 0.0, 4.0);   // la prod occupe aussi cette zone (a moins de 8 dB sous la voix) : le flou s'additionne, on creuse davantage
    mudCut = std::clamp (std::max (mudCut, 2.5), 0.0, 7.0);               // toujours un creux dans les bas-mediums (ta chaine : -3,5 dB vers 400 Hz)
    R.mudCut = mudCut; R.mudMask = mudMask;
    int kLm = kMud; if (devBox > devMud + 2.0) kLm = kBox;
    const double lmF = std::clamp (an.bandCentre (kLm), 200.0, 650.0);
    const double lmG = -mudCut;
    const double lsG = devLow > 3.0 ? -std::clamp (0.5 * devLow, 0.0, 4.0) : (devLow < -3.0 ? std::clamp (-0.4 * devLow, 0.0, 3.0) : 0.0);

    double hmG, hmF = 3200.0;
    if (devPres < -2.0) hmG = std::clamp (-0.7 * devPres, 0.0, 5.0);
    else if (devPres > 4.0) { hmG = -std::clamp (0.5 * (devPres - 4.0), 0.0, 4.0); hmF = std::clamp (an.bandCentre (kHarsh), 2500.0, 5000.0); }
    else hmG = 2.5;
    if (hasProd)
    {
        R.presBoost = std::clamp (0.8 * (presMask + 2.0), 0.0, 5.0);
        if (R.presBoost > 0.5) { hmG += R.presBoost; hmF = std::clamp (an.bandCentre (kPres), 2000.0, 5000.0); }
    }
    hmG = std::clamp (hmG, -5.0, 6.0);
    double hsG = std::clamp (-0.6 * devAir + 1.5, -3.0, 6.0);
    if (hasProd && airMask > 2.0) hsG -= 1.0;
    const double airHigh = std::clamp (22.0 - 2.0 * devAir - (hasProd && airMask > 2.0 ? 6.0 : 0.0), 8.0, 60.0);

    // --- de-esser
    double sibSum = 0, centSum = 0; int ns = 0;
    for (int f : vf) if (an.sibRatio (f) > 0.30f)
    {
        sibSum += an.lev (f) + 10.0 * std::log10 (std::max (1e-6, (double) an.sibRatio (f)));
        centSum += an.sibCentroid (f); ++ns;
    }
    double deAmt = 40, deFreq = 6500;
    if (ns >= std::max (3, (int) (0.01 * (double) vf.size())))
    {
        R.sibRel = sibSum / ns - p80;
        deAmt = std::clamp (35.0 + (R.sibRel + 24.0) * 4.0, 30.0, 85.0);
        deFreq = std::clamp (0.95 * centSum / ns, 5000.0, 9000.0);
    }

    // --- chaleur, delay, reverb
    const double warm = (devLow < -3.0 || devMud < -3.0) ? 42.0 : 35.0;      // voix fine : un peu plus de chaleur
    const double dense = voicedFrac >= 0.75 ? 1.0 : 0.0;
    const double bonus = tuneOn ? 3.0 : 0.0;
    const double revMix = (dense > 0.5 ? 4.0 : 7.0) + (tuneOn ? 5.0 : 0.0), dlyMix = (dense > 0.5 ? 6.0 : 10.0) + bonus;   // reverb discrete (voix non chantee : presque absente)

    auto& st = R.set;
    st.push_back ({ "inGain", (float) inG });
    if (hasProd) st.push_back ({ "outGain", (float) outG });
    st.push_back ({ "eqOn", 1.f }); st.push_back ({ "hpOn", 1.f }); st.push_back ({ "hpFreq", (float) hp });
    st.push_back ({ "lmF", (float) lmF }); st.push_back ({ "lmG", (float) lmG }); st.push_back ({ "lmQ", 1.4f });
    st.push_back ({ "lsF", 150.f }); st.push_back ({ "lsG", (float) lsG });
    st.push_back ({ "hmF", (float) hmF }); st.push_back ({ "hmG", (float) hmG });
    st.push_back ({ "hsF", 10500.f }); st.push_back ({ "hsG", (float) hsG });
    st.push_back ({ "compOn", 1.f }); st.push_back ({ "compThr", (float) thr }); st.push_back ({ "compRatio", (float) ratio });
    st.push_back ({ "compAtk", 10.f }); st.push_back ({ "compRel", (float) rel }); st.push_back ({ "compMakeup", (float) makeup });
    st.push_back ({ "comp2On", 1.f }); st.push_back ({ "comp2Amt", 40.f }); st.push_back ({ "compWarm", (float) warm });
    st.push_back ({ "fxOn", 1.f });
    st.push_back ({ "deessOn", 1.f }); st.push_back ({ "deessAmt", (float) deAmt }); st.push_back ({ "deessFreq", (float) (deFreq * 0.78) }); st.push_back ({ "deessHigh", (float) std::min (deFreq * 1.55, 16000.0) }); st.push_back ({ "deessRange", 12.f });
    st.push_back ({ "airOn", 1.f }); st.push_back ({ "airMid", 26.f }); st.push_back ({ "airHigh", (float) airHigh });
    st.push_back ({ "satOn", 1.f }); st.push_back ({ "satDrive", 22.f }); st.push_back ({ "satTone", 8.f }); st.push_back ({ "satMix", 70.f });
    st.push_back ({ "widthOn", 1.f }); st.push_back ({ "width", 118.f });
    st.push_back ({ "dlyOn", 1.f }); st.push_back ({ "dlyMix", (float) dlyMix }); st.push_back ({ "dlyNote", 1.f }); st.push_back ({ "dlyFb", 28.f }); st.push_back ({ "dlyHP", 300.f });
    st.push_back ({ "dlyDuck", 75.f }); st.push_back ({ "dlyLP", 6000.f });
    st.push_back ({ "revOn", 1.f }); st.push_back ({ "revMix", (float) revMix }); st.push_back ({ "revMode", 2.f }); st.push_back ({ "revDecay", 0.7f }); st.push_back ({ "revPre", 20.f }); st.push_back ({ "revDamp", 65.f }); st.push_back ({ "revHigh", 8000.f }); st.push_back ({ "revLow", 220.f });
    st.push_back ({ "revDuck", hasProd ? 50.f : 30.f });

    R.ok = true;
    R.message = hasProd ? "Voix + prod OK" : "Mix OK";

    // --- tonalite (uniquement si l'autotune est active) : la prod (si ecoutee) et la melodie chantee votent ensemble
    if (tuneOn)
    {
        double hv[12] = {}, hpc[12] = {}, hmix[12];
        double sv = 0, sp = 0;
        if (mp.size() >= 120)
        {
            double cs = 0, sn = 0;
            for (double m : mp) { cs += std::cos (2.0 * 3.14159265358979 * (m - std::round (m))); sn += std::sin (2.0 * 3.14159265358979 * (m - std::round (m))); }
            const double conc = std::sqrt (cs * cs + sn * sn) / (double) mp.size();
            const double off = conc > 0.3 ? std::atan2 (sn, cs) / (2.0 * 3.14159265358979) : 0.0;
            for (double m : mp) { const int pc = ((((int) std::lround (m - off)) % 12) + 12) % 12; hv[pc] += 1.0; }
            for (double x : hv) sv += x;
        }
        if (hasProd) { for (int i = 0; i < 12; ++i) { hpc[i] = an.prodChroma()[(size_t) i]; sp += hpc[i]; } }
        if (sv <= 0 && sp <= 0) R.message += " - tonalite inchangee";
        else
        {
            for (int i = 0; i < 12; ++i) hmix[i] = (sp > 0 ? 0.6 * hpc[i] / sp : 0.0) + (sv > 0 ? (sp > 0 ? 0.4 : 1.0) * hv[i] / sv : 0.0);
            int key; bool minor; double corr, second;
            detectKey (hmix, key, minor, corr, second);
            R.keyCorr = corr;
            if (corr < 0.5) R.message += " - tonalite incertaine";
            else
            {
                R.key = key; R.scale = minor ? 2 : 1;
                st.push_back ({ "tuneKey", (float) key }); st.push_back ({ "tuneScale", (float) R.scale });
                R.message += juce::String (" - ") + noteName (key) + (minor ? " mineur" : " majeur");
            }
        }
    }
    return R;
}
} // namespace yv
