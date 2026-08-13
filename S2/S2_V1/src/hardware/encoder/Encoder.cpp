#include "Encoder.h"
#include "../../config.h"

volatile long    Encoder::_counter      = 0;
long             Encoder::_lastReported = 0;
volatile uint8_t Encoder::_state        = 0;
int              Encoder::currentVPotLevel = 0;

// Tabla Gray code — índice: (prevA<<3)|(prevB<<2)|(currA<<1)|(currB)
// Devuelve: -1, 0, +1
static const int8_t ENC_TABLE[16] = {
     0, -1, +1,  0,
    +1,  0,  0, -1,
    -1,  0,  0, +1,
     0, +1, -1,  0
};

void IRAM_ATTR Encoder::_isr() {
    // Lectura directa de registro — segura en ISR
    uint32_t reg = GPIO.in;  // lee los 32 GPIOs de una vez
    uint8_t curr = ((reg >> ENCODER_PIN_A) & 1) << 1
                 | ((reg >> ENCODER_PIN_B) & 1);
    int8_t  step = ENC_TABLE[(_state << 2) | curr];
    _counter    -= step;  // signo invertido: derecha=+ / izquierda=- (2026-08-13 09:08)
    _state       = curr;
}

void Encoder::begin() {
    pinMode(ENCODER_PIN_A, INPUT);   // pull-ups externos
    pinMode(ENCODER_PIN_B, INPUT);

    noInterrupts();
    _counter = 0;  // fuerza init a 0
    _state = ((uint8_t)digitalRead(ENCODER_PIN_A) << 1)
           |  (uint8_t)digitalRead(ENCODER_PIN_B);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), _isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), _isr, CHANGE);
    interrupts();
}

void Encoder::update() { /* ISR lo maneja */ }

long Encoder::getCount() {
    noInterrupts();
    long v = _counter;
    interrupts();
    return v;
}

bool Encoder::hasChanged() {
    long cur = getCount();
    if (cur != _lastReported) {
        _lastReported = cur;
        return true;
    }
    return false;
}

void Encoder::reset() {
    noInterrupts();
    _counter      = 0;
    interrupts();
    _lastReported = 0;
}
