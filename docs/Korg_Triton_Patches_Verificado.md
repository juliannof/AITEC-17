# Korg Triton (Rack) — Listado de Programs/Combinations (Verificado)

**Fuentes cruzadas (2026-07-04, ampliado 2026-07-13):**
1. `TritonR_VNL_EFGJ1.pdf` (Korg Voice Name List oficial, PRELOAD.PCG) — nombres de Programs/Combinations, texto nativo.
2. `TRITON_Rack_MIDIimp.TXT` (Korg TRITON-Rack MIDI Implementation, Rev 1.6, Jul.14.'03) — Bank Select MSB/LSB, Mode Change SysEx.
3. `TritonR_PG_E3.pdf` (Korg TRITON-Rack Parameter Guide, p.109 "Bank Map") — confirma msb/lsb de Bank G / g(1)-g(9) / g(d).

Solo bancos PRELOAD.PCG instalados en este equipo (INT-A/B/C/D + Bank G + Bank g(d)). No incluye
EXB-A..H ni INT-E/F (placas de expansión, no instaladas — no aparecen en el VNL de este equipo).
g(1)-g(9) (variaciones GM2 de Bank G) existen en el PRELOAD.PCG pero no están implementadas en
firmware — tabla dispersa (no todos los PC tienen nombre propio, el resto cae a Bank G con "*"
delante) que requiere extracción exhaustiva del VNL, pendiente.

**Nombres extraídos programáticamente** del texto del VNL (`pdftotext -layout` + parser por
columnas, con lista de categorías conocidas para separar nombre/categoría cuando el espaciado
de la maquetación las juntaba) — verificados por conteo exhaustivo (128/128 sin huecos en cada
banco) y muestreo visual contra el PDF. Si detectas un nombre incorrecto en hardware, repórtalo
para corregir la tabla puntual.

---

## Bank Select — Program y Combination (comparten MSB/LSB)

| Banco | MSB | LSB | Nº | Origen |
|---|---|---|---|---|
| INT-A | 0x00 | 0x00 | 128 | Program + Combination |
| INT-B | 0x00 | 0x01 | 128 | Program + Combination |
| INT-C | 0x00 | 0x02 | 128 | Program + Combination |
| INT-D | 0x00 | 0x03 | 128 | Program + Combination |
| Bank G (GM) | 0x79 | 0x00 | 128 | Solo Program |

Program y Combination **comparten el mismo Bank Select** — el modo activo (qué interpreta el
Triton al recibir Bank+PC) se controla aparte con el mensaje **MODE CHANGE** (SysEx, Func 4E):

```
F0 42 3g 50 4E 00 mm F7
```
`g` = canal MIDI (0-15), `mm`: `00`=COMBI PLAY, `02`=PROG PLAY (resto de modos en el manual,
no relevantes aquí: 01=COMBI EDIT, 03=PROG EDIT, 04=MULTI, 05=DEMO/SNG, 06=SAMPLING, 07=GLOBAL, 08=DISK).

---

## Combinations — Bank INT-A

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 000 | Romance Layers | 032 | Tiney Harmonic | 064 | ModernPiano | 096 | Comp-o-Net |
| 001 | Lonely Moon | 033 | after-T-house | 065 | Gig Split | 097 | Happily Split |
| 002 | DynOrchestra 1 | 034 | Small Orchestra | 066 | Woodwinds | 098 | Dry Chamber Ens. |
| 003 | Drum'n'Bass Kit | 035 | Organic Beats | 067 | Steely Keys | 099 | 70's Mirror Ball |
| 004 | Vocoderhythm | 036 | /\Gods Bathtub/\ | 068 | **Weightless** | 100 | Dust Devil |
| 005 | [(Goliath )] | 037 | Talkin'RaveSynth | 069 | BIG Wash Synth | 101 | Power Synth |
| 006 | Ugly Momma D.Kit | 038 | Arp Session Kit | 070 | Super Hip Kit | 102 | The16th Strummer |
| 007 | My Baby's Asleep | 039 | Wavetable Guitar | 071 | Harmonic Whisper | 103 | New Breed of Gtr |
| 008 | Knob as Drawbar1 | 040 | 2nd Harm.on Knob | 072 | Rock Organ | 104 | Pipes on SW&Knbs |
| 009 | Indian Ocean | 041 | Fisatic Dreams | 073 | Peace On Earth | 105 | Tropic Of Cancer |
| 010 | *Pad Tropolis* | 042 | Simple Pad | 074 | I'm sorry... | 106 | Please stay calm |
| 011 | Big Jazz Band | 043 | Big Brassed | 075 | Sop/Alt/Tnr/Bari | 107 | Warm Brass Ens. |
| 012 | Bell Amis | 044 | Angelic Bells | 076 | Emtim Bells | 108 | Comfort Bell |
| 013 | Golden Strings | 045 | CinematicStrings | 077 | Smoothie Strings | 109 | Sugar Plums |
| 014 | Leading Lady | 046 | Hidden Rhythm | 078 | Pad & ShakuLead | 110 | Ambient House |
| 015 | Barter Town | 047 | The Rain Forest | 079 | All Was Lost | 111 | Alien Invasion |
| 016 | Digi SynPiano 1 | 048 | Big Bottom Clav | 080 | EP Stack (Knobs) | 112 | Acoustic Mix |
| 017 | [Ribbon Rappin'] | 049 | Bass & PianoPad | 081 | DanceSynthSplit | 113 | 5th Synth Split |
| 018 | Rhythms & Bows | 050 | French Ensemble | 082 | Bows 'n' Brass | 114 | Sweet Dyn-Orch. |
| 019 | Stabbin' Split | 051 | : Crusin'Compton: | 083 | DoItToYaFeet (C) | 115 | Prodisynth Split |
| 020 | Falling Leaves | 052 | ***Ice*Rain*** | 084 | <<SYNERGY>> | 116 | Planet Pad |
| 021 | Noisy Funk Synth | 053 | Spiky & Tight | 085 | Cosmic Sweeper | 117 | Square Pad Lead |
| 022 | Drumfest | 054 | Crazy Hit Tricks | 086 | SciFi ChaseScene | 118 | Atmosphere Hit! |
| 023 | Strumming Guitar | 055 | Sofia's Place | 087 | Mountainside | 119 | Guitar Sings |
| 024 | Ballad Organ | 056 | Upper&Lower Org. | 088 | Gospel Church | 120 | Holy Pipes |
| 025 | World Winds | 057 | Irish Ballade | 089 | Koto Environment | 121 | Gamelan Band |
| 026 | Universal Choir | 058 | The BeeG Pad | 090 | BeeG Pad2 Bright | 122 | **The Toy Box** |
| 027 | The Power Trio | 059 | MutedButSpeakin' | 091 | Spittin' Brass | 123 | FunkyBrass Sect. |
| 028 | Random Blocks | 060 | Bellish Pad | 092 | Rotary Bellz | 124 | Little Princess |
| 029 | Bows&KnobHarpsi | 061 | AfterRainStrings | 093 | Strings Of Silk | 125 | Bowed Strings |
| 030 | Home Suite Home | 062 | DreamingForMiles | 094 | SweetBabyBlues | 126 | Life Together |
| 031 | Meteor Shower | 063 | Melanco | 095 | Jungle Stalker | 127 | One FingerTVShow |

## Combinations — Bank INT-B

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 000 | Foster Layer EP | 032 | Dark Bell EP | 064 | Wide L/R Piano | 096 | Synthetic EP's |
| 001 | Phatt Bass Split | 033 | Bass/EP Pad Beat | 065 | Bass / Vibe | 097 | The Sneaky 3 |
| 002 | DynOrchestra 2 | 034 | Brash Tpts&Choir | 066 | Knob as Drawbar2 | 098 | Nu Vintage Orch |
| 003 | Formant & Lead | 035 | Slammin'Bs&Piano | 067 | *HipTrip Split* | 099 | Way Smooth Grv. |
| 004 | Beat Meditation | 036 | Whopper Pad | 068 | Haunted Waters | 100 | Wooshboard |
| 005 | BIG Power Synth | 037 | Ana Logger | 069 | Mutant SoftSynth | 101 | Dynamic Change |
| 006 | Velo HiFi Kit | 038 | Mineshaft | 070 | Snap!ReggaeOrgan | 102 | Knob Mixer Kit |
| 007 | Alchemy Layer | 039 | Magic Guitar | 071 | Time For Bed | 103 | Arp-A-Listic |
| 008 | Jazz Organ 2 | 040 | 3rd Harm.on Knob | 072 | Velo Perc Organ | 104 | Pipes on Knobs |
| 009 | Prince of Egypt | 041 | Peasant Song | 073 | Quidam | 105 | Rain Festival |
| 010 | 5th.Dimension | 042 | Real EP & Pad | 074 | Hi Key-Beauty-EP | 106 | Wave Of Time |
| 011 | HybridBrassStab | 043 | Dry & Funky Keys | 075 | Sax Banquet | 107 | Warm & Horny |
| 012 | Cascade Bells | 044 | Happy Bells | 076 | Vaderian Bells | 108 | Bell and Tine |
| 013 | ClassicAnaString | 045 | Oct. Strings | 077 | *Arctic Strings* | 109 | Piano &Strings |
| 014 | SimplePad&SopSax | 046 | Warrior Princess | 078 | Recorder & Orch. | 110 | Riddem & EP |
| 015 | {Jealous Robots} | 047 | Garage Nite | 079 | Dirty Beat | 111 | Suspected Hit |
| 016 | Digi SynPiano 2 | 048 | Cutting Clav | 080 | Piano Nu-agey | 112 | MegaBrass&Piano |
| 017 | < The Trio> | 049 | Jazz Bass/Gtr. | 081 | Rock Bass/Gtr. | 113 | Tough to say |
| 018 | Orchestra | 050 | Orchestral Brass | 082 | Gotham Snd.Track | 114 | Pipes&Choir SW |
| 019 | Waterboy Split | 051 | slow... crawl... | 083 | Junga-jam | 115 | Bad Girl Shuffle |
| 020 | Dawn Arp. Pad | 052 | Forget-Me-Not | 084 | || TRANSIT || | 116 | The digital sea |
| 021 | Mr. Rezzocomp | 053 | Buzz-A-Log | 085 | Res-Inator | 117 | Digitalian |
| 022 | Percfest!! | 054 | * Instant Euro* | 086 | [<Moon Jam>] | 118 | Mr. Trendy |
| 023 | 12-Strings (Arp) | 055 | Harp Strings | 087 | Rock Is King! | 119 | Amby Plucked |
| 024 | Gospel Organ | 056 | Split ClickOrgan | 088 | Knob as Drawbar3 | 120 | Soft/Tutti on SW |
| 025 | Beyond Nepal... | 057 | Super Stereo EP | 089 | Indian Layers | 121 | Steel Pan Band |
| 026 | ...Water to Wine | 058 | << Godchild >> | 090 | Imaginary PizPad | 122 | Future Japan |
| 027 | Trp/Tb/Alto/Tenr | 059 | Fat Horn Section | 091 | SynBrass Tapper | 123 | Wackadoo |
| 028 | Stereo Mallets | 060 | Pad & Logs | 092 | Mallet Piano | 124 | Soft Piano Pad |
| 029 | Strings & Wings | 061 | Orchestral Pipes | 093 | LifeSigns EP | 125 | Universal Orch. |
| 030 | Night Reeds | 062 | Oboe Split | 094 | SplitHornSection | 126 | Rock it ship |
| 031 | Puff Comper | 063 | Echo Jamm | 095 | Saucy Squares | 127 | BX-3 Click Organ |

## Combinations — Bank INT-C

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 000 | Layered A.Piano | 032 | EP Bell Split | 064 | Ballad Layer EP | 096 | Wurly Dream EP |
| 001 | In The Room | 033 | Smooth Split | 065 | Hybrid Split | 097 | Bumpin'&Thumpin' |
| 002 | Film Tools | 034 | DblReeds&Strings | 066 | Piano Concerto | 098 | The Ascension |
| 003 | Invisible Sun | 035 | The Compin' Man | 067 | Happy Feet !! | 099 | Bad*Street Split |
| 004 | Chromium Pad | 036 | Inhabited World | 068 | Panner-Emma | 100 | Nebular Motion |
| 005 | Huge Ober Sweeps | 037 | Analog Punch | 069 | Analog 101 | 101 | Soft Blip*Synth |
| 006 | Alien Jungle Jam | 038 | Speed Racer | 070 | Re-Mix-Maven | 102 | "...MY toys!" |
| 007 | Encore (High C) | 039 | Warm&Fuzzy Gtr | 071 | Hybrid Gtr Pad | 103 | Toro,Toro,Ole!!! |
| 008 | Dirty Drawbars | 040 | JazzOrgan/Split | 072 | Vital Organ | 104 | Positive@SW&Knbs |
| 009 | Synthitar Ens. | 041 | VoicyPianoLayer | 073 | BellyPianoLayer | 105 | World Atmosphere |
| 010 | Movie Pad | 042 | TaiChi Dawn | 074 | Good Luck EP Pad | 106 | Powerful Throats |
| 011 | Earth Brass | 043 | Brass n' Saxes | 075 | Saxobrass | 107 | Warm Low Brass |
| 012 | RandomPanninBelz | 044 | -Miracle Bells- | 076 | Triple Bell | 108 | Bell Animations |
| 013 | Lush String Pad | 045 | SmoothOctaveStr | 077 | Dirty Robbers | 109 | Burble Swirl |
| 014 | Tenor & Pad | 046 | Moon's Glow | 078 | Alien Lovers | 110 | E.Pianic Layers |
| 015 | Millennium Files | 047 | MATRIX | 079 | ... Zero G's ... | 111 | Digital Diver |
| 016 | Sympathetic EP | 048 | Clav.usFunkus | 080 | 'round the Piano | 112 | Blaster Keys |
| 017 | Bass &Elec.Grand | 049 | Soft Split-Pad | 081 | Stab-In-A-Stab | 113 | Square Roots |
| 018 | ThickStringBrass | 050 | *Warm Orchestra* | 082 | Poly Sixual | 114 | Warm Bars |
| 019 | Rez Stab Split | 051 | Velo Bass Stack | 083 | korg@groove.comp | 115 | Snowboard Hop |
| 020 | Plasma Beings | 052 | Gone Forever | 084 | Ice Castle | 116 | Shh-I'm Sweeping |
| 021 | ModulaTech*Synth | 053 | Big and Buzzy | 085 | Analog SquareWah | 117 | Mir Appears |
| 022 | %%Orgambient%% | 054 | Stratosphere | 086 | Something Wicked | 118 | SYNCing Feeling |
| 023 | LastChanceToSee | 055 | E.Grand Bell Pad | 087 | Layered M1 Piano | 119 | Complex EP Grand |
| 024 | Jazz Perc (Knob) | 056 | OrganVel->Brass | 088 | ReggaeAlvinBlues | 120 | Organ Principle |
| 025 | SlowDance Piano | 057 | Highland Tines | 089 | Clicky E.Piano | 121 | Manhattan Chase |
| 026 | Opera"Wind Free" | 058 | Triton Voices | 090 | Clean Aire | 122 | Musical Clowns |
| 027 | Future Brass | 059 | Muted BigBand | 091 | Aggessive Horns | 123 | Mustang Saxes |
| 028 | Wood+Steel Bars | 060 | Isle Of Indigo | 092 | Venus Percs | 124 | Belle |
| 029 | Super Strings | 061 | * Epic Cinema * | 093 | Solo & Ensemble | 125 | Digimidgets |
| 030 | Mystic Presence | 062 | TV Detectv Drive | 094 | Hold Me... EP | 126 | Vacancy... |
| 031 | Next Millennium | 063 | AimForTheMonster | 095 | TrancenDance | 127 | PerihelionSweep |

## Combinations — Bank INT-D

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 000 | Ballad Grand | 032 | Breathy Bell EP | 064 | Heaven Layers | 096 | The Ballad EP |
| 001 | R&B Lead Split | 033 | Hot Tub Split | 065 | Titanical Split | 097 | Bass&RezKey<99> |
| 002 | Pompus Brass | 034 | Velo Orch Winds | 066 | Dbl. Reed Gang | 098 | Harpsicord Suite |
| 003 | Mr Happy Arp ! | 035 | The Organ Groove | 067 | Bass & LFO-er | 099 | Bass & Comp Jam |
| 004 | Phases Of Angels | 036 | Respirator | 068 | Northern Stars | 100 | Juicy Reflection |
| 005 | Syzzle Synth | 037 | TalkBoxBass<Rbn> | 069 | The Pitz | 101 | BottlePowerPad |
| 006 | Sonic FX Mixer | 038 | Cyborg Scratch | 070 | 2Hand Beats&Bass | 102 | Tranquil~Stream~ |
| 007 | The Guitarist | 039 | Rich Guitarist | 071 | *Slinky* | 103 | Steam Factory |
| 008 | Straight Wheels | 040 | Jazz Overdrive | 072 | Lovely Old VOX | 104 | Grand Procession |
| 009 | Marching Pipes | 041 | Accordion (SW) 1 | 073 | Accordion (SW) 2 | 105 | Svengali Dusk |
| 010 | Pearly-Gates Gig | 042 | *Dying Star* | 074 | SecretGroove Pad | 106 | Alpha Mega Synth |
| 011 | French Brass | 043 | PowrBrassSession | 075 | Techno Fugue | 107 | Smooth DynoBrass |
| 012 | Modern Monastery | 044 | *)BarbieBellz(* | 076 | Linear Synthesis | 108 | Dreamy*Lillies |
| 013 | String Concerto | 045 | 3-Octave Strings | 077 | Antarctica | 109 | BellAsian |
| 014 | Leading to Peace | 046 | Splatsburg | 078 | Electric Pad | 110 | Sweetpad & LEADZ |
| 015 | Alien Translator | 047 | Deadly Gasses | 079 | Don'tLandTheShip | 111 | Da Bomb Rap |
| 016 | GlassyEP&Strings | 048 | Village Pump | 080 | The Key Elements | 112 | E. Pierre |
| 017 | Stationary Split | 049 | Old Friends | 081 | Jungle Split | 113 | Fat Analog Split |
| 018 | Epic Orchestra | 050 | Jet Stream | 082 | King Cobras | 114 | PhunkyWahWahClav |
| 019 | Prog Rock Fest | 051 | RibbonREZ JAM!! | 083 | Street Cats | 115 | Crazy Wah Cafe |
| 020 | Metamorphoses | 052 | Spheoretical | 084 | Land Of Dreams | 116 | Virtual Journey |
| 021 | Ravalogue | 053 | DiscoLivesHERE!! | 085 | BellZarra 6/8 | 117 | Jazzy BottleLead |
| 022 | Carnival Parade | 054 | Streetwalk | 086 | BIG Gnarly Lead | 118 | TalkBoxGtr <Rbn> |
| 023 | Pastoral Guitar | 055 | Heaven's Bells | 087 | Rhythm Mysters | 119 | Mandolin Strums |
| 024 | <Standard Bars> | 056 | Jazz A Bee | 088 | Glacier Pad | 120 | Sunday Sermon |
| 025 | Shogun | 057 | October Fest | 089 | African Festival | 121 | Triton Bay |
| 026 | Symphonic Voices | 058 | Pulsing 5 Pad | 090 | WideOpenSpaces | 122 | Morning Doves |
| 027 | Small Sax Ens | 059 | Technified | 091 | Ghosttown Cornet | 123 | Radiologizer |
| 028 | Lincoln Logs | 060 | AIRY Bellz | 092 | ClockmakersDream | 124 | West Indian Way |
| 029 | Violin Sect. | 061 | SyrupySynthStrs | 093 | Serene Strings | 125 | Orchestr.Strings |
| 030 | Bass-n-Bellz | 062 | Pad & Knob1 Lead | 094 | BigB.Sect.-Split | 126 | Any Clock'll Do |
| 031 | Nunavut Lights | 063 | OpenPodBayDoor! | 095 | PortaMenthos | 127 | * User's Slot * |

## Programs — Bank INT-A

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 000 | Noisy Stabber | 032 | Power Snap Synth | 064 | Ana Brass/Lead | 096 | Digital PolySix |
| 001 | Acoustic Piano | 033 | Romance Piano | 065 | 90's Piano | 097 | Clav |
| 002 | Chipper Dayglow | 034 | Arp Angeles | 066 | Wild Arp | 098 | Goa Lover |
| 003 | Legato Strings | 035 | Arco Strings | 067 | Stereo Strings | 099 | AnalogStrings1&2 |
| 004 | ! {Tricky} Kit ! | 036 | Standard Kit | 068 | HipHop Kit | 100 | BD&SD Kit 1 |
| 005 | Acoustic Guitar | 037 | Wet Dist. Guitar | 069 | Jazz Guitar | 101 | Nylon Guitar |
| 006 | Nasty Bass | 038 | 30303 Mega Bass | 070 | Euro 8va Bass | 102 | Jungle Rez Bass |
| 007 | BX3 Vel Switch | 039 | Dark Jazz-Organ | 071 | Distortion Organ | 103 | Full Drawbars |
| 008 | Rez. Down | 040 | Clouds of Air | 072 | Tsunami Waves | 104 | Money Pad |
| 009 | Fat Brass | 041 | SFZ Brass ST | 073 | Glen & The Boys | 105 | Burnin' Brass |
| 010 | <Techno Vox Box> | 042 | Loop-Iteria | 074 | Steam Sweeps | 106 | VenusianStories |
| 011 | Fresh Breath | 043 | Oooh Voices ST | 075 | White Pad EP | 107 | SynPiano X |
| 012 | Smooth Sine Lead | 044 | HipHop Lead | 076 | Old & Analog | 108 | Syncro City |
| 013 | Sax Ensemble | 045 | Flute | 077 | DoubleReed | 109 | Recorder |
| 014 | Swirling Dreams | 046 | Sinistar*Android | 078 | Aqua Phonics | 110 | Astral Dreams |
| 015 | Monkey Skulls | 047 | Log Drum | 079 | Ensemble Bell | 111 | Bali Gamelon |
| 016 | Metalic Rez | 048 | Rez. Sweep | 080 | Cosmic Furnace | 112 | Super Saw Brass |
| 017 | Studio Stage EP | 049 | R&B E.Piano | 081 | Velo Whirly | 113 | Sweeping EP |
| 018 | Flip Blip | 050 | Arp Twins | 082 | Techno Organ Hit | 114 | Jungle Melody |
| 019 | Camera Strings | 051 | PizzAnsamble | 083 | Few Bows Here | 115 | WatcherOfTheSky |
| 020 | House Kit | 052 | Psycho Kit | 084 | Mega Drum Hit | 116 | Orchestra&Ethnic |
| 021 | Chorus E.Guitar | 053 | Vox Wah Chicks | 085 | Mute Monster | 117 | Power-Chords&FX |
| 022 | Acoustic Bass | 054 | SuperSwitch Bass | 086 | Stein Bass | 118 | E.Bass Finger |
| 023 | M1 Organ | 055 | Jazz Organ 1 | 087 | Perc Short Decay | 119 | Pipe Mixture |
| 024 | Digi Ice Pad | 056 | Ravelian Pad | 088 | Antartic Wind | 120 | {(Meditate}) |
| 025 | Trumpet | 057 | Classic F.Horn | 089 | Trombone Hard | 121 | Tight Brass |
| 026 | Sonic Blast | 058 | Monster Island | 090 | Orbiting Probes | 122 | Soundscapes |
| 027 | Choir of Light | 059 | Aaah Voices ST | 091 | Phantom Of Tine | 123 | Sitar Sitar |
| 028 | Rezbo | 060 | Trancer Lead | 092 | Seq DDL Lead | 124 | Espress Lead |
| 029 | TenorSax Growl-Y | 061 | AltoSax2Breath-Y | 093 | BariSax Growl -Y | 125 | SopranoSax Br.-Y |
| 030 | Tinklin' Pad | 062 | Crimson 5ths | 094 | Percussive Hits | 126 | Flying Machines |
| 031 | VS Bell Boy | 063 | Moving Bellz | 095 | Vibraphone | 127 | Krystal Bells |

## Programs — Bank INT-B

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 000 | Synth Sweeper | 032 | Pop Synth Pad | 064 | SynthBrass | 096 | Rezzo Release |
| 001 | Attack Piano | 033 | Piano Pad | 065 | Classic Piano | 097 | Sticky Rez Clav |
| 002 | StaccatoPizzHit | 034 | Karma Sutra | 066 | Big Hit in India | 098 | Hit Me 2 Times!! |
| 003 | Octave Strings | 035 | Symphonic Bows | 067 | Solo Violin | 099 | Santur |
| 004 | Jazz/Brush Kits | 036 | Standard Kit 2 | 068 | Drum'n'Bass Kit | 100 | BD&SD Kit 2 |
| 005 | FingertipsGuitar | 037 | Amp D. Guitar | 069 | Strato-Chime | 101 | Old 12-String |
| 006 | Dark R&B Bass | 038 | 30303 Square | 070 | Digi Syn Bass | 102 | Ramp Jungle Bass |
| 007 | Old Tone-Wheel | 039 | Sine DWGS-Organ | 071 | Classic Click | 103 | Killer B |
| 008 | The Pad | 040 | Simple Sine Pad | 072 | DJ Touch | 104 | L/R Piano(Knob1) |
| 009 | Brass Expression | 041 | Octave Brass Exp | 073 | Big Band Plunger | 105 | Trumpet Ens. |
| 010 | PiezoMix Guitar | 042 | CyborgFactoryHit | 074 | New Voyage | 106 | MotionSoundTrack |
| 011 | Slow Choir ST | 043 | E.G. Harmonics | 075 | Arctic Voices | 107 | Processed E.Gtr. |
| 012 | Phat Saw Lead | 044 | Auto Pilot | 076 | Goa Message | 108 | Fat Syn Sync |
| 013 | Old Shakuhachi | 045 | BambuSilverFlute | 077 | English Horn | 109 | Spitz Bottle |
| 014 | OXYGEN | 046 | Dolphin Ride | 078 | E.Bass Pick 2 | 110 | MixFisaMaster 3 |
| 015 | Velo Kalimba | 047 | Marimba | 079 | Tropico Bells | 111 | Future Bell |
| 016 | Future Syn Pad | 048 | Gliding Squares | 080 | Mega Big Synth | 112 | Flute Pad |
| 017 | Dyno Tine EP | 049 | Night Tines EP | 081 | Vintage EP | 113 | Hybrid Digi EP |
| 018 | Brass Impact Hit | 050 | Mondo'Rimba | 082 | Space Pod for 2 | 114 | Harp |
| 019 | String Quartet | 051 | HybridStrg/Choir | 083 | Indian Stars | 115 | Harmonica |
| 020 | Processed Kit | 052 | Cymbals Kit | 084 | Dragon Gong | 116 | Percussion Kit |
| 021 | Vintage Stratt | 053 | Funkin' Guitar | 085 | Feedback D. Gtr. | 117 | Dynamic E.Guitar |
| 022 | Fretless Bass | 054 | Poinker Bass | 086 | Sweet Fretless | 118 | Decayed Bass |
| 023 | Tekno Organ*Bass | 055 | Gospel Organ | 087 | Percussion BX3 | 119 | Church Pipes |
| 024 | Brass Pad | 056 | HarpsyKorg 8'+4' | 088 | Dark Night | 120 | Digital Bells |
| 025 | Muted Trumpet | 057 | Horns & Ensemble | 089 | Trombone Ens. | 121 | Classic Fanfare |
| 026 | Fisa Americana | 058 | The Great Wall | 090 | Marc Tree | 122 | Hemispheres |
| 027 | Vocalesque | 059 | Full Vox Pad | 091 | Take Voices | 123 | Indian Frets Gtr |
| 028 | Glide Lead | 060 | Musette | 092 | Fisa Cassotto | 124 | Film Brass |
| 029 | TenorSax Brth.-Y | 061 | AltoSax1 Growl-Y | 093 | Bassoon | 125 | Jazz Clarinet |
| 030 | Transformation | 062 | E.Bass Pick 1 | 094 | PedalSteelGuitar | 126 | Pro-Dyno EP |
| 031 | Thin Bell-s-park | 063 | Magical Bells | 095 | Koto | 127 | Tinkle Bells |

## Programs — Bank INT-C

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 000 | Techno Phonic | 032 | Auto Groove | 064 | Raunchy Brass | 096 | Square Snaps |
| 001 | Warm E.Grand | 033 | Piano Pad 2 | 065 | Piano&String/Pad | 097 | Clav Snap |
| 002 | Rave <Ribbon> | 034 | Techno Stat | 066 | Old Record Hit 2 | 098 | Golithanic Stab |
| 003 | Stereo Strings 2 | 035 | Viola Solo | 067 | String World | 099 | ComponentStrings |
| 004 | WAcKy HiPHop Kit | 036 | D'n'B Gate Kit | 068 | Gamelan Gong | 100 | Jazzy Pitch Kit |
| 005 | Spanish Guitar | 037 | SingleCoil+Piezo | 069 | Super Clean Gtr | 101 | Fingertips12 Gtr |
| 006 | Phat Bass | 038 | SynBassRes | 070 | Digi Bass 1 | 102 | Reso Bass |
| 007 | Dirty "B" | 039 | Superdark Organ | 071 | Perc-2ndHarmonic | 103 | Super BX Perc |
| 008 | Super Sweeper | 040 | Goliath's Waves | 072 | Sweeper Strings | 104 | Ghostly Strings |
| 009 | DynaBrassStereo1 | 041 | Blind as a Bat | 073 | BrassSlow Stereo | 105 | Attack Brass ST |
| 010 | <ReverseVoxBox> | 042 | Rave Stabz | 074 | Dirty Wounds | 106 | Compu-shift |
| 011 | Vocalscaping | 043 | Oooh Choir | 075 | Choir Mmh | 107 | Ooh/Aah VoicesST |
| 012 | Sine Switcher | 044 | X-Mod Raver | 076 | Sync Kronicity | 108 | Grain Board |
| 013 | Breathy BariSax | 045 | Breathy AltoSax | 077 | Classic Flute | 109 | Bari Blaster |
| 014 | Stereo WaveSweep | 046 | Spectrum Alloy | 078 | Water is Deep | 110 | JS/Ribbon Pad |
| 015 | Mallet Clocker | 047 | Log Drum & Bells | 079 | Digital Bells 2 | 111 | Ribbon Rhythm |
| 016 | PowerRezSweep | 048 | Square Rez | 080 | Dual Filterz | 112 | Power Saw |
| 017 | Pro-Stage EP | 049 | Dark and Warm | 081 | Classic Wurly | 113 | Reson Piano |
| 018 | Motion Raver | 050 | Old Record Hit 1 | 082 | Organ Impact | 114 | Pop Guitar Hit |
| 019 | Rosin Strings | 051 | Ensemble & Solo | 083 | Violin & Viola | 115 | Analog Velvet |
| 020 | Nasty TRicKy Kit | 052 | Tiny Perc. Kit | 084 | Psycho Pitch Kit | 116 | KING<>KONG Kit |
| 021 | Nu Strat | 053 | CountryNuStrings | 085 | CleanMute-Guitar | 117 | Arpeggio Guitar |
| 022 | A.Bass Buzzing | 054 | Slap Bass v/s | 086 | Fretless Switch | 118 | E.Bass Finger 2 |
| 023 | Perky Tonewheels | 055 | Jazz Organ 2 | 087 | BX3 Short Decay | 119 | Organ Positive |
| 024 | DigitalThroats 1 | 056 | Harmonic Pad | 088 | Cross Sweeper | 120 | Mellow Movie Pad |
| 025 | Trombone Soft | 057 | Tubas Gold | 089 | Trombones | 121 | Movie Brass |
| 026 | Video Blaster | 058 | Birds & Bugs | 090 | Cosmic Toys | 122 | One Note Stories |
| 027 | Grand Choir | 059 | Aah Choir | 091 | DigitalThroats 2 | 123 | Ether Voices |
| 028 | A leadload | 060 | Busy Sync | 092 | Fire Wave | 124 | Brian's Sync |
| 029 | Soft TenorSax | 061 | Warm Oboe | 093 | Wooden Flute | 125 | PanFlute |
| 030 | Cyclic X-Fades | 062 | Stereo Rez Sweep | 094 | Smoothy Pad | 126 | Glass Vox |
| 031 | Breathy Bells | 063 | GlassBell Bright | 095 | Hybrid Bell | 127 | Bottle-Bell |

## Programs — Bank INT-D

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 000 | Ribbon Morpher | 032 | SynthPianoid | 064 | Cat Lead (-Y) | 096 | Old Portamento |
| 001 | Super Grand | 033 | Chorus Piano | 065 | Classic Tines | 097 | Mutant Clav |
| 002 | Raggae Gtr Hit | 034 | Cyber Choir | 066 | SharpBottleBlow | 098 | Random Pulsator |
| 003 | PizzicatoSection | 035 | Rich Lead | 067 | Dance Lead | 099 | Mini Lead |
| 004 | UGLY HoUSe Kit | 036 | Djembe/Log Drum | 068 | Piano Layers | 100 | DWGS E.Piano |
| 005 | Rhythmic E.Gtr | 037 | Guitarish | 069 | Filter Gate | 101 | Rhythm Of Life |
| 006 | Dirty Man Bass | 038 | Hybrid Bass | 070 | Digi Bass 2 | 102 | SuperSwtch Bass2 |
| 007 | Killer BX-3 | 039 | Good Old "B" | 071 | Rotary Organ | 103 | Full Pipes |
| 008 | Ultra Res. Sweep | 040 | Analog Pad | 072 | Electrik Brass | 104 | Metal SlowPad |
| 009 | Power Brass | 041 | French Horn Sect | 073 | Frozen Glaciers | 105 | BeBop Cornet |
| 010 | Just Do It!!! | 042 | Vanishing Planet | 074 | Warm Flugel Horn | 106 | Cosmic Waves |
| 011 | Dream Voices | 043 | Classic Vox | 075 | Wispy Dry Wind | 107 | Fluttering EP |
| 012 | Octo Lead | 044 | BrightPulseLead | 076 | RibbonYoHeadOff! | 108 | AMSFeedbackLead |
| 013 | Warm Shakuhachi | 045 | Digi Layer Keys | 077 | Pipe Tutti | 109 | Tomorrow's Pad |
| 014 | Cinema Pad | 046 | OutOfTheClouds | 078 | Syn Ghostly | 110 | BPM Filter Sweep |
| 015 | Kalimba Soft | 047 | Garbage Mallets | 079 | Warm Steel Drums | 111 | Small Harp |
| 016 | Thunderlog | 048 | Band Passed | 080 | Solar Surfing | 112 | Dark Element |
| 017 | Club E.Piano | 049 | Stereo Digi EP | 081 | Splat Slap | 113 | Silky Digi EP |
| 018 | Symphonic Waves | 050 | Rhythm Rezonator | 082 | Rhythmnosis | 114 | Moon Talker |
| 019 | X-Synced | 051 | Syn Pipes | 083 | The Mandolin | 115 | Sitar/Tamboura |
| 020 | Tamborine Dream | 052 | *SpaceShip* | 084 | Crazy Wah Wah | 116 | Rave Volutionary |
| 021 | Joystick Gtr(-Y) | 053 | Time Machines | 085 | Square Wave Lead | 117 | Hubble Pad |
| 022 | Fretless Snap | 054 | Thumb Bass | 086 | Fretless Bass 2 | 118 | DynaThumb Bass |
| 023 | Overdrive BX3 | 055 | Dirty Jazz Organ | 087 | Old VOX Legend | 119 | "Flauto" Pipes |
| 024 | That Heaven Vibe | 056 | Lonley Sphinx | 088 | Freedom Pad | 120 | Noble Brite Pad |
| 025 | DynaBrassStereo2 | 057 | The Avenger | 089 | Classic Digi EP | 121 | Fugue In L.F.O. |
| 026 | Distant Lights | 058 | Bad Weather | 090 | Loonie Bin | 122 | Black Box |
| 027 | OohSlow VoicesST | 059 | Cript-on | 091 | ResRes One | 123 | Station Of Waves |
| 028 | Electro Lead | 060 | Thin AnaLead | 092 | Unholy Water | 124 | R&B Lead |
| 029 | WarPipes | 061 | Wacky A.Bass | 093 | Shirk Hit | 125 | Motion Ocean ! |
| 030 | Pods In The Pad | 062 | Virtual Traveler | 094 | Lava Lakes | 126 | Tight Brass 2 |
| 031 | Finger Cymbal | 063 | Digi-Bell | 095 | E.Bass Finger 3 | 127 | Moon Cycles |

## Programs — Bank G (General MIDI, PC 1-128)

| # | Nombre | # | Nombre | # | Nombre | # | Nombre |
|---|---|---|---|---|---|---|---|
| 001 | Acoustic Piano | 033 | Acoustic Bass | 065 | Soprano Sax | 097 | Ice Rain |
| 002 | Bright Piano | 034 | Fingered Bass | 066 | Alto Sax | 098 | Sound Track |
| 003 | El.Grand Piano | 035 | Picked Bass | 067 | Tenor Sax | 099 | Crystal |
| 004 | Honkey-Tonk | 036 | Fretless Bass | 068 | Baritone Sax | 100 | Atmosphere |
| 005 | Electric Piano 1 | 037 | Slap Bass 1 | 069 | Oboe | 101 | Brightness |
| 006 | Electric Piano 2 | 038 | Slap Bass 2 | 070 | English Horn | 102 | Goblins |
| 007 | Harpsichord | 039 | Synth Bass 1 | 071 | Bassoon | 103 | Echo Drops |
| 008 | Clavi. | 040 | Synth Bass 2 | 072 | Clarinet | 104 | Star Theme |
| 009 | Celesta | 041 | Violin | 073 | Piccolo | 105 | Sitar 1 |
| 010 | Glockenspiel | 042 | Viola | 074 | Flute | 106 | Banjo |
| 011 | Music Box | 043 | Cello | 075 | Recorder | 107 | Shamisen |
| 012 | Vibraphone | 044 | Contrabass | 076 | Pan Flute | 108 | Koto |
| 013 | Marimba | 045 | Tremolo Strings | 077 | Blown Bottle | 109 | Kalimba |
| 014 | Xylophone | 046 | Pizzicato Str. | 078 | Shakuhachi | 110 | Bagpipe |
| 015 | Tubular Bells | 047 | Orchestral Harp | 079 | Whistle | 111 | Fiddle |
| 016 | Santur | 048 | Timpani | 080 | Ocarina | 112 | Shanai |
| 017 | Drawbar Organ 1 | 049 | Strings | 081 | Detuned Square | 113 | Tinkle Bell |
| 018 | PercussiveOrgan1 | 050 | Slow Strings | 082 | Detuned Sawtooth | 114 | Agogo |
| 019 | Rock Organ | 051 | Synth Strings 1 | 083 | Synth Calliope | 115 | Steel Drums |
| 020 | Church Organ | 052 | Synth Strings 2 | 084 | Chiff Lead | 116 | Woodblock |
| 021 | Reed Organ | 053 | Choir Aahs 1 | 085 | Charang | 117 | Taiko |
| 022 | Accordion 1 | 054 | Voice Oohs | 086 | Air Voice | 118 | Melodic Tom 1 |
| 023 | Harmonica | 055 | Synth Vox | 087 | 5th Sawtooth | 119 | Synth Drum |
| 024 | Bandoneon | 056 | Orchestra Hit | 088 | Bass & Lead | 120 | Reverse Cymbal |
| 025 | Nylon Guitar 1 | 057 | Trumpet | 089 | Fantasia | 121 | Gtr.Fret Noise |
| 026 | Steel Guitar | 058 | Trombone 1 | 090 | Warm Pad | 122 | Breath Noise |
| 027 | Jazz Guitar | 059 | Tuba | 091 | Polyphonic Synth | 123 | Seashore |
| 028 | Clean Guitar | 060 | Muted Trumpet 1 | 092 | Space Voice | 124 | Bird Tweet 1 |
| 029 | Muted Guitar | 061 | French Horn | 093 | Bowed Glass | 125 | Telephone Ring 1 |
| 030 | Overdrive Guitar | 062 | Brass Section 1 | 094 | Metallic Pad | 126 | Helicopter |
| 031 | DistortionGuitar | 063 | Synth Brass 1 | 095 | Halo Pad | 127 | Applause |
| 032 | Guitar Harmonics | 064 | Synth Brass 2 | 096 | Sweep Pad | 128 | Gun Shot |
