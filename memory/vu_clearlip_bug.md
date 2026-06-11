---
name: vu_clearclip_bug
description: VU P4 — clearClip (0xF) sobreescribía vuLevels a 0 por P4_CH_OFFSET incorrecto — RESUELTO commit e206d9f
metadata: 
  node_type: memory
  type: project
  originSessionId: 38fff52b-f664-4748-acd1-10e96fd8d0f7
---

Bug VU P4: barra caía a cero en subidas abruptas. Resuelto 2026-06-11, commit `e206d9f`.

**Causa raíz:** `MIDIProcessor.cpp` línea 292:
```cpp
case 0x0F: clearClip = true; normalizedLevel = vuLevels[targetChannel]; break;
```
`targetChannel` = 0–7 (sin `P4_CH_OFFSET=8`). `vuLevels[0–7]` siempre 0 → clearClip → `vuLevels[dispCh] = 0` → barra a cero.

**Por qué "solo en la subida":** transitorios rápidos clipan (Logic envía 0xE) e inmediatamente clearClip (0xF). Subidas lentas no generan clip, no hay 0xF, no hay bug.

**Fix:** clearClip en rama propia, solo actualiza `vuClipState`. No toca nivel, timestamp ni peak.

**Why:** MIDI Monitor fue clave — los datos reales de Logic mostraron el patrón clip/clearClip. Sin datos reales, imposible diagnosticar. [[midi_monitor_tool]]

**How to apply:** En cualquier procesado de Channel Pressure MCU, P4_CH_OFFSET debe aplicarse antes de leer arrays de estado. clearClip nunca debe actualizar el nivel del VU.
