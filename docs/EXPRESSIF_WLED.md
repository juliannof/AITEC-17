# EXPRESSIF_WLED — Control de iluminación WLED desde el ExPressif

> **ESTADO (2026-10-12 15:14): FASE 1 VALIDADA EN BANCO (log del usuario). FASE 2 PENDIENTE.**
> Validado: WiFi por el C6 asocia (`<tu SSID>`, IP 192.168.1.54, RSSI -38) y se leen los dos WLED (`191 on=0 bri=255 ps=-1`, `195 on=0 bri=255 ps=-1`, verificado contra `/json/state` real). Barrido de redes y credenciales por pantalla funcionan.
> Fase 1 = prueba de WiFi + lectura de estado por log (`src/lights/WledClient.*`).
> La pantalla Lights (`UILights`) sigue siendo de relleno. Desactivable con `WLED_ENABLE 0` en `config.h`.
> Proyecto: `MASTER_S3-P4/P4_JC4880P433C/` (ExPressif). No afecta a S2, S3 ni P4_JC1060P470C.

---

## 1. Objetivo

Controlar desde el ExPressif la iluminación WLED de casa: encender/apagar, brillo, presets/playlists y color/efecto, y que **el reposo del ExPressif apague las luces**.

## 2. Decisiones ya tomadas por el usuario (2026-10-11)

| Tema | Decisión |
|------|----------|
| Vía | **WiFi directo** desde ExPressif (HTTP al WLED), sin pasar por el C3 |
| Dispositivos | **191 = principal**, **195 = workstation** (selector **Todas / Principal / Workstation**) |
| "Todas" | El ExPressif manda HTTP a **ambos** directamente; **no** depender del sync UDP |
| Reposo | El reposo del ExPressif (L0 / inactividad) **apaga las luces**; al despertar vuelven a su estado anterior |
| Control | "Todo": on/off, brillo, presets/playlists, color/efecto |
| UI | Pantalla táctil "LUCES" (LVGL). **No hay teclas NeoTrellis libres** (las 32 están asignadas) |
| Implementación | **Aún no.** Esperar "hazlo" del usuario |

## 3. Datos reales de los WLED (lectura, 2026-10-11 19:56)

| | **191 "Lamparita"** | **195 "Led Ordenador"** |
|---|---|---|
| URL | `http://192.168.1.191/` | `http://192.168.1.195/` |
| Chip | ESP32 (heap libre ~149 KB) | ESP32-S2 (heap libre ~67 KB) |
| LEDs | 697, ≥10 segmentos (máx. 32) | 83, 4 segmentos |
| Audio | AudioReactive I2S **activo** | apagado |
| Sync UDP (grupo 1) | envía **y** recibe | **solo** recibe |
| Versión | WLED 0.15.0 (187 efectos, 71 paletas) | WLED 0.15.0 (187 efectos, 71 paletas) |

- El 195 es el S2 con poca memoria → **peticiones pequeñas, sin polling agresivo**, una a la vez.
- No fiarse del sync UDP (hoy no están en sintonía: 191 apagado, 195 encendido).
- IP de ambos en `config.h` (fuente única). Las credenciales WiFi **no** van en `config.h` (ver §5).

## 4. API WLED que se usará (JSON, puerto 80)

| Acción | Petición |
|--------|----------|
| Estado | `GET /json/state` (pequeño) |
| Info | `GET /json/info` |
| Nombres de efectos / paletas | `GET /json/eff`, `GET /json/pal` |
| Presets | `GET /presets.json` |
| Encender/apagar | `POST /json/state` `{"on":true}` / `{"on":false}` |
| Brillo | `{"bri":0-255}` |
| Transición (décimas de s) | `{"transition":30}` = 3 s |
| Preset / playlist | `{"ps":N}` / `{"pl":N}` |
| Color/efecto (por segmento) | `{"seg":[{"id":0,"col":[[r,g,b]],"fx":N,"sx":0-255,"ix":0-255,"pal":N}]}` |
| AudioReactive (solo 191) | `{"AudioReactive":{"on":true}}` |

**Apagado en reposo:** `{"on":false,"transition":30}` (fundido ~3 s). Alternativa: nightlight `{"nl":{"on":true,"dur":1,"mode":1,"tbri":0}}` (la duración mínima es 1 min, menos útil aquí).
**Restaurar al despertar:** antes de apagar, leer `on` y `bri` actuales de cada WLED (`GET /json/state`) y reponerlos; **no** encender un WLED que ya estaba apagado.

> Verificar la forma exacta de cada campo contra la documentación oficial de WLED 0.15.0 al implementar.

## 5. Credenciales WiFi

> **(2026-10-12 13:50) Decisión cambiada por el usuario: se introducen por pantalla y se guardan en NVS.**
> Modo Lights → botón **CONFIGURAR WIFI** → pantalla con campos SSID y CLAVE (`lv_textarea`,
> modo contraseña, longitud máx. 32/63) y teclado en pantalla (botones LVGL rotados). VER muestra la clave.
> GUARDAR escribe en NVS (`nvs/WifiStore`, namespace `wifi`) y reconecta sin reiniciar
> (`wledCredentialsChanged()`). Código: `display/UIWifiCfg.*`. La clave queda en claro en NVS (no cifrado).
> **Por qué no `lv_keyboard`:** es un `lv_buttonmatrix` y su `get_button_from_point()` (LVGL 9.5.0) compara el
> toque en bruto contra las teclas sin rotar → rotado tocaría otra tecla. Ver cabecera de `UIWifiCfg.cpp`.
> `secrets.h` queda solo como **respaldo** (NVS tiene prioridad).

> **(2026-10-12 13:58) Barrido de redes:** al abrir la pantalla sin SSID guardado se barren las redes y se elige de la
> lista (solo hay que escribir la clave). Botón REDES para volver a barrer, ESCRIBIR A MANO para redes ocultas.
> **Campos sin `lv_textarea`** (iba muy lento al rotarlo con render parcial): caja + `lv_label` rotado.

(Opciones evaluadas originalmente:)

- ~~Recomendado:~~ `src/secrets.h` (ignorado por git) + `src/secrets.h.example` en el repo sin valores reales. Añadir `src/secrets.h` al `.gitignore` de `P4_JC4880P433C`.
- Alternativa (más trabajo): teclado táctil + NVS.
- ⚠️ **No copiar** la contraseña de `C3_PowerRelay/src/config.h`: está **en claro dentro del repo**. Si el repo es público, rotarla.

## 6. Hardware: WiFi del P4 (ESP32-C6 por SDIO / esp-hosted)

- El P4 **no tiene radio**; usa el **ESP32-C6 de la placa** (confirmado por el usuario).
- Hoy el proyecto **no tiene nada de WiFi** (sin `WiFi.h`, sin esp-hosted).
- Pines SDIO **probables** (por analogía con Guition ESP32-P4-M3-Dev y Waveshare P4-WiFi6; **sin confirmar para la JC4880P433C**):

| Señal | GPIO |
|---|---|
| CLK | 18 |
| CMD | 19 |
| D0-D3 | 14-17 |
| Reset C6 | 54 |

- Sin conflicto con los GPIO que usa el ExPressif hoy (5, 7, 8, 23, 31, 33).
- **Confirmado en el core instalado (2026-10-12 13:36):** Arduino-ESP32 3.3.7 trae WiFi vía esp-hosted para el P4 (`CONFIG_ESP_WIFI_REMOTE_ENABLED=y`, esclavo `ESP32C6`) y su variante `esp32p4` define por defecto SDIO CLK=18, CMD=19, D0-D3=14-17, RESET=54 (los de la tabla). No hace falta tocar `platformio.ini` ni llamar a `WiFi.setPins()`. Si la placa usara otros pines habría que llamar a `WiFi.setPins()` antes de `WiFi.mode()`.
- **Sin verificar:** si el C6 trae firmware esp-hosted esclavo y de qué versión; si la polaridad de reset es la esperada (otra placa P4+C6 usa GPIO24 activo bajo).
- API de pines en el core Arduino instalado (pioarduino 55.03.37): confirmar `WiFi.setPins(...)` antes de usarla.

## 7. Plan por fases

### Fase 1 — Prueba mínima de WiFi (2026-10-12 13:36: IMPLEMENTADA, sin validar en banco)

**Implementado:** `src/lights/WledClient.{h,cpp}` (tarea Core0, prioridad 1, arranca tras `WLED_START_DELAY_MS`=5 s; conecta, y cada `WLED_POLL_MS`=30 s hace `GET /json/state` a 191 y 195 una a una; parsea `on`/`bri`/`ps` con `strstr`, sin ArduinoJson), constantes `WLED_*` en `config.h`, `wledInit()` en `setup()`, `src/secrets.h.example` + `src/secrets.h` (ignorado por git, con placeholders: **rellénalo con tu SSID/clave**; mientras valga `TU_SSID` el módulo lo detecta y no conecta).

**Para probar:** 1) edita `src/secrets.h`; 2) compila y sube; 3) abre el monitor serie a 115200 y busca `[WLED]`.

**Plan original de la fase:**

| Archivo | Cambio |
|---|---|
| `platformio.ini` | **Sin cambios necesarios** (core 3.3.7 ya incluye esp-hosted para P4) |
| `src/secrets.h.example` + `.gitignore` | Plantilla de credenciales; `secrets.h` fuera de git |
| `src/config.h` | `WLED_IP_MAIN "192.168.1.191"`, `WLED_IP_WS "192.168.1.195"`, timeouts y pines SDIO (fuente única) |
| `src/lights/WledClient.h/.cpp` | **Nuevo.** Tarea FreeRTOS en **Core0**, nunca en LVGL. Conecta al WiFi y hace `GET /json/state` a ambos WLED con timeout corto |
| `src/main.cpp` | Crear la tarea tras `neotrellisInit()` |

**Resultado esperado en el log serie:**
```
[WLED] WiFi conectado, IP=192.168.1.xxx
[WLED] 191 on=false bri=42
[WLED] 195 on=true  bri=37
```
Si falla: el log debe distinguir C6 sin respuesta / WiFi sin asociar / WLED inalcanzable.

**Criterios de aceptación Fase 1**
- [ ] El C6 responde y el P4 asocia a la red (`[WLED] WiFi conectado, IP=...`).
- [ ] Se lee el estado de los dos WLED.
- [ ] **USB-MIDI y UI siguen igual** (comprobar con MIDI Monitor y la pantalla).
- [ ] Heap libre y PSRAM sin degradación apreciable (log de `ESP.getFreeHeap()` antes/después).
- [ ] La matrix azul, el reposo y los popups no cambian.

### Fase 2 — Control completo

| Archivo | Cambio |
|---|---|
| `src/lights/WledClient.*` | Cola de órdenes (on, bri, ps, color/efecto) hacia 191/195/ambos; una petición a la vez; estado cacheado |
| `src/display/UILights.*` | **Ya existe como relleno (2026-10-12 13:22); aquí se sustituye su contenido.** Pantalla "LUCES": selector Todas/Principal/Workstation, on/off, slider de brillo, lista de presets leída de WLED, después color/efecto |
| `src/main.cpp` | Hook de reposo. Lights ya se abre con L1/botón MODO (2026-10-12 13:22); `UILights.cpp` hoy es relleno |
| Reposo | En la transición de `g_sleeping`: guardar `on`/`bri` y apagar con fundido; al despertar, restaurar |

**Matiz:** L0 pasaría a "dormir ExPressif **y** luces". Si el usuario quiere dormir solo la pantalla, haría falta un ajuste o gesto distinto.

## 8. Riesgos

| Riesgo | Mitigación |
|---|---|
| C6 sin firmware esp-hosted / pines distintos | Fase 1 aislada; log explícito; no construir UI hasta que conecte |
| WiFi hosted degrada USB-MIDI o la UI | Tarea en Core0 con prioridad baja; comprobar con MIDI Monitor; revertible borrando `src/lights/` |
| 195 (S2) saturado | Peticiones pequeñas, secuenciales, con timeout; sin polling continuo |
| Bloqueo del UI por red | HTTP **nunca** en el task LVGL (Core1) |
| Credenciales en git | `secrets.h` ignorado; no copiar la del C3 |

**Riesgo global: MEDIO** (WiFi nuevo sobre hardware sin validar). Solo ExPressif.

## 9. Pendiente de decidir al retomar

1. ¿`secrets.h` (recomendado) o NVS desde pantalla?
2. ¿IP fija en `config.h` o mDNS?
3. ~~¿Cómo se abre la pantalla LUCES?~~ **Resuelto (2026-10-12 13:22):** es el modo **Lights** del botón de función L1 (modos **ExPressif ↔ Lights**). La pantalla `UILights` ya existe como **relleno**; la Fase 2 sustituye su contenido.
4. ~~¿Empezamos por la Fase 1?~~ **Hecho (2026-10-12 13:36)** con los valores recomendados: `secrets.h` ignorado por git e IP fija en `config.h`. Pendiente validar en banco.

## 10. Documentación relacionada

- **[WIFI_EXPRESSIF.md](WIFI_EXPRESSIF.md)** (2026-10-12 15:17) — documentación técnica de la capa WiFi ya validada (arquitectura, API, UI, trampas, diagnóstico).

- `CHANGELOG.md` → Pendientes (entrada "ExPressif: control de iluminación WLED por WiFi").
- `docs/NEOTRELLLIS.md` §7 (reposo, brillo, teclas L0/L5, popups, matrix azul).
- `C3_PowerRelay/` (controla hoy el 195 y un relé de 10 V).
