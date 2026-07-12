# aitec — Yamaha TG55: Brief de Implementación

**Fuente:** Yamaha TG55G.pdf (manual oficial escaneado, 147 páginas + addendum).
Todos los datos extraídos visualmente página por página. Nada de memoria ni fuentes externas.

---

## 1. Identidad MIDI

| Campo | Valor |
|---|---|
| Fabricante | Yamaha (`43H` en SysEx) |
| Group/Model ID | `35H` (TG55 / SY55) |
| Device Number | configurable 0–15, default `0` → byte `1nH` con `n=0` → `10H` |

---

## 2. SysEx — Parameter Change

### Formato

```
F0 43 1n 35 [struct_MSB] [struct_LSB] 00 [param_n2] 00 [value] F7
```

| Byte | Campo | Notas |
|---|---|---|
| `F0` | SysEx Start | |
| `43` | Yamaha ID | fijo |
| `1n` | Sub-status + device number | `n` = 0–15 |
| `35` | Group/Model ID TG55 | equivalente al `16H` del D-110 |
| struct_MSB | Tipo de estructura (t) | ver tabla de tipos |
| struct_LSB | Sub-índice (f, ee, canal...) | depende del tipo |
| `00` | param MSB | siempre 0 en Parameter Change |
| param_n2 | Número de parámetro | ver tablas por tipo |
| `00` | value MSB | siempre 0 salvo parámetros 16-bit |
| value | Valor | rango según parámetro |
| `F7` | SysEx End | |

**Sin checksum.** El checksum (complemento a 2 de la suma de 7 bits de los bytes de datos) solo se usa en Bulk Dump, no en Parameter Change.

### Tabla de tipos de estructura (struct_MSB)

| struct_MSB | Tipo | struct_LSB | Ref. |
|---|---|---|---|
| `00H` | Multi Common | `00H` | Chart 1 |
| `01H` | Multi Each Voice | `t2` = canal (0–15) | Chart 1 |
| `02H` | Voice Common | `00H` | Chart 2 |
| `03H` | Voice Each Element | `t2` = element (00/01/10/11) | Chart 2 |
| `04H` | Drum Set Voice | `t2` = MIDI note number | Chart 3 |
| `07H` | AWM Element | `t2` = element (00/01/10/11) | Chart 4 |
| `08H` | Effect | `00H` | Chart 5 |
| **`09H`** | **Filter** | **`0feee000B`** (ver abajo) | **Chart 6** |
| `0DH` | Switch Remote | `00H` | Chart 7 |
| `0FH` | System | `00H` | Chart 8 |

---

## 3. Filter — estructura del struct_LSB

Para struct_MSB = `09H`:

```
struct_LSB = 0 f ee 0000 B
             │ │ ││
             │ │ └┘ element number: 00=elem0, 01=elem1, 10=elem2, 11=elem3
             │ └─── filter number:  0=Filter1, 1=Filter2
             └───── siempre 0
```

Ejemplos:

| Filtro | Elemento | struct_LSB |
|---|---|---|
| Filter 1 | Elem 0 | `00H` |
| Filter 1 | Elem 1 | `04H` |
| Filter 1 | Elem 2 | `08H` |
| Filter 1 | Elem 3 | `0CH` |
| Filter 2 | Elem 0 | `40H` |
| Filter 2 | Elem 1 | `44H` |
| Filter 2 | Elem 2 | `48H` |
| Filter 2 | Elem 3 | `4CH` |
| (don't care = Filter Common) | — | cualquier valor con `f=don't care` |

---

## 4. Parámetros de filtro verificados

### Chart 6 — Filter 1 & 2 (struct_MSB = `09H`)

| n2 | Función | Rango | Notas |
|---|---|---|---|
| `00H` | Filter Type | 0–2 | 0:THR, 1:LPF, 2:HPF (HPF solo en Filter 1) |
| **`01H`** | **Cut-off Frequency** | **0–127** | |
| `02H` | Filter Mode | 0–2 | 0:EG, 1:LFO, 2:EGUA |
| `03H` | Key-on Rate 1 | 0–63 | |
| `04H` | Key-on Rate 2 | 0–63 | |
| `05H` | Key-on Rate 3 | 0–63 | |
| `06H` | Key-on Rate 4 | 0–63 | |
| `07H` | Key-off Rate 1 | 0–63 | |
| `08H` | Key-off Rate 2 | 0–63 | |
| `09H`–`0DH` | Key-on Cut-off Level 0–4 | 0–127 | rango real –64 ~ +63 |
| `0EH`–`0FH` | Key-off Cut-off Level 1–2 | 0–127 | rango real –64 ~ +63 |
| `10H` | Rate Scaling | 0–15 | 0–7: 0~+7, 8–15: 0~–7 (bit3=signo) |
| `11H`–`14H` | C_off_lvl Scaling Break Point 1–4 | 0–127 | nota |
| `15H`–`18H` | C_off_lvl Scaling Offset 1–4 | v1:0–1, v2:0–127 | –127 ~ +127 (2 bytes) |

### Chart 6 — Filter Common (struct_MSB = `09H`, sub-sección)

| n2 | Función | Rango | Notas |
|---|---|---|---|
| **`32H`** | **Resonance** | **0–99** | |
| `33H` | Velocity Sensitivity Key-on | 0–15 | 0–7: 0~+7, 8–15: 0~–7 |
| `34H` | Cut-off Modulation Sensitivity | 0–15 | 0–7: 0~+7, 8–15: 0~–7 |

---

## 5. Ejemplos de mensajes SysEx

### Cutoff Frequency — Filter 1 / Elemento 0 / valor 90 / device 0

```
F0 43 10 35 09 00 00 01 00 5A F7
        ↑     ↑  ↑     ↑     ↑
      device Filter1/  param  valor
      =0     Elem0     Cutoff  90
```

### Resonance (Filter Common) / valor 64 / device 0

```
F0 43 10 35 09 00 00 32 00 40 F7
```

### Filter Type → LPF / Filter 1 / Elem 0 / device 0

```
F0 43 10 35 09 00 00 00 00 01 F7
```

---

## 6. Parámetros de Voice Common — asignación de CC

Chart 2 (Voice Common, struct_MSB = `02H`) — relevantes para aitec:

| n2 | Función | Rango | Notas |
|---|---|---|---|
| `12H` | Filter Modulation — Device Assign (MIDI Control#) | 0–121 | 121=AT |
| `13H` | Filter Modulation Range | 0–127 | |
| `18H` | Filter Cut-off — Device Assign (MIDI Control#) | 0–121 | 121=AT |
| `19H` | Cut-off Range | 0–127 | |

> Esto permite **asignar cualquier CC** como modulador de cutoff/filtro a nivel de voz, lo que complementa al SysEx directo.

---

## 7. Banco de voces — estructura

| Banco | Identificador display | Capacidad | Tipo memoria |
|---|---|---|---|
| PRESET | `P` | 64 voces (P01–P64) | ROM — no editable |
| INTERNAL | `I` | 64 voces | RAM no volátil |
| CARD | `C` / `Ↄ` | 64 o 128 voces | MCD32 / MCD64 |

**Program Change:** numeración 0–63 (P01 = PC 0, P64 = PC 63).
**Bank Select:** no especificado explícitamente en el manual para MIDI — se asume comportamiento estándar Yamaha (Bank MSB para selección de banco Preset/Internal/Card).

---

## 8. Lista de Preset Voices (P01–P64)

Verificada desde página 12 del manual. Columna EL = número de elementos AWM (1, 2 o 4); determina cuántos filtros independientes hay por voz.

| No. | EL | Nombre | No. | EL | Nombre |
|---|---|---|---|---|---|
| P01 | 1 | Piano | P33 | 2 | Thumb Bass |
| P02 | 2 | Voyager | P34 | 2 | SynBadBass |
| P03 | 2 | Pro55Brass | P35 | 2 | VCO Bass |
| P04 | 2 | Elektrodes | P36 | 2 | Violin |
| P05 | 4 | Zuratustra | P37 | 2 | ChamberStr |
| P06 | 2 | DawnChorus | P38 | 2 | VCF String |
| P07 | 2 | GX Dream | P39 | 2 | Nova Quire |
| P08 | 2 | GrooveKing | P40 | 2 | Vibraphone |
| P09 | 4 | DistGuitar | P41 | 2 | Takerimba |
| P10 | 4 | ZenAirBell | P42 | 1 | Gloken |
| P11 | 2 | FullString | P43 | 2 | DigiBell |
| P12 | 4 | JazzMan | P44 | 2 | Oriental |
| P13 | 2 | ClassPiano | P45 | 2 | VCO Lead |
| P14 | 2 | RockPiano | P46 | 2 | Spirit VCF |
| P15 | 1 | DX E.Piano | P47 | 2 | OZ Lead |
| P16 | 2 | Hard EP | P48 | 4 | Get Lucky |
| P17 | 2 | Cry Clav | P49 | 4 | Gamma Band |
| P18 | 2 | Funky Clav | P50 | 2 | Metal Reed |
| P19 | 2 | Deep Organ | P51 | 4 | Modomatic |
| P20 | 2 | Warm Organ | P52 | 2 | DataStream |
| P21 | 1 | Trumpet | P53 | 2 | Mystichoir |
| P22 | 4 | Stab Brass | P54 | 2 | St.Michael |
| P23 | 4 | Big Band | P55 | 2 | Scatter |
| P24 | 2 | Orch Brass | P56 | 2 | Triton |
| P25 | 2 | SynthBrass | P57 | 4 | Amazon |
| P26 | 1 | Flute | P58 | 2 | SatinGlass |
| P27 | 1 | Saxophone | P59 | 2 | BrassChime |
| P28 | 2 | FolkGuitar | P60 | 2 | Piano Mist |
| P29 | 2 | 12 String | P61 | 4 | Xanadu |
| P30 | 2 | MuteGuitar | P62 | 2 | WdBass Duo |
| P31 | 2 | SingleCoil | P63 | (61) | Drum Set 1 |
| P32 | 1 | Pick Bass | P64 | (61) | Drum Set 2 |

> P63 y P64 son Drum Sets. `(61)` = 61 notas percutidas (C1–C6), no elementos AWM estándar.

---

## 9. Multitímbrico — selección de canal

TG55 es **16 partes multitimbral**. Canal MIDI por parte se configura via SysEx:

```
struct_MSB = 01H  (Multi Each Voice)
struct_LSB = t2   (canal de voz: 0x00–0x0F = CH1–CH16, bit3=canal, bit0=element)
```

Parámetros relevantes (n2):

| n2 | Función | Rango |
|---|---|---|
| `00H` | Voice on/off + Output Select | b6:0–1 on/off; b0,1,2: output |
| `01H` | Voice Memory Select | 0=int/crd, 1=pre |
| `02H` | Voice Number | 0–63 |
| `03H` | Volume | 0–127 |
| `04H` | Tuning | 0–127 (–64~+63) |
| `05H` | Note Shift | 0–127 (–64~+63) |
| `06H` | Multi Static PAN | 0–63 (0:voice, 1–63: –31~+31) |

---

## 10. Resumen de constantes para el firmware

```c
// TG55 SysEx
#define TG55_SYSEX_MANUF        0x43   // Yamaha
#define TG55_MODEL_ID           0x35   // TG55/SY55
#define TG55_DEVICE_DEFAULT     0x00   // configurable via NVS

// struct_MSB
#define TG55_STRUCT_MULTI_CMN   0x00
#define TG55_STRUCT_MULTI_EACH  0x01
#define TG55_STRUCT_VOICE_CMN   0x02
#define TG55_STRUCT_VOICE_ELEM  0x03
#define TG55_STRUCT_DRUM        0x04
#define TG55_STRUCT_AWM         0x07
#define TG55_STRUCT_EFFECT      0x08
#define TG55_STRUCT_FILTER      0x09
#define TG55_STRUCT_SWITCH      0x0D
#define TG55_STRUCT_SYSTEM      0x0F

// Parámetros de filtro (n2)
#define TG55_PARAM_FILTER_TYPE      0x00  // 0:THR, 1:LPF, 2:HPF
#define TG55_PARAM_CUTOFF           0x01  // 0–127
#define TG55_PARAM_RESONANCE        0x32  // 0–99 (Filter Common)

// struct_LSB para Filter — macro
// f=0 (Filter1) o 1 (Filter2), ee=element 0–3
#define TG55_FILTER_LSB(f, ee)  (((f) << 6) | ((ee) << 4))
```

### Función de construcción del mensaje

```c
// Sin checksum — Parameter Change TG55
void tg55_param_change(uint8_t device, uint8_t struct_msb, uint8_t struct_lsb,
                       uint8_t param_n2, uint8_t value, uint8_t *buf, size_t *len) {
    buf[0] = 0xF0;
    buf[1] = 0x43;
    buf[2] = 0x10 | (device & 0x0F);  // sub-status 0001nnnnB
    buf[3] = 0x35;                     // Model ID TG55
    buf[4] = struct_msb;
    buf[5] = struct_lsb;
    buf[6] = 0x00;                     // param MSB
    buf[7] = param_n2;
    buf[8] = 0x00;                     // value MSB
    buf[9] = value;
    buf[10] = 0xF7;
    *len = 11;
}
```

---

## 11. Diferencias críticas respecto a D-110 y TRITON

| Aspecto | Roland D-110 | Korg TRITON | Yamaha TG55 |
|---|---|---|---|
| Fabricante byte | `41H` | `42H` | `43H` |
| Model ID | `16H` | `50H` | `35H` |
| Checksum en Param Change | Sí (DT1) | No | No |
| Checksum en Bulk Dump | Sí | No | Sí (complemento a 2, 7 bits) |
| Cutoff param | dirección fija `00 25` | Param ID `02H` | n2 = `01H` |
| Resonance param | no confirmado | Param ID `0DH` | n2 = `32H` (Filter Common) |
| Partes multitimbrales | 8 + ritmo | 16 | 16 |
| Capas por voz | 1 (PCM) | hasta 2 OSC | 1, 2 o 4 elementos |

---

*Documento generado para Claude Code. Solo datos verificados contra TG55G.pdf.*
*Fuente: páginas Add-5 → Add-15 (SysEx), páginas 12–13 (Voice List).*
