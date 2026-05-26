/*
 * iMakie PTxx — CLONAR CREDENCIALES WiFi + NVS
 * Board: WEMOS LOLIN S2 Mini
 * Monitor serie: 115200 baud
 *
 * USO:
 *   1. Editar las constantes de credenciales abajo
 *   2. Cambiar TRACK_ID por el ID de esta unidad (1-9)
 *   3. Flashear por USB (una sola vez por unidad)
 *   4. Observar monitor serie — confirmar "Conectado" y "✓ Unidad lista"
 *   5. Flashear el firmware de producción por USB (o usar OTA)
 *
 * ⚠️ NO subir este archivo con credenciales reales al repositorio.
 *    Editar localmente antes de flashear.
 *
 * CREDENCIALES: editar antes de compilar (no se guardan en el repo)
 */

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#define NVS_NS    "ptxx"

#define WIFI_SSID   "<TU_SSID>"       // ← editar antes de flashear
#define WIFI_PASS   "<TU_PASS>"       // ← editar antes de flashear
#define OTA_PASS    "<TU_OTA_PASS>"   // ← editar antes de flashear
#define TRACK_ID    1                 // ← cambiar por cada unidad (1-9)

Preferences prefs;

void dumpNVS() {
    prefs.begin(NVS_NS, true);
    Serial.println("\n──── NVS [ptxx] ────────────────────────");
    Serial.printf("  trackId  : %u\n",   prefs.getUChar ("trackId",  0));
    Serial.printf("  wifiSsid : '%s'\n", prefs.getString("wifiSsid", "").c_str());
    Serial.printf("  wifiPass : (oculto, longitud=%d)\n", prefs.getString("wifiPass", "").length());
    Serial.printf("  otaPass  : (oculto, longitud=%d)\n", prefs.getString("otaPass",  "").length());
    prefs.end();

    prefs.begin("wifiman", true);
    Serial.println("──── NVS [wifiman] ─────────────────────");
    Serial.printf("  ssid : '%s'\n", prefs.getString("ssid", "").c_str());
    Serial.printf("  pass : (oculto, longitud=%d)\n", prefs.getString("pass", "").length());
    prefs.end();

    Serial.println("────────────────────────────────────────");
}

void setup() {
    // Todos los GPIO disponibles a OUTPUT LOW — evita pines al aire que ciegan WiFi
    // EXCLUIDOS: 0 (bootstrap), 19-20 (USB), 26-32 (QSPI flash/PSRAM), 46 (input-only)
    // Root cause documentado en docs/WIFI-OTA.md §5.3 Bug #1 (2026-05-26)
    const uint8_t safePins[] = {
          1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
         11, 12, 13, 14, 15, 16, 17, 18, 21,
         33, 34, 35, 36, 37, 38, 39, 40,
         41, 42, 43, 44, 45
    };
    for (uint8_t pin : safePins) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== iMakie PTxx CLONAR CREDENCIALES ===");

    Serial.println("[NVS] Estado ANTES:");
    dumpNVS();

    // ── Guardar en namespace propio ───────────────────────────
    prefs.begin(NVS_NS, false);
    prefs.putString("wifiSsid", WIFI_SSID);
    prefs.putString("wifiPass", WIFI_PASS);
    prefs.putString("otaPass",  OTA_PASS);
    prefs.putUChar ("trackId",  TRACK_ID);
    prefs.end();

    // ── Guardar en namespace interno de WiFiManager ───────────
    prefs.begin("wifiman", false);
    prefs.putString("ssid", WIFI_SSID);
    prefs.putString("pass", WIFI_PASS);
    prefs.end();

    // ── Scan — verificar que el S2 ve la red ─────────────────
    Serial.println("\n[WIFI] Escaneando redes...");
    int n = WiFi.scanNetworks();
    Serial.printf("[WIFI] %d redes encontradas:\n", n);
    for (int i = 0; i < n; i++) {
        Serial.printf("  '%s'  RSSI=%d  Canal=%d\n",
            WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    }

    // ── Conectar una vez para cachear canal y BSSID ───────────
    Serial.println("\n[WIFI] Conectando...");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
        delay(250);
        Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WIFI] Conectado — IP: %s | RSSI: %d dBm\n",
            WiFi.localIP().toString().c_str(), WiFi.RSSI());
        WiFi.disconnect(false);
        Serial.println("[WIFI] Desconectado — cache guardada");
    } else {
        Serial.println("\n[WIFI] ERROR: no conectado en 15s");
        Serial.println("[WIFI] Verificar SSID/PASS y que el S2 ve la red en el scan");
    }

    Serial.println("\n[NVS] Estado final:");
    dumpNVS();

    Serial.printf("\n✓ Unidad lista con Track ID %u\n", TRACK_ID);
    Serial.println("Flashea ahora el firmware de produccion por USB o OTA.");
}

void loop() {}
