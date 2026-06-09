#pragma once
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "protocol.h"
#include "../config.h"


// ============================================================
//  RS485.h  –  Master ESP32-S3  (integrado en iMakie)
//  Core 1: RS485 polling task (prioridad 5)
//  Core 0: MIDI + leer respuestas de slaves
// ============================================================


// Base de datos por canal (acceso thread-safe desde Core 0)
struct ChannelData {
    // Master → Slave
    char      trackName[8]  = {};
    uint8_t   flags         = 0;
    uint16_t  faderTarget   = 8192;
    uint8_t   vuLevel       = 0;
    uint8_t   vpotValue     = 0;
    bool      dirty         = true;
    bool      calibrate     = false;
    bool      calibrating   = false;   // ← AÑADIR
    AutoMode  autoMode      = AUTO_OFF;
    uint8_t calibRetries = 0;


    // Slave → Master
    uint16_t faderPos         = 0;
    uint8_t  touchState       = 0;
    uint8_t  buttons          = 0;
    uint8_t  prevButtons      = 0;
    int8_t   encoderDelta     = 0;
    uint8_t  encoderButton    = 0;
    uint8_t  prevEncoderButton = 0;
    bool     calibrated       = false;
    bool     responded        = false;
    
};

class RS485Master {
public:
    void begin(uint8_t numSlaves = NUM_SLAVES);
    void startTask(); 
    
    // FreeRTOS task (Core 1)
    static void taskEntry(void* param);
    void        runTask();

    // API Core 0 → RS485 (MIDI → slaves)
    void setTrackName  (uint8_t id, const char* name);
    void setFlags      (uint8_t id, uint8_t flags);
    void setFaderTarget(uint8_t id, uint16_t value14bit);
    void setVuLevel    (uint8_t id, uint8_t value);
    void setVPotValue(uint8_t id, uint8_t rawCC);   // ← NUEVO
    void setCalibrate  (uint8_t id);               // one-shot calibración
    void setAutoMode   (uint8_t id, AutoMode mode); // modo de automatización

    // API RS485 → Core 0 (slaves → MIDI)
    bool               hasNewSlaveData(uint8_t id);
    const ChannelData& getChannel     (uint8_t id);


    void printStats() const;
    void resetStats();

private:
    uint8_t           _numSlaves  = NUM_SLAVES;
    uint8_t           _currentId  = 1;
    SemaphoreHandle_t _mutex      = nullptr;
    ChannelData       _ch[NUM_SLAVES + 1];

    enum class BusState : uint8_t { SEND, WAIT_RESP, GAP };
    BusState _busState   = BusState::SEND;
    uint32_t _stateTimer = 0;
    uint32_t _cycleStart = 0;

    uint8_t  _rxBuf[sizeof(SlavePacket)];
    uint8_t  _rxGot    = 0;
    bool     _rxHeader = false;

    uint32_t _txCount   = 0;
    uint32_t _rxCount   = 0;
    uint32_t _timeouts  = 0;
    uint32_t _crcErrors = 0;

    void _sendPacket   (uint8_t id);
    bool _readResponse ();
    void _handleResponse();
    void _nextSlave    ();
};

extern RS485Master rs485;