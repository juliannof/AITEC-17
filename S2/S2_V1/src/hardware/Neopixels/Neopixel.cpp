// src/hardware/Neopixels/Neopixel.cpp
#include "Neopixel.h"
#include "../../config.h"
#include "../Hardware.h"

Adafruit_NeoPixel neopixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

static uint8_t  neoBrightness  = NEOPIXEL_DEFAULT_BRIGHTNESS;
bool neoWaitingHandshake = true;
static bool lastNeoWaiting = true;
static bool lastRec    = false;
static bool lastSolo   = false;
static bool lastMute   = false;
static bool lastSelect = false;

// Throttle del .show() real (2026-08-13 15:10) — WHY: Adafruit_NeoPixel::show()
// en ESP32/IDF5 usa rmtWrite(..., RMT_WAIT_FOR_EVER), bloqueo de ~600-700µs sin
// timeout. En una ráfaga de cambios seguidos (ej. REC/SOLO/MUTE/SELECT llegando
// en muchos paquetes RS485 durante carga de proyecto), ese bloqueo se paga en
// cada iteración del loop — confirmado en banco correlacionado con [RS485]
// ID MISMATCH (VU meters, que no bloquean así, casi no lo producían). No se
// pierde ninguna actualización: solo se difiere hasta el siguiente hueco.
static uint32_t _lastShowTime = 0;
static bool     _showPending  = false;

void initNeopixels() {
    neopixels.begin();
    neopixels.clear();
    // Todos en azul tenue (esperando handshake)
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        neopixels.setPixelColor(i, 0, 0, NEOPIXEL_DIM_BRIGHTNESS);
    }
    neopixels.show();
    neoWaitingHandshake = true;
    lastNeoWaiting = true;
    log_i("[NEO] Adafruit NeoPixel OK — %d pixels GPIO%d", NEOPIXEL_COUNT, NEOPIXEL_PIN);
}

void setNeopixelGlobalBrightness(uint8_t brightness) {
    neoBrightness = brightness;
}

uint8_t getNeopixelBrightness() {
    return neoBrightness;
}

void setNeopixelState(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx < 0 || idx >= NEOPIXEL_COUNT) return;
    neopixels.setPixelColor(idx, r, g, b);
}

void clearNeopixel(int idx) {
    setNeopixelState(idx, 0, 0, 0);
}

void clearAllNeopixels() {
    neopixels.clear();
}

void showNeopixels() {
    neopixels.show();
}

void updateAllNeopixels() {
    // Detectar cambios: neoWaitingHandshake o estados de botones
    if (neoWaitingHandshake == lastNeoWaiting &&
        recStates == lastRec && soloStates == lastSolo &&
        muteStates == lastMute && selectStates == lastSelect) {
        return;  // Sin cambios, no actualizar
    }

    // Guardar estado actual
    lastNeoWaiting = neoWaitingHandshake;
    lastRec = recStates;
    lastSolo = soloStates;
    lastMute = muteStates;
    lastSelect = selectStates;

    if (neoWaitingHandshake) {
        // Sin conexión (boot, desconexión, timeout, o salir del SAT estando
        // desconectado): todos los LEDs vuelven al azul tenue de espera, igual
        // que initNeopixels(). Antes solo se pintaba una vez, en el boot — nunca
        // se restauraba en desconexiones posteriores. (2026-08-13)
        for (int i = 0; i < NEOPIXEL_COUNT; i++) {
            neopixels.setPixelColor(i, 0, 0, NEOPIXEL_DIM_BRIGHTNESS);
        }
    } else {
        // Conectado: colores por estado de botón
        handleButtonLedState(ButtonId::REC);
        handleButtonLedState(ButtonId::SOLO);
        handleButtonLedState(ButtonId::MUTE);
        handleButtonLedState(ButtonId::SELECT);
    }
    _showPending = true;  // .show() real diferido a tickNeopixelShow() (2026-08-13 15:10)
}

void tickNeopixelShow() {
    if (!_showPending) return;
    if (millis() - _lastShowTime < NEOPIXEL_SHOW_MIN_INTERVAL_MS) return;
    showNeopixels();
    _lastShowTime = millis();
    _showPending  = false;
}

void forceNeopixelRefresh() {
    // Invalida la caché de cambios — la próxima llamada a updateAllNeopixels()
    // repintará aunque neoWaitingHandshake no haya cambiado. Necesario porque
    // SatMenu::open() limpia los LEDs directamente (clearAllNeopixels()) sin
    // pasar por esta caché. (2026-08-13)
    lastNeoWaiting = !neoWaitingHandshake;
}

void handleButtonLedState(ButtonId id) {
    bool    shouldBeOn    = false;
    uint8_t r=0, g=0, b=0;
    int     neopixelIndex = -1;

    switch (id) {
        case ButtonId::REC:
            shouldBeOn    = recStates;
            neopixelIndex = NEOPIXEL_FOR_REC;
            r = BUTTON_REC_LED_COLOR_R;
            g = BUTTON_REC_LED_COLOR_G;
            b = BUTTON_REC_LED_COLOR_B;
            break;
        case ButtonId::SOLO:
            shouldBeOn    = soloStates;
            neopixelIndex = NEOPIXEL_FOR_SOLO;
            r = BUTTON_SOLO_LED_COLOR_R;
            g = BUTTON_SOLO_LED_COLOR_G;
            b = BUTTON_SOLO_LED_COLOR_B;
            break;
        case ButtonId::MUTE:
            shouldBeOn    = muteStates;
            neopixelIndex = NEOPIXEL_FOR_MUTE;
            r = BUTTON_MUTE_LED_COLOR_R;
            g = BUTTON_MUTE_LED_COLOR_G;
            b = BUTTON_MUTE_LED_COLOR_B;
            break;
        case ButtonId::SELECT: {
            shouldBeOn    = selectStates;
            neopixelIndex = NEOPIXEL_FOR_SELECT;
            r = BUTTON_SELECT_LED_COLOR_R;
            g = BUTTON_SELECT_LED_COLOR_G;
            b = BUTTON_SELECT_LED_COLOR_B;
            break;
        }
        default: return;
    }

    if (neopixelIndex == -1) return;

    // O encendido (DEFAULT_BRIGHTNESS) o tenue de morir (ULTRA_DIM)
    uint8_t brightness = shouldBeOn ? NEOPIXEL_DEFAULT_BRIGHTNESS : NEOPIXEL_ULTRA_DIM;
    uint8_t fr = (r * brightness) / 255;
    uint8_t fg = (g * brightness) / 255;
    uint8_t fb = (b * brightness) / 255;

    setNeopixelState(neopixelIndex, fr, fg, fb);
}
