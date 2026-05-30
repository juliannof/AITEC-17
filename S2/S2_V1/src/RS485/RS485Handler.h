#pragma once

#include <Arduino.h>
#include "RS485.h"
#include "../protocol.h"
#include "../hardware/fader/FaderADC.h"   // ← era "hardware/..." sin ../
#include "../SAT/SatMenu.h"

namespace RS485Handler {

    void onMasterData(const MasterPacket& pkt);
    SlavePacket buildResponse(FaderADC& faderADC, SatMenu& satMenu);
    void checkTimeout(unsigned long lastRxTime);

    // ─── Internal — routing AutoMode (2026-05-30 09:35) ──────
    // WHAT: helpers privados del handler para enrutar fader target según AutoMode.
    // WHY:  centraliza toda la lógica de modo en RS485Handler. Motor queda
    //       como actuador puro sin conocimiento de AutoMode.
    // Solo se exponen aquí para tests/SAT futuros; uso normal es interno.
    namespace Internal {
        void     _applyFaderTarget(AutoMode mode, uint16_t target);
        uint32_t _touchDebounceForMode(AutoMode mode);
    }

} // namespace RS485Handler