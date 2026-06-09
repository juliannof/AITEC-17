# Migración P4 JC4880P433C → JC1060P470C — Notas y errores

> Fecha base: 2026-06-09. Placa nueva: **ESP32-P4 + display JD9165 (MIPI-DSI, 1024×600 landscape nativo) + touch GT911 (I2C)**.
> Repo de referencia del fabricante: `~/Downloads/JC1060P470C_I_W-main` (demo Arduino LVGL v9 = fuente de verdad de pines).

## Estado actual (2026-06-09 23:22)

Revertido al **punto funcional** tras romper la pantalla durante el rediseño de UI:
- ✅ Driver display **JD9165** vendorizado en `src/lcd/` (`esp_lcd_jd9165.c/.h`, v2.0.2 de esp-iot-solution).
- ✅ `UIOffline` migrado a landscape (sin rotación, centrado).
- ↩️ `UIHeader.cpp` y `UIPage3.cpp` revertidos a su versión original (portrait) vía `git checkout`.
- ↩️ `content_area` de `Display.cpp` revertido a `(0,0,HEADER_X,P4_H)`.
- ⚠️ Touch GT911: pull-up quitado, RST/INT en NC. En el último arranque NO aparece
  `GT911 init FALLO`, así que el init podría estar teniendo éxito igualmente → el indev
  se registra → posible atasco del render (ver error 4). **Pendiente** garantizar que el
  touch NO se registre/lea de forma bloqueante para descartarlo del todo.

## Errores encontrados y causa raíz

### 1. `idf_component.yml` NO se procesa (pioarduino + framework=arduino)
- Síntoma: `fatal error: esp_lcd_jd9165.h: No such file or directory`.
- Causa: con `framework = arduino` puro, pioarduino no resuelve managed components.
- Solución: **vendorizar** el driver en `src/lcd/` e incluir con ruta `"lcd/esp_lcd_jd9165.h"` (igual que el GT911 en `src/touch/`).

### 2. Macros del componente JD9165 son C99 → no compilan en C++ (`Display.cpp`)
- `.phy_clk_src = 0` (no convierte a enum), `.flags.use_dma2d` (designated anidado).
- Solución: rellenar `esp_lcd_dsi_bus_config_t` / `dbi` / `dpi_panel_config` a mano.
- ⚠️ En C++ los designated initializers exigen **orden de declaración**:
  - `esp_lcd_dpi_panel_config_t`: `virtual_channel` primero.
  - `esp_lcd_video_timing_t`: `h_size, v_size, hsync_pulse_width, hsync_back_porch, hsync_front_porch, vsync_pulse_width, vsync_back_porch, vsync_front_porch`.
  - `jd9165_vendor_config_t` **NO** tiene campo `.flags`.
- El `.c` vendorizado necesita `#define ESP_LCD_JD9165_VER_MAJOR/MINOR/PATCH` (los genera el build IDF, ausentes al vendorizar).

### 3. Pines display/touch (confirmados con el demo del fabricante)
- Display: `LCD_RST=27`, `LCD_BL=23`, 1024×600.
- Touch GT911: `SDA=7`, `SCL=8`, **`RST=-1`, `INT=-1` (NC)**, dirección 0x5D, **`I2C_NUM_1`** (con `i2c_new_master_bus`), **pull-up interno I2C habilitado**.
- ⚠️ Trampa: poner `I2C_NUM_0` rompe el display (lo usa Wire del core). El fabricante usa NUM_0 pero con driver I2C **legacy**.

### 4. 🔴 Pantalla iluminada y NEGRA al activar el touch
- Síntoma: al habilitar el pull-up, el GT911 inicializa OK → se registra el indev de LVGL → la lectura I2C del touch **en cada refresco** atasca el render → pantalla iluminada pero sin dibujo.
- Con el touch desactivado (sin indev), el display dibuja bien.
- **Pendiente**: leer el GT911 de forma NO bloqueante (throttle o tarea aparte) antes de reactivar el pull-up.

### 5. 🔴 La placa no arranca sin el monitor serie abierto
- Síntoma: pantalla apagada hasta abrir el monitor; con monitor, el `setup()` empieza a los ~4-13 s (vs ~0,2 s con reset RTS).
- Causa: `ARDUINO_USB_CDC_ON_BOOT=1` + `CORE_DEBUG_LEVEL=5` → el informe de arranque del core se escribe al USB-CDC y **bloquea hasta que un host abre el puerto**. `Serial.setTxTimeoutMs(0)` no basta (se ejecuta dentro de `setup()`, tarde).
- El demo del fabricante arranca sin monitor porque usa consola por **USB-Serial-JTAG** y un `Serial.begin()` simple, sin `USB.begin()` ni debug verbose.
- `platformio.ini` es idéntico al P4 legado → no es config de build nueva.
- **Pendiente**: bajar `CORE_DEBUG_LEVEL` y/o cambiar la ruta de consola para no bloquear el arranque.

## Pendiente (orden sugerido)
1. **Arranque sin monitor** (bloqueo CDC) — prioritario: sin esto la placa no es usable sin PC.
2. **Touch no bloqueante** + reactivar pull-up.
3. **Rediseño UI landscape** (Header arriba + 8 canales en columnas), página a página validando: Header, Page3, Page1, Page3B, Menu.
4. Eliminar subproyecto legado `P4_JC4880P433C` tras validar todo en hardware.
