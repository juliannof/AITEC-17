// ============================================================
//  main.cpp  —  iMakie PTxx Track S2
// ============================================================
#include <Arduino.h>
#include "config.h"
#include "display/Display.h"
#include "display/LovyanGFX_config.h"
#include "OTA/OtaManager.h"
#include "hardware/fader/FaderADC.h"
#include "hardware/fader/FaderTouch.h"
#include "hardware/encoder/Encoder.h"
#include "hardware/Hardware.h"
#include "hardware/Neopixels/Neopixel.h"
#include "hardware/Motor/Motor.h"
#include "RS485/RS485.h"
#include "RS485/RS485Handler.h"
#include "protocol.h"
#include "hardware/button/ButtonManager.h"
#include "SAT/SatMenu.h"
#include "display/SpriteUtils.h"
// #include "nvs/NVSValidator.h"  // DESACTIVADO
#include <driver/dac_oneshot.h>


// ─── Objetos globales ─────────────────────────────────────────
LGFX        tft;
LGFX_Sprite header(&tft), mainArea(&tft), vPotSprite(&tft);
FaderADC    faderADC;

// ─── Estado de canal ──────────────────────────────────────────
String trackName        = "Track  ";
bool  recStates    = false;
bool  soloStates   = false;
bool  muteStates   = false;
bool  selectStates = false;
bool  vuClipState  = false;
float vuPeakLevels    = 0.0f;
float faderPositions  = 0.0f;
float vuLevels        = 0.0f;
unsigned long vuLastUpdateTime     = 0;
unsigned long vuPeakLastUpdateTime = 0;

static bool     _suspended = false;
static SatMenu* satMenu    = nullptr;

// ─────────────────────────────────────────────────────────────
//  Callbacks SAT
// ─────────────────────────────────────────────────────────────
static void _satMotorOff()  { Motor::stop(); _suspended = true;  }
static void _satMotorOn()   { Motor::init(); _suspended = false; }
static void _satBrightness(uint8_t b) { setScreenBrightness(b); }
static void _satRS485Off()  { _suspended = true;  }
static void _satRS485On()   { _suspended = false; needsTOTALRedraw = true; }
static void _satReboot()    { ESP.restart(); }
static void _satMotorDrive(int pwm) { /* Motor::driveRaw(pwm); */ }
static void _satConfigSaved(const SatConfig& cfg) { rs485.begin(cfg.trackId); }
static void _satWiFiOta() {
    satMenu->close();
    setScreenBrightness(0);
    // LEDs apagados explícitamente (2026-08-13) — antes solo se apagaban si se
    // llegaba aquí a través del SAT (SatMenu::open() ya los apaga). El clic de
    // encoder en splash llama esta función directo, sin pasar por el SAT, y
    // dejaba los LEDs en su patrón azul de espera hasta reiniciar en OTA.
    clearAllNeopixels();
    showNeopixels();
    Preferences prefs;
    prefs.begin("ptxx", false);
    prefs.putBool("otaMode", true);
    prefs.end();
    Serial.printf("[SAT] Guardado otaMode=1, reiniciando en OTA-only...\n");
    Serial.flush();
    delay(100);
    ESP.restart();
}
static void _satLedsOff() {
    clearAllNeopixels();
    showNeopixels();


}
static void _satLedsRestore() { forceNeopixelRefresh(); }  // fuerza repintado tras cerrar SAT (2026-08-13)
static void _satSuspendSprites() {
    header.deleteSprite();
    mainArea.deleteSprite();
    vPotSprite.deleteSprite();
    log_i("Sprites suspendidos | PSRAM libre: %d", ESP.getFreePsram());
}
static void _satRestoreSprites() {
    mainArea.setColorDepth(16);
    mainArea.setPsram(true);
    mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);

    header.setColorDepth(16);
    header.setPsram(true);
    header.createSprite(TFT_WIDTH, HEADER_HEIGHT);

    vPotSprite.setColorDepth(16);
    vPotSprite.setPsram(true);
    vPotSprite.createSprite(TFT_WIDTH, VPOT_HEIGHT);

    _logSpriteAlloc("header",    header);
    _logSpriteAlloc("mainArea",  mainArea);
    _logSpriteAlloc("vPotSprite",vPotSprite);
    needsTOTALRedraw = true;
}
static void _otaStatus(const char* msg) {
    if (satMenu && satMenu->isOpen())
        satMenu->showStatus(msg);
}
static void _satLedsTest(int idx, uint8_t r, uint8_t g, uint8_t b) {
    log_i("[SAT-LED] idx=%d rgb=(%d,%d,%d)", idx, r, g, b);
    setNeopixelState(idx, r, g, b);
    showNeopixels();
}

// =============================================================
//  setup
// =============================================================
void setup() {
    // GPIO flotantes ciegan el radio WiFi en ESP32-S2 — poner todos a OUTPUT LOW
    // ANTES de cualquier init. Excluidos: 0 (bootstrap), 19-20 (USB), 26-32 (QSPI), 46 (input-only)
    // Cada módulo reconfigura sus pines en su propio init() posterior. (2026-05-26)
    static const uint8_t safePins[] = {
         1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
        11, 12, 13, 14, 15, 16, 17, 18, 21,
        33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45
    };
    for (uint8_t pin : safePins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    // ⚠️ SAFETY: Motor EN (GPIO14) MUST be LOW immediately to prevent movement
    pinMode(MOTOR_EN, OUTPUT);
    digitalWrite(MOTOR_EN, LOW);
    delay(10);
    Motor::init();
    Serial.begin(115200);
    Serial.setDebugOutput(true);
    delay(500);

    Serial.printf("\n[BOOT] FW_VERSION=%s FW_BUILD_ID=%d\n", FW_VERSION, FW_BUILD_ID);
    Serial.flush();

    // Leer PWM range de NVS (fallback a config.h si vacío) (2026-05-19)
    Motor::initPWM();

    // Detectar OTA-only mode
    Preferences prefs;
    prefs.begin("ptxx", true);
    bool otaMode = prefs.getBool("otaMode", false);
    prefs.end();

    if (otaMode) {
        Serial.printf("[BOOT] === OTA-ONLY MODE ===\n");
        Serial.flush();

        // Limpiar flag INMEDIATAMENTE — una vez detectado, su propósito cumplió
        Preferences prefs2;
        prefs2.begin("ptxx", false);
        prefs2.remove("otaMode");
        prefs2.end();
        Serial.printf("[BOOT] Flag otaMode limpiado\n");
        Serial.flush();

        // Mínimo necesario: display SIN sprites + WiFi OTA
        initDisplay(true);  // true = otaOnlyMode, NO crea sprites
        Serial.printf("[OTA-ONLY] Display iniciado (sin sprites)\n");
        Serial.flush();

        otaManager.begin();
        otaManager.enableForUpload(true);  // true = otaOnlyMode

        // Si llegamos aquí, WiFi falló en OTA-only mode
        Serial.printf("[OTA-ONLY] WiFi falló, reiniciando...\n");
        Serial.flush();
        delay(100);
        ESP.restart();
        return;  // Nunca llega aquí
    }

    // MODO NORMAL: boot completo
    initNeopixels();
    log_i("NeoPixels OK");

    initDisplay();
    log_i("Display OK");

    // DESACTIVADO: NVSValidator
    // if (NVSValidator::validate() == NVSStatus::CORRUPTED) {
    //     NVSValidator::reset();  // Repara y reinicia
    //     return;  // Nunca llegará aquí
    // }

    drawSplashScreen();
    setScreenBrightness(BRIGHTNESS_SPLASH);

    otaManager.begin();

    delay(100);
    faderADC.begin();
    if (!faderADC.isOk()) {
        log_e("[BOOT] ADS1115 no encontrado — LED GPIO15 parpadeará");
    }
    log_i("Fader iniciado.");

    initHardware();
    log_i("Hardware OK");

    // FaderTouch DESACTIVADO completamente (2026-06-14) — capacitivo no fiable en todo el recorrido
    // Para reactivar: descomentar init/callbacks/update — el gancho manual (delta ADC) fue
    // eliminado (2026-08-14), usar Motor::isManualTouchDetected() (capacitivo) como consumidor
    // FaderTouch::init();
    // FaderTouch::onTouch([]()   { digitalWrite(LED_BUILTIN_PIN, HIGH); });
    // FaderTouch::onRelease([]() { digitalWrite(LED_BUILTIN_PIN, LOW);  });
    log_i("FaderTouch DESACTIVADO");

    setVPotLevel(VPOT_DEFAULT_LEVEL);
    Encoder::begin();
    log_i("Encoder OK");

    satMenu = new SatMenu(&tft);
    satMenu->onMotorOff      (_satMotorOff);
    satMenu->onMotorOn       (_satMotorOn);
    satMenu->onMotorDrive    (_satMotorDrive);
    satMenu->onBrightness    (_satBrightness);
    satMenu->onRS485Off      (_satRS485Off);
    satMenu->onRS485On       (_satRS485On);
    satMenu->onReboot        (_satReboot);
    satMenu->onConfigSaved   (_satConfigSaved);
    satMenu->onWiFiOta       (_satWiFiOta);
    satMenu->onLedsTest      (_satLedsTest);
    satMenu->onLedsOff       (_satLedsOff);
    satMenu->onLedsRestore   (_satLedsRestore);
    satMenu->onSuspendSprites(_satSuspendSprites);
    satMenu->onRestoreSprites(_satRestoreSprites);

    otaManager.onStatus(_otaStatus);
    log_i("SatMenu OK");

    ButtonManager::begin(&tft, satMenu);
    ButtonManager::setOtaCallback(_satWiFiOta);  // clic encoder sin Logic → activar OTA directo (2026-08-13)
    log_i("ButtonManager OK");

    if (psramFound()) {
        log_i("PSRAM: %u KB total, %u KB libre",
            ESP.getPsramSize() / 1024, ESP.getFreePsram() / 1024);
    } else {
        log_e("ERROR: PSRAM no detectada");
    }

    uint8_t slaveId = satMenu->getConfig().trackId;  // ← mover aquí arriba
    // Motor máquina de estados v2: inicia en IDLE
    // loop() ejecutará Motor::update() que lo llevará a GOING_TO_MIN si ADC > 30
    // Motor listo para órdenes S3 (FLAG_CALIB, setTarget)

    log_i("Track ID: %d", slaveId);
    rs485.begin(slaveId);

    

    // ⚠️ TEMPORAL: Auto-calibración sin S3 (testing únicamente)
    // En producción, S3 enviará FLAG_CALIB vía RS485
    // REMOVER cuando S3 esté disponible

    log_i("=== BOOT completo | heap libre: %d bytes ===", ESP.getFreeHeap());
}

// ─── AUTO-CALIB (automático a 10s del boot) ──────────────
static unsigned long g_bootTime = 0;
static bool g_calibStarted = false;

// ────────────────────────────────────────────────────────────────────
// INSTRUMENTACIÓN micros() — BRIEF A (2026-08-20). Peor caso por bloque.
// Solo mide. Quitar o bajar a log_v cuando se cierre el saneamiento del bus.
// ────────────────────────────────────────────────────────────────────
static uint32_t _instrMaxOnMaster   = 0;  // onMasterData()
static uint32_t _instrMaxBuildResp  = 0;  // buildResponse()
static uint32_t _instrMaxSendResp   = 0;  // sendResponse()
static uint32_t _instrMaxRxToSend   = 0;  // gap: hasNewData TRUE → sendResponse hecho
static uint32_t _instrMaxMotor      = 0;  // Motor::update()
static uint32_t _instrMaxDisplay    = 0;  // updateDisplay()
static uint32_t _instrMaxNeopixel   = 0;  // updateAllNeopixels()+tickNeopixelShow()
static uint32_t _instrLastReport    = 0;

static inline void _instrKeepMax(uint32_t& slot, uint32_t v) { if (v > slot) slot = v; }

// =============================================================
//  loop
// =============================================================
void loop() {
    // Parpadeo LED GPIO15 si ADS1115 no se encontró en el boot (2026-06-15)
    if (!faderADC.isOk()) {
        static uint32_t _adsBlinkLast = 0;
        static bool     _adsBlinkOn   = false;
        if (millis() - _adsBlinkLast >= 500) {
            _adsBlinkLast = millis();
            _adsBlinkOn   = !_adsBlinkOn;
            digitalWrite(LED_BUILTIN_PIN, _adsBlinkOn ? HIGH : LOW);
        }
    }

    // OTA siempre tiene máxima prioridad, incluso si SAT está abierto
    // Actualizar ADC SIEMPRE (incluso en SAT) para Test Mode live feedback (2026-05-10 21:57)
    faderADC.update();
    Motor::setADC(faderADC.getFaderPos());  // Motor recibe ADC ANTES de SAT check

    // Autocalibración de boot: diferida hasta tener lecturas reales del ADS1115.
    // En setup() _motor_adcPos aún no refleja la posición física → requestCalibration()
    // tomaría la rama equivocada (creería el fader en 0 esté donde esté). (2026-07-20)
    // FaderADC no expone hasNewReading() públicamente — se usa Motor::getRawADC() > 0
    // como proxy de "ya hay al menos una lectura real aplicada" (setADC() satura a
    // MOTOR_ADC_MIN como mínimo una vez se llama, nunca deja el 0 inicial).
    static bool     _bootCalibDone    = false;
    static uint8_t  _bootAdcSamples   = 0;
    // Jitter anti-cascada: cada S2 arranca con un retardo aleatorio distinto para
    // no disparar todos los motores a la vez al energizar el rig. (2026-08-13 09:36)
    static uint32_t _bootCalibDelayMs = random(CALIB_BOOT_JITTER_MAX_MS);
    if (!_bootCalibDone) {
        if (Motor::getRawADC() > 0) _bootAdcSamples++;
        if (_bootAdcSamples >= 10 && millis() - g_bootTime >= _bootCalibDelayMs) {
            Motor::requestCalibration();
            _bootCalibDone = true;
            log_i("[BOOT] autocalibración disparada (adc=%d, jitter=%lums)", Motor::getRawADC(), _bootCalibDelayMs);
        }
    }

    // LOG ADC cada 5s (reducido para diagnóstico limpio — 2026-05-19)
    static uint32_t lastLog = 0;
    if (millis() - lastLog > 5000) {
        log_d("[ADS] raw=%d pos=%d motor=%d touch=%d", faderADC.getRawLast(), faderADC.getFaderPos(), Motor::getRawADC(), Motor::isManualTouchDetected());
        lastLog = millis();
    }

    if (satMenu && satMenu->isOpen()) {
        satMenu->update();
        return;
    }

    ButtonManager::update();

    // REC reinicia calibración en SAT > Motor > Calibración (2026-05-12 19:07)
    if (satMenu && satMenu->isOpen() && satMenu->isMotorCalibScreen()) {
        if (ButtonManager::getButtonFlags() & FLAG_REC) {
            Motor::startCalib();
            log_i("[MAIN] REC: reiniciando calibración");
        }
    }

    if (satMenu && satMenu->isOpen()) return;

    // ┌─ Procesar encoder ANTES de RS485 para capturar delta actualizado
    if (!satMenu->isEncoderConsumed()) {
        Encoder::update();
        if (Encoder::hasChanged()) {
            int newLevel = constrain((int)(Encoder::getCount() / 4), -7, 7);
            if (newLevel != Encoder::currentVPotLevel) {
                Encoder::currentVPotLevel = newLevel;
                needsVPotRedraw = true;
            }
        }
    }
    // └─

    if (!_suspended) {
        rs485.update();
        static unsigned long lastRxTime = millis();

        if (rs485.hasNewData()) {
            lastRxTime = millis();
            uint32_t _tRxStart = micros();

            uint32_t _t0 = micros();
            RS485Handler::onMasterData(rs485.getData());
            _instrKeepMax(_instrMaxOnMaster, micros() - _t0);

            _t0 = micros();
            SlavePacket resp = RS485Handler::buildResponse(faderADC, *satMenu);
            _instrKeepMax(_instrMaxBuildResp, micros() - _t0);

            _t0 = micros();
            rs485.sendResponse(resp);
            _instrKeepMax(_instrMaxSendResp, micros() - _t0);

            _instrKeepMax(_instrMaxRxToSend, micros() - _tRxStart);

            ButtonManager::clearButtonFlags();
            ButtonManager::clearEncoderButton();
            Encoder::reset();
        }

        RS485Handler::checkTimeout(lastRxTime);
    }

    // Motor::update() SOLO si SAT no está en Test Mode activo (2026-05-10 20:35)
    if (!(satMenu && satMenu->isOpen())) {
        uint32_t _tm = micros();
        Motor::update();
        _instrKeepMax(_instrMaxMotor, micros() - _tm);
    }

    // FaderTouch::update();  // DESACTIVADO (2026-06-14) — ver setup()

    // ─── AUTO-CALIB DESACTIVADO — S3 ordena vía RS485 FLAG_CALIB (2026-05-16 07:48) ───
    // Razón: Arquitectura maestro-esclavo — S3 es autoridad única para calibración
    // Antes: S2 calibraba automáticamente a 10s, conflicto con FLAG_CALIB de S3
    // Ahora: S2 SOLO calibra si S3 lo ordena explícitamente en boot handshake
    // Guard de cooldown (Motor.cpp) previene reinicios involuntarios
    /*
    if (!g_calibStarted && millis() - g_bootTime > 10000) {
        if (!Motor::isCalibrated()) {
            Motor::startCalib();
            g_calibStarted = true;
            log_i("[AUTOCAL] Iniciando calibración automática...");
        }
    }
    */

    updateButtons();
    handleVUMeterDecay();

    uint32_t _td = micros();
    updateDisplay();
    _instrKeepMax(_instrMaxDisplay, micros() - _td);

    uint32_t _tn = micros();
    updateAllNeopixels();
    tickNeopixelShow();  // aplica el .show() diferido por throttle (2026-08-13 15:10)
    _instrKeepMax(_instrMaxNeopixel, micros() - _tn);

    // ── Reporte de peor caso cada 5s (BRIEF A) ──
    if (millis() - _instrLastReport >= 5000) {
        _instrLastReport = millis();
        log_i("[INSTR] MAX(us) onMaster=%lu build=%lu send=%lu rx2send=%lu motor=%lu disp=%lu neo=%lu",
              (unsigned long)_instrMaxOnMaster, (unsigned long)_instrMaxBuildResp,
              (unsigned long)_instrMaxSendResp, (unsigned long)_instrMaxRxToSend,
              (unsigned long)_instrMaxMotor,    (unsigned long)_instrMaxDisplay,
              (unsigned long)_instrMaxNeopixel);
        // Reset de máximos para ver el peor caso de cada ventana de 5s
        _instrMaxOnMaster = _instrMaxBuildResp = _instrMaxSendResp = 0;
        _instrMaxRxToSend = _instrMaxMotor = _instrMaxDisplay = _instrMaxNeopixel = 0;
    }
}
