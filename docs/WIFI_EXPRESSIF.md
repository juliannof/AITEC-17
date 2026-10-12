# WIFI_EXPRESSIF — WiFi en el ExPressif (ESP32-P4 + ESP32-C6)

> **Estado (2026-10-12 15:16):** capa WiFi **validada en banco** con el log del usuario: asociación
> a la red de casa, lectura del estado de los dos WLED, credenciales por pantalla en NVS y barrido de
> redes. El **control de luces (Fase 2)** está **sin implementar**; ver [EXPRESSIF_WLED.md](EXPRESSIF_WLED.md).
> Proyecto: `MASTER_S3-P4/P4_JC4880P433C/` (ExPressif). No afecta a S2, S3 ni P4_JC1060P470C.

---

## 1. Por qué importa

El ESP32-P4 **no tiene radio WiFi**. Esta placa lleva un **ESP32-C6** como coprocesador de radio,
conectado por **SDIO** y gestionado con **ESP-Hosted**. El ExPressif, que hasta ahora solo hablaba
USB-MIDI y NeoTrellis, pasa a poder hablar con cualquier dispositivo de la red local:

- **Hoy (validado):** leer el estado de los dos WLED de casa.
- **Siguiente (Fase 2, pendiente):** controlarlos (on/off, brillo, presets, color) y apagarlos al dormir.
- **Ideas, no planificadas:** OTA del firmware, hora por NTP, otros dispositivos HTTP/UDP de la red.

## 2. Hardware

| Elemento | Valor | Origen |
|---|---|---|
| Radio | ESP32-C6 (solo **2.4 GHz**) | Confirmado por el usuario |
| Enlace | SDIO, ESP-Hosted (`CONFIG_ESP_WIFI_REMOTE_ENABLED=y`, esclavo `ESP32C6`) | `sdkconfig` del core instalado |
| CLK / CMD | GPIO 18 / GPIO 19 | Variante `esp32p4` del core |
| D0-D3 | GPIO 14, 15, 16, 17 | Variante `esp32p4` del core |
| Reset del C6 | GPIO 54 | Variante `esp32p4` del core |
| Core Arduino | Arduino-ESP32 **3.3.7** (platform pioarduino 55.03.37) | `framework-arduinoespressif32` |

- **No hace falta tocar `platformio.ini`**: el core trae ESP-Hosted para P4 precompilado y esos
  pines son los de la placa (validado: el C6 responde).
- Sin conflicto con los GPIO del ExPressif (5, 7, 8, 23, 31, 33).
- Si otra placa usara pines distintos, habría que llamar a `WiFi.setPins(clk, cmd, d0, d1, d2, d3, rst)`
  **antes** de `WiFi.mode()`.
- El C6 **no ve redes de 5 GHz**. Si el SSID es solo 5 GHz no aparecerá en el barrido.

## 3. Arquitectura software

```
 Core 1 (LVGL / UI)                          Core 0 (tarea "WLED", prioridad 1)
 ───────────────────                         ───────────────────────────────────
 UILights ──▶ botón CONFIGURAR WIFI
 UIWifiCfg ──▶ wledScanRequest()  ─────────▶ doScan()  (WiFi.scanNetworks)
           ◀── wledScanGet()/Seq()  ◀──────  lista en s_nets (mutex)
           ──▶ wifiStoreSave() (NVS)
           ──▶ wledCredentialsChanged() ───▶ reconecta sin reiniciar
           ◀── wledWifiState()/wledWifiIp()  máquina de estados + poll WLED
                                             fetchState() ──▶ HTTP GET /json/state
                                                              ├─ 192.168.1.191 (principal)
                                                              └─ 192.168.1.195 (workstation)
```

**Reglas:**

1. **HTTP y WiFi viven solo en la tarea WLED (Core 0).** Nunca desde el task LVGL (Core 1).
2. La UI solo llama a la API `wled*` (no bloqueante) y lee estado cacheado.
3. La tarea arranca a los `WLED_START_DELAY_MS` (5 s) para no competir con el arranque de USB-MIDI y la UI.
4. **Interruptor:** `WLED_ENABLE 0` en `config.h` desactiva todo el módulo (incluido el WiFi).
5. Una petición HTTP a la vez y con timeout corto: el WLED 195 es un ESP32-S2 con ~67 KB de heap libre.

### Archivos

| Archivo | Papel |
|---|---|
| `src/lights/WledClient.{h,cpp}` | Tarea, máquina de estados WiFi, barrido, lectura de WLED |
| `src/nvs/WifiStore.{h,cpp}` | Credenciales en NVS (namespace `wifi`) |
| `src/display/UIWifiCfg.{h,cpp}` | Pantalla de SSID/clave, lista de redes y teclado |
| `src/display/UILights.{h,cpp}` | Pantalla del modo Lights; hoy de relleno + botón CONFIGURAR WIFI |
| `src/display/UIMode.{h,cpp}` | Alterna modos ExPressif ↔ Lights (L1 / botón MODO) |
| `src/secrets.h.example` | Plantilla de credenciales de respaldo (`secrets.h` ignorado por git) |
| `src/config.h` | Constantes `WLED_*` y `WIFI_*` |

## 4. Estados del WiFi

```
        credenciales (NVS o secrets.h)
NOCREDS ───────────────────────────────▶ CONNECTING ──ok──▶ CONNECTED ──poll cada 30 s──▶ WLED
   ▲                                          │                    │
   │ (sin credenciales: espera 500 ms)        └─fallo (20 s)─▶ FAILED ──reintento a 10 s──┐
   └──────────────────────────────────────────────────────────────────────◀──────────────┘
        wledCredentialsChanged() ⇒ WiFi.disconnect() y vuelve a CONNECTING (sin reiniciar)
```

- **Prioridad de credenciales:** NVS primero; `secrets.h` solo de respaldo (si no vale el placeholder `TU_SSID`).
- **Sin credenciales** la tarea **espera** (no termina): en cuanto se guardan desde la pantalla, conecta.
- El estado se expone con `wledWifiState()`; la IP con `wledWifiIp()`.

## 5. Barrido de redes

`doScan()` (en la tarea WLED):

1. Si el WiFi no estaba iniciado, `WiFi.mode(WIFI_STA)` (arranca ESP-Hosted aunque no haya credenciales).
2. **Si no está conectado:** desactiva el auto-reintento y hace `WiFi.disconnect()` (ver §10, trampa 5).
3. `WiFi.scanNetworks(async=true)` y espera propia de `WLED_SCAN_TIMEOUT_MS` (15 s; la librería bloqueante espera 60 s).
4. Un reintento si el primer lanzamiento falla.
5. Deduplica por SSID (se queda con el más fuerte), descarta ocultas y ordena por RSSI. Máx. `WLED_SCAN_MAX` = 12.

| `wledScanError()` | Significado |
|---|---|
| 0 | OK |
| -1 | `WiFi.mode(STA)` falló: el C6 / ESP-Hosted no inicia |
| -2 | El barrido no se lanzó |
| -3 | Timeout: el C6 no devuelve el fin del barrido |

## 6. API pública (`lights/WledClient.h`)

| Función | Uso |
|---|---|
| `wledInit()` | Crea la tarea; llamar al final de `setup()` |
| `wledWifiState()` / `wledWifiIp()` / `wledWifiConnected()` | Estado para la UI |
| `wledScanRequest()` / `wledScanBusy()` / `wledScanSeq()` / `wledScanGet()` / `wledScanError()` | Barrido de redes |
| `wledCredentialsChanged()` | Tras guardar credenciales nuevas |
| `wledGetCached(dev, out)` | Último `on` / `bri` / `ps` leído, sin red |

`WledDev`: `WLED_DEV_MAIN` (191), `WLED_DEV_WS` (195).

## 7. Pantalla de configuración (UIWifiCfg)

Acceso: modo **Lights** (L1 o botón MODO) → **CONFIGURAR WIFI**.

- **Sin SSID guardado:** barre las redes al abrir y muestra la lista (icono, nombre, dBm, "(abierta)").
  Tocas tu red → pasa directo a la **CLAVE**.
- **Teclado en pantalla:** 4 filas × 10 teclas, minúsculas / mayúsculas / símbolos.
  **MAYUS:** 1 toque = una mayúscula, 2.º = **BLOQ**, 3.º = minúsculas. `?123` cambia a símbolos.
- **Botones:** MAYUS, ?123, ESPACIO, BORRAR, **VER** (muestra la clave), **REDES** (barrer de nuevo),
  CANCELAR, **GUARDAR** (NVS + reconexión inmediata). **ESCRIBIR A MANO** para redes ocultas.
- Estado en vivo arriba: `CONECTANDO...`, `CONECTADO <IP>`, `ERROR: SSID/CLAVE?`, `SIN CREDENCIALES`.
- Longitudes máximas: SSID 32, clave 63.

## 8. Constantes (`config.h`, fuente única)

| Constante | Valor | Significado |
|---|---|---|
| `WLED_ENABLE` | 1 | 0 desactiva el módulo |
| `WLED_IP_MAIN` / `WLED_IP_WS` | 192.168.1.191 / .195 | Principal / workstation |
| `WLED_START_DELAY_MS` | 5000 | Retardo antes de iniciar WiFi |
| `WLED_WIFI_TIMEOUT_MS` | 20000 | Espera máx. a asociar |
| `WLED_HTTP_TIMEOUT_MS` | 2000 | Por petición HTTP |
| `WLED_POLL_MS` | 30000 | Lectura periódica (diagnóstico, Fase 1) |
| `WLED_SCAN_TIMEOUT_MS` | 15000 | Espera máx. del barrido |
| `WLED_SCAN_MAX` | 12 | Redes máx. guardadas |
| `WLED_TASK_STACK` | 8192 | Pila de la tarea |
| `WIFI_SSID_MAX` / `WIFI_PASS_MAX` | 32 / 63 | Longitudes máximas |

## 9. Dispositivos WLED

Datos leídos con consultas de solo lectura (2026-10-11):

| | **191 "Lamparita"** (principal) | **195 "Led Ordenador"** (workstation) |
|---|---|---|
| Chip | ESP32 | ESP32-S2 (poca RAM) |
| LEDs / segmentos | 697 / ≥10 | 83 / 4 |
| AudioReactive | I2S, activo | apagado |
| Sync UDP (grupo 1) | envía y recibe | solo recibe |
| Versión | WLED 0.15.0, 187 efectos, 71 paletas | idem |

- **Endpoint usado hoy:** `GET /json/state`; se extraen `on`, `bri` y `ps` con `strstr()` (son los primeros campos).
- **No fiarse del sync UDP** para el control: la Fase 2 enviará HTTP a ambos directamente.
- El 195 es el que ya maneja el proyecto `C3_PowerRelay`.

## 10. Lecciones aprendidas (trampas encontradas en esta integración)

| # | Trampa | Síntoma | Solución |
|---|---|---|---|
| 1 | `lv_obj_remove_style_all()` **después** de `lv_obj_set_pos/size` (en LVGL 9 son estilos locales y se borran) | La lista se ve pero **no responde al toque**; teclas apiladas en (0,0) | Llamarlo **antes** de colocar/dimensionar |
| 2 | `lv_textarea` rotado + render parcial en bandas con búfers en PSRAM + cursor parpadeante | Teclado **muy lento** | Caja sin rotar + `lv_label` rotado, buffers propios, sin animación de cursor |
| 3 | `lv_keyboard` es un `lv_buttonmatrix` y su detección de tecla **ignora la rotación** (LVGL 9.5.0) | Tocaría otra tecla distinta de la que se ve | Teclas `lv_button` + `lv_label` con `rot_label()` |
| 4 | Abrir un namespace NVS inexistente en solo lectura | `[E] nvs_open failed: NOT_FOUND` repetido | Abrirlo en lectura/escritura |
| 5 | Barrer mientras la STA **asocia o reintenta** | `scanNetworks` devuelve -2, "el C6 no lanza el barrido" | Cortar el reintento y `disconnect()` antes de barrer |
| 6 | Tratar el código negativo de `scanNetworks` como "0 redes" | Fallo oculto | `wledScanError()` y mensaje real en pantalla |
| 7 | Barrido bloqueante de la librería | Hasta 60 s colgado | Asíncrono con espera propia de 15 s |

## 11. Diagnóstico

| Síntoma | Causa probable | Acción |
|---|---|---|
| `SIN CREDENCIALES` | No hay SSID en NVS ni `secrets.h` | Lights → CONFIGURAR WIFI |
| `ERROR: EL C6 NO INICIA EL WIFI` (-1) | ESP-Hosted no arranca: firmware del C6 o pines | Log `ESP-Hosted`/`H_SDIO`; revisar pines |
| `ERROR: EL C6 NO LANZA EL BARRIDO` (-2) | STA asociando, o enlace SDIO caído | Reintentar; ver log `[WLED] barriendo redes (estado WiFi=...)` |
| `TIMEOUT DEL BARRIDO` (-3) | El C6 no devuelve el fin | Log; reiniciar |
| `0 REDES VISTAS` | Router solo 5 GHz, o fuera de alcance | Comprobar que el SSID emite en 2.4 GHz |
| `ERROR: SSID/CLAVE?` | Clave errónea | Volver a introducirla |
| `HTTP -1 ... Host is unreachable` | Sin WiFi en ese instante | Normal mientras reconecta |

Monitor serie a 115200; prefijo `[WLED]`.

## 12. Seguridad

- La clave WiFi queda **en claro en NVS** (no cifrado). Aceptable para este equipo; no sale del dispositivo ni va a git.
- `src/secrets.h` está en `.gitignore`. **No copiar** credenciales a `config.h` ni a documentación.
- ⚠️ `C3_PowerRelay/src/config.h` tiene SSID y clave en claro **dentro del repo**: si el repo es público,
  rotar la clave y moverla a un fichero ignorado.
- Los WLED no tienen autenticación por defecto: solo accesibles en la LAN.

## 13. Validación y pendientes

**Validado en banco (2026-10-12, log del usuario):**
- [x] El C6 responde con los pines por defecto y asocia (`<tu SSID>`, IP 192.168.1.54, RSSI -38).
- [x] Lectura de ambos WLED, contrastada con `GET /json/state` real.
- [x] Credenciales por pantalla en NVS; reconexión sin reiniciar.
- [x] Barrido de redes; selección de red y entrada de la clave.

**Sin validar / pendiente:**
- [ ] Bloqueo de mayúsculas (BLOQ) y todas las teclas del teclado en uso real prolongado.
- [ ] Impacto del WiFi sobre USB-MIDI en uso real (comprobar con MIDI Monitor).
- [ ] **Fase 2:** control (on/off, brillo, presets, color), selector Todas/Principal/Workstation,
  pantalla Lights real y **apagado de las luces al entrar en reposo**.

## 14. Cómo extender

- **Otro dispositivo WLED:** añadir `WLED_IP_xxx` en `config.h`, una entrada en `WledDev` y en
  `kDevIp`/`kDevName` de `WledClient.cpp`; mantener las peticiones secuenciales.
- **Otras acciones HTTP:** hacerlas siempre dentro de la tarea WLED, con timeout corto.
- **Control (Fase 2):** una cola de órdenes hacia la tarea; la UI nunca bloquea.

## 15. Documentación relacionada

- [EXPRESSIF_WLED.md](EXPRESSIF_WLED.md) — brief del control de luces (decisiones y Fase 2).
- [NEOTRELLLIS.md](NEOTRELLLIS.md) §7 — reposo, modos ExPressif/Lights, teclas L0/L1/L5.
- `CHANGELOG.md` — historial de sesiones (2026-10-12).
