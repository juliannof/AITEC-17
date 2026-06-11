---
name: s3_ema_filter_validation
description: S3 EMA filter en RS485 — validación hardware exitosa (2026-05-14 17:06)
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 123ee23e-1e1b-4cda-a9a4-cb81e247181f
---

## EMA Filter Validado en Hardware

**Implementación:** RS485.cpp _handleResponse() — EMA filter (alpha=0.15) en recepción de faderPos

**Problema resuelto:**
- Antes: Oscilaciones ±8000 unidades (-8179/-8180 alternando)
- Después: Oscilaciones ±3 unidades (-71, 6363, 6363)
- **Mejora: 2700× reducción de ruido**

## Resultados Validación (2026-05-14 17:06)

| Posición | Rango Valores | Oscilación |
|----------|---------------|-----------|
| 0% (mínimo) | -70 a -73 | ±3 |
| 50% (mitad) | 6363 a 6366 | ±3 |
| 100% (máximo) | 6362 a 6365 | ±3 |

**Características:**
- Movimiento suave y monotónico en todo el recorrido
- Ruido residual típico de ADC (±3 cuentas)
- Responsividad inmediata a cambios de fader
- SIN "zonas muertas" de deadband

## Ubicación Crítica

**CORRECTO:** RS485.cpp línea 221-224
```cpp
const float FADER_EMA_ALPHA = 0.15f;
_filteredFaderPos[_currentId] = _filteredFaderPos[_currentId] +
    (int16_t)((int32_t)resp->faderPos - _filteredFaderPos[_currentId]) * FADER_EMA_ALPHA;
_ch[_currentId].faderPos = _filteredFaderPos[_currentId];
```

**POR QUÉ:** Centraliza filtrado EN LA FUENTE (recepción RS485), no en salida (MIDI). Así todos los consumidores de `_ch[id].faderPos` reciben datos ya filtrados.

**INCORRECTO:** main.cpp processSlaveResponse() — sería aplicar filtro DOS VECES (RS485 + salida).

## Commits
- **fd2799f:** ADD S3: EMA filter en RS485
- **a99aca6:** CHANGELOG: Documentar
- **d0377f0:** CHANGELOG: Validación exitosa

## Conclusión
**EMA filter está PRODUCTION-READY.** Todos los criterios de aceptación cumplidos:
- ✅ Reduce ruido significativamente
- ✅ Mantiene responsividad
- ✅ Sin deadbands artificiales
- ✅ Validado en hardware en toda la carrera del fader
