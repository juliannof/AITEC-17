# DISPLAY — Pantalla ST7789V3 y Sprites PSRAM (iMakie S2)

Documentación exhaustiva del subsistema de display. Incluye hardware ST7789V3, layout, sprites LovyanGFX, actualización, y troubleshooting.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-26  
**Estado:** En producción (240×280 SPI3, sprites PSRAM)

---

## 1. HARDWARE DISPLAY

### 1.1 Panel ST7789V3

| Parámetro | Valor |
|-----------|-------|
| **Chip** | ST7789V3 |
| **Resolución** | 240×280 píxeles |
| **Interface** | SPI3_HOST |
| **Frecuencia escritura** | 10MHz (unidireccional, sin MISO) |
| **Frecuencia lectura** | 8MHz |
| **Modo color** | RGB565 (16-bit) |
| **Backlight** | PWM 500Hz, GPIO3 |
| **Alimentación** | Rail 5V (PCB V2) |

### 1.2 Configuración LovyanGFX

```cpp
// LovyanGFX_config.h
tft.setColorDepth(16);           // RGB565
tft.setMemoryHeightInBit(320);   // memory_height
tft.setOffsetYInBit(20);         // offset_y (ST7789 interno)
tft.setInvert(true);             // invertir colores
tft.setRGBOrder(false);          // GRB order (no RGB)
tft.setFrequency(10000000, 8000000);  // write=10MHz, read=8MHz
```

**Pulso RST obligatorio:**
```cpp
// ANTES de tft.init()
digitalWrite(GPIO33, LOW);
delay(100);
digitalWrite(GPIO33, HIGH);
tft.init();
```

### 1.3 Pinout Display

| Señal | GPIO | Función |
|-------|------|---------|
| SCLK (CLK) | 7 | Clock SPI3 |
| MOSI (DIN) | 4 | Data in (sin MISO) |
| DC | 6 | Data/Command select |
| CS | 5 | Chip Select |
| RST | 33 | Reset (manual pulso 100ms) |
| BL | 3 | Backlight PWM 500Hz |

---

## 2. LAYOUT DISPLAY

### 2.1 Estructura Visual

```
┌─────────────────────────────┐  Y=0
│     [Header 40px]           │  ← Track name + flags (REC/SOLO/MUTE/SELECT)
├─────────────────────────────┤  Y=40
│                             │
│      [Main Area]            │
│      (180×240)              │  ← Gráfico barras/fader + info
│                             │
│                             │
├─────────────────────────────┤  Y=280
│     [VU Meter 60px]         │  ← Pico + decay exponencial
├─────────────────────────────┤  Y=340
│     [VPot Ring 60px]        │  ← Anillo 15 posiciones (-7..+7) + encoder
└─────────────────────────────┘  Y=400

Total virtual: 240×400 (offset_y=20 en ST7789 físico)
```

**Componentes:**

| Sprite | Dimensiones | Localización | Contenido |
|--------|------------|--------------|----------|
| `header` | 240×40 | Y=0-39 | Track name + flags |
| `mainArea` | 180×240 | Y=40-279 | Fader gráfico, info |
| `vuSprite` | 60×240 | Y=40-279 (right) | VU meter vertical |
| `vPotSprite` | 240×60 | Y=340-399 | VPot ring + encoder |

### 2.2 Sprites PSRAM

**Obligatorio:** PSRAM habilitado y `setPsram(true)` antes de `createSprite()`

```cpp
// Display.cpp setup()
mainArea.setColorDepth(16);
mainArea.setPsram(true);
mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);

header.setColorDepth(16);
header.setPsram(true);
header.createSprite(TFT_WIDTH, HEADER_HEIGHT);

vuSprite.setColorDepth(16);
vuSprite.setPsram(true);
vuSprite.createSprite(TFT_WIDTH - MAINAREA_WIDTH, MAINAREA_HEIGHT);

vPotSprite.setColorDepth(16);
vPotSprite.setPsram(true);
vPotSprite.createSprite(TFT_WIDTH, VPOT_HEIGHT);
```

**Verificación dirección PSRAM:**
```cpp
if (esp_ptr_external_ram(mainArea.getBuffer())) {
    Serial.println("[PSRAM] ✓ mainArea en PSRAM (0x3f8xxxxx)");
}
```

---

## 3. ACTUALIZACIÓN DISPLAY

### 3.1 Ciclo Principal

```cpp
// main.cpp loop()
updateDisplay();

// Display.cpp
void updateDisplay() {
    if (needsTOTALRedraw) {
        redrawAll();           // Todas sprites
        needsTOTALRedraw = false;
    } else if (needsVPotRedraw) {
        redrawVPot();          // Solo VPot ring
        needsVPotRedraw = false;
    }
    // ... pushImage() a tft
}
```

### 3.2 Banderas de Redibujado

```cpp
// Declaradas en Display.cpp
extern bool needsTOTALRedraw;      // Redibuja todo
extern bool needsMainAreaRedraw;   // Main + VU
extern bool needsHeaderRedraw;     // Solo header
extern bool needsVUMetersRedraw;   // Solo VU
extern bool needsVPotRedraw;       // Solo VPot ring
```

**Cuándo actualizar:**
- `needsTOTALRedraw` — SAT cierra, boot, conexión RS485 restaurada
- `needsVPotRedraw` — Encoder movió (cada cambio)
- `needsMainAreaRedraw` — Fader cambió posición (>5 cuentas delta)
- `needsVUMetersRedraw` — VU level cambió (actualización continua)

### 3.3 Timing Actualización

```
Frame target: 30 FPS (33ms/frame)

Loop:
  0ms    updateDisplay()
    ├─ Redibuja sprites en RAM (PSRAM)
    ├─ pushImage() a tft (SPI3)
    └─ ~5-10ms total
  
  5ms    Otras tareas (motor, encoder, buttons)
  
  33ms   Next frame
```

**Latencia:** <50ms típicamente (imperceptible)

---

## 4. CONFIGURACIÓN LOVYANGFX

### 4.1 platformio.ini (S2)

```ini
[env:lolin_s2_mini]
platform = espressif32
board = lolin_s2_mini
framework = arduino
board_build.arduino.memory_type = qio_qspi
```

**Obligatorio:** `qio_qspi` para PSRAM (QSPI en paralelo con flash)

### 4.2 Bibliotecas

```
LovyanGFX 1.2.19 (sin NeoPixelBus — conflicto LEDC)
Adafruit_NeoPixel (no NeoPixelBus)
```

---

## 5. TROUBLESHOOTING DISPLAY

### 5.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| Display negro | RST no ejecutado o SPI desconectado | Verificar pulso GPIO33 ANTES init() |
| Imagen invertida | Configuración invert/rgb_order mal | Revisar LovyanGFX_config.h |
| Parpadeos frecuentes | needsTOTALRedraw siempre true | Check SAT menu, conexión RS485 |
| Sprites no se crean | PSRAM no habilitado | Verificar `setPsram(true)` ANTES `createSprite()` |
| Memoria insuficiente | Sprites demasiado grandes o sin PSRAM | Usar `esp_ptr_external_ram()` para debug |
| SPI timeout | Frecuencia escriba demasiado alta | Reducir freq_write de 10MHz a 5MHz |
| Display lag | Redibujado bloqueante en loop principal | Usar banderas needsXXXRedraw (no redibuja siempre) |

### 5.2 Debugging Logs

**Display inicializado correctamente:**
```
[DISPLAY] Inicializando ST7789V3
[DISPLAY] LovyanGFX init OK
[DISPLAY] Sprites PSRAM:
  header: 19200 bytes (0x3f8xxxxx)
  mainArea: 86400 bytes (0x3f8xxxxx)
  vuSprite: 28800 bytes (0x3f8xxxxx)
  vPotSprite: 28800 bytes (0x3f8xxxxx)
[DISPLAY] Total: 163200 bytes
[DISPLAY] ✓ Ready
```

**Pulso RST debug:**
```cpp
Serial.println("[DISPLAY] Pulso RST en GPIO33...");
digitalWrite(33, LOW);
delay(100);
digitalWrite(33, HIGH);
Serial.println("[DISPLAY] RST listo");
```

---

## 6. SAT Y DISPLAY

### 6.1 SAT Menu Suspende Sprites

```cpp
// SatMenu.cpp - _satSuspendSprites()
void _satSuspendSprites() {
    header.deleteSprite();
    mainArea.deleteSprite();
    vuSprite.deleteSprite();
    vPotSprite.deleteSprite();
    log_i("Sprites suspendidos | PSRAM libre: %d", ESP.getFreePsram());
}
```

**Razón:** Libera ~163KB PSRAM para SAT menu (diagnósticos)

### 6.2 SAT Restaura Sprites

```cpp
// Cuando cierra SAT o usuario selecciona "Restore"
void _satRestoreSprites() {
    mainArea.setColorDepth(16);
    mainArea.setPsram(true);
    mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);
    // ... resto de sprites
    needsTOTALRedraw = true;
}
```

---

## 7. COLORES Y PALETA

### 7.1 Macros Colores (16-bit RGB565)

```cpp
// config.h
#define COLOR_16_BITS(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))

// VU Meter colores
#define VU_GREEN_OFF  COLOR_16_BITS(0, 20, 0)       // Verde oscuro
#define VU_GREEN_ON   TFT_GREEN                     // Verde brillante
#define VU_YELLOW_OFF COLOR_16_BITS(20, 20, 0)      // Amarillo oscuro
#define VU_YELLOW_ON  TFT_YELLOW                    // Amarillo brillante
#define VU_RED_OFF    COLOR_16_BITS(20, 0, 0)        // Rojo oscuro
#define VU_RED_ON     TFT_RED                       // Rojo brillante
// VU_PEAK_COLOR (gris) OBSOLETO (2026-05-26): peak usa blendColor565() ON↔OFF
```

---

## 8. HISTORIA CAMBIOS

### 8.1 2026-05-10: Calibración Brillo

**Problema:** Pantalla demasiado brillante (100%) o demasiado oscura

**Fix:** `setScreenBrightness(255)` en boot, slider SAT 0-255

**Status:** ✅ Resuelto

### 8.3 2026-05-27: Brightness a config.h — valores revisados

Todos los valores hardcodeados movidos a `config.h` como fuente única de verdad.

| Define | Valor | Momento |
|--------|-------|---------|
| `BRIGHTNESS_SPLASH` | 50 | Boot, splash, espera Logic desconectado |
| `BRIGHTNESS_SELECTED` | 180 | Canal activo/seleccionado (Logic conectado) |
| `BRIGHTNESS_UNSELECTED` | 70 | Canal no seleccionado |
| `BRIGHTNESS_OTA` | 50 | Pantalla OTA WiFi |

**Comportamiento añadido (2026-05-27):** Al desconectar Logic (`DISCONNECTED` o timeout RS485), `RS485Handler.cpp` llama `drawSplashScreen()` + `setScreenBrightness(BRIGHTNESS_SPLASH)` — pantalla vuelve al splash en lugar de apagarse a negro.

**Status:** ✅ Implementado, pendiente validación hardware

### 8.2 2026-05-26: VU Meter — refactor completo

| Cambio | Detalle |
|--------|---------|
| Dibujo diferencial | Solo redibuja segmentos cuyo color cambió respecto al frame anterior |
| Fix decay timer | `vuLastUpdateTime` solo se actualiza cuando VU sube (no cada paquete RS485) |
| Fix S3 stale VU | Timeout 200ms en S3: si Logic no envía Channel Pressure → fuerza vuLevel=0 |
| Peak hardware-style | Segmento peak se dibuja en su color ON (verde/amarillo/rojo), no en blanco/gris |
| Peak fade 300ms | Tras hold 2s, peak se desvanece con blendColor565() en 12 pasos × 25ms |

**Status:** ✅ Implementado, pendiente validación hardware

---

## 10. VU METER — Arquitectura Completa (2026-05-26)

### 10.1 Visión General

El VU meter se dibuja **directamente sobre `tft`** (no usa sprite). Ocupa la banda derecha de la pantalla junto a `mainArea`. El redibujado es diferencial: solo los segmentos cuyo color cambió respecto al frame anterior se repintan.

### 10.2 Geometría (`namespace VU`)

```cpp
namespace VU {
    static constexpr int X      = MAINAREA_WIDTH + 3;    // 183px desde izquierda
    static constexpr int Y_TOP  = HEADER_HEIGHT + 4;     // 44px desde arriba
    static constexpr int W      = 42;                    // ancho barra
    static constexpr int H      = MAINAREA_HEIGHT - 10;  // alto total
    static constexpr int SEGS   = 12;                    // segmentos (0=bajo, 11=alto)
    static constexpr int PAD    = 2;                     // hueco entre segmentos
    static constexpr int CORNER = 2;                     // radio esquinas
    static constexpr int SEG_H  = (H - PAD*(SEGS-1)) / SEGS;

    // Estado diferencial
    int8_t   lastActive    = -1;   // -1 = nunca dibujado → fuerza fondo completo
    int8_t   lastPeak      = -1;
    bool     lastClip      = false;
    uint8_t  peakAlpha     = 255;  // alpha del segmento peak (255=pleno, 0=invisible)
    uint8_t  lastPeakAlpha = 255;
    uint32_t peakFadeTime  = 0;    // timestamp inicio paso de fade (0=fade inactivo)
}
```

**Asignación de colores por segmento:**

| Segmentos | Color ON | Color OFF |
|-----------|----------|-----------|
| 0–7 (bajo) | `VU_GREEN_ON` | `VU_GREEN_OFF` |
| 8–9 (medio) | `VU_YELLOW_ON` | `VU_YELLOW_OFF` |
| 10–11 (alto) | `VU_RED_ON` | `VU_RED_OFF` |

### 10.3 Redibujado Diferencial

```cpp
void drawVUMeters() {
    int  active   = (int)round(vuLevels     * VU::SEGS);  // segmentos activos
    int  peak     = (int)round(vuPeakLevels * VU::SEGS);
    if (peak > 0) peak--;                                  // índice exacto del segmento peak
    bool showPeak = (vuPeakLevels > vuLevels + 0.001f) && VU::peakAlpha > 0;
    if (!showPeak) peak = -1;

    // Primer redibujado: fondo completo + todos los segmentos
    if (VU::lastActive < 0) {
        tft.fillRect(MAINAREA_WIDTH, HEADER_HEIGHT, ...);
        for (int i = 0; i < VU::SEGS; i++)
            vuDrawSeg(i, vuSegColor(i, active, peak, clip, VU::peakAlpha));
    } else {
        // Diferencial: solo segmentos que cambiaron de color
        for (int i = 0; i < VU::SEGS; i++) {
            uint16_t cNow  = vuSegColor(i, active,         peak,         clip,         VU::peakAlpha);
            uint16_t cPrev = vuSegColor(i, VU::lastActive, VU::lastPeak, VU::lastClip, VU::lastPeakAlpha);
            if (cNow != cPrev) vuDrawSeg(i, cNow);
        }
    }
    // Guardar estado para próximo frame
    VU::lastActive    = active;
    VU::lastPeak      = showPeak ? peak : -1;
    VU::lastClip      = clip;
    VU::lastPeakAlpha = VU::peakAlpha;
}
```

### 10.4 Peak Hold + Fade (2026-05-26)

```
Señal VU sube → peak=vuLevels, alpha=255, peakLastUpdate=now
                     │
                     ▼
              [Hold 2000ms]
                     │
                     ▼
              peakFadeTime=now (marcar inicio fade)
                     │
           ┌─────────┴─────────────┐
           │  cada 25ms            │
           ▼                       │
    peakAlpha -= 21    ◄───────────┘
           │
    peakAlpha <= 0?
       │ sí        │ no
       ▼           └── continuar
  vuPeakLevels=0
  peak desaparece
```

**Reglas:**
- Si el nivel vuelve a alcanzar el peak durante el fade → `alpha=255`, fade cancelado
- Si el nivel supera el peak en cualquier momento → `vuPeakLevels=vuLevels`, `alpha=255`
- El segmento peak se dibuja en el color ON del rango correspondiente (no gris), con transparencia progresiva

### 10.5 `blendColor565()` — Interpolación RGB565

```cpp
static uint16_t blendColor565(uint16_t a, uint16_t b, uint8_t alpha) {
    // alpha=255 → color a (ON), alpha=0 → color b (OFF)
    uint8_t r  = ((((a>>11)&0x1F)*alpha) + (((b>>11)&0x1F)*(255u-alpha))) >> 8;
    uint8_t g  = ((((a>> 5)&0x3F)*alpha) + (((b>> 5)&0x3F)*(255u-alpha))) >> 8;
    uint8_t bl = (((a&0x1F)*alpha)        + ((b&0x1F)*(255u-alpha)))        >> 8;
    return (r << 11) | (g << 5) | bl;
}
```

Usada exclusivamente por `vuSegColor()` para el segmento peak durante el fade.

### 10.6 `handleVUMeterDecay()` — Bucles de Decaimiento

Llamada cada ciclo de `loop()`. Gestiona tres bucles independientes:

| Bucle | Condición | Acción |
|-------|-----------|--------|
| **Nivel VU** | `now - vuLastUpdateTime > 100ms` | `vuLevels -= 1/12` hasta 0 |
| **Peak fade** | `now - vuPeakLastUpdateTime > 2000ms` | `peakAlpha -= 21` cada 25ms |
| **Guard** | `vuPeakLevels < vuLevels` | `vuPeakLevels = vuLevels`, `alpha=255` |

**Fuente del timer VU:** `vuLastUpdateTime` se actualiza en `RS485Handler::onMasterData()` **solo cuando el nivel sube** (no en cada paquete). Así, cuando el audio para, el timer no se renueva y el decay dispara correctamente.

**Fuente del timeout S3:** Si Logic deja de enviar Channel Pressure, S3 fuerza `vuLevel=0` al S2 correspondiente tras 200ms de silencio (fix en `S3/main.cpp taskCore0()`).

---

## 9. BUG CONOCIDO — dB Fader Intermitente (2026-05-26)

### 9.1 Síntoma

El valor de dB en `mainArea` («`-23.4 dB`») **muestra el target de Logic, no la posición física real del fader**. Funciona cuando Logic está controlando el fader activamente. Cuando el usuario mueve el fader a mano, el dB no se actualiza.

### 9.2 Causa Raíz

```cpp
// RS485Handler.cpp — onMasterData()
float newFader = pkt.faderTarget / 27000.0f;   // ← target de Logic
if (fabsf(faderPositions - newFader) > 0.001f) {
    faderPositions = newFader;                   // ← no es la posición ADC real
}

// Display.cpp — drawMainArea()
// faderPositions = lo que Logic QUIERE, no donde está físicamente el fader
```

`faderPositions` refleja el **comando de Logic**, no la posición real del fader medida por ADS1115.

### 9.3 Escenarios de Error

| Escenario | Valor mostrado | Valor real |
|-----------|---------------|------------|
| Logic controla fader | ✅ Correcto | — |
| Usuario mueve fader a mano | ❌ Congelado en último target Logic | posición real diferente |
| Fader desconectado de Logic (touchState=1) | ❌ Sigue mostrando target Logic | posición real |
| Motor calibrando | ❌ muestra target, fader está en 0 | ADC en tránsito |

### 9.4 Fix Pendiente

Cambiar la fuente del valor dB de `faderPositions` (target Logic) a `Motor::getPosition()` (posición ADC real calibrada, 0.0–1.0):

```cpp
// Display.cpp — drawMainArea()  [FIX PENDIENTE]
float realPos = Motor::isCalibrated() ? Motor::getPosition() : faderPositions;
// usar realPos en lugar de faderPositions para el cálculo dB
```

**Bloqueante:** `Motor::getPosition()` devuelve 0.0 si no calibrado → necesita guard. Tampoco se redibuja `mainArea` cuando cambia el ADC (solo cuando cambia `faderPositions`). Requeriría enganchar el redibujado al ciclo de `Motor::setADC()` o a un timer periódico.

**Estado:** ⚠️ Sin prioridad — afecta solo al display informativo, no al control real del motor.

---

## 10. REFERENCIAS

- **MOTOR.md** — SAT suspende/restaura sprites, `Motor::getPosition()`, `Motor::isCalibrated()`
- **BUTTONS.md** — Actualización display en response a botones
- **CLAUDE.md** — Directivas obligatorias
- **S2/README.md** — Display pinout

---

## Últimas Actualizaciones

- **(2026-08-13)** Apagado por inactividad en Splash (Logic desconectado): 2 min sin actividad (touch fader, REC/SOLO/MUTE/SELECT, encoder) → fundido de brillo a 0 en 8s (`SPLASH_DIM_TIMEOUT_MS`/`SPLASH_DIM_FADE_MS`, `config.h`). Cualquier actividad restaura brillo al instante. Detalle: `CHANGELOG.md` sesión 2026-08-13.
- **(2026-05-26)** §9 Bug conocido: dB fader muestra target Logic, no posición ADC real — documentado con fix pendiente
- **(2026-05-26)** §10 VU Meter: geometría, diferencial, peak hold+fade 300ms, blendColor565()
- **(2026-05-16)** Creado DISPLAY.md como documento exhaustivo, trasladado contenido de CLAUDE.md
