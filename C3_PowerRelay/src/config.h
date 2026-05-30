/**
 * config.h - Configuración del sistema de control de mesa
 * 
 * ESP32-C3 Mini - Control de motor paso a paso + relé 10V
 */

#pragma once

#include <Arduino.h>

// ============================================================================
// PINES DE CONEXIÓN
// ============================================================================

// Motor paso a paso
#define DIR_PIN             5       // Dirección del motor
#define STEP_PIN            6       // Paso del motor
#define ENABLE_PIN          20      // Habilitación del motor

// Sensor ultrasónico
#define TRIG_PIN            10      // Trigger del sensor
#define ECHO_PIN            7       // Echo del sensor

// Final de carrera
#define ENDSTOP_PIN         21      // Final de carrera (INPUT_PULLUP)

// Relé de 10V
#define RELAY_PIN            1       // Control del relé (GPIO1)

// ============================================================================
// PARÁMETROS DEL MOTOR
// ============================================================================

#define MAX_SPEED           1500.0f     // Velocidad máxima (pasos/seg)
#define ACCELERATION        500.0f      // Aceleración (pasos/seg^2)
#define HOMING_SPEED        1500.0f     // Velocidad de homing
#define MAX_TRAVEL_STEPS    40000L      // Recorrido máximo desde home
#define HOMING_BACK_OFF_STEPS 100       // Pasos de retroceso tras FDC

// ============================================================================
// PARÁMETROS DEL SENSOR ULTRASÓNICO
// ============================================================================

#define UMBRAL_DISTANCIA    4           // Distancia de activación (cm)
#define TIMEOUT_US          25000L      // Timeout del sensor (25ms)
#define SENSOR_DELAY        100         // Tiempo entre lecturas (ms)

// ============================================================================
// CONFIGURACIÓN SERIAL
// ============================================================================

#define SERIAL_BAUD_RATE    115200      // Velocidad del puerto serie

// ============================================================================
// ANTIREBOTE DEL FDC
// ============================================================================

//volatile unsigned long lastEndstopTime = 0;
const unsigned long ENDSTOP_DEBOUNCE = 50;  // 50ms

// ============================================================================
// RELÉ
// ============================================================================
#define RELAY_ACTIVATION_TIME   2     // Segundos

// ============================================================================
// WIFI (opcional, para futuras funciones de monitoreo remoto)
// ============================================================================
#define WIFI_SSID       "Julianno-WiFi"
#define WIFI_PASSWORD   "JULIANf1"
#define WLED_IP         "192.168.1.195"
