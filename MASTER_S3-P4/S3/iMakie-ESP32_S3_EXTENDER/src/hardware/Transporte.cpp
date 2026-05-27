#include "Transporte.h"
#include "../midi/MIDIProcessor.h"
#include "../config.h"

namespace Transporte {

// LEDs físicos (ordénalos según tu hardware)
static const uint8_t LEDS[] = { LED_REC, LED_PLAY, LED_FF, LED_STOP, LED_RW };
static const uint8_t BTNS[] = { BTN_REC, BTN_PLAY, BTN_FF, BTN_STOP, BTN_RW };
static const uint8_t N = 5;

static Button2 buttons[5] = {
    Button2(BTN_REC),
    Button2(BTN_PLAY),
    Button2(BTN_FF),
    Button2(BTN_STOP),
    Button2(BTN_RW)
};

// CÓDIGOS CORREGIDOS según lo que Logic espera (Note On)
// Orden: REC, PLAY, FF, STOP, RW
static const uint8_t MCU_TRANSPORT_NOTES[] = {
    0x5F,  // RECORD (90 5F Lo7)
    0x5E,  // PLAY   (90 5E Lo7)
    0x5C,  // FF     (90 5C Lo7)
    0x5D,  // STOP   (90 5D Lo7)
    0x5B   // RW     (90 5B Lo7)
};

// BONUS: Loop si lo necesitas
// static const uint8_t LOOP_NOTE = 0x56;  // (90 56 Lo7)

void setLed(uint8_t pin, bool on) {
    digitalWrite(pin, on ? LOW : HIGH); // ánodo común 5V, sink
}

static void sendNoteOn(uint8_t note) {
    log_i("[TRANSP BTN] Note On 0x%02X", note);
    byte msg[] = { 0x90, note, 0x7F };
    sendMIDIBytes(msg, sizeof(msg));
}

static void onButtonPressed(Button2& b) {
    for (uint8_t i = 0; i < N; i++) {
        if (&buttons[i] == &b) {
            sendNoteOn(MCU_TRANSPORT_NOTES[i]);
            return;
        }
    }
}

static void onButtonReleased(Button2& b) {
    for (uint8_t i = 0; i < N; i++) {
        if (&buttons[i] == &b) {
            byte msg[] = { 0x80, MCU_TRANSPORT_NOTES[i], 0x00 };
            sendMIDIBytes(msg, sizeof(msg));
            return;
        }
    }
}

void begin() {
    for (uint8_t i = 0; i < N; i++) {
        pinMode(LEDS[i], OUTPUT);
        setLed(LEDS[i], false);
        buttons[i].setPressedHandler(onButtonPressed);
        buttons[i].setReleasedHandler(onButtonReleased);
    }

    // Secuencia de encendido para testear LEDs
    for (uint8_t i = 0; i < N; i++) {
        setLed(LEDS[i], true);
        delay(150);
        setLed(LEDS[i], false);
    }

    delay(50);
}

void setLedByNote(uint8_t note, bool on) {
    switch (note) {
        case 94:  // PLAY/STOP — vel 127 = play, vel 0 = stop
            log_i("[TRANSP] PLAY=%d STOP=%d", on, !on);
            setLed(LED_PLAY, on);
            setLed(LED_STOP, !on);
            break;
        case 95:  // REWIND
            log_i("[TRANSP] REC=%d", on);
            setLed(LED_REC, on);
            break;
        case 97:  // FORWARD
            log_i("[TRANSP] FF=%d", on);
            setLed(LED_FF, on);
            break;
    }
}

void setAllLedsOff() {
    for (uint8_t i = 0; i < N; i++)
        setLed(LEDS[i], false);
}

void update() {
    for (uint8_t i = 0; i < N; i++) {
        buttons[i].loop();
    }
}

} // namespace Transporte