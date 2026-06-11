# Display 16 Pistas en P4 — Arquitectura y Plan de Implementación
*(2026-06-11 02:50)*

---

## Contexto

P4 tiene una pantalla de 1024×600px — suficiente para mostrar 16 canales de 64px de ancho.

El sistema tiene dos superficies Mackie MCU:

| Dispositivo | Family | Tracks Logic | Bus RS485 | Slaves |
|---|---|---|---|---|
| S3 (Extender) | 0x15 | 1–8 | Bus B | S2 ×8 |
| P4 (Master MCU) | 0x14 | 9–16 | Bus A | S2 ×9 |

Logic envía los datos de cada superficie **únicamente** a su conexión USB-MIDI. P4 solo recibe tracks 9–16 de forma nativa. Para mostrar las 16 pistas en el display del P4 necesita también los datos de tracks 1–8 que Logic envía a S3.

---

## Solución: macOS como puente MIDI (sin hardware adicional)

```
Logic Pro
  │
  ├─ USB MIDI → S3  (tracks 1–8: VU, faders, nombres, estados)
  │                │
  │                └─ IAC Bus → Routing app → P4 USB MIDI (canal remapeado)
  │
  └─ USB MIDI → P4  (tracks 9–16: VU, faders, nombres, estados)
```

### IAC Bus + MIDI Routing en macOS

1. **Activar IAC Driver** en Audio MIDI Setup → MIDI Studio → IAC Driver → "Dispositivo" activo
2. **App de routing** (cualquiera de estas):
   - **MIDI Patchbay** (gratuito) — regla: fuente = S3 out, destino = P4 in, remapear canal MIDI 1→2
   - **MidiPipe** (gratuito) — pipe: input S3 → channel remapper → output P4
   - **Script Python** con `mido` / `rtmidi` si se prefiere automatizar al boot

### Remapeo de canal MIDI

S3 envía todos sus mensajes en **canal MIDI 1** (Channel Pressure, PitchBend, Note, CC).  
La app de routing los reenvía a P4 en **canal MIDI 2**.

P4 firmware distingue el banco por canal:

| Canal MIDI (0-indexed) | Fuente | Tracks Logic | Índice display |
|---|---|---|---|
| 0 (canal MIDI 1) | P4 nativo | 9–16 | display[8..15] |
| 1 (canal MIDI 2) | S3 via routing | 1–8 | display[0..7] |

---

## Cambios en P4 Firmware

### 1. Arrays globales — ampliar a 16 (main.cpp / config.h)

```cpp
// Antes:
float vuLevels[9]          = {};
float vuPeakLevels[9]      = {};
float faderPositions[9]    = {};
String trackNames[9];
bool recStates[8]          = {};
bool soloStates[8]         = {};
bool muteStates[8]         = {};
bool selectStates[8]       = {};
uint8_t vpotValues[8]      = {};

// Después:
float vuLevels[16]         = {};
float vuPeakLevels[16]     = {};
float faderPositions[16]   = {};
String trackNames[16];
bool recStates[16]         = {};
bool soloStates[16]        = {};
bool muteStates[16]        = {};
bool selectStates[16]      = {};
uint8_t vpotValues[16]     = {};
```

### 2. MIDIProcessor.cpp — processChannelPressure

```cpp
void processChannelPressure(byte channel, byte value) {
    int bankOffset = 0;
    int ch = channel;

    if (channel == 0) {
        // Banco P4 nativo (tracks 9-16 en Logic, índices 8-15 en display)
        bankOffset = 8;
        int targetChannel = (value >> 4) & 0x0F;
        if (targetChannel >= 8) return;
        targetChannel += bankOffset;
        // ... procesar igual que antes con targetChannel 8-15
    } else if (channel == 1) {
        // Banco S3 mirrorado (tracks 1-8 en Logic, índices 0-7 en display)
        bankOffset = 0;
        int targetChannel = (value >> 4) & 0x0F;
        if (targetChannel >= 8) return;
        // ... procesar igual con targetChannel 0-7
    } else if (channel >= 2 && channel <= 8) {
        // Formato alternativo S3 (canal por canal) — offset 0
        // targetChannel = channel - 2 + bankOffset
    }
}
```

### 3. MIDIProcessor.cpp — processPitchBend

```cpp
void processPitchBend(byte channel, int bendValue) {
    // canal 0-7: banco P4 → display index = channel + 8
    // canal 8-15 (si routing los reenvía): banco S3 → display index = canal - 8
    // Ajustar lógica de disconnect detection para solo banco propio
}
```

### 4. MIDIProcessor.cpp — processNote (rec/solo/mute/select)

Notas 0–31 codifican group+track para los 8 canales del banco.  
Con routing, el canal MIDI 2 indica banco S3 → añadir offset 0 (o usar el canal para distinguir).

### 5. MIDIProcessor.cpp — processControlChange (VPot, nombres)

- CC 48–55: VPot para 8 canales del banco activo
- SysEx 0x12: nombres de pista (offset 0–55 para 8 tracks × 7 chars)

Ambos necesitan el offset de banco según el canal MIDI del mensaje origen.

### 6. Desconexión (processPitchBend)

La detección de desconexión (9 faders a 0 en <150ms) debe operar **solo sobre el banco propio (P4)**, no sobre los datos mirrorados de S3.

---

## Cambios en UIPage3 (Display)

### Layout 16 columnas

```
#define NUM_CH   16
#define CH_W     (P4_W / NUM_CH)   // 64px por canal
```

Con 64px por canal el layout actual (arco panorama 52px) es demasiado ancho.  
Elementos a rediseñar para 64px:

| Elemento | Actual | 64px |
|---|---|---|
| Botón S / M | 112px ancho | 54px (CH_W - 10) |
| Arc panorama | 52px | 40px (reducir PAN_SZ) |
| Nombre pista | texto centrado | fuente 10–12pt |
| VU segments | 112px ancho | 54px |

### Separador visual entre bancos

Línea vertical entre columna 7 y columna 8 para indicar el límite S3/P4.

### Orden visual de las 16 columnas

```
[0..7]  = tracks 1–8  (banco S3, datos via routing)
[8..15] = tracks 9–16 (banco P4, datos nativos)
```

---

## Plan de Implementación

```
Fase 1 — macOS routing (validación, sin tocar firmware)
  [ ] Activar IAC Bus en Audio MIDI Setup
  [ ] Configurar MIDI Patchbay: S3 out → P4 in, canal 1→2
  [ ] Verificar en MIDI Monitor que P4 recibe ambos bancos
  [ ] Confirmar que S3 sigue funcionando (routing no debe interrumpir S3)

Fase 2 — Firmware P4: ampliar arrays y procesar banco S3
  [ ] Ampliar arrays a 16 en main.cpp
  [ ] processChannelPressure: distinguir canal 0 (P4) vs canal 1 (S3)
  [ ] processPitchBend: offset por banco
  [ ] processNote: offset por banco (rec/solo/mute/select)
  [ ] processControlChange: offset VPot
  [ ] SysEx 0x12 (nombres): offset por banco
  [ ] Proteger disconnect detection para solo banco P4

Fase 3 — UIPage3: rediseño 16 columnas
  [ ] NUM_CH = 16, CH_W = 64
  [ ] Ajustar tamaños de elementos al nuevo ancho
  [ ] Separador visual entre bancos (columna 7→8)
  [ ] Etiquetas de banco opcionales (1-8 / 9-16)
```

---

## Notas y Riesgos

- **S3 no se modifica** — solo su salida MIDI se mirroriza en macOS
- **Routing debe arrancar automáticamente con Logic** — usar app con autostart o Login Items
- **Latencia**: IAC Bus añade <1ms, despreciable para display
- **Si routing no está activo**: P4 muestra solo sus 8 canales (tracks 9–16), columnas 0–7 en blanco. Sistema sigue operativo.
- **SysEx nombres (0x12)** es la parte más delicada del routing — los offsets de carácter deben trasladarse correctamente al banco S3
- **Disconnect detection** — crítico: no debe dispararse por datos del banco S3 (faders S3 a 0 no indican desconexión de P4)
