# DISPLAY P4 — ST7701S MIPI-DSI (ESP32-P4)

Documentación del subsistema de display P4. Display grande 480×800 con LVGL v9 para interfaz master MCU.

**Responsable:** iMakie Development Team
**Última actualización:** 2026-05-30 11:30
**Estado:** En producción (ST7701S MIPI-DSI driver custom ESP-IDF + LVGL v9)

---

## 1. HARDWARE DISPLAY P4

### 1.1 Especificación

| Parámetro | Valor |
|-----------|-------|
| **Chip** | ST7701S |
| **Interface** | MIPI-DSI 2-lane (1000 Mbps/lane) |
| **Resolución** | 480×800 píxeles (portrait nativo) |
| **Modo color** | RGB565 (16 bpp en el panel) |
| **Orientación física** | Portrait (480×800) — se visualiza en landscape por montaje girado 90° |
| **Backlight** | LEDC PWM, GPIO 23, 5 kHz, 10-bit |
| **Reset** | GPIO 5 |
| **Alimentación MIPI PHY** | LDO interno canal 3 @ 2500 mV |

### 1.2 Pinout (Integrado en placa GUITION)

- **MIPI-DSI:** Integrado en placa GUITION JC4880P443C — 2 data lanes
- **Backlight:** GPIO 23 (LEDC canal 0, timer 1)
- **Reset:** GPIO 5
- **Touch GT911:** I2C_NUM_1 — SDA GPIO 7, SCL GPIO 8 (400 kHz)
- **No requiere GPIO de datos adicionales** — comunicación por MIPI DSI

---

## 2. STACK SOFTWARE (driver custom + LVGL v9)

> ⚠️ **El display NO usa LovyanGFX.** Usa un driver custom ESP-IDF para el ST7701S
> (`src/lcd/`) y la API moderna de **LVGL v9** (`lv_display_create`). Cualquier
> documentación o ejemplo que mencione `tft.init()` o `lv_disp_drv_register` es de
> LVGL v8 y NO aplica a este proyecto.

### 2.1 Inicialización real (`src/display/Display.cpp` → `initDisplay()`)

```cpp
// 1. Backlight LEDC + LDO para el PHY MIPI
backlight_init();
init_mipi_dsi_power();              // esp_ldo_acquire_channel, 2500 mV

// 2. Bus DSI (2 lanes, 1000 Mbps) + IO DBI
esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus);
esp_lcd_new_panel_io_dbi(dsi_bus, &dbi_cfg, &io);

// 3. Panel ST7701S (driver custom), DPI config, num_fbs = 2
esp_lcd_dpi_panel_config_t dpi_cfg =
    ST7701_480_360_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
esp_lcd_new_panel_st7701(io, &panel_cfg, &s_panel);
esp_lcd_panel_reset(s_panel);
esp_lcd_panel_init(s_panel);

// 4. Touch GT911 por I2C_NUM_1 (driver C esp_lcd_touch_gt911)
esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &s_tp);

// 5. LVGL v9
lv_init();
s_disp = lv_display_create(LCD_H_RES, LCD_V_RES);   // 480 × 800 PORTRAIT
lv_display_set_buffers(s_disp, buf1, buf2,
                       LCD_H_RES * 100 * sizeof(lv_color_t),
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
lv_display_set_flush_cb(s_disp, /* esp_lcd_panel_draw_bitmap → flush_ready */);
```

### 2.2 Buffers

- 2 buffers en **PSRAM** (`MALLOC_CAP_SPIRAM`), cada uno `480 × 100 × sizeof(lv_color_t)`.
- Modo `LV_DISPLAY_RENDER_MODE_PARTIAL` — LVGL redibuja por franjas, no el frame completo.
- El panel tiene `num_fbs = 2` (doble framebuffer en el lado DSI).

### 2.3 Backlight

```cpp
displaySetBrightness(uint8_t percent);   // 0..100 → duty LEDC 10-bit (0..1023)
```

---

## 3. 🔄 SISTEMA DE ORIENTACIÓN: Portrait dibujado → Landscape visualizado

> **DOCUMENTO CANÓNICO.** Esta es la fuente de verdad sobre la orientación del P4.
> El README del P4 solo contiene un resumen con puntero aquí.

### 3.1 Hallazgo central

El P4 **dibuja toda la UI en un lienzo portrait nativo 480×800**, pero el dispositivo
se monta girado 90° y **se mira en landscape (800 visual ancho × 480 visual alto)**.

La conversión portrait→landscape **NO se hace a nivel de panel ni de display global**,
sino **rotando cada elemento de TEXTO individualmente 90°** con `transform_rotation`.
Es deliberado pero frágil: entenderlo es obligatorio antes de tocar cualquier `UIPageX`.

**Resumen en una frase:** el framebuffer es portrait real; el landscape existe solo en
el ojo del usuario y en los `transform_rotation` de cada label.

### 3.2 Diagrama del flujo de orientación

```
┌─────────────────────────────────────────────────────────────────────┐
│ 1. PANEL FÍSICO ST7701S          MIPI-DSI, 480×800 portrait nativo   │
│    Display.cpp:139  flags = { swap_xy=0, mirror_x=0, mirror_y=0 }    │
│    → El panel NO rota nada. Framebuffer portrait tal cual.           │
├─────────────────────────────────────────────────────────────────────┤
│ 2. LVGL DISPLAY                  lv_display_create(480, 800)         │
│    Display.cpp:153  → canvas portrait 480 ancho × 800 alto          │
│    NO existe lv_display_set_rotation(). Sin rotación global.        │
├─────────────────────────────────────────────────────────────────────┤
│ 3. GEOMETRÍA BASE (config.h)     Todo el layout se piensa portrait  │
│    P4_W=480 (ancho)  P4_H=800 (alto)                                 │
│    NUM_CH=8 canales apilados VERTICALMENTE, CH_H=100px (8×100=800)   │
│    HEADER_X=410 → franja header en x:410..480 (70px) al lateral     │
├─────────────────────────────────────────────────────────────────────┤
│ 4. ROTACIÓN PER-WIDGET           Solo el TEXTO se gira 90°           │
│    lv_obj_set_style_transform_rotation(obj, 900, 0)  // 900 = 90.0° │
│    + pivote al centro del propio label                              │
│    Rectángulos (barras de fader, fondos) NO se rotan (son simétricos)│
├─────────────────────────────────────────────────────────────────────┤
│ 5. VISIÓN DEL USUARIO            Dispositivo montado girado 90°      │
│    → 800 px visual ancho × 480 px visual alto (landscape)           │
│    Los 8 canales, en portrait apilados en vertical, en landscape se │
│    perciben como 8 columnas en horizontal.                          │
└─────────────────────────────────────────────────────────────────────┘
```

### 3.3 Capa por capa (verificado en código, 2026-05-30)

| Capa | Dónde | Qué hace | Rota |
|------|-------|----------|:----:|
| Panel ST7701S | `Display.cpp:139` | `swap_xy=0, mirror_x=0, mirror_y=0` | ❌ No |
| LVGL display | `Display.cpp:153` `lv_display_create(480,800)` | Lienzo portrait nativo | ❌ No |
| Buffers | `Display.cpp:148-156` | `480×100` líneas, `RENDER_MODE_PARTIAL`, PSRAM | ❌ No |
| Geometría | `config.h:46-55` `P4_W/P4_H/NUM_CH/CH_H/HEADER_X` | Layout definido en portrait | ❌ No |
| Texto UI | `UIMenu/UIHeader/UIPage1/UIPage3/UIPage3B/UIOffline` | `transform_rotation(900)` + pivote centro | ✅ 90° |
| Touch GT911 | `Display.cpp:133-180` | `x_max=480, y_max=800, swap_xy=0`, coords directas | ❌ No |

### 3.4 Patrón de código exacto

Cada elemento de texto se rota así (ejemplo de `UIMenu.cpp:28-30`):

```cpp
lv_obj_set_style_transform_rotation(obj, 900, 0);       // 900 decigrados = 90° horario
lv_obj_set_style_transform_pivot_x(obj, LV_PCT(50), 0); // pivote en el centro del label
lv_obj_set_style_transform_pivot_y(obj, LV_PCT(50), 0);
```

En `UIHeader.cpp` el pivote se da en píxeles absolutos (`mw/2`, `mh/2`, `tw/2`, `th/2`)
en lugar de `LV_PCT(50)`, pero el efecto es idéntico: gira el texto sobre su propio centro.

**Dónde vive la rotación (todos los archivos):**

| Archivo | Líneas | Elementos rotados |
|---------|--------|-------------------|
| `src/display/UIMenu.cpp` | 28 | Etiquetas del menú lateral |
| `src/display/UIHeader.cpp` | 59, 94, 99 | Modo, timecode + su "ghost" |
| `src/display/UIPage1.cpp` | 127 | Labels de canal (faders) |
| `src/display/UIPage3.cpp` | 61 | Texto de página 3 |
| `src/display/UIPage3B.cpp` | 74 | Texto de página 3B |
| `src/display/UIOffline.cpp` | 53, 75 | Logo + label parpadeante "offline" |

### 3.5 ⚠️ Implicaciones y trampas (crítico para mantenimiento)

1. **`transform_rotation` en LVGL v9 es SOLO visual.** Rota el render, **no** rota la
   geometría de layout ni el área de hit-testing. El *bounding box* para eventos táctiles
   sigue sin girar. Un widget rotado con área clicable asimétrica registrará el toque en
   una zona distinta a la que el usuario ve.

2. **El touch NO compensa coordenadas.** En `Display.cpp:174-175` se asigna
   `data->point.x = x; data->point.y = y;` directo, en espacio portrait (480×800). Coincide
   con la geometría base portrait (la real, no la rotada visualmente) — por eso funciona pese
   a no haber transformación: tanto el hit-testing de LVGL como el touch operan en el MISMO
   espacio portrait sin rotar. **Si algún día se rota el display de forma global, habrá que
   rotar también el touch o el toque quedará invertido.**

3. **Sistema frágil por diseño: la rotación es opt-in por widget.** Cualquier label nuevo
   que olvides rotar aparecerá en portrait (girado 90° respecto al resto de la UI). No hay
   rotación global que lo cubra automáticamente.

4. **Solo se rota texto, no contenedores.** Las barras de fader y los fondos son rectángulos
   cuya forma se define directamente en coordenadas portrait y resultan correctos en landscape
   sin rotar. Solo el texto necesita girar para ser legible. No añadas `transform_rotation` a
   rectángulos: deformaría el layout.

5. **Razonas en dos sistemas a la vez.** Las posiciones/tamaños se calculan en el espacio
   portrait (`P4_W=480` ancho, `P4_H=800` alto), pero la composición que ve el usuario es
   landscape. Equivalencia mental de coordenadas: `(x, y)_portrait` se percibe en
   `(y, 480−x)_landscape`.

### 3.6 Hallazgos secundarios (limpieza pendiente)

- **Código muerto — `src/touch/gt911_touch.{h,cpp}`:** define una clase `gt911_touch` con
  método `set_rotation(uint8_t)`, pero **no se instancia en ningún sitio**. El touch activo es
  el driver C `esp_lcd_touch_gt911` (usado en `Display.cpp`). Candidato a eliminar.
- **Flag sin uso — `EXAMPLE_LVGL_PORT_PPA_ROTATION_ENABLE 1`** (`src/touch/pins_config.h:16`):
  sugiere rotación por el acelerador PPA, pero **no hay ninguna referencia al PPA en el código
  de display**. La rotación real es la per-widget descrita arriba. El flag es residuo de un
  ejemplo y no afecta al comportamiento.

---

## 4. PANTALLA DE BOTONES (UIPage1)

Botonera táctil en pantalla: rejilla **8 columnas × 4 filas = 32 botones** que emiten
comandos Mackie Control (MCU) a Logic Pro y reflejan el feedback de LEDs de Logic.

**Código:** `MASTER_S3-P4/P4/src/display/UIPage1.{cpp,h}` + arrays en `src/config.h`.

> No confundir con:
> - **`docs/BUTTONS.md`** → botones FÍSICOS del S2 (REC/SOLO/MUTE/SELECT, GPIO 37-40).
> - **`docs/BUTTONS_P4.md` / `docs/NEOTRELLLIS.md`** → matriz FÍSICA NeoTrellis 4×8 del P4.
>
> Esta sección cubre la **pantalla** táctil (LVGL), no botones físicos.

### 4.1 Comportamiento

Cada botón:

- **Al pulsar:** envía `Note On` MIDI canal 1 → `0x90 <nota> 0x7F`.
- **Al soltar:** envía `0x90 <nota> 0x00` (note off por velocidad 0).
- **Refleja estado:** color pleno cuando Logic devuelve el LED activo; color ÷4 cuando
  inactivo (`btnStatePG1[i]`, feedback MIDI de Logic).
- **Modo SHIFT:** el botón i26 conmuta todo el set de etiquetas a una capa alternativa.

Es momentáneo y bidireccional: **emite** comandos MCU y **muestra** los LEDs reportados.

### 4.2 Flujo de código

**Envío MIDI** (`btn_event_cb`, `UIPage1.cpp:56`):

```cpp
LV_EVENT_PRESSED  → si MIDI_NOTES_PG1[idx] != 0x00 → sendMIDIBytes({0x90, nota, 0x7F})
LV_EVENT_RELEASED → si MIDI_NOTES_PG1[idx] != 0x00 → sendMIDIBytes({0x90, nota, 0x00})
```

- Si la nota es `0x00`, el botón **no envía nada** (ver §4.5).
- El botón **31** (`>>PG2`) está interceptado: `PRESSED` hace `return` con un `TODO`.

**Feedback de estado** (`applyButtonState`, `UIPage1.cpp:39`):

- `s_colActive[i]` = `PALETTE_HEX[BTN_COLOR_IDX[i]]`; `s_colInactive[i]` = `dimHex(...)` (RGB `>>2`).
- Texto: negro si el fondo activo es claro (`needsBlackText`, luminancia >160), amarillo si
  SHIFT activo, blanco en el resto.
- `btnStatePG1[32]` (en `config.h`, `extern`) es la fuente de estado; refresco cuando
  `needsButtonsRedraw` está marcado (`uiPage1Update`).

**Geometría:** la página ocupa `HEADER_X × P4_H` = `410 × 800` (portrait). Celdas
`cell_x ≈ 102px`, `cell_y = 100px`. Las **filas se invierten** al posicionar
(`(P1_ROWS-1-row)*cell_x`, `UIPage1.cpp:110`) y cada **etiqueta se rota 90°** para ser
legible en landscape (ver §3).

### 4.3 Mapa completo de los 32 botones

Índice lógico `i = fila*8 + columna` (filas 0-3, columnas 0-7). "Función MCU" = significado
estándar de esa nota en el protocolo Mackie.

**Fila 0 (i0–i7) — Asignación de V-Pot / vista**

| i | Label | SHIFT | Nota | Función MCU estándar | Color |
|---|-------|-------|------|----------------------|-------|
| 0 | TRACK | GLOBAL | `0x28` | Assign → Track | cian |
| 1 | PAN | FINE | `0x2A` | Assign → Pan/Surround | cian |
| 2 | EQ | LOW | `0x2C` | Assign → EQ | cian |
| 3 | SEND | MID | `0x29` | Assign → Send | cian |
| 4 | PLUG | HI | `0x2B` | Assign → Plug-In | cian |
| 5 | INST | FREQ | `0x2D` | Assign → Instrument | cian |
| 6 | FLIP | ___ | `0x32` | Flip (V-Pot↔fader) | magenta |
| 7 | GLOB | ___ | `0x33` | Global View | magenta |

**Fila 1 (i8–i15) — Automatización / display**

| i | Label | SHIFT | Nota | Función MCU estándar | Color |
|---|-------|-------|------|----------------------|-------|
| 8 | READ | OFF | `0x4A` | Automation Read/Off | verde |
| 9 | WRITE | TRIM | `0x4B` | Automation Write | magenta |
| 10 | TOUCH | LTCH | `0x4D` | Automation Touch | amarillo |
| 11 | LATCH | TCH | `0x4E` | Automation Latch | naranja |
| 12 | TRIM | WRIT | `0x4C` | Automation Trim | naranja |
| 13 | OFF | READ | `0x4F` | ⚠️ nota = **Group** (no "off") | verde |
| 14 | SOLO0 | UNSOLO | `0x57` | ⚠️ nota = **Drop/Punch** (Solo MCU es `0x5A`) | verde |
| 15 | SMPT | UNMUTE | `0x35` | SMPTE/Beats (toggle timecode) | verde |

**Fila 2 (i16–i23) — Navegación**

| i | Label | SHIFT | Nota | Función MCU estándar | Color |
|---|-------|-------|------|----------------------|-------|
| 16 | CALIB | SHIFT | `0x00` | ⚠️ **No envía MIDI** (nota 0) — inerte | amarillo |
| 17 | SCRUB | ALT | `0x65` | Scrub | amarillo |
| 18 | NUDGE | OPT | `0x66` | ⚠️ nota fuera del rango transport estándar | amarillo |
| 19 | MARK | CMD | `0x54` | Markers | amarillo |
| 20 | CHAN< | CHAN< | `0x30` | Channel Left | azul |
| 21 | CHAN> | CHAN> | `0x31` | Channel Right | azul |
| 22 | BANK< | ZOOM- | `0x2E` | Bank Left | azul |
| 23 | BANK> | ZOOM+ | `0x2F` | Bank Right | azul |

**Fila 3 (i24–i31) — Utilidades / modificadores**

| i | Label | SHIFT | Nota | Función MCU estándar | Color |
|---|-------|-------|------|----------------------|-------|
| 24 | UNDO | REDO | `0x51` | Undo | magenta |
| 25 | SAVE | SAVE AS | `0x50` | Save | verde |
| 26 | SHIFT | OK | `0x46` | **Shift** (+ conmuta el set de etiquetas) | blanco |
| 27 | CTRL | CNCL | `0x47` | ⚠️ nota = **Option** (no Control) | blanco |
| 28 | OPT | MARK | `0x48` | ⚠️ nota = **Control** (no Option) | blanco |
| 29 | CMD | NUDGE | `0x49` | CMD/Alt | blanco |
| 30 | ENTER | TAP | `0x53` | Enter | verde |
| 31 | >>PG2 | >>PG2 | `0x00` | ⚠️ **No hace nada** — `TODO` cambio a PG2 | rojo |

### 4.4 Botón SHIFT y capa de etiquetas

El botón **i26 (SHIFT)** tiene comportamiento doble:

1. Envía la nota Mackie Shift (`0x46`) como cualquier botón.
2. Cuando `btnStatePG1[26]` se activa, `uiPage1SetShift(true)` **sustituye todas las
   etiquetas** por `LABELS_PG1_SHIFT` (`config.h`) y el texto pasa a amarillo.

Disparado desde `uiPage1UpdateButton(index,active)` (caso especial `index==26`) y
`uiPage1UpdateAllButtons()` (`s_shiftActive = btnStatePG1[26]`).

> La capa SHIFT cambia **lo que se muestra**, NO la nota enviada: cada botón sigue mandando
> su `MIDI_NOTES_PG1[i]`. El significado "shifted" depende de cómo lo interprete Logic.

### 4.5 Pendientes y divergencias (verificar antes de dar por bueno)

**Botones inertes (nota `0x00`):**
- **i16 CALIB** — etiquetado pero sin nota → no envía MIDI. Si debe disparar calibración de
  slaves, NO está cableado por esta vía.
- **i31 >>PG2** — `PRESSED` hace `return` (`// TODO: cambio a PG2`). La Página 2 no existe
  aún, aunque `MIDI_NOTES_PG2[32]` ya está en `config.h:175`.

**Divergencias etiqueta ↔ nota** (la etiqueta no coincide con la función MCU estándar):

| i | Label | Nota | Estándar MCU | Observación |
|---|-------|------|--------------|-------------|
| 13 | OFF | `0x4F` | Group | "Read/Off" sería `0x4A` |
| 14 | SOLO0 | `0x57` | Drop/Punch | Solo MCU es `0x5A` |
| 18 | NUDGE | `0x66` | (no estándar) | fuera del rango transport/jog |
| 27 | CTRL | `0x47` | Option | CTRL y OPT **intercambiados** |
| 28 | OPT | `0x48` | Control | respecto a notas MCU estándar |

> Pueden ser intencionales (mapeadas a cómo responde *tu* Logic). **Verificar con MIDI
> Monitor** capturando el tráfico real antes de "corregir". No tocar `config.h` sin esa
> validación.

### 4.6 Paleta de color (`PALETTE_HEX[9]`, `config.h:135`)

| idx | Color | Hex | | idx | Color | Hex |
|-----|-------|-----|-|-----|-------|-----|
| 0 | off | `0x000000` | | 5 | cian | `0x00CCCC` |
| 1 | rojo | `0xFF0000` | | 6 | magenta | `0xCC00CC` |
| 2 | verde | `0x00BB00` | | 7 | blanco | `0xDDDDDD` |
| 3 | azul | `0x0000FF` | | 8 | naranja | `0xFF6600` |
| 4 | amarillo | `0xFFFF00` | | | | |

Activo → color pleno. Inactivo → mismo color a ÷4 de brillo (`dimHex`).

### 4.7 API pública (`UIPage1.h`)

| Función | Propósito |
|---------|-----------|
| `uiPage1Create(parent)` | Construye la rejilla de 32 botones |
| `uiPage1Update()` | Redibuja si `needsButtonsRedraw` |
| `uiPage1UpdateButton(index, active)` | Actualiza un botón (caso especial SHIFT en i26) |
| `uiPage1UpdateAllButtons()` | Aplica `btnStatePG1[]` a los 32 |
| `uiPage1SetShift(bool)` | Conmuta el set de etiquetas normal/SHIFT |
| `uiPage1SetVisible(bool)` | Muestra/oculta la página (`LV_OBJ_FLAG_HIDDEN`) |
| `uiPage1Destroy()` | Elimina la página y resetea punteros |
| `uiPage1GetRoot()` | Devuelve el objeto raíz LVGL |

---

## 5. CONFIGURACIÓN

### 5.1 platformio.ini (real)

```ini
[env:esp32-p4]
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.37/platform-espressif32.zip
board = esp32-p4
board_build.partitions = default_16MB.csv
board_build.flash_size = 16MB
board_build.psram_type = opi
lib_deps =
    lvgl/lvgl@^9.5.0
    tamctec/TAMC_GT911@^1.0.2
```

> El driver ST7701S es **código propio** en `src/lcd/` (no es una `lib_deps`).

### 5.2 Geometría (config.h)

```cpp
#define P4_W      480              // ancho portrait
#define P4_H      800              // alto portrait
#define NUM_CH    8                // canales
#define CH_H      (P4_H / NUM_CH)  // 100 px por canal (apilados en vertical)
#define HEADER_X  410              // inicio franja header
#define HEADER_W  (P4_W - HEADER_X) // 70 px
```

---

## 6. TROUBLESHOOTING

### 6.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Display negro | DSI/PHY no inicializado | Verificar `init_mipi_dsi_power()` (LDO) + orden `esp_lcd_panel_init()` |
| Backlight apagado | LEDC no configurado o duty 0 | `displaySetBrightness(100)`; revisar GPIO 23 |
| Colores invertidos | RGB order incorrecto | `panel_cfg.rgb_ele_order = ESP_LCD_COLOR_SPACE_RGB` |
| Un widget aparece girado 90° respecto al resto | Falta su `transform_rotation(900)` | Añadir rotación + pivote centro (ver §3.4) |
| Toque no coincide con lo que se ve | `transform_rotation` no rota hit-testing (es visual) | Revisar §3.5 punto 1-2; el área clicable está en portrait |
| Lag de widgets | LVGL redibuja bloqueante | Usar tasks/timers para no bloquear el flush |

### 6.2 Debugging

**LVGL debug logs (v9):**
```cpp
lv_log_register_print_cb([](lv_log_level_t level, const char* msg) {
    Serial.printf("[LVGL] %s\n", msg);
});
```

---

## 7. REFERENCIAS

- **DISPLAY.md** — Display S2 (ST7789V3, LovyanGFX sprites), diferencias con P4
- **ARCHITECTURE_P4.md** — dual-core, `g_switchToPage`, VU decay, race conditions de redibujo
- **TOUCH.md** — GT911 capacitivo, I2C_NUM_1, calibración
- **LVGL v9 docs:** https://docs.lvgl.io/
- **P4 README:** [MASTER_S3-P4/P4/README.md](../MASTER_S3-P4/P4/README.md)

---

## Últimas Actualizaciones

- **(2026-05-30 11:30)** Reescrito a la realidad del código: driver custom ESP-IDF ST7701S + LVGL v9 (corregidas referencias erróneas a LovyanGFX/LVGL v8). Añadida sección canónica §3 "Sistema de orientación Portrait→Landscape" (rotación per-widget, trampas, hallazgos de limpieza) y §4 "Pantalla de botones (UIPage1)" (mapa completo de los 32 botones MCU, capa SHIFT, paleta, divergencias etiqueta↔nota). El README del P4 ahora solo referencia este documento.
- **(2026-05-16)** Creado DISPLAY_P4.md como documento P4-específico, extraído de CLAUDE.md
