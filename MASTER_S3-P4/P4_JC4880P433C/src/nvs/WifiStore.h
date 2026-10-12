// nvs/WifiStore.h — Credenciales WiFi en NVS (AITEC 2026-10-12 13:50)
// Namespace NVS "wifi", claves "ssid" y "pass". Se introducen desde la
// pantalla (display/UIWifiCfg.*). OJO: NVS no está cifrado — la clave queda
// en claro en la flash del equipo (aceptable para este uso, no se sube a git).
#pragma once
#include <stddef.h>

// Carga SSID y clave. Devuelve true si hay SSID guardado. Los buffers quedan
// siempre terminados en '\0' (vacíos si no hay nada).
bool wifiStoreLoad(char* ssid, size_t ssidSize, char* pass, size_t passSize);
bool wifiStoreSave(const char* ssid, const char* pass);
void wifiStoreClear();
