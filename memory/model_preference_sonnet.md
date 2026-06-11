---
name: model_preference_sonnet
description: User prefers Sonnet model for investigations and debugging
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 25bf79fe-1338-4e89-8b41-0364711830f7
---

**Usar Sonnet (claude-sonnet-4-6) para sesiones futuras**

**Why:** Sonnet es más potente que Haiku para investigaciones de bugs complejos, auditoría de código, y análisis multi-archivo.

**How to apply:** 
- Cuando user abre sesión nueva en este proyecto → cambiar modelo a Sonnet antes de empezar trabajo
- Haiku es suficiente para: documentación, cambios simples, commits
- Sonnet necesario para: debugging profundo, auditoría protocolo, análisis arquitectura

**Decisión:** (2026-05-20 06:59) — User solicitó cambio a Sonnet para próxima sesión (bug S3 MIDI investigation requiere análisis profundo)
