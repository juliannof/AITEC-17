// midi/JVPatches.h — Roland JV-2080 patch name lookup  (AITEC 2026-06-30)
// Fuente: Roland_JV-2080_Patches_Verificado.md (cruzado Owner's Manual p.176-179
//         + Roland Faxback #10454). NO usar otras fuentes.
#pragma once
#include <stdint.h>

// Devuelve el nombre del patch para la combinación {msb, lsb, pc}.
// pc es 0-based (0-127). Devuelve nullptr si el banco es desconocido.
//
// Bancos soportados:
//   USER  MSB=0x50 LSB=0
//   PR-A  MSB=0x51 LSB=0
//   PR-B  MSB=0x51 LSB=1  (slots 126-127 sin nombre → nullptr)
//   PR-C  MSB=0x51 LSB=2
//   PR-D  MSB=0x51 LSB=3  (GM)
//   PR-E  MSB=0x51 LSB=4
const char* jvPatchName(uint8_t msb, uint8_t lsb, uint8_t pc);

// Nombre corto del banco para display (max 5 chars).
const char* jvBankLabel(uint8_t msb, uint8_t lsb);
