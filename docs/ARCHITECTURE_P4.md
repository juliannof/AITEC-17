# ARCHITECTURE P4 — Dual-Core Tareas (ESP32-P4)

> ℹ️ **Arquitectura de tareas válida para JC1060P470C (placa activa).**
> Las referencias a NeoTrellis son de la placa antigua JC4880P433C (legacy). Ver `MASTER_S3-P4/P4_JC4880P433C/`.

Documentación de arquitectura de tareas P4. Sistema multi-core con FreeRTOS en ESP32-P4 dual-core.

**Responsable:** iMakie Development Team  
**Última actualización:** 2026-05-16  
**Estado:** En producción (dual-core, race conditions conocidas)

---

## 1. SISTEMA DUAL-CORE P4

### 1.1 Hardware

**ESP32-P4:**
- 2 cores ARM Cortex-A76 (12GHz cada uno)
- FreeRTOS + configurable scheduling
- Memoria compartida (SRAM, DRAM)

### 1.2 Tareas Distribución

```
Core 0 (taskCore0):
  - Main loop: RS485 TX/RX, button read
  - LVGL rendering
  - Display update
  
Core 1 (taskCore1):
  - VU meter decay (audio processing)
  - NeoTrellis LED updates
  - Touch polling
```

---

## 2. FLUJO DE DATOS

### 2.1 Variables Compartidas

```cpp
// Global shared state
volatile uint16_t vuLevels[18];           // [0-8] P4, [9-17] S3
volatile bool g_switchToPage3 = false;
volatile bool g_switchToPage3A = false;
volatile bool g_switchToPage3B = false;
volatile bool g_switchToOffline = false;
```

### 2.2 Acceso sin Mutex (⚠️ RACE CONDITION CONOCIDA)

```cpp
// Core 0 — escribe
void taskCore0() {
    while (1) {
        // ... RS485 lee fader posición ...
        vuLevels[i] = fader_adc_value;  // ESCRIBE
        // ... sin mutex
    }
}

// Core 1 — lee
void taskCore1() {
    while (1) {
        // ... decay exponencial ...
        uint16_t level = vuLevels[i];   // LEE (posible race)
        level = level * 0.95;
        vuLevels[i] = level;
        // ... potencial inconsistencia
    }
}
```

---

## 3. RACE CONDITIONS CONOCIDAS

### 3.1 VU Meter Decay

**Problema:**
- Core 0 escribe `vuLevels[i]` cuando lee RS485
- Core 1 lee y modifica `vuLevels[i]` para decay
- **Sin mutex:** posible lectura inconsistente (half-written)

**Síntoma:**
- VU meter salta erraticamente
- Decay no suave (picos/caídas abruptas)

**Estado:** ⚠️ **Sin resolver** (2026-05-16)

### 3.2 Página Switch (g_switchToPage3, etc.)

**Situación:**
- Core 0 lee flag y resetea
- Core 1 intenta escribir simultáneamente
- **Potencial:** flag se pierde

**Solución implementada:** `volatile bool` es atómico en ARM (32-bit), pero lectura-modificación-escritura NO es.

---

## 4. SINCRONIZACIÓN

### 4.1 Opciones Disponibles

**Mutex (recomendado para vuLevels):**
```cpp
SemaphoreHandle_t vuMutex = xSemaphoreCreateMutex();

// Core 0
xSemaphoreTake(vuMutex, portMAX_DELAY);
vuLevels[i] = value;
xSemaphoreGive(vuMutex);

// Core 1
xSemaphoreTake(vuMutex, portMAX_DELAY);
uint16_t level = vuLevels[i];
level = level * 0.95;
vuLevels[i] = level;
xSemaphoreGive(vuMutex);
```

**Atomic (más ligero):**
```cpp
std::atomic<uint16_t> vuLevels[18];

// Core 0
vuLevels[i].store(value, std::memory_order_release);

// Core 1
uint16_t level = vuLevels[i].load(std::memory_order_acquire);
```

**Ring Buffer (decoupling):**
```cpp
// Core 0 escribe en buffer A
// Core 1 procesa buffer B (anterior)
// Swap cada ciclo → sin race
```

---

## 5. TIMING CRÍTICO

### 5.1 Ciclos

| Tarea | Core | Periodo | Crítico |
|-------|------|---------|---------|
| RS485 poll | 0 | 20ms | SÍ (timeout) |
| LVGL render | 0 | 33ms | NO (tolera lag) |
| VU decay | 1 | 100ms | SÍ (suavidad) |
| Touch poll | 1 | 50ms | NO |
| NeoTrellis LED | 1 | 100ms | NO (visual) |

### 5.2 Priority (FreeRTOS)

```cpp
xTaskCreatePinnedToCore(
    taskCore0,
    "RS485+LVGL",
    STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 1,  // Prioridad alta
    NULL,
    0  // Core 0
);

xTaskCreatePinnedToCore(
    taskCore1,
    "VU+Touch+NeoTrellis",
    STACK_SIZE,
    NULL,
    configMAX_PRIORITIES - 2,  // Prioridad media
    NULL,
    1  // Core 1
);
```

---

## 6. DEBUGGING

### 6.1 Task Monitor

```cpp
void printTaskStats() {
    TaskStatus_t status[4];
    uint32_t runtime;
    uint32_t total = ulTaskGetIdleRunTimeCounter();
    
    UBaseType_t count = uxTaskGetSystemState(status, 4, &runtime);
    
    for (int i = 0; i < count; i++) {
        Serial.printf("[TASK] %s CPU=%.1f%%\n",
                      status[i].pcTaskName,
                      (float)status[i].ulRunTimeCounter / runtime * 100);
    }
}
```

### 6.2 Race Condition Detection

```cpp
// Logging de acceso a vuLevels
#define VU_WRITE(idx, val) do { \
    Serial.printf("[Core%d] VU[%d] W %u\n", xPortGetCoreID(), idx, val); \
    vuLevels[idx] = val; \
} while(0)

#define VU_READ(idx) ({ \
    uint16_t _v = vuLevels[idx]; \
    Serial.printf("[Core%d] VU[%d] R %u\n", xPortGetCoreID(), idx, _v); \
    _v; \
})
```

---

## 7. RECOMENDACIONES FUTURAS

### 7.1 Fix VU Meter Decay

**Opción 1 — Mutex (simplest):**
```cpp
// Add SemaphoreHandle_t vuMutex in global
// Wrap vuLevels acceses en Core0 y Core1
// Costo: ~20µs por crítica sección
```

**Opción 2 — Atomic (lightweight):**
```cpp
// Cambiar vuLevels a std::atomic<uint16_t>
// Operaciones lock-free en ARM64
// Costo: negligible
```

**Opción 3 — Ring Buffer (decoupling):**
```cpp
// Core0 escribe in buffer A, Core1 procesa B
// Swap cada ciclo → zero contention
// Costo: extra 18×2 uint16_t = 72 bytes
```

---

## 8. REFERENCIAS

- **P4 README:** [MASTER_S3-P4/P4/README.md](../MASTER_S3-P4/P4/README.md)
- **STATUS.md** — Bugs conocidos P4
- **FreeRTOS docs:** https://www.freertos.org/
- **ESP32-P4 datasheet:** Espressif

---

## Últimas Actualizaciones

- **(2026-05-16)** Creado ARCHITECTURE_P4.md como documento P4-específico, extraído de CLAUDE.md
