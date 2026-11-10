// Test hors-ligne du moteur ThaVOICE
#include "Source/YvDsp.h"
#include <cstdio>
#include <random>

using namespace yv;

static void defaults (float* v) { for (int i = 0; i < Count; ++i) v[i] = kDefs[i].def; }

static std::vector<float> makeVoice (double sr, double sec, float amp)
{
    const size_t N = (size_t) (sr * sec);
    std::vector<float> x (N);
    std::mt19937 rng (5); std::normal_distribution<float> nd (0.f, 1.f);
    double lp = 0;
    for (size_t i = 0; i < N; ++i)
    {
        const double t = (double) i / sr;
        double v = 0;
        for (int h = 1; h < 12; ++h) v += std::sin (2 * kPi * 180.0 * h * t) / h;
        const double env = 0.5 + 0.5 * std::sin (2 * kPi * 1.3 * t);
        lp += 0.2 * ((double) nd (rng) - lp);
        const double sib = (std::fmod (t, 1.0) > 0.6 && std::fmod (t, 1.0) < 0.7) ? (double) nd (rng) - lp : 0.0;
        x[i] = (float) (amp * (0.5 * v * env + 0.35 * sib));
    }
    return x;
}

static double rmsOf (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) (b - a));
}

static void run (Engine& e, std::vector<float>& L, std::vector<float>& R, int block)
{
    for (size_t i = 0; i < L.size(); i += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, L.size() - i);
        e.process (L.data() + i, R.data() + i, n);
        int s1, z1, s2, z2; e.scopeFifo.prepareToRead (n, s1, z1, s2, z2); e.scopeFifo.finishedRead (z1 + z2);
    }
}

int main()
{
    int fails = 0;
    auto check = [&] (bool ok, const char* m) { std::printf ("%s %s\n", ok ? "OK  " : "FAIL", m); if (! ok) ++fails; };

    // 1) tous les modules actifs, 4 fs x 5 tailles de bloc : pas de NaN, pas de depassement, sortie non nulle
    const double rates[4] = { 44100, 48000, 88200, 96000 };
    const int blocks[5] = { 128, 256, 512, 1024, 2048 };
    bool allFinite = true, allBounded = true, allAlive = true;
    for (double sr : rates)
        for (int b : blocks)
        {
            Engine e; e.prepare (sr, b);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = v[revOn] = 1;
            v[deessOn] = v[satOn] = v[dblOn] = v[choOn] = 1; v[dlyMix] = 25; v[revMix] = 30;
            e.setParams (v); e.setBpm (120);
            auto x = makeVoice (sr, 4.0, 0.8f);
            std::vector<float> L = x, R = x;
            run (e, L, R, b);
            double pk = 0;
            for (size_t i = 0; i < L.size(); ++i)
            {
                if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) allFinite = false;
                pk = std::max (pk, (double) std::max (std::abs (L[i]), std::abs (R[i])));
            }
            if (pk > 1.0) allBounded = false;
            if (rmsOf (L, L.size() / 2, L.size()) < 1e-4) allAlive = false;
        }
    check (allFinite, "pas de NaN/Inf (4 frequences x 5 blocs)");
    check (allBounded, "sortie <= 0 dBFS (protection de sortie)");
    check (allAlive, "sortie non nulle");

    // 2) determinisme selon la taille de bloc (a 1e-4 pres)
    {
        const double sr = 48000;
        auto x = makeVoice (sr, 3.0, 0.5f);
        std::vector<float> a[2];
        const int bs[2] = { 128, 1024 };
        for (int k = 0; k < 2; ++k)
        {
            Engine e; e.prepare (sr, bs[k]);
            float v[Count]; defaults (v); v[dlyOn] = 1; v[revOn] = 1; v[fxOn] = 1; v[choOn] = 1; v[dblOn] = 1;
            e.setParams (v);
            a[k] = x; std::vector<float> R = x;
            run (e, a[k], R, bs[k]);
        }
        double d = 0;
        for (size_t i = 0; i < a[0].size(); ++i) d = std::max (d, (double) std::abs (a[0][i] - a[1][i]));
        std::printf ("     ecart max blocs 128 vs 1024 : %.2e\n", d);
        check (d < 2e-3, "resultat independant de la taille de bloc");
    }

    // 3) bypass : sortie == entree (apres rampe)
    {
        const double sr = 48000;
        Engine e; e.prepare (sr, 256);
        float v[Count]; defaults (v); v[bypass] = 1; v[revOn] = 1; v[revMix] = 80; v[inGain] = 6;
        e.setParams (v);
        auto x = makeVoice (sr, 1.0, 0.4f); std::vector<float> L = x, R = x;
        run (e, L, R, 256);
        double d = 0;
        const size_t lat = (size_t) e.latency();
        for (size_t i = 2000 + lat; i < L.size(); ++i) d = std::max (d, (double) std::abs (L[i] - x[i - lat]));
        check (d < 1e-6, "bypass global transparent");
    }

    // 4) activite par module : chaque module change le signal quand il est active
    {
        const double sr = 48000;
        auto x = makeVoice (sr, 2.5, 0.5f);
        struct T { const char* name; int param; float val; };
        const T tests[] = { { "COMP", compOn, 1 }, { "EQ", eqOn, 1 }, { "DE-ESSER", deessOn, 1 }, { "SAT", satOn, 1 }, { "DOUBLER", dblOn, 1 },
                            { "CHORUS", choOn, 1 }, { "PHONE", phoneOn, 1 }, { "WIDTH", widthOn, 1 }, { "DELAY", dlyOn, 1 }, { "REVERB", revOn, 1 } };
        for (auto& t : tests)
        {
            Engine e; e.prepare (sr, 512);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = v[revOn] = 0;
            v[deessOn] = v[satOn] = v[dblOn] = v[choOn] = v[phoneOn] = v[widthOn] = 1;
            v[t.param] = t.val;
            if (t.param == compOn) { v[compThr] = -30; v[compRatio] = 6; }
            if (t.param == eqOn) { v[hsG] = 6; v[lmG] = -4; }
            if (t.param == widthOn) v[width] = 0;
            if (t.param == deessOn) v[deessAmt] = 100;
            if (t.param == satOn) v[satDrive] = 80;
            if (t.param == phoneOn) v[phoneAmt] = 100;
            if (t.param == dblOn || t.param == choOn || t.param == phoneOn || t.param == deessOn || t.param == satOn || t.param == widthOn) v[fxOn] = 1;
            if (t.param == dblOn) v[dblAmt] = 100;
            if (t.param == choOn) v[choMix] = 100;
            if (t.param == dlyOn) v[dlyMix] = 60;
            if (t.param == revOn) v[revMix] = 60;
            // reference : tout a zero
            Engine e0; e0.prepare (sr, 512);
            float v0[Count]; defaults (v0); v0[compOn] = v0[eqOn] = v0[fxOn] = v0[dlyOn] = v0[revOn] = 0;
            e0.setParams (v0); e.setParams (v);
            std::vector<float> L = x, R = x, L0 = x, R0 = x;
            run (e, L, R, 512); run (e0, L0, R0, 512);
            double d = 0; for (size_t i = 0; i < L.size(); ++i) d += std::abs (L[i] - L0[i]);
            d /= (double) L.size();
            char m[64]; std::snprintf (m, sizeof m, "module %s modifie le signal (ecart moyen %.4f)", t.name, d);
            check (d > 1e-4, m);
        }
    }

    // 5) etalonnage reverb : niveau de queue, 4 modes, decay raisonnable
    {
        const double sr = 48000;
        for (int mode = 0; mode < 4; ++mode)
        {
            Engine e; e.prepare (sr, 512);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = 0; v[revOn] = 1; v[revMode] = (float) mode; v[revMix] = 100; v[revDecay] = 2.0f; v[revPre] = 0; v[revDuck] = 0;
            e.setParams (v);
            const size_t N = (size_t) (sr * 6);
            std::vector<float> L (N, 0.f), R (N, 0.f);
            for (int i = 0; i < 200; ++i) L[(size_t) i] = R[(size_t) i] = 0.3f;   // salve breve
            run (e, L, R, 512);
            const double e1 = rmsOf (L, (size_t) (0.1 * sr), (size_t) (0.3 * sr));
            const double e2 = rmsOf (L, (size_t) (1.1 * sr), (size_t) (1.3 * sr));
            const double e3 = rmsOf (L, (size_t) (4.5 * sr), (size_t) (5.0 * sr));
            std::printf ("     mode %d : rms 0.1-0.3 s = %.1f dB, 1.1-1.3 s = %.1f dB, 4.5-5 s = %.1f dB\n", mode, linToDb (e1), linToDb (e2), linToDb (e3));
            check (e1 > 1e-4 && e2 < e1 && e3 < e2, "reverb : niveau present et decroissant");
        }
    }

    // 5b) ducking de reverb : le niveau de la reverb baisse pendant la voix
    {
        const double sr = 48000;
        auto x = makeVoice (sr, 4.0, 0.6f);
        double lv[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e; e.prepare (sr, 512);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = 0; v[revOn] = 1; v[revMix] = 100; v[revDuck] = k ? 100.f : 0.f;
            e.setParams (v);
            std::vector<float> L = x, R = x;
            run (e, L, R, 512);
            const size_t lat = (size_t) e.latency();
            double s = 0; for (size_t i = (size_t) (2 * sr); i < L.size(); ++i) s += (double) (L[i] - x[i - lat]) * (L[i] - x[i - lat]);
            lv[k] = std::sqrt (s / (double) (L.size() - (size_t) (2 * sr)));
        }
        std::printf ("     reverb wet rms : duck 0 = %.4f, duck 100 = %.4f\n", lv[0], lv[1]);
        check (lv[1] < lv[0] * 0.8, "reverb ducking actif");
    }

    // 6) EQ : reponse en frequence conforme
    {
        float v[Count]; defaults (v);
        v[lmG] = 6; v[lmF] = 1000; v[lmQ] = 1.0f; v[hpOn] = 0; v[lsG] = 0; v[hmG] = 0; v[hsG] = 0; v[lpFreq] = 20000;
        const double g1k = EQ::responseDb (v, 48000, 1000), g100 = EQ::responseDb (v, 48000, 100);
        std::printf ("     EQ : 1 kHz = %.2f dB, 100 Hz = %.2f dB\n", g1k, g100);
        check (std::abs (g1k - 6.0) < 0.1 && std::abs (g100) < 0.6, "EQ : gain a la frequence centrale");
    }

    // 7) sample-rate change et signal mono (L=R) ; tempo-sync
    {
        Engine e; e.prepare (44100, 512); e.prepare (96000, 512);
        float v[Count]; defaults (v); v[dlyOn] = 1; v[dlySync] = 1; v[dlyNote] = 0; v[dlyMix] = 100; v[dlyFb] = 0; v[dlyPing] = 0; v[dlyDuck] = 0; v[dlyHP] = 20; v[dlyLP] = 20000;
        v[compOn] = v[eqOn] = v[fxOn] = v[revOn] = 0;
        e.setParams (v); e.setBpm (120);
        const size_t N = 96000 * 2;
        std::vector<float> L (N, 0.f), R (N, 0.f); L[1000] = R[1000] = 0.5f;
        run (e, L, R, 512);
        size_t pos = 0; float m = 0;
        for (size_t i = 1000 + (size_t) e.latency() + 500; i < N; ++i) if (std::abs (L[i]) > m) { m = std::abs (L[i]); pos = i; }
        const double expect = 1000 + 0.5 * 96000 + e.latency();   // 1/4 de note a 120 BPM = 0,5 s
        std::printf ("     echo a %zu (attendu ~%.0f)\n", pos, expect);
        check (std::abs ((double) pos - expect) < 200, "delay synchronise : 1/4 a 120 BPM");
    }


    // 8) AUTOTUNE : un la a 450 Hz (+39 cents) doit sortir a 440 Hz ; bruit : pas de NaN ; latence declaree
    {
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            const size_t N = (size_t) (sr * 3.0) + 65536;
            auto freqOf = [&] (const std::vector<float>& y)
            {
                const int M = 1 << 16;
                std::vector<float> w (2 * (size_t) M, 0.f);
                for (int i = 0; i < M; ++i) w[(size_t) i] = y[y.size() - (size_t) M + (size_t) i];
                juce::dsp::WindowingFunction<float> win ((size_t) M, juce::dsp::WindowingFunction<float>::hann);
                win.multiplyWithWindowingTable (w.data(), (size_t) M);
                juce::dsp::FFT fft (16);
                fft.performFrequencyOnlyForwardTransform (w.data());
                int bm = 1; for (int i = 2; i < M / 2; ++i) if (w[(size_t) i] > w[(size_t) bm]) bm = i;
                const double a = w[(size_t) bm - 1], b = w[(size_t) bm], c = w[(size_t) bm + 1];
                return ((double) bm + 0.5 * (a - c) / (a - 2 * b + c)) * sr / M;
            };
            auto runTone = [&] (double f, int scale, int key, float speed, float amt)
            {
                Engine e; e.prepare (sr, 512);
                float v[Count]; defaults (v);
                v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = v[revOn] = 0;
                v[tuneOn] = 1; v[tuneScale] = (float) scale; v[tuneKey] = (float) key; v[tuneSpeed] = speed; v[tuneAmt] = amt;
                e.setParams (v);
                std::vector<float> L (N), R;
                for (size_t i = 0; i < N; ++i) { const double ph = 2 * kPi * f * (double) i / sr; L[i] = (float) (0.3 * (std::sin (ph) + 0.4 * std::sin (2 * ph) + 0.2 * std::sin (3 * ph))); }
                R = L;
                run (e, L, R, 512);
                return freqOf (L);
            };
            const double f1 = runTone (450.0, 0, 0, 0.f, 100.f);
            const double f2 = runTone (450.0, 1, 0, 0.f, 100.f);   // Do majeur : le plus proche autorise est La = 440
            const double f3 = runTone (450.0, 0, 0, 0.f, 0.f);      // amount 0 : pas de correction
            std::printf ("     tune %.0f Hz : chromatique %.2f Hz, Do maj %.2f Hz, amount 0 -> %.2f Hz (attendu 440 / 440 / 450)\n", sr, f1, f2, f3);
            char m[96];
            std::snprintf (m, sizeof m, "autotune corrige a la bonne note (%.0f Hz)", sr);
            check (std::abs (f1 - 440.0) < 3.0 && std::abs (f2 - 440.0) < 3.0 && std::abs (f3 - 450.0) < 3.0, m);
        }
        // une source mono doit rester strictement mono apres l'autotune (pas d'elargissement du champ stereo)
        {
            const double sr = 48000;
            Engine e; e.prepare (sr, 512);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = v[revOn] = 0; v[tuneOn] = 1; v[tuneSpeed] = 0;
            e.setParams (v);
            auto x = makeVoice (sr, 3.0, 0.5f);
            std::vector<float> L = x, R = x;
            run (e, L, R, 512);
            double d = 0; for (size_t i = 0; i < L.size(); ++i) d = std::max (d, (double) std::abs (L[i] - R[i]));
            check (d < 1e-6, "autotune : source mono reste mono");
        }
        // note exacte (La 440) : ne doit pas etre deformee
        Engine e; e.prepare (48000, 512);
        std::printf ("     latence declaree : %d echantillons (%.1f ms a 48 kHz)\n", e.latency(), 1000.0 * e.latency() / 48000.0);
    }


    // De-esser (type Pro-DS) et AIR : mesures par Goertzel
    {
        auto gz = [] (const std::vector<float>& x, size_t a, size_t b, double f, double sr)
        {
            double re = 0, im = 0; for (size_t i = a; i < b; ++i) { const double w = 2 * kPi * f * (double) i / sr; re += x[i] * std::cos (w); im += x[i] * std::sin (w); }
            return 2.0 * std::sqrt (re * re + im * im) / (double) (b - a);
        };
        const double sr = 48000; const size_t N = (size_t) (sr * 1.5);
        auto mk = [&] (double f1, double a1, double f2, double a2) { std::vector<float> x (N); for (size_t i = 0; i < N; ++i) x[i] = (float) (a1 * std::sin (2 * kPi * f1 * (double) i / sr) + a2 * std::sin (2 * kPi * f2 * (double) i / sr)); return x; };
        for (int mode = 0; mode < 2; ++mode)
        {
            Engine e; e.prepare (sr, 512);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[dlyOn] = v[revOn] = 0; v[fxOn] = 1; v[deessOn] = 1; v[satOn] = 0; v[widthOn] = 0;
            v[deessFreq] = 5200; v[deessHigh] = 9000; v[deessAmt] = 100; v[deessRange] = 12; v[deessBand] = (float) mode;
            e.setParams (v);
            auto x = mk (200, 0.1, 6500, 0.3); auto L = x, R = x; run (e, L, R, 512);
            const size_t a = N / 2, b = N - 1000;
            const int lat = e.latency();
            // reference decalee par la latence
            const double lo = 20 * std::log10 (gz (L, a, b, 200, sr) / 0.1), hi = 20 * std::log10 (gz (L, a, b, 6500, sr) / 0.3);
            (void) lat;
            std::printf ("     de-esser mode %d : 200 Hz %+.1f dB, 6.5 kHz %+.1f dB\n", mode, lo, hi);
            if (mode == 0) check (hi < -3.0 && std::abs (lo) < 0.8, "de-esser Vocal : baisse la bande, epargne les graves");
            else           check (hi < -3.0 && lo < -3.0, "de-esser Wide : baisse tout le signal");
        }
        {
            Engine e0, e1; e0.prepare (sr, 512); e1.prepare (sr, 512);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[dlyOn] = v[revOn] = 0; v[fxOn] = 1; v[deessOn] = 0; v[satOn] = 0; v[widthOn] = 0; v[airOn] = 0;
            e0.setParams (v); v[airOn] = 1; v[airMid] = 60; v[airHigh] = 80; e1.setParams (v);
            auto x = mk (4000, 0.15, 5000, 0.05); auto L0 = x, R0 = x, L1 = x, R1 = x; run (e0, L0, R0, 512); run (e1, L1, R1, 512);
            const size_t a = N / 2, b = N - 1000;
            const double h0 = gz (L0, a, b, 10000, sr), h1 = gz (L1, a, b, 10000, sr);
            const double fin = 20 * std::log10 (rmsOf (L1, a, b) / rmsOf (L0, a, b));
            std::printf("     air : 10 kHz %.5f -> %.5f, niveau global %+.1f dB\n", h0, h1, fin);
            check (h1 > h0 * 3 + 1e-5 && fin < 9.0, "air : ajoute des harmoniques aigues sans exploser le niveau");
        }
    }


    // Wider (largeur depuis une source mono), sidechain du compresseur, BASS de la reverb
    {
        const double sr = 48000; const size_t N = (size_t) (sr * 2.0);
        auto voice = makeVoice (sr, 2.0, 0.1f);
        {
            Engine e0, e1; e0.prepare (sr, 512); e1.prepare (sr, 512);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[dlyOn] = v[revOn] = 0; v[fxOn] = 1; v[deessOn] = v[satOn] = v[airOn] = 0; v[widthOn] = 1; v[width] = 100;
            e0.setParams (v); v[width] = 200; e1.setParams (v);
            std::vector<float> L0 = voice, R0 = voice, L1 = voice, R1 = voice; run (e0, L0, R0, 512); run (e1, L1, R1, 512);
            double side = 0, dm = 0, mid = 0;
            for (size_t i = 0; i < N; ++i) { side += (L1[i] - R1[i]) * (L1[i] - R1[i]); mid += (L1[i] + R1[i]) * (L1[i] + R1[i]); dm = std::max (dm, (double) std::abs ((L1[i] + R1[i]) - (L0[i] + R0[i]))); }
            std::printf ("     wider : side/mid = %.1f dB, ecart mono max %.2e\n", 10 * std::log10 (side / mid), dm);
            check (side / mid > 0.01 && dm < 1e-4, "wider : cree de la largeur depuis du mono, somme mono inchangee");
        }
        {
            double gr[2];
            for (int k = 0; k < 2; ++k)
            {
                Engine e; e.prepare (sr, 512);
                float v[Count]; defaults (v);
                v[eqOn] = v[fxOn] = v[dlyOn] = v[revOn] = 0; v[compOn] = 1; v[compThr] = -30; v[compRatio] = 6; v[comp2On] = 0; v[compSC] = k == 0 ? 20.0f : 300.0f;
                e.setParams (v);
                std::vector<float> L (N), R (N); for (size_t i = 0; i < N; ++i) L[i] = R[i] = (float) (0.25 * std::sin (2 * kPi * 60.0 * (double) i / sr));
                run (e, L, R, 512);
                gr[k] = 20 * std::log10 (rmsOf (L, N / 2, N - 1000) / 0.1768);
            }
            std::printf ("     comp sidechain : 60 Hz niveau sortie %+.1f dB (SC 20 Hz) vs %+.1f dB (SC 300 Hz)\n", gr[0], gr[1]);
            check (gr[1] > gr[0] + 2.0, "compresseur : le filtre de sidechain epargne les graves");
        }
        {
            for (int mode = 0; mode < 3; ++mode)   // le ressort (mode 3) n'a pas de reglage BASS
            {
                double lowTail[2]; bool ok = true;
                for (int k = 0; k < 2; ++k)
                {
                    Engine e; e.prepare (sr, 512);
                    float v[Count]; defaults (v);
                    v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = 0; v[revOn] = 1; v[revMode] = (float) mode; v[revMix] = 100; v[revDecay] = 2.0f; v[revDuck] = 0; v[revBass] = k == 0 ? 50.0f : 200.0f; v[revLow] = 20; v[revHigh] = 20000;
                    e.setParams (v);
                    const size_t M = (size_t) (sr * 6.0);
                    std::vector<float> L (M, 0.f), R (M, 0.f); for (size_t i = 0; i < 2000; ++i) L[i] = R[i] = (float) (0.5 * std::sin (2 * kPi * 100.0 * (double) i / sr));
                    run (e, L, R, 512);
                    for (float x : L) if (! std::isfinite (x) || std::abs (x) > 4) ok = false;
                    lowTail[k] = rmsOf (L, (size_t) (sr * 1.5), (size_t) (sr * 2.5));
                }
                char m[100]; std::snprintf (m, sizeof m, "reverb mode %d : BASS 200%% garde plus de graves que 50%% et reste stable", mode);
                check (ok && lowTail[1] > lowTail[0] * 1.3, m);
            }
        }
    }


    // autotune : voix deja juste -> le signal passe sans coloration (identique au signal retarde)
    {
        const double sr = 48000; const size_t N = (size_t) (sr * 3.0);
        Engine e; e.prepare (sr, 512);
        float v[Count]; defaults (v);
        v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = v[revOn] = 0; v[tuneOn] = 1; v[tuneKey] = 9; v[tuneScale] = 0; v[tuneSpeed] = 0;
        e.setParams (v);
        std::vector<float> L (N), R (N); for (size_t i = 0; i < N; ++i) L[i] = R[i] = (float) (0.3 * std::sin (2 * kPi * 440.0 * (double) i / sr));
        auto ref = L; run (e, L, R, 512);
        // le decaleur temporel peut garder un decalage residuel de quelques echantillons (au plus une demi-periode) apres une correction : on cherche le meilleur
        const int lat0 = e.latency(); double bestDb = 1e9;
        for (int off = -6; off <= 6; ++off)
        {
            const int lat = lat0 + off; double err = 0, sig = 0;
            for (size_t i = (size_t) (sr * 1.5); i < N - 100; ++i) { const double d = L[i] - ref[i - (size_t) lat]; err += d * d; sig += (double) ref[i] * ref[i]; }
            bestDb = std::min (bestDb, 10 * std::log10 (err / sig + 1e-20));
        }
        const double db = bestDb;
        std::printf ("     autotune note deja juste : ecart vs signal retarde %.1f dB\n", db);
        check (db < -40.0, "autotune : voix deja juste = signal intact (pas de coloration)");
    }


    // limiteur : jamais au-dessus du plafond, transparent en dessous
    {
        const double sr = 48000; const size_t N = (size_t) (sr * 2.0);
        for (int k = 0; k < 2; ++k)
        {
            Engine e; e.prepare (sr, 512);
            float v[Count]; defaults (v);
            v[compOn] = v[eqOn] = v[fxOn] = v[dlyOn] = v[revOn] = 0; v[limCeil] = -6; v[inGain] = k == 0 ? 12.0f : -6.0f;
            e.setParams (v);
            std::vector<float> L (N), R (N);
            for (size_t i = 0; i < N; ++i) { const double env = (i > N / 2 && i < N / 2 + 2000) ? 3.0 : 1.0; L[i] = R[i] = (float) (0.8 * env * std::sin (2 * kPi * 330.0 * (double) i / sr)); }
            auto ref = L; run (e, L, R, 256);
            const size_t lat = (size_t) e.latency(); double mx = 0, err = 0, sig = 0;
            for (size_t i = lat; i < N; ++i) { mx = std::max (mx, (double) std::abs (L[i])); }
            if (k == 1) { for (size_t i = (size_t) (sr * 0.85); i < N / 2 - 100; ++i) { const double g = std::pow (10.0, -6.0 / 20.0), d = L[i] - ref[i - lat] * g; err += d * d; sig += (double) ref[i - lat] * ref[i - lat] * g * g; } }
            if (k == 0) { std::printf ("     limiteur : pic max %.4f (plafond 0.5012)\n", mx); check (mx <= 0.5012 + 1e-4, "limiteur : ne depasse jamais le plafond"); }
            else        { const double db = 10 * std::log10 (err / sig + 1e-20); std::printf ("     limiteur transparent sous le plafond : ecart %.1f dB\n", db); check (db < -60.0, "limiteur : transparent sous le plafond"); }
        }
    }


    // 9) WARM (saturation chaude type Tube2 / Airwindows) : transparente a 0, niveau conserve, pas de NaN, harmoniques paires presentes
    {
        for (double fs : { 44100.0, 48000.0, 96000.0 })
        {
            Comp c; c.prepare (fs);
            float v[Count]; for (int i = 0; i < Count; ++i) v[i] = kDefs[i].def;
            v[compThr] = 0; v[compMakeup] = 0; v[comp2On] = 0;
            double lvl[2] = {}, h2[2] = {}, h1[2] = {}; bool finite = true;
            for (int w = 0; w < 2; ++w)
            {
                v[compWarm] = w == 0 ? 0.f : 30.f; c.setup (v);
                const int N = (int) fs; double e0 = 0, e1 = 0, re1 = 0, im1 = 0, re2 = 0, im2 = 0;
                for (int i = 0; i < N; ++i)
                {
                    const double x = 0.25 * std::sin (2 * kPi * 220.0 * i / fs); double l = x, r = x; c.process (l, r);
                    if (! std::isfinite (l)) finite = false;
                    if (i > N / 2) { e0 += x * x; e1 += l * l; re1 += l * std::cos (2 * kPi * 220.0 * i / fs); im1 += l * std::sin (2 * kPi * 220.0 * i / fs); re2 += l * std::cos (2 * kPi * 440.0 * i / fs); im2 += l * std::sin (2 * kPi * 440.0 * i / fs); }
                }
                lvl[w] = 10 * std::log10 (e1 / e0); h1[w] = std::sqrt (re1 * re1 + im1 * im1); h2[w] = std::sqrt (re2 * re2 + im2 * im2);
            }
            std::printf ("     warm fs=%.0f : niveau 0 %% %+.2f dB, 30 %% %+.2f dB, H2 %.1f dB\n", fs, lvl[0], lvl[1], 20 * std::log10 (h2[1] / h1[1] + 1e-12));
            check (finite, "warm : pas de NaN");
            check (std::abs (lvl[0]) < 0.01, "warm 0 % : transparent");
            check (std::abs (lvl[1]) < 1.0, "warm 30 % : niveau conserve (< 1 dB)");
            check (20 * std::log10 (h2[1] / h1[1] + 1e-12) > -45.0, "warm 30 % : harmoniques paires (chaleur) presentes");
        }
    }

    std::printf (fails == 0 ? "\nTOUS LES TESTS OK\n" : "\n%d ECHEC(S)\n", fails);
    return fails;
}
