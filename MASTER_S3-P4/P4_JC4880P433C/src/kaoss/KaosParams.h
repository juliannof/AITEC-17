// kaoss/KaosParams.h — Catálogo de parámetros MIDI nombrados por synth (AITEC 2026-07-14)
// Fuente: aitec_kaos_brief_2026-07-13.md §4. Un CC = un parámetro con nombre corto,
// independiente del eje (X/Y) — el usuario compone la pareja desde el editor en
// pantalla (UIKaosEdit), no vienen pre-empaquetados en "memorias" fijas.
#pragma once
#include <stdint.h>
#include "../config.h"

struct KaosParam {
    uint8_t     cc;
    const char* name;
};

// Lista de parámetros nombrados del synth. count=0 y puntero nullptr si el
// synth no tiene catálogo verificado todavía (TG55, D-110 — brief §4: 0/10).
const KaosParam* kaosParamList(ExSynth synth, uint8_t& count);

// Nombre del parámetro para un CC ya guardado en un slot (puede no estar en
// el catálogo actual si se cambió de synth o se guardó a mano) — nullptr si
// no se encuentra.
const char* kaosParamName(ExSynth synth, uint8_t cc);
