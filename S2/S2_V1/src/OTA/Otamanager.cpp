// ============================================================
//  OtaManager.cpp  —  iMakie PTxx Track S2 (SINGLE-CORE)
// ============================================================
#include "OtaManager.h"
#include "../config.h"
#include "../hardware/Motor/Motor.h"
#include "../RS485/RS485.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NVS_NS          "ptxx"
#define NVS_SSID        "wifiSsid"
#define NVS_PASS        "wifiPass"
#define NVS_OTA_PASS    "otaPass"

#define OTA_PORT        3232

OtaManager otaManager;

// ─────────────────────────────────────────────────────────────
void OtaManager::begin() {
    _otaActive = false;
}

// ─────────────────────────────────────────────────────────────
void OtaManager::tick() {
    // Con ElegantOTA, server.handleClient() está en loop bloqueante de enableForUpload()
    // No necesita ser llamado desde tick()
}

unsigned long ota_progress_millis = 0;

void onOTAStart() {
    Serial.println("[OTA] Update started!");
}

void onOTAProgress(size_t current, size_t final) {
    if (millis() - ota_progress_millis > 1000) {
        ota_progress_millis = millis();
        Serial.printf("[OTA] Progress: %u / %u bytes\n", current, final);
    }
}

void onOTAEnd(bool success) {
    if (success) {
        Serial.println("[OTA] Update finished successfully!");
    } else {
        Serial.println("[OTA] Error during update!");
    }
}

// ─────────────────────────────────────────────────────────────
void OtaManager::enableForUpload(bool otaOnlyMode) {
    extern LGFX tft;
    extern void setScreenBrightness(uint8_t brightness);

    char ssid[64]    = {};
    char pass[64]    = {};
    char otaPass[33] = {};

    // 1. Cargar credenciales
    if (!_loadCredentials(ssid, pass, otaPass)) {
        Serial.printf("[OTA] ERROR: Sin credenciales\n");
        _status("Sin credenciales. Config WiFi primero.");
        return;
    }

    // 2. Conectar WiFi
    WiFi.mode(WIFI_STA);
    Serial.printf("[OTA] Namespace: '%s'\n", NVS_NS);
    Serial.printf("[OTA] Conectando a %s...\n", ssid);
    Serial.printf("[OTA] SSID='%s' | PASS='%s' | OTA='%s'\n", ssid, pass, otaPass);
    WiFi.begin(ssid, pass);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");

    if (WiFi.status() != WL_CONNECTED) {
        Serial.printf("[OTA] WiFi failed after 10s\n");
        _status("WiFi: no conectado.");
        return;
    }

    Serial.println("");
    Serial.print("[OTA] Connected to ");
    Serial.println(ssid);
    Serial.print("[OTA] IP address: ");
    Serial.println(WiFi.localIP());

    // 3. Crear servidor y ElegantOTA (LITERAL al ejemplo)
    static WebServer server(80);

    server.on("/", []() {
        server.sendHeader("Location", "/update");
        server.send(302);
    });

    ElegantOTA.begin(&server, otaPass[0] ? "ota" : "", otaPass);  // Basic Auth si otaPass en NVS (2026-05-26)
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);

    server.begin();
    Serial.println("[OTA] HTTP server started");

    // 4. Mostrar pantalla OTA (2026-05-27)
    setScreenBrightness(200);
    delay(50);

    uint8_t lastOctet = WiFi.localIP()[3];
    char lastOctetStr[4];
    snprintf(lastOctetStr, sizeof(lastOctetStr), "%d", lastOctet);

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    // Header rojo
    tft.fillRect(0, 0, tft.width(), 38, TFT_RED);
    tft.setTextFont(4);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("WiFi OTA", tft.width() / 2, 19);

    // Subtítulo
    tft.setTextFont(2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString("Esperando upload...", tft.width() / 2, 54);

    // Línea separadora roja
    tft.drawFastHLine(20, 68, tft.width() - 40, TFT_RED);

    // Último octeto IP — grande, rojo, centrado
    tft.setTextFont(7);
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.drawString(lastOctetStr, tft.width() / 2, 155);
    tft.setTextSize(1);

    // IP completa — pequeña, gris, referencia
    tft.setTextFont(2);
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    tft.drawString(WiFi.localIP().toString().c_str(), tft.width() / 2, 248);

    // 5. Loop bloqueante ElegantOTA (LITERAL al ejemplo)
    while (true) {
        server.handleClient();
        ElegantOTA.loop();
    }
}

// ─────────────────────────────────────────────────────────────
void OtaManager::disable() {
    Serial.printf("[OTA] disable() — desconectando WiFi\n");
    Serial.flush();
    if (_otaActive) {
        _otaActive = false;
    }
    WiFi.disconnect(true);
    Serial.printf("[OTA] WiFi desconectado\n");
    Serial.flush();
    if (_cbStatus) _cbStatus("WiFi apagado.");
}

// ─────────────────────────────────────────────────────────────
void OtaManager::_showOtaScreen() {
    extern void setScreenBrightness(uint8_t brightness);
    extern LGFX tft;

    uint8_t lastOctet = WiFi.localIP()[3];

    tft.setTextFont(0);
    tft.setTextColor(TFT_WHITE);

    char buf[4];
    snprintf(buf, sizeof(buf), ".%u", lastOctet);
    tft.drawString(buf, tft.width() / 2, tft.height() / 2);

    setScreenBrightness(5);
}

// ─────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────
bool OtaManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

bool OtaManager::hasCredentials() const {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    String s = prefs.getString(NVS_SSID, "");
    prefs.end();
    return s.length() > 0;
}

// ─────────────────────────────────────────────────────────────
bool OtaManager::_loadCredentials(char* ssid, char* pass, char* otaPass) {
    Preferences prefs;
    prefs.begin(NVS_NS, true);
    String s = prefs.getString(NVS_SSID, "");
    String p = prefs.getString(NVS_PASS, "");
    String o = prefs.getString(NVS_OTA_PASS, "");
    prefs.end();

    if (s.length() == 0) return false;

    strncpy(ssid,    s.c_str(), 63);
    strncpy(pass,    p.c_str(), 63);
    strncpy(otaPass, o.c_str(), 32);
    return true;
}

void OtaManager::_saveCredentials(const char* ssid, const char* pass, const char* otaPass) {
    // Guardar en namespace "ptxx" (como sketch provisioning)
    Preferences prefs;
    prefs.begin(NVS_NS, false);
    prefs.putString(NVS_SSID,     ssid);
    prefs.putString(NVS_PASS,     pass);
    prefs.putString(NVS_OTA_PASS, otaPass);
    prefs.end();

    // Guardar en namespace "wifiman" (como sketch provisioning)
    prefs.begin("wifiman", false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", pass);
    prefs.end();
}

void OtaManager::_status(const char* msg) {
    Serial.printf("[OTA] %s\n", msg);
    if (_cbStatus) _cbStatus(msg);
}
