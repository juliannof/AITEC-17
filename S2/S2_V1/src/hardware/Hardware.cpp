// src/hardware/Hardware.cpp
#include "Hardware.h"
#include "../config.h"

// Hardware.cpp no gestiona estados de botones ni LEDs.
// Solo detecta pulsaciones y notifica via callbacks.
// RS485Handler es la fuente de verdad para recStates/soloStates/etc.

// ** Instancias Button2 **
Button2 buttonRec(BUTTON_PIN_REC, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonSolo(BUTTON_PIN_SOLO, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonMute(BUTTON_PIN_MUTE, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonSelect(BUTTON_PIN_SELECT, BUTTON_USE_INTERNAL_PULLUP);
Button2 buttonEncoderSelect(ENCODER_SW_PIN, BUTTON_USE_INTERNAL_PULLUP);

// ===================================
// --- CALLBACKS GLOBALES ---
// ===================================
static ButtonEventCallback globalButtonEventCallback  = nullptr;

// ===================================
// --- MAPEO DE BOTONES ---
// ===================================
struct ButtonMapping {
    Button2& button;
    ButtonId id;
};

const ButtonMapping buttonMappings[] = {
    {buttonRec,           ButtonId::REC           },
    {buttonSolo,          ButtonId::SOLO          },
    {buttonMute,          ButtonId::MUTE          },
    {buttonSelect,        ButtonId::SELECT        },
    {buttonEncoderSelect, ButtonId::ENCODER_SELECT},
};
const size_t NUM_BUTTON2_BUTTONS = sizeof(buttonMappings) / sizeof(buttonMappings[0]);

// ===================================
// --- FORWARD DECLARATIONS ---
// ===================================
static ButtonId getButtonIdFromInstance(Button2& btn);
static void handleButtonPress(Button2& btn);
static void handleButtonRelease(Button2& btn);

// =========================================================================
// --- FUNCIONES PÚBLICAS ---
// =========================================================================
void initHardware() {
    pinMode(ENCODER_PIN_A, INPUT);
    pinMode(ENCODER_PIN_B, INPUT);

    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i) {
        buttonMappings[i].button.setPressedHandler(handleButtonPress);
        buttonMappings[i].button.setReleasedHandler(handleButtonRelease);
    }

    pinMode(LED_BUILTIN_PIN, OUTPUT);
    digitalWrite(LED_BUILTIN_PIN, LOW);
}

// ===================================
// --- updateButtons ---
// ===================================
void updateButtons() {
    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i) {
        buttonMappings[i].button.loop();
    }
}

// ===================================
// --- Callbacks ---
// ===================================
void registerButtonEventCallback(ButtonEventCallback callback)  { globalButtonEventCallback = callback; }

// ===================================
// --- Funciones internas ---
// ===================================
static ButtonId getButtonIdFromInstance(Button2& btn) {
    for (size_t i = 0; i < NUM_BUTTON2_BUTTONS; ++i)
        if (&buttonMappings[i].button == &btn) return buttonMappings[i].id;
    return ButtonId::UNKNOWN;
}

static void handleButtonPress(Button2& btn) {
    ButtonId id = getButtonIdFromInstance(btn);
    // Solo notificar — Logic decide el estado via RS485
    if (globalButtonEventCallback) globalButtonEventCallback(id);
}

static void handleButtonRelease(Button2& btn) {
    // Release no se notifica — RS485 gestiona los flags
    (void)btn;
}

// ===================================
// --- LED integrado ---
// ===================================
void setLedBuiltin(bool state)  { digitalWrite(LED_BUILTIN_PIN, state ? HIGH : LOW); }
void toggleLedBuiltin()         { digitalWrite(LED_BUILTIN_PIN, !digitalRead(LED_BUILTIN_PIN)); }