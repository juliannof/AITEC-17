# NEOTRELLLIS — Matriz 4×8 RGB (iMakie P4) ⚠️ LEGACY

> ⚠️ **[LEGACY — JC4880P433C]**
> NeoTrellis **no existe en la placa activa JC1060P470C**.
> Este documento describe el hardware de la placa antigua. Código fuente: `MASTER_S3-P4/P4_JC4880P433C/`
> Marcado como legacy (2026-06-11).

Documentación del subsistema NeoTrellis P4 **[LEGADO]**. Matriz táctil RGB 4×8 — placa GUITION JC4880P433C.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** ⚠️ LEGACY — Solo aplicable a P4_JC4880P433C

---

## 1. HARDWARE NEOTRELLLIS

### 1.1 Especificación

| Parámetro | Valor |
|-----------|-------|
| **Tipo** | 2× Adafruit NeoTrellis 4×4 |
| **Matriz total** | 4 filas × 8 columnas (32 botones) |
| **LEDs** | RGB addressable (WS2812B compatible) |
| **Interface** | I2C |
| **Direcciones I2C** | 0x2F (izquierda), 0x2E (derecha) |

### 1.2 Topología Física

```
┌─────────────────────────────┐
│  Izquierda (0x2F)│Derecha (0x2E) │
├─────────┼──────────┤
│ [0-3]   │ [4-7]    │  Fila 0
│ [8-11]  │ [12-15]  │  Fila 1
│ [16-19] │ [20-23]  │  Fila 2
│ [24-27] │ [28-31]  │  Fila 3
└─────────┴──────────┘

Índices lineales: 0-31 (o matriz [fila][col])
```

### 1.3 Pinout P4

```
NeoTrellis I2C (I2C_NUM_0):
  SDA = GPIO33
  SCL = GPIO31
  
Dirección izquierda: 0x2F
Dirección derecha:   0x2E
```

---

## 2. INICIALIZACIÓN

### 2.1 Setup I2C y Devices

```cpp
// P4 setup()
Wire.begin(GPIO33, GPIO31);  // I2C_NUM_0: SDA=33, SCL=31

// Inicializar NeoTrellis left
neotrellis_left.begin(0x2F);
neotrellis_left.pixels.setBrightness(255);

// Inicializar NeoTrellis right
neotrellis_right.begin(0x2E);
neotrellis_right.pixels.setBrightness(255);

// Activar eventos botones
for (int i = 0; i < 16; i++) {
    neotrellis_left.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
    neotrellis_right.activateKey(i, SEESAW_KEYPAD_EDGE_RISING);
}
```

---

## 3. CONTROL DE BOTONES Y LEDS

### 3.1 Leer Presiones

```cpp
// En loop
uint32_t buttons_left = neotrellis_left.readKeys();
uint32_t buttons_right = neotrellis_right.readKeys();

// Decodificar botones presionados
for (int i = 0; i < 16; i++) {
    if (buttons_left & (1 << i)) {
        handleNeoTrellisPress(i, 0);  // Índice 0-15, lado left
    }
    if (buttons_right & (1 << i)) {
        handleNeoTrellisPress(i + 16, 1);  // Índice 16-31, lado right
    }
}
```

### 3.2 Control de LEDs RGB

```cpp
// Encender LED en posición (índice 0-31)
void setNeoTrellisLED(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 16) {
        neotrellis_left.pixels.setPixelColor(index, r, g, b);
        neotrellis_left.pixels.show();
    } else {
        neotrellis_right.pixels.setPixelColor(index - 16, r, g, b);
        neotrellis_right.pixels.show();
    }
}
```

### 3.3 Patrones de LED

```cpp
// Ejemplo: rainbow por fila
for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 8; col++) {
        int index = row * 8 + col;
        uint32_t color = Wheel((index * 256 / 32) & 255);
        setNeoTrellisLED(index, 
                         (color >> 16) & 0xFF,
                         (color >> 8) & 0xFF,
                         color & 0xFF);
    }
}
```

---

## 4. MAPEO A FADERS

### 4.1 Arquitectura

```
Fader 1-9 (Master P4)
    │
    ├─ NeoTrellis [0-8] (fila 0)
    │   RGB color: posición fader
    │   ON/OFF: grabando vs armed
    └─ Presión: mute/solo/select

Fader 10-17 (S3 Extender)
    ├─ NeoTrellis [8-15] (fila 1)

etc.
```

### 4.2 Feedback Visual

```cpp
void updateNeoTrellisFader(int track_idx, uint16_t fader_pos, uint8_t flags) {
    int neoindex = track_idx;  // 0-31 mapping
    
    // Color según posición fader (HSV)
    uint8_t hue = (fader_pos * 256) / 27000;  // 0-255 hue
    uint32_t color = Wheel(hue);
    
    // Brillo según estado
    uint8_t brightness = (flags & FLAG_ARMED) ? 255 : 128;
    
    setNeoTrellisLED(neoindex, 
                     ((color >> 16) & 0xFF),
                     ((color >> 8) & 0xFF),
                     (color & 0xFF));
}
```

---

## 5. TROUBLESHOOTING

### 5.1 Síntomas Comunes

| Síntoma | Causa Probable | Verificación |
|---------|----------------|--------------|
| NeoTrellis no responde | Dirección I2C incorrecta | Verificar 0x2F (left) / 0x2E (right) |
| LEDs no encienden | Pixels no inicializados | Verificar `pixels.begin()` en setup |
| Botones lag | Polling lento | Reducir tiempo loop, usar ISR si disponible |
| LEDs parpadeantes | Voltaje insuficiente | Verificar alimentación 3.3V |
| Colores incorrectos | Orden GRB vs RGB | Verificar librería Adafruit_NeoPixel |
| Solo funciona left o right | I2C bus problem | Verificar pull-ups 10kΩ en SDA/SCL |

### 5.2 Debugging

**I2C Scanner:**
```cpp
void scan_neotrelllis() {
    for (uint8_t addr = 0x20; addr < 0x30; addr++) {
        if (Wire.beginTransmission(addr) == 0) {
            Serial.printf("NeoTrellis en 0x%02X\n", addr);
            Wire.endTransmission();
        }
    }
}
```

**LED Test:**
```cpp
// Encender todos los LEDs en rojo
for (int i = 0; i < 32; i++) {
    setNeoTrellisLED(i, 255, 0, 0);
}
```

---

## 6. REFERENCIAS

- **Adafruit NeoTrellis:** https://learn.adafruit.com/adafruit-neotrellis
- **Adafruit_seesaw:** https://github.com/adafruit/Adafruit_seesaw
- **P4 README:** [MASTER_S3-P4/P4/README.md](../MASTER_S3-P4/P4/README.md)
- **LEDS.md** — NeoPixel S2 (similar control RGB)

---

## 7. ExPressif — REPOSO Y TECLAS L0 / L5 (2026-10-11 19:25)

Aplica al proyecto `MASTER_S3-P4/P4_JC4880P433C/` (ExPressif). Panel izquierdo (0x2F).

| Tecla | Función | LED |
|-------|---------|-----|
| **L0** | Dormir / despertar | Metrónomo (parpadea con MIDI Clock); en reposo, latido tenue |
| **L5** | HOLD (toggle) | Estado de HOLD |

**Reposo = pantalla a 0 + todos los NeoTrellis apagados** (el MCU sigue activo).

- **Latido de L0 en reposo (2026-10-11 19:39):** seno con gamma 2, pico = `brLevel(TRELLIS_DIM_ABS)` (~3, tenue), ciclo 3 s. Sigma-delta temporal actualizado a 5 ms (200 Hz): en reposo `taskCore0` corre a 5 ms; las teclas se leen cada 20 ms. (19:36: pico = mitad del activo, 20 ms → demasiado luminoso y con escalones.)
- **Entra:** L0, o inactividad de `SLEEP_TIMEOUT_MS` (5 min, `config.h`) sin tecla ni toque.
- **No duerme solo** mientras entra MIDI Clock (Logic en marcha). L0 sí duerme siempre.
- **Sale:** L0 o toque en pantalla. El toque que despierta se descarta hasta soltar el dedo.
- **En reposo:** el resto de teclas y el touch no generan acciones ni MIDI.
- **Con Bank abierto:** L0 duerme; la página anterior sigue en L4/L8/L12.
- **Estado:** `g_sleeping`, `g_lastActivityMs`, `g_trellis_sleepToggle`, `g_touchWake` (`config.h`/`main.cpp`).
- **Threading:** Core1 decide dormir/despertar y fija el brillo; Core0 repinta los LEDs al ver el cambio de `g_sleeping` (el bus I2C no se comparte entre cores).
- **Validado en banco (2026-10-11 19:25):** dormir/despertar con L0 y touch, HOLD en L5, metrónomo en L0.

### 7.1 Brillo (L2/L6) escala también los NeoTrellis (2026-10-11 19:28)

- Nivel LED = `base × g_displayBrightness / TRELLIS_BR_REF` (`TRELLIS_BR_REF = 80`), mínimo 1, máximo 255. Con el brillo por defecto (80 %) los niveles son los de `TRELLIS_BRIGHTNESS`/`TRELLIS_DIM_ABS`.
- Aplica a LEDs activos, en reposo, metrónomo y latido del reposo.
- **Siempre**, también con el Bank abierto: `neotrellisBankShowPage()` guarda caché de la última página y `neotrellisUpdate()` (Core0) la repinta al cambiar el brillo. Sin cambios en `UIBank.cpp`.
- Al despertar del reposo se repinta Bank o Kaoss según `g_bankOpen` (antes pisaba el Bank con LEDs Kaoss).

### 7.2 Popup de preset (2026-10-11 19:32)

- Al pulsar una tecla de preset del NeoTrellis (R0-R15, L3/L7/L11/L15) aparece un popup centrado ~2,4 s (2026-10-11 19:36, antes 1,2 s): `PRESET n` (1-20) + nombre X + nombre Y (`kaoss.nameX()/nameY()`).
- Código: `display/UIPresetPopup.{h,cpp}` (mismo patrón que `UIBrightnessPopup`); se invoca en `main.cpp` tras `kaoss.setPreset()` en el bloque `g_trellis_setPreset`.
- Solo NeoTrellis; el botón PRESET táctil no lo dispara.
- Se crea después del popup de brillo (z-order topmost).

### 7.3 Secuencia de encendido "matrix azul" (2026-10-11 19:44)

- Solo al encender: `neotrellisInit()` la arranca y dura `NEO_INTRO_MS` (3000 ms, = `BOOT_SCREEN_MS`), en paralelo a la pantalla de arranque. **No** se repite al despertar del reposo.
- Efecto (2026-10-11 19:48): las 4 filas son carriles; una gota por carril avanza de **izquierda a derecha**, cabeza `NEO_INTRO_HEAD` y estela `NEO_INTRO_COL` de 4 columnas que se atenúa. Lenta: 1..2,5 columnas/s, con entrada escalonada por fila. Fundido en los últimos 400 ms y repintado normal al terminar. (19:44: caía de arriba a abajo a 5..11 filas/s.)
- **No interrumpible:** durante la secuencia las teclas se ignoran.
- Escala con el brillo de pantalla (`brLevel`). Código: `introTick()` en `NeoTrellis.cpp`, ejecutado en Core0.

### 7.4 Botón de función L1 y modos ExPressif / Lights (2026-10-12 13:22)

- **L1 ya no es PANIC.** Es el **botón de función**: alterna entre los dos modos, **ExPressif ↔ Lights**. PANIC (reset de ejes a 64/64) se ha retirado de NeoTrellis y de la pantalla.
- **ExPressif** es la interfaz de siempre (Kaos + Banco). **El Banco no es un modo**: sigue siendo una superposición dentro de ExPressif, con L4 pulsación larga como hasta ahora.
- **Botón táctil:** el antiguo PANIC/"ALL OFF" ahora es **MODO** (sub-label "LIGHTS >") y hace lo mismo que L1. Solo es visible en ExPressif.
- **LED de L1** = color del modo: ExPressif azul `COL_UIMODE_EXPRESSIF`, Lights amarillo `COL_UIMODE_LIGHTS`.
- **Desde el Banco:** en Banco L1 es un slot de contenido. Para ir a Lights se cierra primero el Banco (L4 largo) y luego L1.
- **Pantalla LIGHTS: de relleno** ("LIGHTS / proximamente"). Valida la lógica de modos; tocar la pantalla vuelve a ExPressif. El contenido real (WLED por WiFi) está en `docs/EXPRESSIF_WLED.md`.
- **En Lights** solo responden L0 (reposo), L1 (modo) y L2/L6 (brillo); el resto de teclas están inactivas y L4 largo no abre el Banco.
- **Código:** `display/UIMode.{h,cpp}` (`uiModeNext()`, `uiModeBankToggle()`; el modo se deriva del estado real de la pantalla Lights), `display/UILights.{h,cpp}` (relleno). Flags: `g_trellis_modeNext` (antes `g_trellis_panic`), `g_lightsOpen`. Colores y `UiMode` en `config.h`.

### 7.5 Memoria del sonido exacto por instrumento (2026-10-12 13:27)

- **Cada instrumento (synth) recuerda su sonido exacto**: banco (MSB/LSB) + PC, **en Sonidos y en Performances**. Al volver a un synth, el Banco abre en **su** banco, en **la página** de su sonido y con **ese sonido marcado**. No se reenvía ningún Program Change al cambiar de synth: el instrumento conserva su sonido y solo se restaura la interfaz.
- **Por banco:** cada banco de cada instrumento y rol recuerda su último sonido (`bankLastSel*`, ahora con parámetro `perf`). Cambiar de banco salta a ese sonido y lo envía (antes solo Sonidos; ahora también Performances).
- **Banco activo por instrumento y rol:** nuevo `synthActiveBankSet/Get` (`nvs/FavStore.*`), array `[NUM_SYNTHS][2]`.
- **Persistencia:** RAM al elegir; NVS al cerrar el Banco y cada ~60 s (`bankLastSelFlushIfDirty()`). Claves NVS: `bls`/`blsn` (Sonidos, las de siempre, **sin migración**), `blc`/`blcn` (Performances, nueva), `sab` (banco activo, nueva).
- **Restauración:** `restoreCtxForSynth()` en `UIBank.cpp`, al cambiar de synth (`syncBankSelToSynth()`) y al crear el Banco; el banco guardado se valida contra los bancos del synth activo.

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado NEOTRELLLIS.md como documento P4-específico, extraído de CLAUDE.md
- **(2026-10-11 19:25)** ExPressif: reposo (L0) y HOLD movido a L5, ver §7
- **(2026-10-11 19:28)** ExPressif: L2/L6 escalan los NeoTrellis (incl. Bank), repintado correcto al despertar, ver §7.1
- **(2026-10-11 19:32)** ExPressif: popup de preset al pulsar teclas de preset, ver §7.2
- **(2026-10-11 19:36)** ExPressif: popup de preset 2,4 s; latido de reposo fluido (seno + sigma-delta)
- **(2026-10-11 19:39)** ExPressif: latido de reposo más tenue (pico = nivel de reposo) y más fluido (dithering a 200 Hz)
- **(2026-10-11 19:44)** ExPressif: secuencia de encendido "matrix azul" en NeoTrellis (solo al arrancar), ver §7.3
- **(2026-10-11 19:48)** ExPressif: lluvia de encendido de izquierda a derecha y mucho más lenta
- **(2026-10-12 13:22)** ExPressif: L1 = botón de función ExPressif↔Lights (sin PANIC), pantalla Lights de relleno, ver §7.4
- **(2026-10-12 13:27)** ExPressif: memoria del sonido exacto por instrumento (Sonidos y Performances), ver §7.5
