---
name: p4-estado-desarrollo
description: P4 (Master MCU) en desarrollo ACTIVO desde junio 2026 — migración UI landscape + bugs RS485
metadata: 
  node_type: memory
  type: project
  originSessionId: 72917132-4bad-4fa3-95e7-b8409230997d
---

P4 (`MASTER_S3-P4/P4_JC1060P470C/`) está en **desarrollo activo** desde 2026-06-09. Ya no es baja prioridad.

**Estado actual (2026-06-11):**
- ✅ Display JD9165 1024×600 landscape funciona (driver vendorizado de fabricante)
- ✅ LVGL v9 RENDER_MODE_FULL + 2 buffers PSRAM (~1.2MB×2)
- ✅ UIOffline, UIHeader, UIPage3 (VU+canales), UIPage1 (botones), UIMenu, UIPage3B — migradas a landscape nativo
- ✅ Header interactivo: botones táctiles → MIDI, navegación entre páginas
- ✅ VU meters con draw callback (16 objetos, no 192)
- ❌ Touch GT911 inoperativo — `i2c transaction failed` en boot, guard aplicado
- ❌ RS485 task nunca arranca — `startTask()` no llamado en `setup()`
- ❌ Placa negra hasta abrir monitor — `setTxTimeoutMs(0)` no aplicado
- ❌ `case 0x61` pone `g_logicConnected=0` — slaves muertos durante toda la sesión

**Why:** Migración de placa JC4880P433C → JC1060P470C completada en junio 2026. S2/S3 están en estado estable; P4 es el frente activo.

**How to apply:** Al planificar trabajo, P4 va antes que S2/S3 salvo regresión crítica en S2/S3. Ver [[p4_bugs_criticos]] para los bugs bloqueantes. Ver CHANGELOG.md tabla pendientes para lista completa.
