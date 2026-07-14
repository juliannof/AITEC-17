// midi/TritonPatches.h — Korg Triton (Rack) Program/Combination name lookup (AITEC 2026-07-04)
// Fuente: docs/Korg_Triton_Patches_Verificado.md (VNL oficial + MIDI Implementation).
// Solo PRELOAD.PCG: INT-A..D + Bank G (GM) + Bank g(d) (GM Drums, 2026-07-13). NO usar otras fuentes.
#pragma once
#include <stdint.h>

// Nombre de Program para {msb, lsb, pc}. pc 0-based (0-127).
// Bancos: INT-A msb=0x00 lsb=0x00, INT-B lsb=0x01, INT-C lsb=0x02, INT-D lsb=0x03,
//         Bank G (GM) msb=0x79 lsb=0x00, Bank g(d) (GM Drums) msb=0x78 lsb=0x00 —
//         solo 9 PC tienen nombre (resto nullptr, TritonR_VNL_EFGJ1.pdf p.24-25).
// Devuelve nullptr si el banco es desconocido o el PC no tiene nombre asignado.
const char* tritonProgName(uint8_t msb, uint8_t lsb, uint8_t pc);

// Nombre de Combination para {msb, lsb, pc} — mismos MSB/LSB que Program (el modo
// Program/Combination se distingue con el SysEx MODE CHANGE, no por Bank Select,
// ver TRITON_Rack_MIDIimp.TXT Func 4E). Bancos: INT-A..D igual que tritonProgName.
const char* tritonCombiName(uint8_t msb, uint8_t lsb, uint8_t pc);
