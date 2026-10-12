// nvs/WifiStore.cpp — Credenciales WiFi en NVS (AITEC 2026-10-12 13:50)
#include "WifiStore.h"
#include <Preferences.h>
#include <string.h>

// Una instancia por llamada: se usa desde el task LVGL (Core1) y desde el
// task WLED (Core0), así no se comparte estado de Preferences entre cores.
bool wifiStoreLoad(char* ssid, size_t ssidSize, char* pass, size_t passSize) {
    if (ssid && ssidSize) ssid[0] = '\0';
    if (pass && passSize) pass[0] = '\0';
    Preferences p;
    // (2026-10-12 14:59) Lectura/escritura (readOnly=false) a propósito: en solo lectura, mientras
    // el namespace no existe, Preferences registra "nvs_open failed: NOT_FOUND"
    // como [E] en cada consulta (la tarea WLED consulta cada 500 ms sin credenciales).
    // Abrirlo en RW lo crea vacío una vez y no vuelve a haber error.
    if (!p.begin("wifi", false)) return false;
    String s = p.getString("ssid", "");
    String k = p.getString("pass", "");
    p.end();
    if (ssid && ssidSize) strlcpy(ssid, s.c_str(), ssidSize);
    if (pass && passSize) strlcpy(pass, k.c_str(), passSize);
    return s.length() > 0;
}

bool wifiStoreSave(const char* ssid, const char* pass) {
    Preferences p;
    if (!p.begin("wifi", false)) return false;
    bool ok = (p.putString("ssid", ssid ? ssid : "") > 0 || !ssid || !ssid[0]);
    p.putString("pass", pass ? pass : "");
    p.end();
    return ok;
}

void wifiStoreClear() {
    Preferences p;
    if (!p.begin("wifi", false)) return;
    p.clear();
    p.end();
}
