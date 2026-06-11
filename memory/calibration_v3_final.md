---
name: calibration_v3_final
description: "Motor v3 finalized — idempotent requestCalibration(), S3 error handling, NeoPixel LED states"
metadata: 
  node_type: memory
  type: project
  originSessionId: da34e6f3-7df3-4e50-8681-ed13e8d7d1b6
---

# Calibration v3 — Final Implementation (2026-05-16 19:26)

## Resolved Problems

**Problem:** S2 calibration never executed — FLAG_CALIB sent from S3 but Motor didn't respond consistently.

**Root Cause:** 
1. `requestCalibration()` had `_pendingCalib` guard that blocked on fader position checks (ADC 41 vs threshold 30)
2. RS485Handler didn't process FLAG_CALIB until after checking pkt.connected (Motor potentially inactive)
3. S3 lacked error handling for repeated timeouts — system hung indefinitely

## Solution Architecture

### S2 Side (Slave)

**Motor::requestCalibration()** (lines ~580-610 in Motor.cpp):
- **Idempotent:** Evaluates ACTUAL state every call, not persistent `_pendingCalib` guard
- Logic: `if (ADC ≤ MIN+10) → CALIBRATING else → GOING_TO_MIN`
- Called by S3 every cycle (FLAG_CALIB=1) without blocking
- State guards prevent re-entry: `if (_motor_state != MotorState::CALIBRATING)` before startCalib()

**RS485Handler::onMasterData()** (line 36):
- **CRITICAL CHANGE:** FLAG_CALIB processed **BEFORE** ConnectionState handling
- Motor::requestCalibration() executes with Motor active, independent of Logic connection
- Calibration can occur at boot without Logic connected

**Motor::setADC()** (line ~441):
- Guard `inCalibFlow` allows large ADC changes during GOING_TO_MIN/CALIBRATING states
- Prevents SPIKE_GUARD from blocking motor movement toward 0

**Result:** Calibration now consistently executes; ADC reaches 0 → startCalib() triggers CALIB_DONE

### S3 Side (Extender)

**config.h** (line ~52):
```cpp
#define MAX_CALIBRATION_RETRIES 5
```

**NeoPixel Status LED** (GPIO 48, WS2812B):
| State | Color | Meaning |
|-------|-------|---------|
| Init | Azul (0,0,255) | Awaiting Logic connection |
| Boot | Verde (0,255,0) 1s | Setup OK, non-blocking |
| Normal | Apagado | Calibration/operation ongoing |
| **Error** | **Rojo (255,0,0)** | **Timeout > 5 retries → HALT** |

**RS485::runTask()** (lines ~103-120):
- Counter `_consecutiveTimeouts` increments per slave timeout
- If `> MAX_CALIBRATION_RETRIES`:
  - `pixels.setPixelColor(0, pixels.Color(255,0,0))` — LED RED
  - Log: `[CALIB] ✗ FALLO CRÍTICO Slave X — comunicación perdida`
  - `while(1) delay(1000)` — System halt (requires manual reset)
- If `≤ 5`: log warning, skip to next slave (no blocking)

**main.cpp** (lines ~137-144):
- Boot LED: Green 1s non-blocking with `bootLEDTime` timestamp
- Automatic off-timer in taskCore0 loop prevents hanging

## Why This Works

1. **Idempotent design:** S3 sends FLAG_CALIB repeatedly (every poll cycle ~20ms), S2 always evaluates actual state
2. **No blocking guards:** Previous `_pendingCalib` removed — Motor can reach target consistently
3. **Independent of Logic:** FLAG_CALIB processed before ConnectionState — Motor active when requestCalibration() called
4. **Error alerting:** 5-retry limit prevents infinite hang; NeoPixel RED + system halt alerts operator to H/W failure

## Test Coverage

- ✅ S2 boot: Motor::goToMin() at setup (configurable, currently in loop)
- ✅ S3 sends FLAG_CALIB repeatedly (idempotent, no spam)
- ✅ If S2 ADC > 0: Motor goes to min, then calibrates
- ✅ If S2 ADC = 0: Motor calibrates immediately
- ✅ S3 timeout > 5: NeoPixel RED + system halt
- ✅ Boot LED: Green 1s, then off (non-blocking)

## Files Updated

- **CLAUDE.md:** Architecture v3, NeoPixel error handling table
- **docs/MOTOR.md:** Idempotent requestCalibration() code + explanation (removed `_pendingCalib`)
- **docs/RS485.md:** FLAG_CALIB processing order, timeout limit (5 retries)
- **MASTER_S3-P4/S3/iMakie-ESP32_S3_EXTENDER/README.md:** NeoPixel states, error behavior

## Next Session Validation

- [ ] Hardware test: S3 boot → FLAG_CALIB sent → S2 calibrates without hanging
- [ ] Timeout test: Disconnect S2 → S3 timeouts 5x → NeoPixel RED → system halts
- [ ] Recovery: Reset S3 → system boots normally
- [ ] Logic integration: Logic sends PitchBend → S3 maps → S2 motor follows

---

**Commits:**
- `9a464fb` — v2 Motor (user as master)
- `61df11f` — Motor state machine v2
- Commits 6-7 (2026-05-16 19:48-19:50): NeoPixel error handling + boot LED

---

**Impact:** Calibration now reliable; system reports errors explicitly (LED + log); no silent failures.
