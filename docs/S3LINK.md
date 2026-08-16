# S3LINK — Enlace serie punto a punto S3 ↔ P4

**Estado:** implementado y validado en banco (2026-08-16).

---

## 1. Propósito

El S3 (Extender) y el P4 (Master) son dos masters MCU/RS485 completamente
independientes (ver `docs/RS485.md` y `docs/RS485_P4.md`) — cada uno recibe
MIDI de Logic Pro por su propio puerto USB-MIDI y controla su propio bus de
slaves S2. Antes de esta sesión no existía ningún enlace directo entre ambos.

`S3LINK` añade un canal serie dedicado, ajeno a los buses RS485 propios de
cada MCU, con dos funciones:

- **S3 → P4:** el S3 reenvía el estado de sus 8 canales (nombre de pista,
  REC/MUTE/SOLO/SELECT, vúmetro) para que el P4 los muestre en su pantalla
  junto a sus propios 9 canales — columnas 0-7 de `UIPage3.cpp`, reservadas
  desde antes por `P4_CH_OFFSET=8` pero nunca alimentadas hasta ahora.
- **P4 → S3:** heartbeat — el P4 envía PING periódico y usa la ausencia de
  PONG para saber que el S3 ha dejado de responder.

## 2. Transporte

UART físico nuevo (`Serial2`), independiente del `Serial1` que cada MCU ya
usa para su propio bus RS485.

| | TX | RX | Baud |
|---|---|---|---|
| **P4** (`P4_JC1060P470C/src/config.h`) | GPIO1 | GPIO2 | 115200 |
| **S3** (`S3/iMakie-ESP32_S3_EXTENDER/src/config.h`) | GPIO18 | GPIO17 | 115200 |

Cableado cruzado: P4 TX(1)→S3 RX(17), S3 TX(18)→P4 RX(2), + GND común entre
placas.

**Pines descartados durante el diseño** (documentado para no repetir el
mismo error):
- S3 GPIO37 — reservado por la PSRAM Octal del módulo (`board_build.arduino.memory_type = qio_opi`
  en `platformio.ini`); GPIO33-37 están conectados internamente al chip de
  PSRAM y no son GPIO de propósito general.
- S3 GPIO1 — ya usado por `RS485_ENABLE_PIN` del bus propio S3↔S2.

## 3. Protocolo (`s3_link_protocol.h`, duplicado en ambos proyectos)

Mismo patrón que `protocol.h` (RS485): CRC8 idéntico polinomio, structs
`__attribute__((packed))` con `static_assert` de tamaño.

```cpp
#define S3LINK_START         0xA5
#define S3LINK_TYPE_CHANNEL  0x01   // S3 → P4
#define S3LINK_TYPE_PING     0x02   // P4 → S3
#define S3LINK_TYPE_PONG     0x03   // S3 → P4

struct S3LinkChannelFrame {   // 13 bytes
    uint8_t  start;           // S3LINK_START
    uint8_t  type;            // S3LINK_TYPE_CHANNEL
    uint8_t  channel;         // 0-7
    char     trackName[7];    // Mackie Scribble Strip, sin null (igual que MasterPacket.trackName)
    uint8_t  flags;           // FLAG_REC | FLAG_SOLO | FLAG_MUTE | FLAG_SELECT (protocol.h)
    uint8_t  vuLevel;         // 0-127
    uint8_t  crc;             // CRC8 sobre [type..vuLevel]
};

struct S3LinkPingPongFrame {  // 3 bytes
    uint8_t start;
    uint8_t type;              // PING o PONG
    uint8_t crc;                // CRC8 sobre [type]
};
```

El payload de `S3LinkChannelFrame` reutiliza exactamente el formato que el
S3 ya usa para sus propios S2 (`MasterPacket.trackName[7]` + bitmask de
flags) — no se inventó codificación nueva.

## 4. Módulo `S3Link` (`S3Link/S3Link.h` + `.cpp`, una copia por MCU)

### Lado S3 (emisor de datos, receptor de heartbeat)

- `begin()` — abre `Serial2` en TX18/RX17 @115200.
- `setTrackName(ch, name)` / `setFlags(ch, flags)` / `setVuLevel(ch, value)`
  — cachean el campo por canal (`ChCache _ch[8]`) y disparan el envío de la
  trama completa del canal. `setTrackName`/`setFlags` envían inmediato
  (eventos raros); `setVuLevel` throttla a máx. 1 envío/30ms por canal
  (evita saturar el UART con el ritmo de Channel Pressure de Logic).
- `update()` — procesa bytes entrantes; si es `PING` responde `PONG`
  inmediatamente.

Enganchado en `midi/MIDIProcessor.cpp` junto a las llamadas ya existentes a
`rs485.setTrackName/setFlags/setVuLevel` (mismo punto de disparo, mismo
dato, sin duplicar lógica de parseo MIDI):

| Función | Línea aprox. | Hook |
|---|---|---|
| `tickTrackNameDebounce()` | ~101 | `s3Link.setTrackName(t, _pendingName[t])` |
| `processChannelPressure()` (canal 0, nibble) | ~319 | `s3Link.setVuLevel(targetChannel, vuLevel7bit)` |
| `processChannelPressure()` (canal 1-7) | ~325 | `s3Link.setVuLevel(targetChannel, value)` |
| `processNote()` (REC/SOLO/MUTE/SELECT) | ~601 | `s3Link.setFlags(track_idx, flags)` — enmascara bits de AutoMode internamente |

### Lado P4 (receptor de datos, emisor de heartbeat)

- `begin()` — abre `Serial2` en TX1/RX2 @115200.
- `update()` — procesa bytes entrantes:
  - `S3LINK_TYPE_CHANNEL` → escribe directo en `trackNames/recStates/soloStates/muteStates/selectStates/vuLevels[0..7]`
    (los índices que `UIPage3.cpp` ya dibuja) y dispara
    `needsButtonsRedraw`/`needsVUMetersRedraw`. VU se normaliza a float
    0.0-1.0 igual que `processChannelPressure()` propio del P4 (mismo
    tratamiento de `vuLastUpdateTime`/`vuPeakLevels` para que el decay de
    `handleVUMeterDecay()` funcione igual en los canales 0-7 que en los 8-15).
  - Cualquier trama válida (canal o PONG) actualiza `_lastRxMs`.
  - Envía `PING` cada `S3LINK_HEARTBEAT_MS` (500ms).
  - `g_s3Connected = (millis() - _lastRxMs) < S3LINK_TIMEOUT_MS` (1500ms).

`g_s3Connected` (`extern volatile bool`, definida en `S3Link.cpp`,
declarada en `config.h`) queda disponible para el resto del firmware pero
**no se consume todavía en la UI** — pendiente de una futura iteración
(atenuar visualmente las columnas 0-7 en `UIPage3.cpp` cuando el S3 se
cae). Ver `CHANGELOG.md`.

## 5. Diagnóstico de heartbeat (temporal, activo)

`S3Link::update()` en el P4 imprime cada 2s:

```
[S3LINK] connected=%d ping=%lu pong=%lu lastRx=%lums
```

Contadores `_pingSentCount`/`_pongRecvCount` — si suben en paralelo 1:1, el
heartbeat es sano. Si `pong` se queda parado mientras `ping` sigue subiendo,
falta el enlace físico o el S3 no está respondiendo.

## 6. Validación en banco (2026-08-16)

- Boot OK en ambos lados: `S3Link OK — TX:18 RX:17` (S3), `S3Link OK — TX:1 RX:2` (P4).
- Heartbeat confirmado sin pérdidas tras ~4.7 min de uptime:
  `[S3LINK] connected=1 ping=563 pong=563 lastRx=88ms`.
- Nota de cableado: el pinout final usado fue `S3LINK_TX_PIN=1` / `S3LINK_RX_PIN=2`
  en el P4 (el valor original del diseño) — un intento intermedio con los
  pines intercambiados fue una corrección de prueba que se descartó; el
  cableado físico coincidía con el plan original.
- Flujo de datos de canal (nombre/REC/MUTE/SOLO/SELECT/VU en columnas 0-7
  de la pantalla del P4) confirmado por el usuario en banco.

## 7. Pendiente

- Consumir `g_s3Connected` en `UIPage3.cpp` para indicar visualmente que el
  S3 no responde (columnas 0-7 atenuadas/grises).
- Quitar o reducir el log de diagnóstico de heartbeat una vez el sistema
  lleve tiempo estable en producción (`S3Link.cpp` del P4, comentario
  "quitar tras validar en banco").
