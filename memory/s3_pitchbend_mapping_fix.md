---
name: s3_pitchbend_mapping_fix
description: S3 mapeo PitchBend — rango real Logic confirmado 0-14845 (MIDI monitor 2026-05-18 canal 2)
metadata: 
  node_type: memory
  type: project
  originSessionId: 032bcc47-508d-43b7-a406-08004fb4fbbb
---

## Rango Real de Logic — Confirmado por MIDI Monitor (2026-05-18)

Logic Pro NO usa el rango MIDI completo (0-16383). El span real es **14845** (raw unsigned).

| Posición | Monitor (signed) | Raw unsigned |
|----------|-----------------|--------------|
| Mínimo (fondo) | -8192 | 0 |
| Máximo (tope) | 6653 | 14845 |

**Constante en config.h:** `LOGIC_PITCHBEND_MAX = 14845`  
**Cálculo:** 6653 − (−8192) = 14845. Confirmado en canal 2 (2026-05-18 17:58:30).

**Why:** El valor 14848 que aparecía antes era una aproximación incorrecta (diferencia de 3 unidades). El MIDI monitor en canal 2 mide +6653 signed → raw = 6653 + 8192 = 14845.

## Mapeo Correcto (2026-05-18)

**Entrada (Logic → S2):** `processPitchBend` pasa `bendClamped` directamente a `setFaderTarget`.
- `setFaderTarget` mapea internamente: 0–14845 → ADC 0–27000 (o rango calibrado)
- NO pre-convertir en `processPitchBend` — doble conversión causaba valores desbocados

**Salida (S2 → Logic):**
- `(faderPos * LOGIC_PITCHBEND_MAX / 27000) & 0x3FFF` — usa constante config.h

**How to apply:** Usar siempre `LOGIC_PITCHBEND_MAX` (config.h S3) como span máximo. Nunca hardcodear 14848.
