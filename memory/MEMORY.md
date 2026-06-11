# Memory Index — iMakie / AITEC 17

## Preferencias & Feedback

- [Aprobación implícita](feedback_aprobacion_implicita.md) — Respuesta del usuario con ajustes = aprobación directa, no pedir confirmación extra
- [Model Preference Sonnet](model_preference_sonnet.md) — Sonnet para debugging/investigación; Haiku suficiente para docs simples

## Directivas de Código

- [config.h Source of Truth](config_h_source_of_truth.md) — config.h es fuente única para NUM_SLAVES, pines, timings — nunca asumir
- [NUM_SLAVES por MCU](num_slaves_production.md) — S3: testing=1/prod=8; P4: 0 (bus A sin slaves aún); nunca preguntar

## Estado del Proyecto P4

- [P4 Estado Desarrollo](p4_low_priority.md) — P4 ACTIVO desde jun 2026: UI landscape ✅, RS485/touch/CDC ❌
- [P4 Bugs Críticos](p4_bugs_criticos.md) — 3 bugs 🔴 Alta: startTask(), case 0x61, setTxTimeoutMs(0) — prerequisito RS485
- [P4 Board Migration](p4_board_migration.md) — JC1060P470C: driver JD9165 fabricante, RENDER_FULL, I2C_NUM_1 touch, trampas hardware

## Herramientas & Entorno

- [MIDI Monitor Tool](midi_monitor_tool.md) — App macOS captura tráfico MIDI real de Logic; usar antes de diagnosticar bugs VU/fader/VPot
- [IntelliSense PlatformIO VS Code](intellisense_pio_vscode.md) — Error includePath: fix con "Rebuild IntelliSense Index" en Command Palette

## Fixes & Patrones

- [VU clearClip Bug](vu_clearlip_bug.md) — P4_CH_OFFSET debe aplicarse siempre; clearClip nunca toca el nivel VU (patrón reutilizable)

## Pendientes Técnicos

- [MIDI Traffic Optimization](midi_traffic_optimization_pending.md) — PITCHBEND_DEADBAND 80→150, reducir 850 msgs/s en S3

## Proyecto

- [Project Name](project_name_change.md) — Nombre oficial: AITEC 17 (antes iMakie). P4 tiene logo, S2 pendiente
