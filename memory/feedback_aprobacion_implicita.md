---
name: feedback-aprobacion-implicita
description: "Si el usuario ya dio aprobación implícita con su respuesta, no pedir confirmación adicional antes de ejecutar"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: 32844966-f2c2-4533-b53c-c3c14de7bf95
---

Cuando el usuario responde directamente a una propuesta con ajustes ("rojo en vez de azul, números más grandes"), eso ES la aprobación. No mostrar mockup + preguntar "¿confirmas?" — ejecutar directo con los ajustes indicados.

**Why:** "eres más bruto" + "aquí se documenta hasta cuando voy a mear" — el protocolo de documentación e informes es SIEMPRE correcto y obligatorio. El problema fue pedir confirmación redundante cuando ya la había dado.

**How to apply:**
- Propuesta → usuario ajusta → ejecutar con ajustes (no volver a preguntar)
- Informe MCU siempre, para todo cambio, sin excepción
- La documentación exhaustiva es parte del contrato de trabajo aquí
