---
name: midi-monitor-tool
description: "MIDI Monitor (macOS) disponible para capturar tráfico MIDI real de Logic Pro — usar antes de diagnosticar bugs de VU, faders, VPot"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 27843bc2-ffbe-4683-a331-7d7cacdb2960
---

MIDI Monitor (app macOS) está disponible para capturar tráfico MIDI real.

**Por qué:** Antes de asumir bug en firmware, verificar qué envía Logic exactamente (frecuencia, valores, gaps).

**Cómo usarlo:** Conectar Logic Pro y observar mensajes raw. Clave para diagnosticar:
- Channel Pressure (0xD0): VU level — frecuencia y valores reales
- PitchBend (0xE0): fader positions
- Control Change (0xB0): VPot, LEDs transport
- SysEx (0xF0): handshake Mackie MCU

**How to apply:** Cuando el usuario reporte comportamiento anómalo en VU, faders, o cualquier subsistema MIDI — pedir captura de MIDI Monitor antes de proponer fixes en firmware.
