#include "FaderADC.h"
#include "../../config.h"

volatile bool FaderADC::_newData = false;
uint32_t FaderADC::_lastLogTime = 0;

void IRAM_ATTR FaderADC::_alertISR() {
    _newData = true;
}

void FaderADC::begin() {
    // 400kHz Fast Mode — pasar en begin() evita el warning "Bus already started"
    // que ignoraba el setClock() posterior (2026-06-15)
    _i2c.begin(ADS_SDA_PIN, ADS_SCL_PIN, 400000);

    if (!_ads.begin(ADS_I2C_ADDR, &_i2c)) {
        log_e("[ADC] ADS1115 not found at 0x%02X", ADS_I2C_ADDR);
        return;
    }

    _ads.setGain(GAIN_ONE);
    _ads.setDataRate(RATE_ADS1115_860SPS);

    pinMode(ADS_ALERT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(ADS_ALERT_PIN),
                    FaderADC::_alertISR, FALLING);

    _ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);

    for (int i = 0; i < 10; i++) {
        if (_newData) {
            _newData = false;
            int16_t raw = _ads.getLastConversionResults();
            if (raw < 0) raw = 0;
            _rawLast = raw;
            _adsLogIdx = 0;
            _adsOk = true;
            log_i("[ADC] ADS1115 OK  GAIN_ONE  860SPS  ALERT=IO%d  seed=%d", ADS_ALERT_PIN, _rawLast);
            return;  // Éxito
        }
        delay(10);
    }

    // Timeout: ISR no respondió
    log_e("[ADC] ADS1115 ISR timeout — no data en 100ms");
}

void FaderADC::update() {
    if (!_newData) return;
    _newData = false;

    int16_t adcRaw = _ads.getLastConversionResults();
    if (adcRaw < 0) adcRaw = 0;

    // Validar rango esperado (0–27000)
    if (adcRaw < 0 || adcRaw > MOTOR_ADC_MAX) {
        log_w("[ADC] Valor fuera de rango: %d (esperado %d-%d)", adcRaw, 0, MOTOR_ADC_MAX);
        return;  // Descartar lectura inválida
    }

    _faderPos = (uint16_t)adcRaw;
    _rawLast  = (int)adcRaw;

    _logReading(adcRaw, _faderPos);

    // Log cada 500ms para debugging setup (si se quita, cambiar a log_v. Nunca borrar)
    uint32_t now = millis();
    if (now - _lastLogTime >= 500) {
        _lastLogTime = now;
        log_v("[ADC] raw=%d pos=%d min=%d max=%d", adcRaw, _faderPos, _calibratedFaderMin, _calibratedFaderMax);
    }
}

void FaderADC::setCalibration(uint16_t minVal, uint16_t maxVal) {
    _calibratedFaderMin = minVal;
    _calibratedFaderMax = maxVal;
}

void FaderADC::dumpAdsLog() {
    log_i("[ADC] Dump circular buffer (256 muestras): timestamp,raw,pos");
    for (int i = 0; i < ADS_LOG_SIZE; i++) {
        Serial.printf("%u,%d,%d\n",
            _adsLog[i].timestamp,
            _adsLog[i].raw,
            _adsLog[i].pos);
    }
    log_i("[ADC] Dump complete");
}
