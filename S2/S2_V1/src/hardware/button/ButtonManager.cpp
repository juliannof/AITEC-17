// ============================================================
//  ButtonManager.cpp  —  iMakie PTxx Track S2
// ============================================================
#include "ButtonManager.h"
#include "SAT/SatMenu.h"
#include "display/Display.h"
#include "protocol.h"

extern bool recStates, soloStates, muteStates, selectStates;
extern bool needsMainAreaRedraw, needsHeaderRedraw;
extern void handleButtonLedState(ButtonId id);
extern Button2 buttonRec;

namespace ButtonManager {

static LovyanGFX* _tft   = nullptr;
static SatMenu*   _sat   = nullptr;
static uint8_t    _flags = 0;
static uint8_t    _encoderBtnCount = 0;
static std::function<void()> _cbOta = nullptr;

// ─────────────────────────────────────────────────────────────
static void _onRecPressed(Button2& btn) {
    (void)btn;
}

static void _onRecReleased(Button2& btn) {
    if (_sat && _sat->isOpen()) return;

    // REC abre el SAT en splash con un hold corto (no 3s como antes, pero
    // tampoco instantáneo) — mientras no hay Logic conectado. Con Logic
    // conectado, REC es solo REC. (2026-08-13)
    if (logicConnectionState != ConnectionState::CONNECTED) {
        // Ventana de gracia tras boot: pinMode() del Button2 global corre en
        // init estático, antes de setup() — un glitch eléctrico del pin en
        // esa ventana puede leerse como pulsación real. (2026-08-13)
        if (millis() < BOOT_INPUT_SETTLE_MS) return;
        // Hold mínimo real: filtra toques/rebotes breves que antes (con el
        // hold de 3s) nunca llegaban a disparar nada. (2026-08-13)
        if (btn.wasPressedFor() < SAT_OPEN_HOLD_MS) return;
        if (_sat) _sat->open();
        return;
    }

    static unsigned long lastRecTime = 0;
    unsigned long now = millis();
    if (now - lastRecTime >= 300) {
        lastRecTime = now;
        _flags |= FLAG_REC;
        _flags |= FLAG_SELECT;  // cualquier botón selecciona la pista (2026-08-20)
    }
}

static void _onButtonEvent(ButtonId id) {
    if (_sat && _sat->isOpen()) return;

    static unsigned long lastRecTime    = 0;
    static unsigned long lastSoloTime   = 0;
    static unsigned long lastMuteTime   = 0;
    static unsigned long lastSelectTime = 0;
    static constexpr unsigned long DEBOUNCE_MS = 300;

    unsigned long now = millis();

    switch (id) {
        case ButtonId::SOLO:
            if (now - lastSoloTime < DEBOUNCE_MS) break;
            lastSoloTime = now;
            _flags |= FLAG_SOLO;
            _flags |= FLAG_SELECT;  // cualquier botón selecciona la pista (2026-08-20)
            break;
        case ButtonId::MUTE:
            if (now - lastMuteTime < DEBOUNCE_MS) break;
            lastMuteTime = now;
            _flags |= FLAG_MUTE;
            _flags |= FLAG_SELECT;  // cualquier botón selecciona la pista (2026-08-20)
            break;
        case ButtonId::SELECT:
            if (now - lastSelectTime < DEBOUNCE_MS) break;
            lastSelectTime = now;
            _flags |= FLAG_SELECT;
            break;
        case ButtonId::ENCODER_SELECT:
            // Pulsación simple abre el SAT SOLO mientras se muestra la splash screen
            // (sin conexión con Logic) — (2026-08-07). El clic de VPot hacia Logic no
            // tiene ningún efecto en ese estado (no hay sesión activa que lo reciba),
            // así que sustituir esa función ahí no pierde nada. En operación normal
            // (CONNECTED) el encoder sigue mandando el clic de VPot como siempre.
            if (logicConnectionState != ConnectionState::CONNECTED) {
                // Ventana de gracia tras boot — ver _onRecReleased() (2026-08-13)
                if (millis() < BOOT_INPUT_SETTLE_MS) break;
                if (_cbOta) _cbOta();
            } else {
                _encoderBtnCount++;
                needsVPotRedraw = true;
                _flags |= FLAG_SELECT;  // cualquier botón selecciona la pista (2026-08-20)
            }
            break;
        default: break;
    }
}

// ─────────────────────────────────────────────────────────────
void begin(LovyanGFX* tft, SatMenu* sat) {
    _tft   = tft;
    _sat   = sat;
    _flags = 0;
    buttonRec.setPressedHandler (_onRecPressed);
    buttonRec.setReleasedHandler(_onRecReleased);
    registerButtonEventCallback (_onButtonEvent);
}

void update() {
    // Mecanismo de pulsación larga eliminado (2026-08-13) — REC abre el SAT
    // con clic simple en _onRecReleased(). Nada que hacer aquí.
}

void setSatMenu(SatMenu* sat) { _sat = sat; }
void setOtaCallback(std::function<void()> cb) { _cbOta = cb; }
uint8_t getButtonFlags()      { return _flags; }
void    clearButtonFlags()    { _flags = 0; }
uint8_t getEncoderButton()    { return _encoderBtnCount; }
void    clearEncoderButton()  { _encoderBtnCount = 0; }

} // namespace ButtonManager