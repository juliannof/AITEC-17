#pragma once
// ============================================================
//  OtaManager.h  —  iMakie PTxx Track S2
//
//  Gestiona WiFi y OTA via ElegantOTA.
//  Credenciales provisionadas por sketch Arduino (USB, una vez).
//
//  MODO DE USO (desde SAT menu → WiFi OTA):
//   enableForUpload()  → conecta red guardada + ElegantOTA activo
//
//  WiFi permanece APAGADO en operación normal.
//  NVS namespace: "ptxx"  (mismo que sketch provisioning)
// ============================================================
#include <Arduino.h>
#include <functional>
#include <WebServer.h>

using CbStatus = std::function<void(const char* msg)>;  // feedback al display

class OtaManager {
public:
    // ── Ciclo de vida ─────────────────────────────────────────
    void begin();                   // Llama en setup() — apaga WiFi
    void tick();                    // Llama en loop() — maneja ArduinoOTA

    // ── Acciones desde SAT ────────────────────────────────────
    void enableForUpload(bool otaOnlyMode = false);  // Conecta red + inicia ElegantOTA
    void disable();                 // Desconecta y apaga WiFi

    // ── Callback de estado → display ─────────────────────────
    void onStatus(CbStatus cb) { _cbStatus = cb; }

    // ── Estado ────────────────────────────────────────────────
    bool isOtaActive()  const { return _otaActive; }
    bool isConnected()  const;
    bool hasCredentials() const;

private:
    bool     _otaActive  = false;
    CbStatus _cbStatus;

    void _status(const char* msg);
    void _showOtaScreen();
    bool _loadCredentials(char* ssid, char* pass, char* otaPass);
    void _saveCredentials(const char* ssid, const char* pass, const char* otaPass);
};

extern OtaManager otaManager;