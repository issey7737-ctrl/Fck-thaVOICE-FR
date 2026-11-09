#include "Source/YvAuto.h"
#include <cstdio>
#include <random>
using namespace yv;
static int fails = 0;
#define CHECK(c, msg) do { if (c) std::printf("OK   %s\n", msg); else { std::printf("FAIL %s\n", msg); ++fails; } } while (0)

// voix synthetique : notes chantees (gamme donnee), voyelle a pente -10 dB/oct, sifflantes entre les phrases
static void makeVoice (std::vector<float>& L, double fs, double sec, const std::vector<int>& scaleSemis, int tonic, double mudDb, double lpHz, double sibGain, double detCents, unsigned seed)
{
    std::mt19937 r (seed); std::normal_distribution<double> g (0, 1);
    const int N = (int) (fs * sec); L.assign ((size_t) N, 0.f);
    Biquad mud, lp, hp, hp2; mud.peaking (fs, 300.0, 1.0, mudDb); if (lpHz < 20000) lp.lowPass (fs, lpHz, 0.7071); hp.highPass (fs, 5000, 0.7071); hp2.lowPass (fs, 9500, 0.7071);
    int pos = 0; double ph = 0;
    while (pos < N)
    {
        const int deg = scaleSemis[(size_t) (r() % scaleSemis.size())];
        const int midiN = 60 + ((tonic + deg) % 12) + (r() % 2 ? 0 : -12);
        const double f0 = 440.0 * std::pow (2.0, (midiN - 69 + detCents / 100.0) / 12.0);
        const int len = (int) (fs * (0.30 + 0.25 * (r() % 100) / 100.0));
        for (int i = 0; i < len && pos + i < N; ++i)
        {
            const double t = (double) i / fs, env = std::min (1.0, t * 30.0) * std::min (1.0, (len - i) / (fs * 0.03));
            ph += 2 * M_PI * f0 * (1.0 + 0.004 * std::sin (2 * M_PI * 5.5 * t)) / fs;
            double v = 0; for (int h = 1; h <= 40 && h * f0 < 0.45 * fs; ++h) v += std::sin (h * ph) / std::pow ((double) h, 1.6);
            L[(size_t) (pos + i)] = (float) (0.22 * env * v);
        }
        pos += len;
        if (r() % 3 == 0) // sifflante
        {
            const int sl = (int) (fs * 0.12);
            for (int i = 0; i < sl && pos + i < N; ++i) L[(size_t) (pos + i)] += (float) (sibGain * hp2.process (hp.process (g (r))) * std::min (1.0, (double) (sl - i) / (fs * 0.02)));
            pos += sl;
        }
    }
    for (auto& x : L) { x = (float) mud.process ((double) x); if (lpHz < 20000) x = (float) lp.process ((double) x); }
}
static AutoResult run (const std::vector<float>& L, double fs, bool tuneOn, const std::vector<float>* P = nullptr)
{
    VoiceAnalyzer an; an.prepare (fs); an.start();
    for (size_t i = 0; i < L.size() && an.listening(); i += 480)
    {
        const int n = (int) std::min<size_t> (480, L.size() - i);
        if (P) an.push (L.data() + i, L.data() + i, n, P->data() + i, P->data() + i);
        else   an.push (L.data() + i, L.data() + i, n);
    }
    return computeAutoSettings (an, tuneOn);
}
// prod : accords (pad) sur une tonique, basse, charleston ; tilt : gain a 300 Hz et a 3 kHz pour simuler une prod chargee
static void makeProd (std::vector<float>& P, double fs, double sec, int tonic, bool minor, double mudDb, double presDb, double level, unsigned seed)
{
    std::mt19937 r (seed); std::normal_distribution<double> g (0, 1);
    const int N = (int) (fs * sec); P.assign ((size_t) N, 0.f);
    const int chordsMin[4][3] = { { 0, 3, 7 }, { 8, 0, 3 }, { 10, 2, 5 }, { 7, 11, 2 } };   // i - VI - VII - V
    const int chordsMaj[4][3] = { { 0, 4, 7 }, { 9, 0, 4 }, { 5, 9, 0 }, { 7, 11, 2 } };   // I - vi - IV - V
    const auto& chords = minor ? chordsMin : chordsMaj;
    Biquad nz; nz.lowPass (fs, 7000, 0.7071);
    Biquad mud, pres; mud.peaking (fs, 300.0, 1.0, mudDb); pres.peaking (fs, 3000.0, 1.0, presDb);
    for (int i = 0; i < N; ++i)
    {
        const int ci = (int) (i / (fs * 2.0)) % 4; double v = 0;
        for (int k = 0; k < 3; ++k)
        {
            const double f = 440.0 * std::pow (2.0, (60 + tonic + chords[ci][k] - 69) / 12.0);
            for (int h = 1; h <= 6; ++h) v += std::sin (2 * M_PI * f * h * i / fs) / (h * h);
        }
        const double fb = 440.0 * std::pow (2.0, (36 + tonic + chords[ci][0] - 69) / 12.0);
        v += 1.5 * std::sin (2 * M_PI * fb * i / fs);
        const double hat = (std::fmod (i / fs, 0.25) < 0.02) ? g (r) * 0.4 : 0.0;
        P[(size_t) i] = (float) (level * 0.035 * (v + hat + 0.25 * nz.process (g (r))));
    }
    for (auto& x : P) x = (float) pres.process (mud.process ((double) x));
}
static float get (const AutoResult& r, const char* id, float d = -999) { for (auto& kv : r.set) if (std::string (kv.first) == id) return kv.second; return d; }

int main()
{
    const double fs = 48000;
    const std::vector<int> major { 0, 2, 4, 5, 7, 9, 11 }, minorPent { 0, 3, 5, 7, 10 };
    std::vector<float> v;

    makeVoice (v, fs, 40, major, 0, 0, 20000, 0.05, 0, 1);
    auto a = run (v, fs, true);
    std::printf("  voix saine (Do majeur): %s | key=%d scale=%d corr=%.2f | in=%.1f thr=%.1f ratio=%.1f mk=%.1f | mud=%.1f pres=%.1f air=%.1f | sibRel=%.1f deess=%.0f f0=%.0f\n", a.message.toRawUTF8(), a.key, a.scale, a.keyCorr,
                get (a, "inGain"), get (a, "compThr"), get (a, "compRatio"), get (a, "compMakeup"), a.devMud, a.devPres, a.devAir, a.sibRel, get (a, "deessAmt"), a.f0);
    CHECK (a.ok, "analyse terminee");
    CHECK ((a.key == 0 && a.scale == 1) || (a.key == 9 && a.scale == 2), "tonalite Do majeur / La mineur trouvee");
    CHECK (get (a, "inGain") > -18 && get (a, "inGain") < 18, "gain d'entree borne");

    auto aOff = run (v, fs, false);
    CHECK (aOff.key == -1 && get (aOff, "tuneKey") == -999, "autotune OFF : aucune tonalite n'est touchee");

    makeVoice (v, fs, 40, minorPent, 2, 0, 20000, 0.05, 25, 2);   // Re mineur pentatonique, voix desaccordee de +25 cents
    a = run (v, fs, true);
    std::printf("  Re penta mineur +25c : %s | key=%d scale=%d corr=%.2f\n", a.message.toRawUTF8(), a.key, a.scale, a.keyCorr);
    CHECK ((a.key == 2 && a.scale == 2) || (a.key == 5 && a.scale == 1), "tonalite Re mineur / Fa majeur (meme notes) malgre +25 cents");

    makeVoice (v, fs, 40, major, 7, 9, 20000, 0.05, 0, 3);          // voix "boueuse" : +9 dB a 300 Hz
    auto mu = run (v, fs, false);
    makeVoice (v, fs, 40, major, 7, 0, 20000, 0.05, 0, 3);
    auto cl = run (v, fs, false);
    std::printf("  boueuse : devMud=%.1f lmG=%.1f  | saine : devMud=%.1f lmG=%.1f\n", mu.devMud, get (mu, "lmG"), cl.devMud, get (cl, "lmG"));
    CHECK (get (mu, "lmG") < get (cl, "lmG") - 2.0, "voix boueuse : le bas-mediums est coupe davantage");

    makeVoice (v, fs, 40, major, 7, 0, 4500, 0.05, 0, 4);           // voix terne : coupee a 4,5 kHz
    auto du = run (v, fs, false);
    std::printf("  terne : devAir=%.1f hsG=%.1f airHigh=%.0f | saine : devAir=%.1f hsG=%.1f airHigh=%.0f\n", du.devAir, get (du, "hsG"), get (du, "airHigh"), cl.devAir, get (cl, "hsG"), get (cl, "airHigh"));
    CHECK (get (du, "hsG") > get (cl, "hsG") + 1.5, "voix terne : plus d'aigus ajoutes");

    makeVoice (v, fs, 40, major, 0, 0, 20000, 0.30, 0, 5);          // sifflantes fortes
    auto sx = run (v, fs, false);
    makeVoice (v, fs, 40, major, 0, 0, 20000, 0.005, 0, 5);         // presque pas de sifflantes
    auto sy = run (v, fs, false);
    std::printf("  sifflantes fortes : sibRel=%.1f deess=%.0f | faibles : sibRel=%.1f deess=%.0f\n", sx.sibRel, get (sx, "deessAmt"), sy.sibRel, get (sy, "deessAmt"));
    CHECK (get (sx, "deessAmt") > get (sy, "deessAmt") + 10.0, "sifflantes fortes : de-esser plus fort");

    // ---- avec la prod (sidechain)
    {
        std::vector<float> P, V;
        makeVoice (V, fs, 40, minorPent, 0, 0, 20000, 0.05, 0, 11);            // voix en Do mineur pentatonique
        makeProd (P, fs, 40, 0, true, 0, 0, 1.0, 12);                            // prod en Do mineur
        auto w = run (V, fs, true, &P);
        std::printf("  voix+prod Cm : %s | key=%d scale=%d corr=%.2f | outG=%.1f mudCut=%.1f presBoost=%.1f prodOffs=%.1f\n", w.message.toRawUTF8(), w.key, w.scale, w.keyCorr, w.outG, w.mudCut, w.presBoost, w.prodOffs);
        CHECK (w.prod, "prod detectee");
        CHECK ((w.key == 0 && w.scale == 2) || (w.key == 3 && w.scale == 1), "tonalite Do mineur / Mib majeur (prod + voix)");

        // la prod, a elle seule, donne la tonalite meme quand la voix parle (non chantee)
        std::vector<float> noise ((size_t) (fs * 40), 0.f); { std::mt19937 rr (5); std::normal_distribution<double> gg (0, 1); Biquad b; b.lowPass (fs, 2500, 0.7); for (auto& x : noise) x = (float) (0.05 * b.process (gg (rr))); }
        makeProd (P, fs, 40, 2, false, 0, 0, 1.0, 13);                           // prod en Re majeur
        auto w2 = run (noise, fs, true, &P);
        std::printf("  voix sans hauteur + prod Re majeur : %s | key=%d scale=%d corr=%.2f\n", w2.message.toRawUTF8(), w2.key, w2.scale, w2.keyCorr);
        CHECK ((w2.key == 2 && w2.scale == 1) || (w2.key == 11 && w2.scale == 2), "tonalite lue dans la prod seule (Re majeur / Si mineur)");

        makeProd (P, fs, 40, 0, true, 10, 0, 1.0, 14);                           // prod boueuse : +10 dB a 300 Hz
        auto pm = run (V, fs, false, &P);
        makeProd (P, fs, 40, 0, true, -8, 0, 1.0, 14);                           // prod degagee dans le bas-medium
        auto pc = run (V, fs, false, &P);
        std::printf("  prod chargee a 300 Hz : lmG=%.1f (coupe %.1f, masque %.1f) | prod degagee : lmG=%.1f (coupe %.1f, masque %.1f)\n", get (pm, "lmG"), pm.mudCut, pm.mudMask, get (pc, "lmG"), pc.mudCut, pc.mudMask);
        CHECK (pm.mudCut > pc.mudCut + 1.0, "prod chargee a 300 Hz : la voix est creusee davantage");

        makeProd (P, fs, 40, 0, true, 0, 12, 1.0, 15);                           // prod forte dans le 3 kHz : masque la voix
        auto pp = run (V, fs, false, &P);
        makeProd (P, fs, 40, 0, true, 0, -10, 1.0, 15);
        auto pq = run (V, fs, false, &P);
        std::printf("  prod forte a 3 kHz : presence +%.1f (hmF %.0f) | prod faible : +%.1f\n", pp.presBoost, get (pp, "hmF"), pq.presBoost);
        CHECK (pp.presBoost > pq.presBoost + 1.5, "prod forte a 3 kHz : plus de presence sur la voix");

        makeProd (P, fs, 40, 0, true, 0, 0, 1.0, 16); auto lo = run (V, fs, false, &P);
        makeProd (P, fs, 40, 0, true, 0, 0, 4.0, 16);  auto hi = run (V, fs, false, &P);   // prod +12 dB
        std::printf("  niveau prod : outGain %.1f -> %.1f\n", lo.outG, hi.outG);
        CHECK (hi.outG > lo.outG + 6.0, "prod plus forte : la voix remonte pour rester devant");
        CHECK (get (aOff, "outGain") == -999, "sans prod : le niveau de sortie n'est pas touche");
    }

    std::vector<float> sil ((size_t) (fs * 5), 0.f);
    auto e = run (sil, fs, true);
    CHECK (! e.ok, "silence : refuse de regler (pas assez de voix)");

    // gain d'entree : meme voix a -30 dB et a -6 dB
    makeVoice (v, fs, 40, major, 0, 0, 20000, 0.05, 0, 6);
    auto vq = v; for (auto& x : vq) x *= 0.03f;
    auto qq = run (vq, fs, false), lo = run (v, fs, false);
    CHECK (get (qq, "inGain") > get (lo, "inGain") + 20.0, "voix faible : gain d'entree plus eleve");

    std::printf (fails ? "\n%d ECHEC(S)\n" : "\nTOUS LES TESTS OK\n", fails);
    return fails ? 1 : 0;
}
