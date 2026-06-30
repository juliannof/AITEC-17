// midi/JVPatches.cpp — Roland JV-2080 patch name table  (AITEC 2026-06-30)
// Fuente verificada: JV-2080_OM.pdf pp.176-179 + Roland Faxback #10454
// 768 patches totales. Strings en flash (const).
#include "JVPatches.h"

// ── USER (MSB=0x50, LSB=0) — 128 patches ─────────────────────────────────
static const char* const kUser[128] = {
    "2 0 8 0",       "Keep :-)",      "Temple of JV",  "Adrenaline",
    "Rich Dynapad",  "Morning Lite",  "Rain Forest",   "Str + Winds",
    "Booster Bips",  "Jupiterings",   "Sm.Brass Grp",  "Techno Dream",
    "Trancing Pad",  "Mental Chord",  "Feed Me!",      "3 Osc Brass",
    "Planet Asia",   "PieceOfCheez",  "December Sky",  "East Europe",
    "RiversOfTime",  "RD-1000",       "Civilization",  "Pulsatronic",
    "Ring E.Piano",  "Creamy",        "Echo Rhodes",   "202 Rude Bs",
    "HolidayCheer",  "Glider",        "Atmos Harp",    "Phobos",
    "VintagePlunk",  "Dirty Organ",   "X..? Whistle",  "Acid TB",
    "Rotodreams",    "Analog Drama",  "Cyber Dreams",  "P5 Polymod",
    "Clear Guitar",  "Progresso Ld",  "pp Harmonium",  "Blue Notes",
    "RingSequence",  "Enlighten",     "Brass Mutes",   "FM BellPiano",
    "SH-2000",       "Shadows",       "Far East",      "Tube Smoke",
    "Organizer",     "Full Orchest",  "B'on d'moov!",  "Prefab Chime",
    "Player's EP",   "BandPass MOd",  "4pole Bass",    "Octapad",
    "Wire Pad",      "Warm Pipe",     "Spectrum Mod",  "D-50 Rhodes",
    "Solo Strat",    "Dist TB-303",   "Soap Opera",    "Pilgrimage",
    "Sax Choir",     "Dimensional",   "Stacc.Heaven",  "PhaseBlipper",
    "Pure Blips",    "Afterlife",     "JUNO Power!",   "See-Thru EP",
    "JX SqrCarpet",  "Phaser MC",     "Harpsy Clav",   "Blusey OD",
    "Belfry Chime",  "Scat Flute",    "Soundtraque",   "House Chord",
    "Glass Blower",  "DesertCrystl",  "Breathy Brs",   "Jay Vee Solo",
    "Upright Pno",   "Darkshine",     "Exotic Velo",   "Surf's Up!",
    "Grindstone",    "Stringsheen",   "2pole Bass",    "D50FantaPerc",
    "Resojuice",     "Silicon Str",   "Cyber Swing",   "Royale",
    "Echo Piano",    "Sequalog",      "Translucence",  "Organesque",
    "Solo Steel",    "Ballad Trump",  "Dulcitar",      "2.2 Bell Pad",
    "Flute 2080",    "Plik-Plok",     "Triumph Brs",   "Sweep Clav",
    "GR500 TmpDly",  "Unearthly",     "Gluey Pad",     "Innocent EP",
    "Earth Blow",    "D'light",       "Perky Noize",   "Mod DirtyWav",
    "Miniphaser",    "Sci-Fi Str",    "OD 5ths",       "Glistening",
    "Droplet",       "Silky Way",
};

// ── PR-A (MSB=0x51, LSB=0) — 128 patches ─────────────────────────────────
static const char* const kPrA[128] = {
    "64voicePiano",  "Bright Piano",  "Classique",     "Nice Piano",
    "Piano Thang",   "Power Grand",   "House Piano",   "E.Grand",
    "MIDIed Grand",  "Piano Blend",   "West Coast",    "PianoStrings",
    "Bs/Pno+Brs",    "Waterhodes",    "S.A.E.P.",      "SA Rhodes 1",
    "SA Rhodes 2",   "Stiky Rhodes",  "Dig Rhodes",    "Nylon EPiano",
    "Nylon Rhodes",  "Rhodes Mix",    "PsychoRhodes",  "Tremo Rhodes",
    "MK-80 Rhodes",  "MK-80 Phaser",  "Delicate EP",   "Octa Rhodes1",
    "Octa Rhodes2",  "JV Rhodes+",    "EP+Mod Pad",    "Mr.Mellow",
    "Comp Clav",     "Klavinet",      "Winger Clav",   "Phaze Clav 1",
    "Phaze Clav 2",  "Phuzz Clav",    "Chorus Clav",   "Claviduck",
    "Velo-Rez Clv",  "Clavicembalo",  "Analog Clav1",  "Analog Clav2",
    "Metal Clav",    "Full Stops",    "Ballad B",      "Mellow Bars",
    "AugerMentive",  "Perky B",       "The Big Spin",  "Gospel Spin",
    "Roller Spin",   "Rocker Spin",   "Tone Wh.Solo",  "Purple Spin",
    "60's LeadORG",  "Assalt Organ",  "D-50 Organ",    "Cathedral",
    "Church Pipes",  "Poly Key",      "Poly Saws",     "Poly Pulse",
    "Dual Profs",    "Saw Mass",      "Poly Split",    "Poly Brass",
    "Stackoid",      "Poly Rock",     "D-50 Stack",    "Fantasia JV",
    "Jimmee Dee",    "Heavenals",     "Mallet Pad",    "Huff N Stuff",
    "Puff 1080",     "BellVox 1080",  "Fantasy Vox",   "Square Keys",
    "Childlike",     "Music Box",     "Toy Box",       "Wave Bells",
    "Tria Bells",    "Beauty Bells",  "Music Bells",   "Pretty Bells",
    "Pulse Key",     "Wide Tubular",  "AmbienceVibe",  "Warm Vibes",
    "Dyna Marimba",  "Bass Marimba",  "Nomad Perc",    "Ethno Metals",
    "Islands Mlt",   "Steelin Keys",  "Steel Drums",   "Voicey Pizz",
    "Sitar",         "Drone Split",   "Ethnopluck",    "Jamisen",
    "Dulcimer",      "East Melody",   "MandolinTrem",  "Nylon Gtr",
    "Gtr Strings",   "Steel Away",    "Heavenly Gtr",  "12str Gtr 1",
    "12str Gtr 2",   "Jz Gtr Hall",   "LetterFrmPat",  "Jazz Scat",
    "Lounge Gig",    "JC Strat",      "Twin Strats",   "JV Strat",
    "Syn Strat",     "Rotary Gtr",    "Muted Gtr",     "SwitchOnMute",
    "Power Trip",    "Crunch Split",  "Rezodrive",     "RockYurSocks",
};

// ── PR-B (MSB=0x51, LSB=1) — 126 patches (127-128 sin nombre) ────────────
static const char* const kPrB[128] = {
    "Dist Gtr 1",    "Dist Gtr 2",    "R&R Chunk",     "Phripphuzz",
    "Grungeroni",    "Black Widow",   "Velo-Wah Gtr",  "Mod-Wah Gtr",
    "Pick Bass",     "Hip Bass",      "Perc.Bass",     "Homey Bass",
    "Finger Bass",   "Nylon Bass",    "Ac.Upright",    "Wet Fretls",
    "Fretls Dry",    "Slap Bass 1",   "Slap Bass 2",   "Slap Bass 3",
    "Slap Bass 4",   "4 Pole Bass",   "Tick Bass",     "House Bass",
    "Mondo Bass",    "Clk AnalogBs",  "Bass In Face",  "101 Bass",
    "Noiz Bass",     "Super Jup Bs",  "Occitan Bass",  "Hugo Bass",
    "Multi Bass",    "Moist Bass",    "BritelowBass",  "Untamed Bass",
    "Rubber Bass",   "Stereoww Bs",   "Wonder Bass",   "Deep Bass",
    "Super JX Bs",   "W<RED>-Bass",   "HI-Ring Bass",  "Euro Bass",
    "SinusoidRave",  "Alternative",   "Acid Line",     "Auto TB-303",
    "Hihat Tekno",   "Velo Tekno 1",  "Raggatronic",   "Blade Racer",
    "S&H Pad",       "Syncrosonix",   "Fooled Again",  "Alive",
    "Velo Tekno 2",  "Rezoid",        "Raverborg",     "Blow Hit",
    "Hammer Bell",   "Seq Mallet",    "Intentions",    "Pick It",
    "Analog Seq",    "Impact Vox",    "TeknoSoloVox",  "X-Mod Man",
    "Paz <==> Zap",  "4 Hits 4 You",  "Impact",        "Phase Hit",
    "Tekno Hit 1",   "Tekno Hit 2",   "Tekno Hit 3",   "Reverse Hit",
    "SquareLead 1",  "SquareLead 2",  "You and Luck",  "Belly Lead",
    "WhistlinAtom",  "Edye Boost",    "MG Solo",       "FXM Saw Lead",
    "Sawteeth",      "Smoothe",       "MG Lead",       "MG Interval",
    "Pulse Lead 1",  "Pulse Lead 2",  "Little Devil",  "Loud SynLead",
    "Analog Lead",   "5th Lead",      "Flute",         "Piccolo",
    "Pan Pipes",     "Airplaaane",    "Taj Mahal",     "Raya Shaku",
    "Oboe mf",       "Oboe Express",  "Clarinet mp",   "ClariExpress",
    "Mitzva Split",  "ChamberWinds",  "ChamberWoods",  "Film Orch",
    "Sop.Sax mf",    "Alto Sax",      "AltoLead Sax",  "Tenor Sax",
    "Baritone Sax",  "Take A Tenor",  "Sax Section",   "Bigband Sax",
    "Harmonica",     "Harmo Blues",   "BluesHarp",     "Hillbillys",
    "French Bags",   "Majestic Tpt",  "Voluntare",     "2Trumpets",
    "Tpt Sect",      "Mute TP mod",   nullptr,         nullptr,
};

// ── PR-C (MSB=0x51, LSB=2) — 128 patches ─────────────────────────────────
static const char* const kPrC[128] = {
    "Harmon Mute",   "Tp&Sax Sect",   "Sax+Tp+Tb",     "Brass Sect",
    "Trombone",      "Hybrid Bones",  "Noble Horns",   "Massed Horns",
    "Horn Swell",    "Brass It!",     "Brass Attack",  "Archimede",
    "Rugby Horn",    "MKS-80 Brass",  "True ANALOG",   "Dark Vox",
    "RandomVowels",  "Angels Sing",   "Pvox Oooze",    "Longing...",
    "Arasian Morn",  "Beauty Vox",    "Mary-AnneVox",  "Belltree Vox",
    "Vox Panner",    "Spaced Voxx",   "Glass Voices",  "Tubular Vox",
    "Velo Voxx",     "Wavox",         "Doos",          "Synvox Comps",
    "Vocal Oohz",    "LFO Vox",       "St.Strings",    "Warm Strings",
    "Somber Str",    "Marcato",       "Bright Str",    "String Ens",
    "TremoloStrng",  "Chambers",      "ViolinCello",   "Symphonique",
    "Film Octaves",  "Film Layers",   "Bass Pizz",     "Real Pizz",
    "Harp On It",    "Harp",          "JP-8 Str 1",    "JP-8 Str 2",
    "E-Motion Pad",  "JP-8 Str 3",    "Vintage Orch",  "JUNO Strings",
    "Gigantalog",    "PWM Strings",   "Warmth",        "ORBit Pad",
    "Deep Strings",  "Pulsify",       "Pulse Pad",     "Greek Power",
    "Harmonicum",    "D-50 Heaven",   "Afro Horns",    "Pop Pad",
    "Dreamesque",    "Square Pad",    "JP-8 Hollow",   "JP-8Haunting",
    "Heirborne",     "Hush Pad",      "Jet Pad 1",     "Jet Pad 2",
    "Phaze Pad",     "Phaze Str",     "Jet Str Ens",   "Pivotal Pad",
    "3D Flanged",    "Fantawine",     "Glassy Pad",    "Moving Glass",
    "Glasswaves",    "Shiny Pad",     "ShiftedGlass",  "Chime Pad",
    "Spin Pad",      "Rotary Pad",    "Dawn 2 Dusk",   "Aurora",
    "Strobe Mode",   "Albion",        "Running Pad",   "Stepped Pad",
    "Random Pad",    "SoundtrkDANC",  "Flying Waltz",  "Vanishing",
    "5th Sweep",     "Phazweep",      "Big BPF",       "MG Sweep",
    "CeremonyTimp",  "Dyno Toms",     "Sands ofTime",  "Inertia",
    "Vektogram",     "Crash Pad",     "Feedback VOX",  "Cascade",
    "Shattered",     "NextFrontier",  "Pure Tibet",    "Chime Wash",
    "Night Shade",   "Tortured",      "Dissimilate",   "Dunes",
    "Ocean Floor",   "Cyber Space",   "Biosphere",     "Variable Run",
    "Ice Hall",      "ComputerRoom",  "Inverted",      "Terminate",
};

// ── PR-D / GM (MSB=0x51, LSB=3) — 128 patches ────────────────────────────
static const char* const kPrD[128] = {
    "Piano 1",       "Piano 2",       "Piano 3",       "Honky-tonk",
    "E.Piano 1",     "E.Piano 2",     "Harpsichord",   "Clav.",
    "Celesta",       "Glockenspiel",  "Music Box",     "Vibraphone",
    "Marimba",       "Xylophone",     "Tubular-bell",  "Santur",
    "Organ 1",       "Organ 2",       "Organ 3",       "Church Org.1",
    "Reed Organ",    "Accordion Fr",  "Harmonica",     "Bandoneon",
    "Nylon-str.Gt",  "Steel-str.Gt",  "Jazz Gt.",      "Clean Gt.",
    "Muted Gt.",     "Overdrive Gt",  "DistortionGt",  "Gt.Harmonics",
    "Acoustic Bs.",  "Fingered Bs.",  "Picked Bs.",    "Fretless Bs.",
    "Slap Bass 1",   "Slap Bass 2",   "Synth Bass 1",  "Synth Bass 2",
    "Violin",        "Viola",         "Cello",         "Contrabass",
    "Tremolo Str",   "PizzicatoStr",  "Harp",          "Timpani",
    "Strings",       "Slow Strings",  "Syn.Strings1",  "Syn.Strings2",
    "Choir Aahs",    "Voice Oohs",    "SynVox",        "OrchestraHit",
    "Trumpet",       "Trombone",      "Tuba",          "MutedTrumpet",
    "French Horn",   "Brass 1",       "Synth Brass1",  "Synth Brass2",
    "Soprano Sax",   "Alto Sax",      "Tenor Sax",     "Baritone Sax",
    "Oboe",          "English Horn",  "Bassoon",       "Clarinet",
    "Piccolo",       "Flute",         "Recorder",      "Pan Flute",
    "Bottle Blow",   "Shakuhachi",    "Whistle",       "Ocarina",
    "Square Wave",   "Saw Wave",      "Syn.Calliope",  "Chiffer Lead",
    "Charang",       "Solo Vox",      "5th Saw Wave",  "Bass & Lead",
    "Fantasia",      "Warm Pad",      "Polysynth",     "Space Voice",
    "Bowed Glass",   "Metal Pad",     "Halo Pad",      "Sweep Pad",
    "Ice Rain",      "Soundtrack",    "Crystal",       "Atmosphere",
    "Brightness",    "Goblin",        "Echo Drops",    "Star Theme",
    "Sitar",         "Banjo",         "Shamisen",      "Koto",
    "Kalimba",       "Bag Pipe",      "Fiddle",        "Shanai",
    "Tinkle Bell",   "Agogo",         "Steel Drums",   "Woodblock",
    "Taiko",         "Melo. Tom 1",   "Synth Drum",    "Reverse Cym.",
    "Gt.FretNoise",  "Breath Noise",  "Seashore",      "Bird",
    "Telephone 1",   "Helicopter",    "Applause",      "Gun Shot",
};

// ── PR-E (MSB=0x51, LSB=4) — 128 patches ─────────────────────────────────
static const char* const kPrE[128] = {
    "Echo Piano",    "Upright Pno",   "RD-1000",       "Player's EP",
    "D-50 Rhodes",   "Innocent EP",   "Echo Rhodes",   "See-Thru EP",
    "FM BellPiano",  "Ring E.Piano",  "Soap Opera",    "Dirty Organ",
    "Surf's Up!",    "Organesque",    "pp Harmonium",  "PieceOfCheez",
    "Harpsy Clav",   "Exotic Velo",   "HolidayCheer",  "Morning Lite",
    "Prefab Chime",  "Belfry Chime",  "Stacc.Heaven",  "2.2 Bell Pad",
    "Far East",      "Wire Pad",      "PhaseBlipper",  "Sweep Clav",
    "Glider",        "Solo Steel",    "DesertCrystl",  "Clear Guitar",
    "Solo Strat",    "Feed Me!",      "Tube Smoke",    "Creamy",
    "Blusey OD",     "Grindstone",    "OD 5ths",       "East Europe",
    "Dulcitar",      "Atmos Harp",    "Pilgrimage",    "202 Rude Bs",
    "2pole Bass",    "4pole Bass",    "Phaser MC",     "Miniphaser",
    "Acid TB",       "Full Orchest",  "Str + Winds",   "Flute 2080",
    "Scat Flute",    "Sax Choir",     "Ballad Trump",  "Sm.Brass Grp",
    "Royale",        "Brass Mutes",   "Breathy Brs",   "3 Osc Brass",
    "P5 Polymod",    "Triumph Brs",   "Techno Dream",  "Organizer",
    "Civilization",  "Mental Chord",  "House Chord",   "Sequalog",
    "Booster Bips",  "VintagePlunk",  "Plik-Plok",     "RingSequence",
    "Cyber Swing",   "Keep :-)",      "Resojuice",     "B'on d'moov!",
    "Dist TB-303",   "Temple of JV",  "Planet Asia",   "Afterlife",
    "Trancing Pad",  "Pulsatronic",   "Cyber Dreams",  "Warm Pipe",
    "Pure Pipe",     "SH-2000",       "X..? Whistle",  "Jay Vee Solo",
    "Progresso Ld",  "Adrenaline",    "Enlighten",     "Glass Blower",
    "Earth Blow",    "JX SqrCarpet",  "Dimensional",   "Jupiterings",
    "Analog Drama",  "Rich Dynapad",  "Silky Way",     "Gluey Pad",
    "BandPass Mod",  "Soundtraque",   "Translucence",  "Darkshine",
    "D'light",       "December Sky",  "Octapad",       "JUNO Power!",
    "Spectrum Mod",  "Stringsheen",   "GR500 TmpDly",  "Mod DirtyWav",
    "Silicon Str",   "D50FantaPerc",  "Rotodreams",    "Blue Notes",
    "RiversOfTime",  "Phobos",        "2 0 8 0",       "Unearthly",
    "Glistening",    "Sci-Fi Str",    "Shadows",       "Helium Queen",
    "Sci-Fi FX x4",  "Perky Noize",   "Droplet",       "Rain Forest",
};

// ── Lookup ────────────────────────────────────────────────────────────────
const char* jvPatchName(uint8_t msb, uint8_t lsb, uint8_t pc) {
    if (pc >= 128) return nullptr;
    if (msb == 0x50 && lsb == 0) return kUser[pc];
    if (msb == 0x51) {
        switch (lsb) {
            case 0: return kPrA[pc];
            case 1: return kPrB[pc];   // nullptr para slots 126-127
            case 2: return kPrC[pc];
            case 3: return kPrD[pc];
            case 4: return kPrE[pc];
        }
    }
    return nullptr;
}

const char* jvBankLabel(uint8_t msb, uint8_t lsb) {
    if (msb == 0x50 && lsb == 0) return "USER";
    if (msb == 0x51) {
        switch (lsb) {
            case 0: return "PR-A";
            case 1: return "PR-B";
            case 2: return "PR-C";
            case 3: return "GM";
            case 4: return "PR-E";
        }
    }
    return "????";
}
