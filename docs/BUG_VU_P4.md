# BUG: VU Meter P4 — LEDs se apagan durante playback (2026-06-11)

## Síntoma
Durante playback en Logic Pro, los LEDs del VU meter (UIPage3, canales 8-15) se apagan de forma incorrecta:
- En subidas abruptas: LEDs inferiores parpadean / se apagan brevemente
- Con señal sostenida o descendente: la barra no sigue la señal hacia abajo como debería
- El comportamiento varía según los intentos de fix — ninguno ha resuelto ambos casos

## Comportamiento esperado
- Bar = nivel instantáneo de Logic (sube y baja con la señal)
- Peak dot = máximo retenido ~1s, luego fade
- Al parar Logic: bar decae a 0 gradualmente

## Comportamiento S2 (referencia que funciona)
S2 recibe VU vía RS485 de S3 cada ~20ms de forma continua.  
`vuLastUpdateTime` se refresca constantemente → decay nunca dispara durante playback.  
La diferencia arquitectónica clave: **S3 envía el mismo valor continuamente**, P4 recibe Channel Pressure a cadencia desconocida de Logic.

## Código relevante

### MIDIProcessor.cpp — processChannelPressure (estado actual 2bc0079)
```cpp
if (normalizedLevel > 0.0f) vuLastUpdateTime[dispCh] = millis();
// vuLevels bidireccional (sigue Logic en ambas direcciones)
if (normalizedLevel != vuLevels[dispCh]) {
    vuLevels[dispCh] = normalizedLevel;
    stateChanged = true;
}
if (normalizedLevel > vuPeakLevels[dispCh]) {
    vuPeakLevels[dispCh] = normalizedLevel;
    vuPeakLastUpdateTime[dispCh] = millis();
    stateChanged = true;
}
```

### UIPage3.cpp — handleVUMeterDecay (estado actual)
```cpp
void handleVUMeterDecay() {
    const unsigned long DECAY_INTERVAL_MS = 300;
    const unsigned long PEAK_HOLD_TIME_MS = 1000;
    const unsigned long PEAK_FADE_STEP_MS = 25;
    const float         DECAY_AMOUNT      = 1.0f / 12.0f;
    const uint8_t       FADE_STEP         = 64;
    // decay dispara si now - vuLastUpdateTime[i] > DECAY_INTERVAL_MS
```

### Formato Channel Pressure MCU (Mackie)
```
MIDI status: 0xD0 (channel 0)
Value byte:  upper nibble = strip (0-7), lower nibble = level (0-12, 0xE=clip, 0xF=clear clip)
```
P4 aplica `P4_CH_OFFSET=8` → strips 0-7 → display slots 8-15.

## Intentos fallidos (sesión 2026-06-11)

| Commit | Cambio | Resultado |
|--------|--------|-----------|
| `b637bd2` | DECAY_INTERVAL 100→500ms | Sigue apagándose |
| `f51621d` | g_isPlaying via Note 0x5E | Logic no envía 0x5E fiablemente |
| `3b90268` | vuLastUpdateTime incondicional | "Todos apagados menos el activo" |
| `cbd1b0f` | vuLevels bidireccional | Peor |
| `d20ecf8` | Decay solo si DISCONNECTED | Usuario revirtió |
| `378c9d1` | REVERT a lógica original | Back to start |
| `2be8868` | DECAY_INTERVAL 100→300ms | Sigue en subidas abruptas |
| `f440b36` | Timestamp incondicional + sin snap-a-0 | Bajada eterna |
| `0e6ba13` | Timestamp solo si nivel > 0 | Sube bien, se queda arriba |
| `2bc0079` | vuLevels bidireccional + peak independiente | Pendiente test |

## Hipótesis principal sin confirmar
**Cadencia de Channel Pressure de Logic desconocida.**  
Toda la lógica de decay está basada en suposiciones sobre cuándo Logic envía y a qué rate.  
Sin datos reales de MIDI Monitor no es posible calibrar correctamente.

## Análisis MIDI Monitor (2026-06-11 16:43)

**Canal:** todos los mensajes en "Channel 1" de MIDI Monitor = byte 0xD0 = canal 0 en código ✓

**Formato confirmado:** `value = (strip << 4) | level`
- `3` = 0x03 → strip 0, level 3
- `21` = 0x15 → strip 1, level 5
- `116` = 0x74 → strip 7, level 4

**Cadencia:** bursts de hasta 8 mensajes (uno por strip activo) cada ~15-30ms variable.

**CRÍTICO — strips silenciosos:** Logic NO manda level=0 explícitamente. Simplemente **deja de incluir ese strip en el burst**. Cuando un canal se silencia, desaparece de los mensajes.

**Implicación para el código:** timestamp-based decay funciona correctamente:
- Strip activo → messages llegan cada <30ms → timestamp fresco → decay no dispara
- Strip silenciado → messages dejan de llegar → timestamp se congela → decay dispara a los 300ms

**Estado del código en commit 2bc0079:**
- `vuLevels` bidireccional (sigue Logic en ambas direcciones) ✓
- `vuLastUpdateTime` actualizado solo si `normalizedLevel > 0` ✓
- `DECAY_INTERVAL_MS = 300ms` — mayor que la cadencia máxima de Logic (30ms) ✓

**Conclusión:** la arquitectura del commit 2bc0079 es correcta para los datos reales de Logic.

## Resolución (2026-06-11)

**Causa raíz confirmada:** línea 292 de `MIDIProcessor.cpp`:
```cpp
case 0x0F: clearClip = true; normalizedLevel = vuLevels[targetChannel]; break;
```
`vuLevels[targetChannel]` usa índice 0–7 (sin `P4_CH_OFFSET`), que siempre es 0.0f. Al recibir clearClip, `normalizedLevel=0` → `vuLevels[dispCh]=0` → barra a cero.

**Por qué "solo en la subida":** Fast transients clipan brevemente → Logic envía 0xE (clip) seguido de 0xF (clearClip) en ms → el clearClip mataba el nivel. Subidas lentas nunca clipan, no hay 0xF, no hay bug.

**Fix aplicado:** clearClip entra en rama propia, solo actualiza `vuClipState`. No toca `vuLevels`, `vuLastUpdateTime` ni peak.

## Archivos implicados
- `MASTER_S3-P4/P4_JC1060P470C/src/midi/MIDIProcessor.cpp` — processChannelPressure (~línea 278)
- `MASTER_S3-P4/P4_JC1060P470C/src/display/UIPage3.cpp` — handleVUMeterDecay (~línea 287) + vu_draw_cb (~línea 234)
- `MASTER_S3-P4/P4_JC1060P470C/src/config.h` — arrays vuLevels, vuPeakLevels, vuLastUpdateTime (extern)
- `MASTER_S3-P4/P4_JC1060P470C/src/main.cpp` — definición arrays VU
