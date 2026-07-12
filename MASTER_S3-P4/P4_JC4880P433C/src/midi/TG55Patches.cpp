// midi/TG55Patches.cpp — Yamaha TG55 Preset Voice name table (AITEC 2026-07-12)
// Fuente verificada: docs/Yamaha_TG55_Brief_Implementacion.md §8
//   - TG55G.pdf (manual oficial, página 12, Preset Voice List P01-P64).
#include "TG55Patches.h"

static const char* const kPreset[64] = {
    "Piano",       "Voyager",     "Pro55Brass",  "Elektrodes",
    "Zuratustra",  "DawnChorus",  "GX Dream",    "GrooveKing",
    "DistGuitar",  "ZenAirBell",  "FullString",  "JazzMan",
    "ClassPiano",  "RockPiano",   "DX E.Piano",  "Hard EP",
    "Cry Clav",    "Funky Clav",  "Deep Organ",  "Warm Organ",
    "Trumpet",     "Stab Brass",  "Big Band",    "Orch Brass",
    "SynthBrass",  "Flute",       "Saxophone",   "FolkGuitar",
    "12 String",   "MuteGuitar",  "SingleCoil",  "Pick Bass",
    "Thumb Bass",  "SynBadBass",  "VCO Bass",    "Violin",
    "ChamberStr",  "VCF String",  "Nova Quire",  "Vibraphone",
    "Takerimba",   "Gloken",      "DigiBell",    "Oriental",
    "VCO Lead",    "Spirit VCF",  "OZ Lead",     "Get Lucky",
    "Gamma Band",  "Metal Reed",  "Modomatic",   "DataStream",
    "Mystichoir",  "St.Michael",  "Scatter",     "Triton",
    "Amazon",      "SatinGlass",  "BrassChime",  "Piano Mist",
    "Xanadu",      "WdBass Duo",  "Drum Set 1",  "Drum Set 2",
};

const char* tg55ProgName(uint8_t msb, uint8_t lsb, uint8_t pc) {
    (void)msb; (void)lsb;   // sin Bank Select verificado — solo hay PRESET
    if (pc >= 64) return nullptr;
    return kPreset[pc];
}
