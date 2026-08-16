#pragma once
#include <Arduino.h>
#include <Button2.h>



namespace Transporte {
    void initPins();
    void begin();
    void update();
    void setLed(uint8_t pin, bool on);
    void setLedByNote(uint8_t note, bool on);
    void setAllLedsOff();
}