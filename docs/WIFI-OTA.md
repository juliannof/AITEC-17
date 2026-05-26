# WiFi y OTA — Provisioning y Actualización Remota (iMakie)

Documentación exhaustiva del subsistema WiFi, provisioning de credenciales, OTA (Over-The-Air) firmware updates, y almacenamiento NVS.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (ElegantOTA 3.1.7 funcional)

---

## 1. CREDENCIALES Y CONFIGURACIÓN

### 1.1 Credenciales Predeterminadas

```cpp
// config.h (S2)
#define WIFI_SSID_DEFAULT      "<TU_SSID>"
#define WIFI_PASS_DEFAULT      "<TU_PASS>"
#define OTA_PASS_DEFAULT       "<TU_OTA_PASS>"
```

**Red WiFi:**
- SSID: configurado en NVS o `WIFI_SSID_DEFAULT` en `config.h`
- Frecuencia: 2.4GHz (ESP32-S2 no soporta 5GHz)

**OTA:**
- Contraseña de acceso: configurada en NVS o `OTA_PASS_DEFAULT` en `config.h`

### 1.2 Almacenamiento NVS

**Namespace:** `"ptxx"` (S2)

**Claves guardadas:**

| Clave | Tipo | Descripción | Longitud |
|-------|------|-------------|----------|
| `wifiSsid` | string | SSID WiFi | 32 bytes |
| `wifiPass` | string | Contraseña WiFi | 64 bytes |
| `trackId` | uint8 | ID esclavo RS485 (1-9/1-8) | 1 byte |
| `label` | string | Nombre track (Display) | 8 bytes |
| `pwmMin` | uint8 | PWM mínimo motor | 1 byte |
| `pwmMax` | uint8 | PWM máximo motor | 1 byte |
| `touchEn` | bool | Sensor táctil habilitado | 1 byte |
| `motorDis` | bool | Motor deshabilitado | 1 byte |

---

## 2. PROVISIONING DE CREDENCIALES

### 2.1 Flujo Provisioning

**Objetivo:** Almacenar credenciales WiFi en NVS sin necesidad de hardcodear en firmware.

```
Microcontroller (sin credenciales)
         ↓
Sketch provisioning USB (serially)
         ↓
Usuario selecciona SSID
         ↓
Usuario ingresa contraseña
         ↓
Datos guardados en NVS namespace "ptxx"
         ↓
Sketch cierra
         ↓
Firmware principal arranca
         ↓
OtaManager::begin() lee NVS
         ↓
Conecta a WiFi con credenciales guardadas
```

### 2.2 Sketch Provisioning

**Ubicación:** `S2/S2_V1/sketch_provisioning/` (si existe) o `S2/S2_V1/src/main.cpp` (modo provisioning)

```cpp
// Pseudo-código provisioning
void provisioning_mode() {
    Serial.println("=== WiFi Provisioning ===");
    Serial.println("Redes disponibles:");
    
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; i++) {
        Serial.printf("%d. %s (RSSI: %d dBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
    
    Serial.print("Selecciona número (1-N): ");
    String ssid = Serial.readStringUntil('\n');
    
    Serial.print("Ingresa contraseña: ");
    String pass = Serial.readStringUntil('\n');
    
    Serial.print("Ingresa Track ID (1-9): ");
    uint8_t trackId = Serial.readStringUntil('\n').toInt();
    
    // Guardar en NVS
    Preferences prefs;
    prefs.begin("ptxx", false);  // false = read-write
    prefs.putString("wifiSsid", ssid);
    prefs.putString("wifiPass", pass);
    prefs.putUChar("trackId", trackId);
    prefs.end();
    
    Serial.println("✓ Credenciales guardadas. Reiniciando...");
    delay(100);
    ESP.restart();
}
```

### 2.3 Lectura de Credenciales en Boot

```cpp
// main.cpp setup()
void setup() {
    Serial.begin(115200);
    
    Preferences prefs;
    prefs.begin("ptxx", true);  // true = read-only
    
    String wifiSsid = prefs.getString("wifiSsid", WIFI_SSID_DEFAULT);
    String wifiPass = prefs.getString("wifiPass", WIFI_PASS_DEFAULT);
    uint8_t trackId = prefs.getUChar("trackId", 1);
    
    prefs.end();
    
    Serial.printf("[BOOT] SSID: %s, TrackID: %d\n", wifiSsid.c_str(), trackId);
    
    // Pasar al OtaManager
    otaManager.setCredentials(wifiSsid, wifiPass);
    otaManager.setTrackId(trackId);
    otaManager.begin();
}
```

---

## 3. OTA (Over-The-Air) FIRMWARE UPDATES

### 3.1 ElegantOTA 3.1.7

**Librería:** `ayushsharma/ElegantOTA` v3.1.7

**Ventajas:**
- ✅ Web server integrado (puerto 80)
- ✅ Interfaz gráfica en navegador
- ✅ Progreso de carga en tiempo real
- ✅ Validación CRC32 automática
- ✅ Rollback si falla

**Desventajas (vs ArduinoOTA):**
- Requiere WiFi funcional
- Puerto 80 (puede conflictuar)

### 3.2 Integración OTA

```cpp
// OtaManager.cpp
void OtaManager::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid.c_str(), _pass.c_str());
    
    // Esperar conexión (timeout 20s)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[OTA] Conectado a %s, IP: %s\n", 
                      _ssid.c_str(), WiFi.localIP().toString().c_str());
        
        // Iniciar ElegantOTA
        ElegantOTA.begin(&server);
        server.begin();
        
        Serial.println("[OTA] Web server en puerto 80");
        Serial.println("[OTA] Accede a: http://<IP>/update");
    } else {
        Serial.println("[OTA] ✗ Falló conexión WiFi");
    }
}

void OtaManager::update() {
    // Llamar en loop() para procesar peticiones OTA
    server.handleClient();
}
```

### 3.3 Flujo OTA

```
Usuario abre navegador
         ↓
http://<IP_S2>/update
         ↓
Interfaz gráfica ElegantOTA
         ↓
Selecciona firmware.bin (compilado localmente)
         ↓
Carga a S2 (progreso visual)
         ↓
S2 valida CRC32
         ├─ Si OK: actualiza flash, reinicia
         └─ Si error: rollback automático
         ↓
Firmware nuevo corre
```

### 3.4 Acceso OTA en SAT

```
Display normal
         ↓
Encoder push >3s
         ↓
SAT menu
         ↓
Opción "WiFi OTA"
         ↓
SAT cierra, pantalla apagada (ahorra RAM)
         ↓
OTA habilitado
         ↓
Esperando conexión web
         ↓
Usuario carga firmware
         ↓
Reinicia con nuevo firmware
```

**Código SAT:**
```cpp
// SatMenu.cpp - callback WiFi OTA
static void _satWiFiOta() {
    satMenu->close();
    setScreenBrightness(0);  // Apagar pantalla (libera PSRAM)
    
    Preferences prefs;
    prefs.begin("ptxx", false);
    prefs.putBool("otaMode", true);  // Flag para boot
    prefs.end();
    
    Serial.printf("[SAT] OTA mode habilitado, reiniciando...\n");
    delay(100);
    ESP.restart();
}
```

---

## 4. BOOT OTA-ONLY MODE

### 4.1 Flujo Detección OTA-Only

```cpp
// main.cpp setup()
void setup() {
    // 1. Detectar flag otaMode
    Preferences prefs;
    prefs.begin("ptxx", true);
    bool otaMode = prefs.getBool("otaMode", false);
    prefs.end();
    
    if (otaMode) {
        Serial.println("[BOOT] === OTA-ONLY MODE ===");
        
        // 2. Limpiar flag INMEDIATAMENTE (una sola ejecución)
        Preferences prefs2;
        prefs2.begin("ptxx", false);
        prefs2.remove("otaMode");
        prefs2.end();
        
        // 3. Inicializar MÍNIMO: display sin sprites + WiFi OTA
        initDisplay(true);  // true = otaOnlyMode
        otaManager.begin();
        otaManager.enableForUpload(true);
        
        // 4. Si WiFi falló, reiniciar
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[OTA-ONLY] WiFi falló, reiniciando...");
            delay(100);
            ESP.restart();
        }
        return;  // Nunca llega aquí si OTA exitoso
    }
    
    // MODO NORMAL: boot completo
    initNeopixels();
    initDisplay();
    // ... resto del setup
}
```

**Ventaja:** Libera PSRAM para OTA (no crea sprites en OTA-only mode).

### 4.2 Estados OTA-Only

| Estado | Duración | Acción |
|--------|----------|--------|
| **Boot** | 100ms | Detecta flag, limpia, inicia display mínimo |
| **WiFi connect** | <20s | Conecta a SSID guardado |
| **OTA wait** | indefinido | Espera carga web |
| **Upload** | variable | Recibe firmware.bin |
| **Flash** | 2-5s | Escribe flash, valida CRC32 |
| **Reboot** | 100ms | Reinicia con firmware nuevo |

---

## 5. TROUBLESHOOTING WIFI/OTA

### 5.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| **WiFi scan devuelve 0 redes** | **GPIO flotantes — ver Bug #1** | **Añadir bloque safePins antes de WiFi.begin()** |
| WiFi no conecta | Credenciales incorrectas en NVS | Reflashear sketch provisioning |
| OTA no aparece | WiFi no conectado | Check log serial: `[OTA] IP address:` |
| Upload lento | Red congestionada | Intenta cerca del router (2-3m) |
| Falla CRC32 | Firmware corrompido en upload | Reintenta, verifica archivo .bin |
| Reinicia infinito | otaMode flag stuck | Limpiar NVS: `prefs.remove("otaMode")` |
| PSRAM leak | Sprites no liberados en OTA-only | Verificar `initDisplay(true)` |

---

### 5.3 Bug #1 — GPIO Flotantes Bloquean WiFi (2026-05-26) 🔴 PENDIENTE FIX

**Síntoma:** `WiFi.scanNetworks()` devuelve 0 redes. WiFi no conecta en OTA-only mode aunque las credenciales sean correctas.

**Root cause:** El ESP32-S2 tiene ~33 GPIO disponibles. En OTA-only mode, `setup()` solo inicializa `Motor::init()` + `initDisplay(true)` — el resto de pines (NeoPixels, encoder, botones, FaderTouch, ADS1115 I2C, RS485) quedan **flotantes**. Los pines flotantes generan interferencia RF que ciega el radio WiFi.

**Impacto:** OTA no funciona en unidades nuevas ni en cualquier unidad que entre en OTA-only mode.

**Workaround probado:** El sketch de provisioning incluye un bloque `safePins` que pone todos los GPIO disponibles en `OUTPUT LOW` antes de inicializar WiFi. Con este bloque, `scanNetworks()` detecta redes y WiFi conecta correctamente.

**GPIOs seguros a LOW** (excluidos: GPIO0 bootstrap, 19-20 USB, 26-32 QSPI flash/PSRAM, 46 input-only):
```cpp
const uint8_t safePins[] = {
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
    11, 12, 13, 14, 15, 16, 17, 18, 21,
    33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45
};
for (uint8_t pin : safePins) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}
```

**Fix aplicado (2026-05-26):** Bloque `safePins` añadido al inicio de `setup()` en `main.cpp`, antes del check `otaMode` y antes de `Motor::init()` — aplica tanto en boot normal como en OTA-only mode. Los módulos posteriores reconfiguran sus pines en su propio `init()`.

**Fix adicional (2026-05-26):** `OtaManager::enableForUpload()` ahora pasa `otaPass` a `ElegantOTA.begin()` → Basic Auth activo si NVS tiene contraseña OTA.

**Pendiente:** Flash + validación hardware. Sketch provisioning en `S2/provisioning/`.

### 5.2 Debugging Logs

**Conexión WiFi exitosa:**
```
[OTA] Buscando SSID: <TU_SSID>
[OTA] Conectado, IP: 192.168.1.100
[OTA] Web server en puerto 80
[OTA] Accede a: http://192.168.1.100/update
```

**OTA upload exitoso:**
```
[OTA] Cliente conectado
[OTA] Cargando firmware (512KB)
[OTA] Progress: 25% | 50% | 75% | 100%
[OTA] CRC32: OK
[OTA] Flash write: OK
[OTA] Reiniciando...
```

**OTA-only mode:**
```
[BOOT] === OTA-ONLY MODE ===
[BOOT] Flag otaMode limpiado
[OTA-ONLY] Display iniciado (sin sprites)
[OTA-ONLY] WiFi conectado, IP: 192.168.1.101
[OTA-ONLY] Esperando upload...
```

---

## 6. HISTORIAL CAMBIOS

### 6.0 Bug #1 — GPIO Flotantes (2026-05-26) — 🔴 Pendiente fix firmware

**Descubierto:** Sketch provisioning funciona solo cuando se añade bloque `safePins` (OUTPUT LOW en todos los GPIO disponibles). Sin él, `WiFi.scanNetworks()` devuelve 0 — radio WiFi cegado por interferencia de pines flotantes.

**Afectado:** OTA-only mode en `main.cpp` — no llama a `initHardware()` → pines RS485, encoder, botones, NeoPixels, FaderTouch flotantes.

**Fix aplicado:** Bloque `safePins` añadido al inicio de `setup()` antes del check `otaMode`. `otaPass` ahora se pasa a `ElegantOTA.begin()`. Pendiente flash + validación.


### 6.1 2026-05-14: Migración ArduinoOTA → ElegantOTA

**Problema:** ArduinoOTA 3.x no compatible con pioarduino 55.03.37
- Error: missing `esp_ota_get_state_partition()`
- Bloqueaba todo OTA

**Fix:** Cambiar a ElegantOTA 3.1.7
- ✅ Compatible IDF5
- ✅ Web UI visual
- ✅ Mejor feedback usuario

**Commit:** (historial CHANGELOG.md)

### 6.2 OTA-Only Mode (2026-05-10)

**Problema:** SAT abre en WiFi OTA → PSRAM agotado (sprites + OTA server)

**Fix:** Detectar flag `otaMode` en boot
- Saltar sprite creation
- Inicializar display mínimo
- Liberar PSRAM para servidor OTA

---

## 7. REFERENCIAS

- **CLAUDE.md** — Directivas obligatorias
- **MOTOR.md** — SAT callback para WiFi OTA
- **STATUS.md** — Bugs conocidos WiFi/OTA
- **CHANGELOG.md** — Historial actualización ElegantOTA

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado WIFI.md como documento exhaustivo, trasladado contenido de CLAUDE.md
- **(2026-05-26)** Renombrado WIFI.md → WIFI-OTA.md (nombre más descriptivo)
