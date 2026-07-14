// nvs/KaosStore.h — Almacén NVS de memorias Kaos por synth (AITEC 2026-07-14)
// kaos_slot[synth_id][slot_index] = {ccX, ccY} — aitec_kaos_brief_2026-07-13.md §2.
// 20 slots por synth (mapeo físico NeoTrellis L3/L7/L11/L15 + R0-R15, ver
// docs NeoTrellis). Mismo patrón que FavStore.cpp (Preferences, bytes crudos).
//
// Canal MIDI es ÚNICO POR SYNTH, no por slot (corrección 2026-07-14 — "el
// canal midi es unico para el sinte, no por preset") — cada sintetizador del
// rack escucha en un canal fijo independientemente de qué memoria Kaos esté
// activa. Se guarda aparte con kaosLoadChannel()/kaosSaveChannel().
#pragma once
#include <stdint.h>
#include "../config.h"   // ExSynth

#define KAOS_SLOTS 20

struct KaosSlot {
    uint8_t ccX;
    uint8_t ccY;
    uint8_t configured;   // 0 = slot vacío/sin configurar, 1 = tiene datos
};

// Llama una vez en setup() — abre el namespace NVS y siembra el catálogo
// verificado (aitec_kaos_brief_2026-07-13.md §4) la primera vez que arranca
// (flag "seeded"), como valores iniciales editables desde UIKaosEdit.
bool kaosInit();

// false si el slot está vacío (configured==0) — out queda {0,0,0} en ese caso.
bool kaosLoad(ExSynth synth, uint8_t slot, KaosSlot& out);
bool kaosSave(ExSynth synth, uint8_t slot, const KaosSlot& s);

// Canal MIDI del synth (1-16) — un único valor por synth, no por slot.
// kaosLoadChannel() devuelve 1 si nunca se guardó (no hay "vacío", siempre
// hay un canal válido).
uint8_t kaosLoadChannel(ExSynth synth);
bool    kaosSaveChannel(ExSynth synth, uint8_t ch);
