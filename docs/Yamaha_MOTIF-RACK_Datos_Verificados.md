# Yamaha MOTIF-RACK — Datos Verificados

**Fuente única:** `MOTIFRACKE2.pdf` (Data List oficial, Yamaha Corp., © 2003, doc. WA24960 301MWAP25.2-02B0), subido directamente por el usuario.

**Regla de proyecto (aitec):** este archivo contiene solo lo verificable línea por línea contra el PDF oficial. Cualquier dato marcado como ⚠️ requiere verificación adicional antes de usarse en firmware, por posible corrupción de OCR en el documento fuente (duplicación de tablas detectada — ver nota al final).

---

## 1. Identificación del dispositivo (SysEx)

| Campo | Valor |
|---|---|
| Yamaha ID | `43H` |
| Model ID (Native Parameter Change / Bulk Dump) | `6FH` |
| Device Number Code (Identity Reply) | `58H 04H` |
| Reverb/Effect nomenclature | ver tabla de efectos abajo |

**Formato Parameter Change (nativo):**
```
F0H 43H 1nH 6FH ahH amH alH ddH...ddH F7H
```
`n` = Device Number, `ah/am/al` = dirección (High/Mid/Low), `dd` = datos.

**Formato Bulk Dump:**
```
F0H 43H 0nH 6FH bhH blH ahH amH alH ddH...ddH ccH F7H
```
`bh/bl` = Byte Count, `cc` = checksum (7 bits, suma total = 0 mod 128).

**Identity Reply:**
```
F0H 7EH 7FH 06H 02H 43H 00H 41H 58H 04H 00H 00H 00H 7FH F7H
```

Bulk Dump del MOTIF original (Model ID `6BH`) también es reconocido por el MOTIF-RACK (compatibilidad hacia atrás).

---

## 2. Salida óptica/digital

| Parámetro | Valor |
|---|---|
| Formato | S/PDIF óptico |
| Frecuencia de muestreo | **44.1 kHz** (fija, no configurable) |
| Resolución | 24 bit |

⚠️ Distinto del TRITON Rack (48 kHz) — ver comparación ya discutida en este chat. Si se combinan ambas salidas digitales en la UMC1820, se necesita SRC o reloj maestro único.

---

## 3. Control Change relevantes (Tabla MIDI Data Format, sección 3-1-3)

| CC | Nombre | Rango |
|---|---|---|
| 0 / 32 | Bank Select MSB / LSB | 0–127 |
| 1 | Modulation | 0–127 |
| 5 | Portamento Time | 0–127 |
| 6 / 38 | Data Entry MSB / LSB | 0–127 |
| 7 | Main Volume | 0–127 |
| 10 | Pan | 0–127 |
| 11 | Expression | 0–127 |
| 64 | Sustain Switch | on/off |
| 65 | Portamento Switch | on/off |
| 66 | Sostenuto | on/off |
| 71 | Harmonic Content (resonancia) | offset ±64 |
| 72 | EG Release Time | offset ±64 |
| 73 | EG Attack Time | offset ±64 |
| **74** | **Brightness (cutoff)** | offset ±64 |
| 75 | EG Decay Time | offset ±64 |
| 91 | Effect1 Depth (Reverb Send) | 0–127 |
| 93 | Effect3 Depth (Chorus Send) | 0–127 |

⚠️ Nota importante: en el MOTIF-RACK, **CC74 = Brightness/Cutoff** y **CC71 = Harmonic Content/Resonance** — coincide con la convención "estándar" GM/XG, **al contrario de como estaba mal documentado inicialmente para el JV-2080** (donde CC71=Resonance, CC74=Cutoff, verificado). No asumir que todos los sintes del rack comparten el mismo mapeo CC71/74 — confirmado que D-110, JV-2080, TRITON y MOTIF-RACK pueden diferir; verificar cada uno por separado.

---

## 4. Bulk Dump — bloques de voz (direcciones base)

| Bloque | Dirección |
|---|---|
| Normal Voice Common | `40 00 00` |
| Normal Voice Element 1–4 | `41/42/43/44 ee 00` (ee = nº de elemento) |
| Drum Voice Common | `46 00 00` |
| Drum Voice Key (por nota) | `47/48/49/4A ee 00` |
| Voice Plug-in Common | `4C 00 00` |
| Multi Common | `36 00 00` |
| Multi Part 1–16 | `37 pp 00` (pp = 00–0F) |

---

## 5. Categorías de voz (código de 2 letras usado en listas)

Ap, Kb, Or, Gt, Ba, St, Br, Rp, Ld, Pd, Sc, Cp, Dr, Se, Me, Co
(Piano, Keyboard, Organ, Guitar, Bass, Strings, Brass, Reed/Pipe, Lead, Pad, Sequence, Chromatic Perc, Drum, Sound Effect, Musical Effect, Combi)

---

## 6. Nombres de sonido destacados (no piano) — verificados en Normal Voice List

- `Gt Strat` — emulación Fender Stratocaster
- `Gt Sitar` — sitar
- `Gt Koto`, `Gt Shamisen` — instrumentos tradicionales japoneses
- `Kb TX802` — referencia directa al módulo FM Yamaha TX802
- `Or 16+8+5&1/3` — registración clásica de drawbars Hammond
- `Ld GOA LEAD`, `Ld Rap Lead 1/2/3` — sonidos de género específico (Goa trance, Hip-Hop)

### ✅ Corrección de la advertencia previa

Una versión anterior de este archivo advertía de una posible "contaminación por duplicación de OCR" en la Normal Voice List. Se ha re-extraído el PDF directamente desde disco con `pdftotext -layout` (en vez de depender del volcado de texto plano previo, que rompía el layout a dos columnas) y se ha verificado **página por página, byte a byte**. Conclusión:

- **No hay artefacto de OCR.** El PDF real define **8 bancos de 128 voces** (1024 total): Preset1, Preset2, Preset3, Preset4, Preset5, GM, User1, User2.
- La aparente "duplicación" que se detectó antes era real: **el banco User2 viene de fábrica precargado con una copia idéntica de Preset5** (127/127 entradas comparadas coinciden exactamente). Esto es un dato de fábrica legítimo del MOTIF-RACK, no un error de extracción.

---

## 7. Normal Voice List completa (1024 voces, 8 bancos — verificado desde PDF, páginas 2–9)

Ver archivo separado `Yamaha_MOTIF-RACK_Normal_Voice_List.md` con las 8 tablas completas.

---

## 8. Pendiente

- Confirmar mapeo CC71/74 específico si se usa el MOTIF-RACK en el sistema de morphing XY de aitec.
- Decidir si el MOTIF-RACK se integra formalmente como quinto sintetizador del rack (pendiente de confirmación de Julianno).
- Extraer Drum Voice List (pág. 10–22) y Wave List (pág. 23) si se necesitan para firmware.
