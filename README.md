# Fck ThaVOICE (VST3)

Suite vocale tout-en-un : AUTOTUNE, COMP (2 étages + parallèle), EQ (analyseur + courbe), FX (de-esser, saturation, doubleur, chorus, téléphone, largeur), DLY (sync tempo, ping-pong, ducking), REV (Room / Plate / Hall / Spring, ducking), 22 presets, A/B, Reset, Bypass.
Deux vues : SIMPLE (un grand bouton COMP + quelques réglages par rangée, Auto input) et PRO (tout le détail).

## Compiler (sans rien installer)
1. Mets le contenu de ce dossier dans ton dépôt GitHub (dossier `YazaVox`, `CMakeLists.txt`, `.github/workflows/build.yml`).
2. Onglet Actions : le build démarre, attends le vert (5 à 10 min).
3. Télécharge l'artefact `ThaVOICE-VST3-Windows`, copie `Fck ThaVOICE.vst3` dans `C:\Program Files\Common Files\VST3`.
4. FL Studio : Options > Manage plugins > Find plugins.

## Autotune
Réglages inspirés de ce que décrivent les manuels publics d'Auto-Tune (Retune Speed, Humanize, Flex-Tune, Natural Vibrato, Formant) ; l'algorithme est écrit de zéro.
- Active TUNE, choisis la tonalité (KEY) et la gamme (SCALE).
- **RETUNE** (ms) : 0 = saut instantané, effet robot « autotune » ; 10 à 50 = correction naturelle.
- **HUMANIZE** : ralentit la correction sur la partie tenue des notes longues.
- **FLEX** : ne tire la note que quand elle approche de la cible (garde les glissandos).
- **VIBRATO** : réinjecte le vibrato naturel autour de la note corrigée (0 = aucun).
- **FORMANT** : garde le timbre de la voix (évite l'effet « chipmunk »).
- **AMOUNT** : force globale de la correction.
- Le décalage de hauteur utilise **Signalsmith Stretch** (MIT, `YazaVox/third_party`), la détection de note est un algorithme YIN maison.
- Latence fixe de 90 ms, déclarée à FL Studio. Pour enregistrer en direct, mets TUNE sur la piste après l'enregistrement.

## Test du moteur (optionnel)
`YazaVox/tests_yv.cpp` : tests hors-ligne (44,1 / 48 / 88,2 / 96 kHz, blocs 128 à 2048).


## Fck ThaMASTER (VST3, mastering)
Chaine : INPUT > SPECTRAL (equilibre tonal adaptatif a phase lineaire) > COMP (glue 3 bandes) > ANALOG (Tape/Tube/Xfmr/Exciter) > STEREO (largeur + low mono) > EQ (Bass/Air) > CLIP (ecreteur doux) > LIMITER (anticipation + true peak) > OUTPUT.
Mesures BS.1770 : LUFS court terme / integre, true peak, DYNAMICS, image stereo. AUTO regle CLIP + LIMITER pour viser la cible choisie, AUTO IN cale le niveau d'entree, MATCH compare a niveau egal, MONITOR (Input / Delta).
Code : `ThaMaster/Source/` (tests hors-ligne : `ThaMaster/tests_tm.cpp`).

## Mise a jour : WARM + AUTO VOIX (ThaVOICE)
- **WARM** (panneau COMP) : saturation chaude apres le compresseur, basee sur le coeur de Tube2 (Airwindows, MIT) + harmoniques paires. 30 % par defaut. Voir `THIRD_PARTY_LICENSES.txt`.
- **AUTO VOIX** : ecoute ~10 s de voix (et de prod si l'entree sidechain "Prod" est branchee), puis regle niveau, comp, EQ (anti-muddy), de-esser, air, delay/reverb et, si l'autotune est ON, la tonalite.
