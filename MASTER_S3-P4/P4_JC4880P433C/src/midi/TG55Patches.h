// midi/TG55Patches.h — Yamaha TG55 Preset Voice name lookup (AITEC 2026-07-12)
// Fuente: docs/Yamaha_TG55_Brief_Implementacion.md §8 (TG55G.pdf p.12).
// Solo PRESET (64 voces, ROM) — INTERNAL/CARD son RAM editable sin nombres
// de fábrica, no hay nada que verificar ahí.
#pragma once
#include <stdint.h>

// Nombre de Preset Voice para pc (0-based, 0-63 → P01-P64). msb/lsb se
// ignoran — no existe Bank Select verificado para TG55 (ver doc §7), se
// usan solo como firma común con el resto de *ProgName(). Devuelve nullptr
// si pc >= 64.
const char* tg55ProgName(uint8_t msb, uint8_t lsb, uint8_t pc);
