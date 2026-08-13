#pragma once
#include <Arduino.h>

#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#include "../../config.h"    // ← añadir esta línea


class FaderADC {
public:
    void     begin();
    void     update();
    void     dumpAdsLog();
    void     setCalibration(uint16_t minVal, uint16_t maxVal);  // Motor llama al terminar calibración
    uint16_t getFaderPos() const { return _faderPos; }
    int      getRawLast()  const { return _rawLast;  }
    uint16_t getCalibMin() const { return _calibratedFaderMin; }
    uint16_t getCalibMax() const { return _calibratedFaderMax; }
    bool     isOk()        const { return _adsOk; }

private:
    Adafruit_ADS1115 _ads;
    TwoWire _i2c = TwoWire(1);
    static volatile bool _newData;

    struct AdsReading {
        uint32_t timestamp;
        int16_t  raw;
        uint16_t pos;
    };
    static const int ADS_LOG_SIZE = 256;
    AdsReading _adsLog[ADS_LOG_SIZE];
    int _adsLogIdx = 0;

    void _logReading(int16_t raw, uint16_t pos) {
        _adsLog[_adsLogIdx] = {millis(), raw, pos};
        _adsLogIdx = (_adsLogIdx + 1) % ADS_LOG_SIZE;
    }

    static void IRAM_ATTR _alertISR();

    bool     _adsOk    = false;
    uint16_t _faderPos = 0;
    int      _rawLast  = 0;
    float    _faderPosFiltered = -1.0f;  // EMA centralizado (2026-08-13 15:10) — -1 = sin sembrar (primera lectura real, sin arrastre desde 0)
    uint16_t _calibratedFaderMin = 0;     // Mínimo real del fader (guardado por Motor al calibrar)
    uint16_t _calibratedFaderMax = 27000; // Máximo real del fader (default: máximo teórico)
    static uint32_t _lastLogTime;
};