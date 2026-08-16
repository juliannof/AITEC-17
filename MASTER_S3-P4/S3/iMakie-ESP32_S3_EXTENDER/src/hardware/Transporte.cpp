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
    // PWM invertido: ánodo común 5V, sink por GPIO — más tiempo en LOW = más brillo.
    // analogWrite(valor) = % tiempo en HIGH, por eso 255-brillo (2026-08-16 21:59)
    analogWrite(pin, on ? (255 - TRANSPORT_LED_BRIGHTNESS) : 255);
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
            // STOP encendido localmente (2026-08-16 22:30): confirmado con MIDI Monitor
            // que Logic NUNCA manda feedback de la nota 93 al pulsar Stop físico —
            // sin esta línea, LED_STOP no tiene ninguna vía fiable para encenderse
            // tras usar FF/RW (que sí lo apagan por su cuenta). Ver CHANGELOG 22:05.
            if (LEDS[i] == LED_STOP) setLed(LED_STOP, true);
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

// Configura y apaga los 5 GPIO de LED lo antes posible en el boot, minimizando
// la ventana en la que quedan flotantes tras el bootloader (glow tenue fantasma).
// Debe llamarse como primera instrucción de setup(), antes de Serial/USB.
void initPins() {
    for (uint8_t i = 0; i < N; i++) {
        pinMode(LEDS[i], OUTPUT);
        setLed(LEDS[i], false);
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
        case 94:  // PLAY/STOP combo (2026-08-16 22:20: revertido a la relación inversa — Logic
                  // no manda nota 93 explícita de forma fiable, separarlas dejaba STOP sin apagar
                  // nunca. STOP sigue siendo el inverso de PLAY, como validado originalmente.)
            log_i("[TRANSP] PLAY=%d STOP=%d", on, !on);
            setLed(LED_PLAY, on);
            setLed(LED_STOP, !on);
            break;
        // case 93 (STOP explícito) ELIMINADO (2026-08-16 22:25): dos intentos con esta nota
        // dejaron STOP mal (primero nunca apagaba, luego nunca encendía al pulsar) — sospecha:
        // Logic manda un Note Off de esta nota como flash momentáneo que pisa el estado correcto
        // que ya pone el combo de case 94. Sin datos de MIDI Monitor para confirmarlo, se retira
        // por completo — STOP depende solo del combo (case 94) + FF/RW (validado originalmente).
        case 95:  // RECORD (2026-08-16: comentario corregido, decía "REWIND" por error)
            log_i("[TRANSP] REC=%d", on);
            setLed(LED_REC, on);
            break;
        case 92:  // FAST FWD (2026-08-16: era case 97, nota inexistente en MCU — FF real es 0x5C=92)
                  // Al activarse apaga STOP (Logic no lo hace por su cuenta) — pedido original del usuario.
            log_i("[TRANSP] FF=%d", on);
            setLed(LED_FF, on);
            if (on) setLed(LED_STOP, false);
            break;
        case 91:  // REWIND (2026-08-16: no existía case — LED RW nunca recibía feedback de Logic)
                  // Al activarse apaga STOP, mismo motivo que FF.
            log_i("[TRANSP] RW=%d", on);
            setLed(LED_RW, on);
            if (on) setLed(LED_STOP, false);
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