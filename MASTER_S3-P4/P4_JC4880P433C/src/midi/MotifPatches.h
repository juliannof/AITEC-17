// midi/MotifPatches.h — Yamaha MOTIF-RACK Normal Voice name lookup (AITEC 2026-07-12)
// Fuente: docs/Yamaha_MOTIF-RACK_Normal_Voice_List.md (Data List oficial, sin Multi/Drum).
#pragma once
#include <stdint.h>

// Nombre de Normal Voice para {msb, lsb, pc}. pc 0-based (0-127).
// Bancos: Preset1..5 msb=0x3F lsb=0x00..0x04, GM msb=0x00 lsb=0x00,
//         User1 msb=0x3F lsb=0x08, User2 msb=0x3F lsb=0x09 (copia de Preset5 de fábrica).
// msb=0x3F = 63 decimal (MOTIFRACKE2.pdf p.46, tabla Bank Select) — NO 0x63
// (99 decimal, bug corregido 2026-07-12).
// Devuelve nullptr si el banco es desconocido.
const char* motifProgName(uint8_t msb, uint8_t lsb, uint8_t pc);
