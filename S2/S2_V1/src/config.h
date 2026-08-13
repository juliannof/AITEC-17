// src/config.h
#pragma once
#include <Arduino.h>
#include "display/LovyanGFX_config.h"
#include "protocol.h"

extern LGFX tft;
extern LGFX_Sprite header, mainArea, vPotSprite;

// ===================================
// --- ENUMERACIONES (Tipos de Datos) ---
// ===================================
// ** MUY IMPORTANTE: ConnectionState debe definirse solo aquí **
enum class ConnectionState {
    DISCONNECTED,
    INITIALIZING,
    AWAITING_SESSION,
    MIDI_HANDSHAKE_COMPLETE,
    CONNECTED
};

// ===================================
// --- HARDWARE STATUS (para pre_build.py) ---
// ===================================
// Escala: 0=Descartado/No implementado  1=En progreso  2=Estable
//
// HW_STATUS: Motor=1       DRV8833 OK. Logic→S2 funciona. S2→Logic (touch report) en progreso.
// HW_STATUS: RS485=2       Comunicación funcional. Timeouts periódicos pendientes.
// HW_STATUS: Display=2     ST7789V3 + LovyanGFX. Dibuja perfecto. Ver HW_DISPLAY abajo.
// HW_STATUS: ADC=2         ADS1115 resolvió todos los problemas. Estable.
// HW_STATUS: Fader=2       Lectura ADC OK. Mapeo Logic↔ADC + AutoMode routing completo (2026-05-30).
// HW_STATUS: FaderTouch=1  Pin T capacitivo. Base sólida. Falsos positivos en calibración pendientes.
// HW_STATUS: NeoPixels=2   WS2812B con nueva librería. Estable.
// HW_STATUS: Encoder=1     ISR Gray code OK. S2→Logic no funciona aún.
// HW_STATUS: Buttons=2     Debounce estable, sin retrasos. Mapeo MIDI correcto.
//
// --- DISPLAY SUBSYSTEMS (solo documentación — pre_build.py ignora HW_DISPLAY) ---
// HW_DISPLAY: RecordMode=2   Modo grabación funciona
// HW_DISPLAY: VUMeter=2      VU dibuja bien
// HW_DISPLAY: TrackName=2    Nombre de pista correcto
// HW_DISPLAY: dB=1           En progreso
// HW_DISPLAY: VPotEncoder=1  Encoder ring en display, en progreso

// ⚠️ ============================================================ ⚠️
// 🚫  NO TOCAR FW_REVISION  —  SE GESTIONA AUTOMÁTICAMENTE       🚫
//     • pre_upload.py  →  sube en cada flash a hardware
//     • pre-commit     →  sube en cada commit con cambios S2
//     Modificarlo a mano rompe el historial de versiones
// ⚠️ ============================================================ ⚠️
#define FW_REVISION 47


// ===================================
// --- 1. DEFINICIONES (Macros y Constantes) ---
// ===================================

// --- PINES Y HARDWARE ---

// ===================================
// --- MIDI Constants ---
// ===================================
#define MIDI_PB_MAX  16383   // MIDI PitchBend 14-bit max value (0-16383)

// ===================================
// --- RS485 ---
// ===================================

#define RS485_RX_PIN             9
#define RS485_TX_PIN             8
#define RS485_ENABLE_PIN        35   // GPIO35 — libre en S2FN4R2 (PSRAM QSPI interna, no usa GPIO matrix)
#define RS485_BAUD          500000

#define RS485_START_BYTE      0xAA
#define RS485_RESP_BYTE       0xBB

#define BOOT_INPUT_SETTLE_MS  2000   // ignora REC/encoder-click los primeros 2s tras boot — glitch eléctrico de arranque en pin global Button2 (2026-08-13, subido de 1000 — encoder no tiene hold propio, ver SAT_OPEN_HOLD_MS)
#define SAT_OPEN_HOLD_MS       400   // REC en splash: hold mínimo real (Button2::wasPressedFor()) para abrir el SAT — filtra toques/rebotes breves, ya no dispara con glitches cortos (2026-08-13)
#define GOTOMIN_JITTER_MAX_MS 2000   // retardo aleatorio 0-2s antes de goToMin() tras desconexión — anti-cascada, evita que todas las S2 bajen a la vez (pico de corriente + ruido RS485 correlacionado, confirmado en banco 2026-08-13)


// --- SENSOR TÁCTIL DEL FADER ---
#define FADER_TOUCH_PIN     T1 // Pin táctil para el fader (GPIO1 en ESP32-S2)
// Porcentaje del valor base táctil para el umbral de detección (80% significa que detecta si el valor cae por debajo del 80% del valor base).
#define FADER_TOUCH_THRESHOLD_PERCENTAGE 0.7

// --- ADS1115 (I2C ADC externo) ---
#define ADS_SDA_PIN     21
#define ADS_SCL_PIN     34
#define ADS_ALERT_PIN   17
#define ADS_I2C_ADDR    0x48

// ─── FaderADC ─────────────────────────────────────────────────
#define NOISE_WINDOW_SIZE     8
#define NOISE_K_MOVE          3.0f
#define NOISE_K_MICRO         0.3f
#define FADER_EMA_ALPHA_FAST  0.20f


// --- MOTOR ---
#define MOTOR_IN1    18
#define MOTOR_IN2    16
#define MOTOR_EN     14


// Motor — rango válido ADC (Logic 0-16383 mapea a ADS1115 0-27000)
static constexpr uint16_t MOTOR_ADC_MIN            = 20;      // mínimo esperado
static constexpr uint16_t MOTOR_ADC_MAX            = 27000;   // máximo esperado

// ─── Calibración autónoma S2 (2026-07-20) ───────────────────────────────
// TODO HARDWARE: MOTOR_ADC_MAX=27000 es herencia sin dato real. Medir el tope
// físico real del ADS1115 (GAIN_ONE) con un sketch aislado y ajustar. Hasta
// entonces se satura en 27000 (ver FaderADC.cpp / Motor::setADC).
#define CALIB_MIN_SPAN        18000   // span ADC mínimo para aceptar calibración (≈provisional, ajustar con dato real)
#define CALIB_MAX_RETRIES     3       // reintentos locales antes de CalibPhase::ERROR
#define FADER_EMA_ALPHA       0.15f   // filtro posición fader en S2 (bajado desde S3)
#define CALIB_DATA_TIMEOUT_MS 200     // watchdog: sin muestra ADS nueva > esto en calib → ERROR
#define CALIB_BOOT_JITTER_MAX_MS 2000 // retardo aleatorio 0-2s antes de autocalibrar en boot — anti-cascada (2026-08-13 09:36)

// Motor — control de posición (constantes)
static constexpr uint8_t  PWM_MIN                  = 100;
static constexpr uint8_t  PWM_MAX                  = 160;  // calibrado: movimiento correcto sin ruido (2026-05-10 21:55)

// TODO BANCO: umbral provisional — ajustar según fricción real medida en banco (2026-07-22)
// WHY: _positionTick() calculaba PWM proporcional al error de punta a punta, sin llegar
// nunca a PWM_MAX salvo que el target estuviera en el extremo opuesto exacto. Con errores
// grandes pero no máximos (~40-75% del span) el PWM resultante (125-145) no bastaba para
// vencer fricción localizada cerca de los extremos — el fader quedaba atrapado en un
// bucle STALL→cooldown→reintento parcial en vez de moverse con fluidez.
static constexpr uint16_t POSITION_CRUISE_ERR       = 2000;  // cuentas: por encima → PWM_MAX fijo (crucero), por debajo → proporcional (frenado fino)

// Motor — spike guard (rechaza cambios > este valor)
static constexpr uint16_t ADC_SPIKE_GUARD          = 500;     // cuentas máximas entre lecturas (aumentado para Test Mode tolerancia 2026-05-10 22:00)

// Motor — calibración (constantes)
static constexpr uint16_t DEAD_ZONE                = 80;      // error < esto → apagar motor (S1 ruido=60, margen 20)
static constexpr uint16_t ADC_STABILITY_THRESHOLD  = 200;     // cambio máximo para considerar "estable" — aumentado para tolerar ruido EMF en topes (2026-05-27)
static constexpr uint32_t CALIB_STABLE_TIME        = 500;     // ms para considerar estable
static constexpr uint32_t CALIB_SETTLE_MS          = 200;     // ms para medir ruido (antes 500, era demasiado)
static constexpr uint32_t CALIB_MIN_TRAVEL_MS      = 300;     // ms mínimo de viaje antes de medir
static constexpr uint32_t CALIB_TIMEOUT            = 6000;    // ms timeout calibración
static constexpr uint32_t CALIB_STUCK_TIMEOUT      = 1000;    // ms sin movimiento = motor atascado (2026-05-12 20:40)
static constexpr uint32_t CALIB_COOLDOWN_MS        = 2000;    // ms espera mínima antes de reiniciar (2026-05-16 HH:MM)
static constexpr uint8_t  PWM_SLEW                 = 5;       // máximo cambio PWM por tick

// Motor — máquina de estados v2 (2026-05-16 10:45)
static constexpr uint32_t MOTOR_AT_TARGET_TIMEOUT = 30000;    // ms: si sin comando S3, vuelve IDLE

// Motor — maquina de calibración (enum de fases internas)
enum class CalibPhase {
    IDLE,
    KICK_UP, GOING_UP, SETTLE_UP,
    KICK_DOWN, GOING_DOWN, SETTLE_DOWN,
    DONE, ERROR
};

// Motor — variables de estado (calibración y control)
static CalibPhase _motor_phase          = CalibPhase::IDLE;
static uint32_t   _motor_phaseStart     = 0;
static uint32_t   _motor_calibStart     = 0;
static uint32_t   _motor_calibMinDetect = 0;
static uint32_t   _motor_stableStart    = 0;
static uint32_t   _motor_lastCalibDone  = 0;    // timestamp último finish exitoso (2026-05-16)
static int        _motor_stableRef      = 0;

static uint16_t   _motor_adcTop         = 0;
static uint16_t   _calibratedFaderMin         = 0;
static uint16_t   _calibratedFaderMax         = 0;
static uint16_t   _motor_adcSpan        = 0;
static uint16_t   _motor_adcPos         = 0;
static uint16_t   _motor_targetADC      = 0;

static uint16_t   _motor_settleMin      = 27000;  // > máximo rango ADS1115 (26423)
static uint16_t   _motor_settleMax      = 0;      // < mínimo rango ADS1115 (23)
static uint16_t   _motor_noiseTopSpan   = 0;      // Ruido medido en SETTLE_UP
static uint16_t   _motor_noiseBottomSpan = 0;    // Ruido medido en SETTLE_DOWN

static bool       _motor_active         = false;
static int        _motor_currentPWM     = 0;

// Motor — máquina de estados v2 (2026-05-16 10:52)
static bool       _pendingCalib         = false;        // Flag: startCalib en espera después goToMin
static uint8_t    _motor_calibRetries   = 0;            // reintentos de calibración por span corto (2026-07-20)
static bool       _connected            = false;        // Estado de conexión con S3
static bool       _motor_goingToMin     = false;        // Flag: motor bajando a posición 0
static uint16_t   _userDropTarget       = 0;            // ADC capturado cuando usuario soltó fader
static uint16_t   _s3Target             = 0;            // Target actual de S3 (para MOVING_TO_TARGET)
static uint32_t   _atTargetStartTime    = 0;            // timestamp cuando llegó a AT_TARGET

// Motor — detección stall en GOING_TO_MIN (2026-05-19)
static constexpr uint32_t GOTO_MIN_STALL_MS  = 400;   // ms sin movimiento ADC = fader en tope físico
static uint32_t           _goToMinStallStart = 0;
static uint16_t           _goToMinLastADC    = 0;

// Motor — protección global topes mecánicos (2026-05-19)
// Activa en todos los estados EXCEPTO CALIBRATING (que tiene su propio CALIB_STUCK_TIMEOUT)
static constexpr uint32_t STALL_PROTECT_MS     = 400;   // ms motor HW activo sin cambio ADC → apagar
static bool               _motor_hw_active     = false; // true cuando _hwUp()/_hwDown() en curso
static uint32_t           _stallProtectStart   = 0;
static uint16_t           _stallProtectLastADC = 0;

// Motor — cooldown tras STALL (2026-07-20)
// WHY: STALL_PROTECT_MS solo corta el driver un instante — sin este cooldown,
//      el siguiente paquete RS485 (~10-20ms después) re-arma MOVING_TO_TARGET
//      y el motor vuelve a empujar a PWM alto contra el mismo obstáculo,
//      en bucle indefinido. Riesgo térmico real (motor + DRV8833).
static constexpr uint32_t STALL_COOLDOWN_MS    = 2000;  // ms de reposo obligatorio tras un STALL
static uint32_t           _stallCooldownUntil  = 0;      // timestamp: no re-armar movimiento antes de esto
// WHY: si el STALL fue por el usuario resistiendo al motor (AUTO_READ/OFF,
//      "DAW absoluto"), en cuanto suelta ya no hay obstáculo — no tiene
//      sentido esperar el cooldown completo. Si el STALL fue solo (tope
//      físico real, nadie tocando), sí se respeta el cooldown completo. (2026-07-20)
static bool               _stallCooldownFromTouch = false; // true si el STALL activo se originó con touch

// Motor — detección movimiento manual (delta ADC acumulado en ventana de tiempo)
static uint16_t   _motor_lastADCForDelta    = 0;   // ADC anterior (compatibilidad)
static uint32_t   _deltaWindowStart         = 0;   // inicio ventana delta (2026-05-27)
static uint16_t   _deltaWindowRef           = 0;   // ADC referencia inicio ventana (2026-05-27)
static bool       _motor_manualTouchDetected = false;
static uint32_t   _motor_manualTouchStartTime = 0;
static constexpr uint16_t MANUAL_TOUCH_THRESHOLD          = 150;  // umbral delta — motor activo (MOVING_TO_TARGET)
static constexpr uint16_t MANUAL_TOUCH_AT_TARGET_THRESHOLD =  30;  // umbral en ventana 80ms — motor off (50→30, 2026-05-27)
static constexpr uint32_t MANUAL_TOUCH_DEBOUNCE_MS        = 600;  // ms sin movimiento antes de ceder control
static constexpr uint32_t TOUCH_DELTA_WINDOW_MS           =  80;  // ventana acumulación delta — captura movimientos lentos (2026-05-27)
// WHY: el crucero a PWM_MAX (POSITION_CRUISE_ERR) da al motor más inercia al llegar
// a AT_TARGET — el overshoot/asentamiento mecánico puede superar el umbral sensible
// MANUAL_TOUCH_AT_TARGET_THRESHOLD sin que el usuario haya tocado nada. (2026-07-22)
static constexpr uint32_t AT_TARGET_TOUCH_GRACE_MS        = 200;  // ms tras entrar en AT_TARGET, ignora touch

// ─── AutoMode routing — RS485Handler (2026-05-30 09:35) ──────
// WHAT: estado + constantes para enrutar el faderTarget según AutoMode (OFF/READ/WRITE/TRIM/TOUCH/LATCH).
// WHY:  el Motor es actuador puro; toda la lógica de modo vive en RS485Handler.
//       Estas variables están aquí (no en el .cpp) por la directiva CLAUDE.md:
//       "NUNCA static de estado en .cpp, todo en config.h".
// Debounce por modo: TOUCH=80ms (Mackie spec corto), LATCH=300ms (transición suave).
// Resto de modos: MANUAL_TOUCH_DEBOUNCE_MS (600ms) como referencia base.
static constexpr uint32_t AUTOMODE_TOUCH_DEBOUNCE_MS  =  80;  // ms: TOUCH y TRIM
static constexpr uint32_t AUTOMODE_LATCH_DEBOUNCE_MS  = 300;  // ms: LATCH (más largo, transición a "frozen")
// Umbral en cuentas ADC para considerar que Logic ha movido el target lo suficiente
// como para que LATCH abandone el estado frozen (≈1.5% del rango 0-27000).
static constexpr uint16_t AUTOMODE_LATCH_UNFREEZE_ADC = 200;

// Estado RS485Handler: rastrea modo activo, freeze de LATCH y ventana touch reportada.
// _rsCurrentMode      ─ último modo aplicado; en cambio de modo se hace reset.
// _rsLatchFrozen      ─ true cuando LATCH ha capturado posición y mantiene fader fijo.
// _rsLatchFrozenADC   ─ ADC capturado al entrar en frozen (referencia para comparar con target DAW).
// _rsTouchActive      ─ ventana de touch reportada a S3; respeta debounce por modo.
// _rsLastTouchTime    ─ timestamp del último touch crudo detectado (Motor::isManualTouchDetected()).
static AutoMode  _rsCurrentMode      = AUTO_READ;   // default boot: READ, no OFF (2026-07-30)
static bool      _rsLatchFrozen      = false;
static uint16_t  _rsLatchFrozenADC   = 0;
static bool      _rsTouchActive      = false;
static uint32_t  _rsLastTouchTime    = 0;

// ─── FADERTOUCH — detección por sostenimiento ────────────────
static constexpr uint32_t TOUCH_POLL_MS            = 20;      // intervalo de muestreo (ms)
static constexpr uint8_t  TOUCH_BASELINE_SAMPS     = 3;       // muestras para baseline (reducido: 16→3 para no bloquear RS485, 2026-05-23)
static constexpr float    TOUCH_THR_TOUCH          = 0.015f;  // 1.5% por encima baseline para tocar
static constexpr float    TOUCH_THR_RELEASE        = 0.01f;   // 1.0% por encima baseline para liberar
static constexpr uint8_t  TOUCH_SOSTENIMIENTO      = 6;       // frames (~120ms) para confirmar
static constexpr uint32_t TOUCH_BASE_MIN_THRESHOLD = 10000;   // baseline mínimo para evitar falsos
static constexpr uint32_t TOUCH_BASE_MIN_VALUE     = 50;      // valor mínimo inicial de baseline

// --- LED INTEGRADO ---
#define LED_BUILTIN_PIN 15 // Pin del LED integrado en la Lolin D1 ESP32 S2 (GPIO15)
                          // Verifica el diagrama de pines de tu placa si tienes dudas.
// --- CODIGO DEL ENCODER ROTATIVO ---
#define ENCODER_PIN_A       13 // Pin A del encoder
#define ENCODER_PIN_B       12 // Pin B del encoder
#define ENCODER_SW_PIN      11 // GPIO para el Switch (botón) del encoder
#define ENCODER_DEBOUNCE_DELAY_MS 5




// <<<<<<<<<<<<<<<<<<<<<<<< DEFINICIONES DEL ENCODER ROTATIVO >>>>>>>>>>>>>>>>>>>>>>>>>>
// Retardo de rebote para la rotación del encoder en ms. Se usará como valor inicial para la variable global.
#define ENCODER_ROTARY_DEBOUNCE_DELAY_MS 5
// Retardo inicial de rebote para el encoder en ms (si fuera diferente del valor principal).
#define ENCODER_INITIAL_DEBOUNCE_DELAY_MS 50
// Umbral en ms para considerar un "click" del encoder (usado para efectos o lógica avanzada).
#define ENCODER_CLICK_THRESHOLD 500
// Rango de valores (min/max) que el encoder puede controlar, para mapeos o límites.
#define ENCODER_MAX_VALUE 127
#define ENCODER_MIN_VALUE 0


// --- BOTONES FÍSICOS ---
#define BUTTON_PIN_REC      37
#define BUTTON_PIN_SOLO     38
#define BUTTON_PIN_MUTE     39
#define BUTTON_PIN_SELECT   40
#define BUTTON_USE_INTERNAL_PULLUP true // Usar pull-up internos para los botones



// --- Brillo pantalla ---
#define BRIGHTNESS_SPLASH       50   // Boot y espera (sin Logic)
#define BRIGHTNESS_SELECTED    180   // Canal activo/seleccionado
#define BRIGHTNESS_UNSELECTED   70   // Canal no seleccionado

// --- Apagado por inactividad en Splash (sin Logic conectado) (2026-08-13) ---
#define SPLASH_DIM_TIMEOUT_MS 120000  // 2 min sin actividad antes de empezar a atenuar
#define SPLASH_DIM_FADE_MS      8000  // duración del fundido hasta brillo 0
#define BRIGHTNESS_OTA          50   // Pantalla OTA WiFi

// --- NEOPixel ---
#define NEOPIXEL_PIN        36  // Pin GPIO donde están conectados los Neopixels
#define NEOPIXEL_COUNT      4   // Número total de Neopixels
#define NEOPIXEL_DEFAULT_BRIGHTNESS 30 // Brillo cuando botón está activo (0-255)
#define NEOPIXEL_DIM_BRIGHTNESS 5   // brillo atenuado (reservado, no usado actualmente)
#define NEOPIXEL_ULTRA_DIM 1    // "tenue de morir" — botones inactivos, casi apagado pero visible


// --- COLORES PREDETERMINADOS DE LOS BOTONES (PARA CADA NEOPixel) ---
// Se definen los componentes RGB para el estado "encendido" de cada Neopixel asociado a un botón.

// Neopixel 0 (asociado a REC)
#define BUTTON_REC_LED_COLOR_R 255
#define BUTTON_REC_LED_COLOR_G 0
#define BUTTON_REC_LED_COLOR_B 0 // -> Rojo

// Neopixel 1 (asociado a SOLO)
#define BUTTON_SOLO_LED_COLOR_R 255
#define BUTTON_SOLO_LED_COLOR_G 255
#define BUTTON_SOLO_LED_COLOR_B 0 // -> Amarillo

// Neopixel 2 (asociado a MUTE)
#define BUTTON_MUTE_LED_COLOR_R 255
#define BUTTON_MUTE_LED_COLOR_G 0
#define BUTTON_MUTE_LED_COLOR_B 0 // -> Rojo

// Neopixel 3 (asociado a SELECT)
#define BUTTON_SELECT_LED_COLOR_R 150
#define BUTTON_SELECT_LED_COLOR_G 150
#define BUTTON_SELECT_LED_COLOR_B 150 // -> Blanco tenue

// --- COLORES PARA OTROS BOTONES (QUE NO TIENEN NEOPIXEL DEDICADO) ---
// Estos colores se usarán si ENCODER_SELECT o FADER_TOUCH activan un Neopixel
// (ej. si controlan un píxel compartido o si se les asigna uno después).
#define BUTTON_ENCODER_LED_COLOR_R 50
#define BUTTON_ENCODER_LED_COLOR_G 50
#define BUTTON_ENCODER_LED_COLOR_B 50 // Un blanco más tenue

#define BUTTON_FADER_TOUCH_LED_COLOR_R 0
#define BUTTON_FADER_TOUCH_LED_COLOR_G 100
#define BUTTON_FADER_TOUCH_LED_COLOR_B 100 // Cian tenue para el fader (ejemplo)


// --- PROPIEDADES DE BRILLO PARA PIXELES ESPECIALES ---
// Se define el factor de brillo para el píxel 3 (asociado a ButtonId::SELECT).
#define NEOPIXEL_COLOR_3_BRIGHTNESS_FACTOR 15 // 15% para el brillo del píxel 3


// --- MAPEO DE BOTONES A ÍNDICES DE NEOPIXEL ---
// Cada uno de los 4 Neopixels se asocia a uno de los 4 botones primarios.
#define NEOPIXEL_FOR_REC        0
#define NEOPIXEL_FOR_SOLO       1
#define NEOPIXEL_FOR_MUTE       2
#define NEOPIXEL_FOR_SELECT     3


// --- Rango del VPot ---
#define VPOT_MIN_LEVEL -7
#define VPOT_MAX_LEVEL  7
#define VPOT_DEFAULT_LEVEL 0

#define BUTTON_HEIGHT 20
#define BUTTON_WIDTH 56
//#define BUTTON_SPACING 5



// --- INTERFAZ GRÁFICA (TFT) ---
#define TFT_WIDTH  240
#define TFT_HEIGHT 280
#define MAINAREA_WIDTH 190          // Reducido
#define MAINAREA_HEIGHT 220          // Reducido

#define HEADER_HEIGHT 20
#define VPOT_HEIGHT 30              // Altura del área de dibujo del VPot. Usado en Display.cpp

// Los botones REC, SOLO, MUTE, SELECT individuales se dibujan en una 'fila' dentro de MainArea.
#define BUTTON_GROUP_WIDTH MAINAREA_WIDTH // Ancho para el grupo de 4 botones.
// La altura de cada botón individual dentro de la fila.
#define BUTTON_LINE_HEIGHT 25 // Altura de un botón individual (un poco menos que BUTTONS_SECTION_HEIGHT)
#define BUTTON_SPACING 2 // Espacio entre botones

// Dimensiones para cada botón individual (REC, SOLO, MUTE, SELECT)
// (MAINAREA_WIDTH - (3 * BUTTON_SPACING)) / 4 para dividirlo en 4 botones
#define BUTTON_INDIVIDUAL_WIDTH ((BUTTON_GROUP_WIDTH - (3 * BUTTON_SPING)) / 4)

#define VU_METER_WIDTH 50
#define VU_METER_AREA_Y (HEADER_HEIGHT + 140) // Posición Y del área de los Vúmetros.

// <<<<<<<<<<<<<<<< MACRO COLOR_16_BITS - NECESARIA PARA LAS DEFINICIONES DE ABAJO >>>>>>>>>>>>>>>>>>>>
// Macro para definir colores de 16 bits RGB565.
// Usa los 5 bits más significativos para R, los 6 para G, y los 5 para B.
// Permite definir colores antes de que la instancia TFT_eSPI esté disponible.
#ifndef COLOR_16_BITS
#define COLOR_16_BITS(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
#endif

// <<<<<<<<<<<<<<<< COLORES PARA EL VU METER - AHORA COMO MACROS >>>>>>>>>>>>>>>>>>>>
#define VU_GREEN_OFF  COLOR_16_BITS(0, 20, 0)       // Verde oscuro para segmentos inactivos
#define VU_GREEN_ON   TFT_GREEN                    // Verde brillante para segmentos activos
#define VU_YELLOW_OFF COLOR_16_BITS(20, 20, 0)     // Amarillo oscuro
#define VU_YELLOW_ON  TFT_YELLOW                   // Amarillo brillante
#define VU_RED_OFF    COLOR_16_BITS(20, 0, 0)       // Rojo oscuro
#define VU_RED_ON     TFT_RED                      // Rojo brillante
#define VU_PEAK_COLOR COLOR_16_BITS(150, 150, 150) // Color para el indicador de pico


// --- COLORES TFT ---
#define TFT_BG_COLOR TFT_BLACK
#define TFT_TEXT_COLOR TFT_WHITE
#define TFT_BUTTON_COLOR TFT_DARKGREY
#define TFT_REC_COLOR TFT_RED
#define TFT_SOLO_COLOR TFT_ORANGE
#define TFT_MUTE_COLOR TFT_RED
#define TFT_BUTTON_TEXT TFT_WHITE

// Colores MCU para Neopixels o TFT, sin depender de 'tft'
#define TFT_MCU_BLUE     0x025D    // Azul oscuro MCU
#define TFT_MCU_GREEN    0x3F20   // Verde MCU 
#define TFT_MCU_YELLOW   0xFFC0  // Amarillo MCU
#define TFT_MCU_RED      0xF800     // Rojo MCU
#define TFT_MCU_GRAY     0x3186 // Gris azulado estándar TFT
#define TFT_MCU_DARKGRAY 0x0842 // Gris oscuro MCU
#define TFT_AUTO_OFF    0x0842   // gris oscuro (= TFT_MCU_DARKGRAY)
#define TFT_AUTO_READ   0x2500   // verde (= TFT_MCU_GREEN)
#define TFT_AUTO_WRITE  0x500A// lila
#define TFT_AUTO_TOUCH  0xFD20   // naranja vivo
#define TFT_AUTO_LATCH  0xA900   // naranja oscuro

// --- CONSTANTES PARA EL DISPLAY DE TIEMPO ---
#define FONT_CHAR_WIDTH_MONO 12     // Ancho estimado de un carácter en FreeMonoBold12pt7b (ajustar si es necesario)
#define FONT_HEIGHT_MONO 24         // Altura estimada de un carácter en FreeMonoBold12pt7b (ajustar si es necesario)
#define FONT_CENTER_Y_OFFSET_HEADER 5 // Ajuste fino para centrar la fuente en HEADER_HEIGHT/2
#define DOT_OFFSET_X  -2 // Offset X desde el borde derecho del carácter.
#define DOT_OFFSET_Y   6 // Offset Y desde la base del carácter (hacia abajo).
#define DOT_RADIUS     2 // Radio del círculo del punto.
#define DISPLAY_COPY_TIMEOUT 25 // 25ms para sincronización de display

// --- PROTOCOLO UART (Si lo usas) ---
#define USB_TO_UART_START 0xAA
#define USB_TO_UART_END   0xBB
#define ESP_TO_USB_START  0xCC
#define ESP_TO_USB_END    0xDD
#define CMD_RAW_SYSEX     0x50
#define CMD_REC_ARM       0x10
#define CMD_SOLO          0x11


// ===================================
// --- 2. DECLARACIONES GLOBALES (con 'extern') ---

// --- VARIABLES DE ESTADO DE CANAL (Declaradas en Display.cpp, usadas en main.cpp) ---
extern String trackName; // Corregido a singular
extern bool recStates, soloStates, muteStates, selectStates;
extern AutoMode currentAutoMode;
extern bool vuClipState; 
extern float vuLevels;
extern unsigned long vuLastUpdateTime;
extern float vuPeakLevels;             
extern unsigned long vuPeakLastUpdateTime;
extern float faderPositions; 

// --- BANDERAS DE REDIBUJO (Declaradas en Display.cpp) ---
extern bool needsTOTALRedraw;      
extern bool needsMainAreaRedraw;   
extern bool needsHeaderRedraw;     
extern bool needsVUMetersRedraw;   
extern bool needsVPotRedraw;       // La bandera para el vPot.

// ===================================
// --- WIFI CREDENTIALS (NVS Default Values) ---
// ===================================
#define WIFI_SSID_DEFAULT      "Julianno-WiFi"
#define WIFI_PASS_DEFAULT      "JULIANf1"
#define OTA_PASS_DEFAULT       "9821"

// ===================================
// --- VARIABLES DE ESTADO GENERAL ---
extern volatile ConnectionState logicConnectionState; // Usa el enum de este mismo config.h


// --- Flags para sincronizar el dibujado del display ---
extern unsigned long lastDisplayCC_Time;
extern bool displayDataUpdated;

// --- Mapeo de caracteres Mackie (si es global) ---
const char MACKIE_CHAR_MAP[64] = {
    ' ', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', // 0-15
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_', // 16-31
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/', // 32-47
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?'  // 48-63
};