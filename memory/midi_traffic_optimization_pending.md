---
name: midi_traffic_optimization_pending
description: Optimización tráfico MIDI — reducir mensajes innecesarios (2026-05-14 17:08)
metadata: 
  node_type: memory
  type: project
  originSessionId: 123ee23e-1e1b-4cda-a9a4-cb81e247181f
---

## Problema: Tráfico MIDI Excesivo

**Síntoma:** Con 17 faders enviando PitchBend cada ~20ms
- 17 faders × 50 updates/s = **850 mensajes MIDI/s**
- Datos actuales: -8180 constante cada 20ms (sin cambio)
- Impacto: Congestión USB, latencia, posible pérdida de mensajes

**Causa:** MIDIProcessor.cpp línea 603
```cpp
if (abs((int16_t)fader14bit - lastSentPitchBend[channel]) > PITCHBEND_DEADBAND) {
    rs485.setFaderTarget(channel + 1, fader14bit);
    lastSentPitchBend[channel] = (int16_t)fader14bit;
}
```

El PITCHBEND_DEADBAND actual es **80** cuentas. Con valores constantes (-8180), después del EMA filter no hay cambios > 80, pero igualmente se está enviando.

## Solución Propuesta

**Aumentar PITCHBEND_DEADBAND:** De 80 → 150-200 cuentas
- Filtra cambios menores en el rango de Logic (0-14848)
- Reduce tráfico MIDI en reposo (cuando fader no se mueve)
- Sin afectar responsividad a movimientos reales

**Ubicación:** MIDIProcessor.cpp línea 28
```cpp
static const int16_t PITCHBEND_DEADBAND = 150;  // antes: 80
```

**Impacto esperado:**
- Reduce 850 msgs/s → ~200-300 msgs/s (solo cambios > 150 cuentas)
- Mantiene responsividad (cambios > 150 se envían inmediatamente)
- NO afecta suavidad (EMA filter ya está filtrando)

## Validación Requerida

- [ ] Compilar con PITCHBEND_DEADBAND = 150
- [ ] Deploy en hardware
- [ ] Verificar con MIDI monitor: valores constantes NO se repiten
- [ ] Verificar movimiento suave del fader (sin "zonas muertas")
- [ ] Medir tráfico MIDI (debería bajar significativamente)

## Criterio de Éxito

- ✅ Tráfico MIDI reducido 60-70%
- ✅ Fader sigue siendo responsivo
- ✅ Sin oscilaciones ±1
- ✅ Sin sensación de "lag" al mover fader

## ESPECIFICACIÓN PARA IMPLEMENTAR MAÑANA

**Ver CHANGELOG.md para especificación completa con código exacto**

Resumen rápido:
1. Agregar `static uint16_t lastSentPb[9] = {0};` en main.cpp
2. Envolver `sendMIDIBytes()` en `if (pb != lastSentPb[slaveId])`
3. Guardar: `lastSentPb[slaveId] = pb;`

Impacto: 850 msgs/s → <100 msgs/s, sin pérdida de resolución

**Commit:** 1e721aa — Especificación detallada en CHANGELOG
