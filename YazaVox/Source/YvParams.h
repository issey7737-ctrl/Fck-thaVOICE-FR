#pragma once

// ThaVOICE : table unique des parametres (moteur, processeur, interface et presets s'appuient dessus).

namespace yv
{
enum Module { MGlobal = 0, MComp, MEq, MFx, MDly, MRev, MTune, kNumModules };
enum Kind { KFloat = 0, KBool, KChoice };

struct Def
{
    const char* id;
    const char* label;
    float lo, hi, def, step, skew;
    Kind kind;
    const char* unit;   // suffixe d'affichage ; pour KChoice : liste "a|b|c"
    int module;
    const char* group;  // sous-groupe dans le module ("" = niveau module)
};

//        enum        id            label      lo   hi   def  step  skew kind    unit        module  group
#define YV_LIST(X) \
    X(bypass,     "bypass",     "BYPASS",   0,   1,   0,    1,    1,   KBool,   "",         MGlobal, "") \
    X(inGain,     "inGain",     "INPUT",   -18,  18,  0,    0.1,  1,   KFloat,  " dB",      MGlobal, "") \
    X(outGain,    "outGain",    "OUTPUT",  -18,  18,  0,    0.1,  1,   KFloat,  " dB",      MGlobal, "") \
    X(limCeil,    "limCeil",    "LIMIT",   -12,  0,  -1,    0.1,  1,   KFloat,  " dB",      MGlobal, "") \
    /* ---- COMP ---- */ \
    X(compOn,     "compOn",     "ON",       0,   1,   1,    1,    1,   KBool,   "",         MComp,   "") \
    X(compThr,    "compThr",    "THRESH",  -50,  0,  -22,   0.1,  1,   KFloat,  " dB",      MComp,   "MAIN") \
    X(compRatio,  "compRatio",  "RATIO",    1,   20,  3.5,  0.1,  0.5, KFloat,  " :1",      MComp,   "MAIN") \
    X(compAtk,    "compAtk",    "ATTACK",   0.1, 100, 12,   0.1,  0.4, KFloat,  " ms",      MComp,   "MAIN") \
    X(compRel,    "compRel",    "RELEASE",  10,  1000, 120, 1,    0.4, KFloat,  " ms",      MComp,   "MAIN") \
    X(compKnee,   "compKnee",   "KNEE",     0,   24,  6,    0.1,  1,   KFloat,  " dB",      MComp,   "MAIN") \
    X(compMakeup, "compMakeup", "MAKEUP",   0,   24,  4,    0.1,  1,   KFloat,  " dB",      MComp,   "MAIN") \
    X(comp2On,    "comp2On",    "ON",       0,   1,   1,    1,    1,   KBool,   "",         MComp,   "STAGE 2") \
    X(comp2Amt,   "comp2Amt",   "AMOUNT",   0,   100, 40,   1,    1,   KFloat,  " %",       MComp,   "STAGE 2") \
    X(compMix,    "compMix",    "MIX",      0,   100, 100,  1,    1,   KFloat,  " %",       MComp,   "PARALLEL") \
    X(compSC,     "compSC",     "SC HPF",   20,  500, 90,   1,    0.5, KFloat,  " Hz",      MComp,   "SIDECHAIN") \
    X(compWarm,   "compWarm",   "AMOUNT",   0,   100, 30,   1,    1,   KFloat,  " %",       MComp,   "WARM") \
    /* ---- EQ ---- */ \
    X(eqOn,       "eqOn",       "ON",       0,   1,   1,    1,    1,   KBool,   "",         MEq,     "") \
    X(hpOn,       "hpOn",       "ON",       0,   1,   1,    1,    1,   KBool,   "",         MEq,     "FILTERS") \
    X(hpFreq,     "hpFreq",     "LOW CUT",  20,  400, 80,   1,    0.5, KFloat,  " Hz",      MEq,     "FILTERS") \
    X(lpFreq,     "lpFreq",     "HIGH CUT", 2000, 20000, 20000, 10, 0.5, KFloat, " Hz",     MEq,     "FILTERS") \
    X(lsF,        "lsF",        "FREQ",     40,  400, 120,  1,    0.5, KFloat,  " Hz",      MEq,     "LOW SHELF") \
    X(lsG,        "lsG",        "GAIN",    -15,  15,  0,    0.1,  1,   KFloat,  " dB",      MEq,     "LOW SHELF") \
    X(lsQ,        "lsQ",        "Q",        0.3, 1.5, 0.7,  0.01, 1,   KFloat,  "",         MEq,     "LOW SHELF") \
    X(lmF,        "lmF",        "FREQ",     100, 2000, 400, 1,    0.5, KFloat,  " Hz",      MEq,     "LOW MID") \
    X(lmG,        "lmG",        "GAIN",    -15,  15,  0,    0.1,  1,   KFloat,  " dB",      MEq,     "LOW MID") \
    X(lmQ,        "lmQ",        "Q",        0.3, 6,   1,    0.01, 0.6, KFloat,  "",         MEq,     "LOW MID") \
    X(hmF,        "hmF",        "FREQ",     1000, 8000, 3000, 1,  0.5, KFloat,  " Hz",      MEq,     "HIGH MID") \
    X(hmG,        "hmG",        "GAIN",    -15,  15,  0,    0.1,  1,   KFloat,  " dB",      MEq,     "HIGH MID") \
    X(hmQ,        "hmQ",        "Q",        0.3, 6,   1,    0.01, 0.6, KFloat,  "",         MEq,     "HIGH MID") \
    X(hsF,        "hsF",        "FREQ",     4000, 16000, 10000, 1, 0.5, KFloat, " Hz",      MEq,     "HIGH SHELF") \
    X(hsG,        "hsG",        "GAIN",    -15,  15,  0,    0.1,  1,   KFloat,  " dB",      MEq,     "HIGH SHELF") \
    X(hsQ,        "hsQ",        "Q",        0.3, 1.5, 0.7,  0.01, 1,   KFloat,  "",         MEq,     "HIGH SHELF") \
    /* ---- FX ---- */ \
    X(fxOn,       "fxOn",       "ON",       0,   1,   1,    1,    1,   KBool,   "",         MFx,     "") \
    X(deessOn,    "deessOn",    "ON",       0,   1,   1,    1,    1,   KBool,   "",         MFx,     "DE-ESSER") \
    X(deessFreq,  "deessFreq",  "LOW",      2000, 12000, 5000, 10,  0.5, KFloat,  " Hz",     MFx,     "DE-ESSER") \
    X(deessHigh,  "deessHigh",  "HIGH",     4000, 18000, 10000, 10, 0.5, KFloat,  " Hz",     MFx,     "DE-ESSER") \
    X(deessAmt,   "deessAmt",   "THRESH",   0,   100, 50,   1,    1,   KFloat,  " %",       MFx,     "DE-ESSER") \
    X(deessRange, "deessRange", "RANGE",    0,   24,  10,   0.1,  1,   KFloat,  " dB",      MFx,     "DE-ESSER") \
    X(deessMode,  "deessMode",  "MODE",     0,   1,   0,    1,    1,   KChoice, "Single vocal|Allround", MFx, "DE-ESSER") \
    X(deessBand,  "deessBand",  "BAND",     0,   1,   0,    1,    1,   KChoice, "Split band|Wide band", MFx, "DE-ESSER") \
    X(deessListen,"deessListen","LISTEN",   0,   1,   0,    1,    1,   KBool,   "",         MFx,     "DE-ESSER") \
    X(satOn,      "satOn",      "ON",       0,   1,   1,    1,    1,   KBool,   "",         MFx,     "SATURATION") \
    X(satDrive,   "satDrive",   "DRIVE",    0,   100, 25,   1,    1,   KFloat,  " %",       MFx,     "SATURATION") \
    X(satTone,    "satTone",    "TONE",    -100, 100, 0,    1,    1,   KFloat,  "",         MFx,     "SATURATION") \
    X(satMix,     "satMix",     "MIX",      0,   100, 100,  1,    1,   KFloat,  " %",       MFx,     "SATURATION") \
    X(airOn,      "airOn",      "ON",       0,   1,   0,    1,    1,   KBool,   "",         MFx,     "AIR") \
    X(airMid,     "airMid",     "MID AIR",  0,   100, 35,   1,    1,   KFloat,  " %",       MFx,     "AIR") \
    X(airHigh,    "airHigh",    "HIGH AIR", 0,   100, 45,   1,    1,   KFloat,  " %",       MFx,     "AIR") \
    X(dblOn,      "dblOn",      "ON",       0,   1,   0,    1,    1,   KBool,   "",         MFx,     "DOUBLER") \
    X(dblAmt,     "dblAmt",     "AMOUNT",   0,   100, 35,   1,    1,   KFloat,  " %",       MFx,     "DOUBLER") \
    X(dblSpread,  "dblSpread",  "SPREAD",   0,   100, 60,   1,    1,   KFloat,  " %",       MFx,     "DOUBLER") \
    X(choOn,      "choOn",      "ON",       0,   1,   0,    1,    1,   KBool,   "",         MFx,     "CHORUS") \
    X(choRate,    "choRate",    "RATE",     0.1, 5,   0.8,  0.01, 0.5, KFloat,  " Hz",      MFx,     "CHORUS") \
    X(choDepth,   "choDepth",   "DEPTH",    0,   100, 35,   1,    1,   KFloat,  " %",       MFx,     "CHORUS") \
    X(choMix,     "choMix",     "MIX",      0,   100, 30,   1,    1,   KFloat,  " %",       MFx,     "CHORUS") \
    X(phoneOn,    "phoneOn",    "ON",       0,   1,   0,    1,    1,   KBool,   "",         MFx,     "PHONE") \
    X(phoneAmt,   "phoneAmt",   "AMOUNT",   0,   100, 100,  1,    1,   KFloat,  " %",       MFx,     "PHONE") \
    X(phoneGrit,  "phoneGrit",  "GRIT",     0,   100, 20,   1,    1,   KFloat,  " %",       MFx,     "PHONE") \
    X(widthOn,    "widthOn",    "ON",       0,   1,   1,    1,    1,   KBool,   "",         MFx,     "WIDTH") \
    X(width,      "width",      "WIDTH",    0,   200, 100,  1,    1,   KFloat,  " %",       MFx,     "WIDTH") \
    /* ---- DLY ---- */ \
    X(dlyOn,      "dlyOn",      "ON",       0,   1,   1,    1,    1,   KBool,   "",         MDly,    "") \
    X(dlySync,    "dlySync",    "SYNC",     0,   1,   1,    1,    1,   KBool,   "",         MDly,    "TIME") \
    X(dlyNote,    "dlyNote",    "NOTE",     0,   5,   1,    1,    1,   KChoice, "1/4|1/8|1/8.|1/16|1/4.|1/2", MDly, "TIME") \
    X(dlyTime,    "dlyTime",    "TIME",     20,  1500, 300, 1,    0.5, KFloat,  " ms",      MDly,    "TIME") \
    X(dlyPing,    "dlyPing",    "PING-PONG", 0,  1,   1,    1,    1,   KBool,   "",         MDly,    "FEEDBACK") \
    X(dlyFb,      "dlyFb",      "FEEDBACK", 0,   90,  30,   1,    1,   KFloat,  " %",       MDly,    "FEEDBACK") \
    X(dlyHP,      "dlyHP",      "HIGH PASS", 20, 2000, 150, 1,    0.5, KFloat,  " Hz",      MDly,    "FEEDBACK") \
    X(dlyLP,      "dlyLP",      "LOW PASS", 500, 20000, 7000, 10, 0.5, KFloat,  " Hz",      MDly,    "FEEDBACK") \
    X(dlyDuck,    "dlyDuck",    "DUCK",     0,   100, 60,   1,    1,   KFloat,  " %",       MDly,    "DUCKING") \
    X(dlyDuckRel, "dlyDuckRel", "RECOVER",  20,  800, 200,  1,    0.5, KFloat,  " ms",      MDly,    "DUCKING") \
    X(dlyMix,     "dlyMix",     "MIX",      0,   100, 0,    1,    1,   KFloat,  " %",       MDly,    "MIX") \
    /* ---- REV ---- */ \
    X(revOn,      "revOn",      "ON",       0,   1,   1,    1,    1,   KBool,   "",         MRev,    "") \
    X(revMode,    "revMode",    "MODE",     0,   3,   1,    1,    1,   KChoice, "Room|Plate|Hall|Spring", MRev, "TYPE") \
    X(revDecay,   "revDecay",   "DECAY",    0.2, 10,  1.8,  0.01, 0.5, KFloat,  " s",       MRev,    "TYPE") \
    X(revPre,     "revPre",     "PRE-DELAY", 0,  250, 20,   1,    1,   KFloat,  " ms",      MRev,    "TYPE") \
    X(revSize,    "revSize",    "SIZE",     0,   100, 50,   1,    1,   KFloat,  " %",       MRev,    "TYPE") \
    X(revDiff,    "revDiff",    "DIFFUSE",  0,   100, 70,   1,    1,   KFloat,  " %",       MRev,    "TYPE") \
    X(revMod,     "revMod",     "MODULATE", 0,   100, 40,   1,    1,   KFloat,  " %",       MRev,    "TYPE") \
    X(revDuck,    "revDuck",    "DUCK",     0,   100, 30,   1,    1,   KFloat,  " %",       MRev,    "MIX") \
    X(revMix,     "revMix",     "MIX",      0,   100, 0,    1,    1,   KFloat,  " %",       MRev,    "MIX") \
    X(revDamp,    "revDamp",    "DAMPING",  0,   100, 50,   1,    1,   KFloat,  " %",       MRev,    "TONE") \
    X(revWidth,   "revWidth",   "WIDTH",    0,   100, 100,  1,    1,   KFloat,  " %",       MRev,    "TONE") \
    X(revLow,     "revLow",     "LOW CUT",  20,  1000, 150, 1,    0.5, KFloat,  " Hz",      MRev,    "TONE") \
    X(revHigh,    "revHigh",    "HIGH CUT", 1000, 20000, 9000, 10, 0.5, KFloat, " Hz",      MRev,    "TONE") \
    X(revBass,    "revBass",    "BASS",     50,  200, 100,  1,    1,   KFloat,  " %",       MRev,    "TONE") \
    /* ---- TUNE ---- */ \
    X(tuneOn,     "tuneOn",     "ON",       0,   1,   0,    1,    1,   KBool,   "",         MTune,   "") \
    X(tuneKey,    "tuneKey",    "KEY",      0,   11,  0,    1,    1,   KChoice, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B", MTune, "PITCH") \
    X(tuneScale,  "tuneScale",  "SCALE",    0,   4,   0,    1,    1,   KChoice, "Chromatic|Major|Minor|Pent Maj|Pent Min", MTune, "PITCH") \
    X(tuneSpeed,  "tuneSpeed",  "RETUNE",   0,   400, 8,    1,    0.4, KFloat,  " ms",      MTune,   "PITCH") \
    X(tuneAmt,    "tuneAmt",    "AMOUNT",   0,   100, 100,  1,    1,   KFloat,  " %",       MTune,   "PITCH") \
    X(tuneHuman,  "tuneHuman",  "HUMANIZE", 0,   100, 0,    1,    1,   KFloat,  " %",       MTune,   "FEEL") \
    X(tuneFlex,   "tuneFlex",   "FLEX",     0,   100, 0,    1,    1,   KFloat,  " %",       MTune,   "FEEL") \
    X(tuneVib,    "tuneVib",    "VIBRATO",  0,   200, 0,    1,    1,   KFloat,  " %",       MTune,   "FEEL") \
    X(tuneFormant,"tuneFormant","FORMANT",  0,   1,   1,    1,    1,   KBool,   "",         MTune,   "FEEL")
#define X(name, id, label, lo, hi, def, step, skew, kind, unit, mod, grp) name,
enum P : int { YV_LIST(X) Count };
#undef X

#define X(name, id, label, lo, hi, def, step, skew, kind, unit, mod, grp) \
    { id, label, (float) (lo), (float) (hi), (float) (def), (float) (step), (float) (skew), kind, unit, mod, grp },
inline const Def kDefs[Count] = { YV_LIST(X) };
#undef X

inline const char* const kModuleTitles[kNumModules] = { "GLOBAL", "COMP", "EQ", "FX", "DLY", "REV", "AUTOTUNE" };
} // namespace yv
