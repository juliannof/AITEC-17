// lights/WledClient.h — Cliente WLED por WiFi (AITEC 2026-10-12 13:32)
// FASE 1: conecta al WiFi (ESP32-C6, esp-hosted) y lee el estado de los WLED,
// solo para diagnóstico por log. El control (on/bri/ps/color) es Fase 2.
// Ver docs/EXPRESSIF_WLED.md. Todo corre en una tarea propia (Core0): NUNCA
// llamar a HTTP desde el task LVGL (Core1).
#pragma once
#include <stdint.h>

struct WledState {
    bool    valid;     // la lectura fue correcta
    bool    on;
    uint8_t bri;       // 0-255
    int16_t ps;        // preset activo (-1 = ninguno)
};

enum WledDev : uint8_t { WLED_DEV_MAIN = 0, WLED_DEV_WS = 1, WLED_DEV_COUNT = 2 };

void  wledInit();                               // crea la tarea; no bloquea setup()
bool  wledWifiConnected();
bool  wledGetCached(WledDev dev, WledState& out);   // último estado leído (sin red)

// Estado del WiFi y credenciales (2026-10-12 13:50: configuración por pantalla)
enum WledWifiState : uint8_t { WLED_WIFI_NOCREDS = 0, WLED_WIFI_CONNECTING, WLED_WIFI_CONNECTED, WLED_WIFI_FAILED };
WledWifiState wledWifiState();
void  wledWifiIp(char* buf, unsigned size);     // "192.168.1.x" o "" si no hay
// Barrido de redes (2026-10-12 13:58): lo ejecuta la tarea WLED (Core0), nunca la UI.
struct WledNet {
    char   ssid[33];
    int8_t rssi;       // dBm
    bool   secure;     // false = red abierta
};
void     wledScanRequest();                       // pide un barrido (no bloquea)
bool     wledScanBusy();                          // true mientras barre (~2-5 s)
int      wledScanError();                         // 0 ok · -1 WiFi.mode(STA) falló (C6/esp-hosted) · -2 barrido no lanzado/falló · -3 timeout (2026-10-12 14:25)
uint32_t wledScanSeq();                           // sube cada vez que hay lista nueva
int      wledScanGet(WledNet* out, int maxN);     // copia la última lista (más fuerte primero)

void  wledCredentialsChanged();                 // llamar tras guardar credenciales nuevas: reconecta sin reiniciar
