---
name: num-slaves-production
description: "NUM_SLAVES por MCU — valores de testing vs producción, nunca preguntar cuántos hay"
metadata: 
  node_type: memory
  type: project
  originSessionId: 72917132-4bad-4fa3-95e7-b8409230997d
---

**S3** (`MASTER_S3-P4/S3/.../config.h`): testing=1, producción=8 (8 faders físicos bus B).

**P4** (`MASTER_S3-P4/P4_JC1060P470C/src/config.h`): `NUM_SLAVES=0` actualmente — bus A sin slaves físicos conectados todavía. Cuando se conecten S2 al bus A: actualizar a N real.

**S2** no tiene NUM_SLAVES — es slave, no master.

**Why:** Nunca asumir cantidad de slaves; siempre leer config.h. El usuario tiene 8 faders físicos en bus B (S3). Bus A (P4) está en desarrollo.

**How to apply:** Nunca preguntar "¿cuántos slaves hay?". Leer config.h antes de cualquier cambio que dependa de NUM_SLAVES.
