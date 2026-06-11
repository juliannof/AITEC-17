---
name: project-name-change
description: El proyecto ya no se llama iMakie — es AITEC 17 (2026-05-20)
metadata: 
  node_type: memory
  type: project
  originSessionId: 96de9955-48a8-4654-9c71-29883886a795
---

El proyecto se renombró de **iMakie** a **AITEC 17** (2026-05-20).

**Why:** Cambio de branding/identidad del producto.

**How to apply:**
- P4: ya tiene logo AITEC 17 en pantalla
- S2: pantalla pequeña ST7789V3 — el nombre aparece en boot screen / header. Pendiente actualizar, pero menor prioridad respecto a funcionalidad
- Docs y código interno pueden seguir usando "iMakie" como nombre histórico hasta que el usuario pida actualización explícita
- No renombrar archivos ni namespaces NVS ("ptxx") salvo petición explícita — riesgo de romper NVS existente
