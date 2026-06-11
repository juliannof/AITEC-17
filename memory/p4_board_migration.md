---
name: p4-board-migration
description: Migración P4 JC4880P433C → JC1060P470C — estado de la migración y decisiones clave de hardware
metadata: 
  node_type: memory
  type: project
  originSessionId: 72917132-4bad-4fa3-95e7-b8409230997d
---

Placa P4 activa: **JC1060P470C** (`MASTER_S3-P4/P4_JC1060P470C/`). La JC4880P433C es proyecto futuro separado — NO tocar.

**Decisiones de hardware confirmadas (2026-06-09):**
- Display: JD9165 MIPI-DSI 1024×600 landscape nativo. Driver: ficheros del fabricante vendorizados en `src/lcd/` (NO esp-iot-solution — su init sequence es incompleta).
- LVGL: `RENDER_MODE_FULL` + 2 buffers pantalla completa en PSRAM + `num_fbs=1`. PARTIAL no funciona con MIPI-DSI DPI.
- Touch GT911: I2C_NUM_1 (no NUM_0 — conflicto con Wire/render), pull-ups internos habilitados (`flags.enable_internal_pullup=true`), RST/INT = -1 (NC), addr 0x5D, SDA=7/SCL=8.
- RS485: TX=52, RX=51, EN=50 (pendiente confirmar contra esquemático físico).
- Backlight: GPIO23. LCD_RST: GPIO27.

**Repo fabricante (fuente de verdad de pines y driver):**
`~/Downloads/JC1060P470C_I_W-main/1-Demo/Demo_Arduino/1_2_Lvgl_V9/esp32p4_arduino_mipi-dsi_lvgl/`

**Trampa I2C confirmada (2026-06-09):** cambiar touch a I2C_NUM_0 mata el render del display (pantalla negra iluminada). Siempre usar I2C_NUM_1 para el touch.

**CPU:** ESP32-P4 ES soporta máx 360MHz. `platformio.ini` tiene `400000000L` (pendiente corregir a `360000000L`).

**Why:** La diferencia de driver (init sequence) entre esp-iot-solution y el fabricante causó 1 sesión entera de pantalla negra. El modo RENDER_PARTIAL causó otra. Documentado para no repetir.

**How to apply:** Ante cualquier problema de display P4 → revisar primero init sequence del driver y modo de render LVGL. Ante touch que no inicia → verificar I2C_NUM_1 + pull-ups. Ver [[p4_bugs_criticos]] para bugs de firmware pendientes.
