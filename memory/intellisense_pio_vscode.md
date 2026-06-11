---
name: intellisense-pio-vscode
description: "Troubleshooting IntelliSense PlatformIO en VS Code — error includePath, Zigbee, c_cpp_properties.json"
metadata: 
  node_type: memory
  type: reference
  originSessionId: 9dd3a0a3-3752-4454-803e-a15c9cc35e08
---

# IntelliSense PlatformIO — VS Code Troubleshooting (2026-05-24)

## Síntoma
"Se han detectado errores de #include. Actualice el valor de includePath. El subrayado ondulado está deshabilitado para esta unidad de traducción."

## Causa raíz
`c_cpp_properties.json` es **AUTO-GENERADO** por PlatformIO. Puede quedar desactualizado o con entradas inválidas:
- Entradas vacías `""` al final de `includePath` y `browse.path` (causa directa del error)
- Generado con `.pio/` desactualizado (si no se ha compilado recientemente)

## Por qué aparece Zigbee (y otras librerías no usadas)
PlatformIO añade TODAS las librerías del framework Arduino-ESP32 al `includePath` de IntelliSense, independientemente de las `lib_deps`. Es cosmético — no afecta compilación.

## Fix
**Command Palette (⇧⌘P) → "PlatformIO: Rebuild IntelliSense Index"**

Regenera `.vscode/c_cpp_properties.json` desde cero. NUNCA editar manualmente — se sobreescribe en cada rebuild.

## Si persiste tras rebuild
`.pio/` desactualizado porque no se ha compilado el proyecto. PlatformIO necesita resolver dependencias antes de generar el índice correcto. Ver [[p4-low-priority]] — P4 no se compila frecuentemente.

## Afecta
- P4: `MASTER_S3-P4/P4/src/display/Display.cpp` — confirmado 2026-05-24
- Puede afectar cualquier MCU si `.pio/` está stale
