---
name: config_h_source_of_truth
description: config.h es fuente única de verdad para NUM_SLAVES y configuración de hardware
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 9316e9c9-61dc-4739-9a43-7801ca976b76
---

## config.h es FUENTE ÚNICA DE VERDAD

**Regla:** Nunca asumir valores de NUM_SLAVES, pines GPIO, o timings. Siempre verificar `config.h` de la MCU correspondiente.

**Why:** Cada MCU (S2, S3, P4) tiene configuración diferente. S3 controla 1 esclavo actualmente, P4 controla 9. Los valores pueden cambiar entre versiones/configuraciones sin aviso previo.

**How to apply:**
1. Antes de cualquier cambio en RS485, NUM_SLAVES, o pines → leer config.h de la MCU
2. Nunca escribir "S3 controla 8 esclavos" sin verificar config.h
3. Si ves NUM_SLAVES en código, trazar hasta config.h para saber el valor real
4. Documentar en CLAUDE.md cuál es el valor actual (2026-05-16: S3=1, P4=9)

**Ubicación de config.h por MCU:**
- S2: `S2/S2_V1/src/config.h`
- S3: `MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/src/config.h`
- P4: `MASTER_S3-P4/P4/src/config.h`

**Ejemplo real (2026-05-16):**
```
S3 config.h línea 15: #define NUM_SLAVES 1  ← S3 controla 1 esclavo
P4 config.h línea 10: #define NUM_SLAVES 9  ← P4 controla 9 esclavos
```

No es bug. Es configuración correcta.
