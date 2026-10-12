// lights/WledClient.cpp — Cliente WLED por WiFi (AITEC 2026-10-12 13:50)
// FASE 1 (diagnóstico) + credenciales por pantalla. Sin dependencias externas:
// el JSON de WLED se lee con strstr() porque "on", "bri" y "ps" son los
// primeros campos de /json/state.
//
// Credenciales: primero NVS (introducidas desde la pantalla, UIWifiCfg); si no
// hay, src/secrets.h como respaldo. Si no hay ninguna la tarea espera, y en
// cuanto se guardan (wledCredentialsChanged()) conecta sin reiniciar.
#include "WledClient.h"
#include "../config.h"
#include "../nvs/WifiStore.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <string.h>
#include <stdlib.h>

#if __has_include("../secrets.h")
#include "../secrets.h"
#else
#define WIFI_SSID "TU_SSID"
#define WIFI_PASS "TU_PASSWORD"
#endif

static WledState      s_state[WLED_DEV_COUNT];
static const char* const kDevIp[WLED_DEV_COUNT]   = { WLED_IP_MAIN, WLED_IP_WS };
static const char* const kDevName[WLED_DEV_COUNT] = { "191 principal", "195 workstation" };

static volatile WledWifiState s_wifiState   = WLED_WIFI_NOCREDS;
static volatile bool          s_credsChanged = false;
static char                   s_ssid[WIFI_SSID_MAX + 1];
static char                   s_pass[WIFI_PASS_MAX + 1];

// Barrido de redes (2026-10-12 13:58)
static volatile bool     s_scanReq  = false;
static volatile bool     s_scanBusy = false;
static volatile uint32_t s_scanSeq  = 0;
static volatile int      s_scanErr  = 0;
static WledNet           s_nets[WLED_SCAN_MAX];
static int               s_netCount = 0;
static SemaphoreHandle_t s_netMux   = NULL;

void     wledScanRequest() { s_scanReq = true; }
bool     wledScanBusy()    { return s_scanBusy || s_scanReq; }
uint32_t wledScanSeq()     { return s_scanSeq; }
int      wledScanError()   { return s_scanErr; }

int wledScanGet(WledNet* out, int maxN) {
    if (!out || maxN <= 0) return 0;
    int n = 0;
    if (s_netMux && xSemaphoreTake(s_netMux, pdMS_TO_TICKS(100)) == pdTRUE) {
        n = (s_netCount < maxN) ? s_netCount : maxN;
        memcpy(out, s_nets, sizeof(WledNet) * (size_t)n);
        xSemaphoreGive(s_netMux);
    }
    return n;
}

bool wledWifiConnected() { return WiFi.status() == WL_CONNECTED; }
WledWifiState wledWifiState() { return s_wifiState; }
void wledCredentialsChanged() { s_credsChanged = true; }

void wledWifiIp(char* buf, unsigned size) {
    if (!buf || !size) return;
    buf[0] = '\0';
    if (WiFi.status() == WL_CONNECTED) strlcpy(buf, WiFi.localIP().toString().c_str(), size);
}

bool wledGetCached(WledDev dev, WledState& out) {
    if (dev >= WLED_DEV_COUNT) return false;
    out = s_state[dev];
    return out.valid;
}

// NVS primero; secrets.h como respaldo (descartando el placeholder).
static bool loadCreds() {
    if (wifiStoreLoad(s_ssid, sizeof(s_ssid), s_pass, sizeof(s_pass))) return true;
    if (strcmp(WIFI_SSID, "TU_SSID") != 0) {
        strlcpy(s_ssid, WIFI_SSID, sizeof(s_ssid));
        strlcpy(s_pass, WIFI_PASS, sizeof(s_pass));
        return true;
    }
    s_ssid[0] = s_pass[0] = '\0';
    return false;
}

// GET /json/state y extrae on/bri/ps. Una petición, timeout corto, sin reintentos.
static bool fetchState(WledDev dev) {
    char url[48];
    snprintf(url, sizeof(url), "http://%s/json/state", kDevIp[dev]);

    HTTPClient http;
    http.setConnectTimeout(WLED_HTTP_TIMEOUT_MS);
    http.setTimeout(WLED_HTTP_TIMEOUT_MS);
    if (!http.begin(url)) { log_w("[WLED] %s: begin() falló", kDevName[dev]); return false; }

    int code = http.GET();
    if (code != 200) {
        log_w("[WLED] %s: HTTP %d (%s)", kDevName[dev], code, HTTPClient::errorToString(code).c_str());
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();

    const char* s = body.c_str();
    const char* pOn  = strstr(s, "\"on\":");
    const char* pBri = strstr(s, "\"bri\":");
    const char* pPs  = strstr(s, "\"ps\":");
    if (!pOn || !pBri) { log_w("[WLED] %s: JSON inesperado", kDevName[dev]); return false; }

    WledState& st = s_state[dev];
    st.on    = (pOn[5] == 't');
    st.bri   = (uint8_t)atoi(pBri + 6);
    st.ps    = pPs ? (int16_t)atoi(pPs + 5) : (int16_t)-1;
    st.valid = true;
    return true;
}

// Espera hasta ms, en pasos de 250 ms, abortando si cambian las credenciales.
static void waitInterruptible(uint32_t ms) {
    uint32_t t0 = millis();
    while ((millis() - t0) < ms && !s_credsChanged) vTaskDelay(pdMS_TO_TICKS(250));
}

// Barrido bloqueante (~2-5 s) dentro de la tarea WLED. Deduplica por SSID
// (se queda con la más fuerte), descarta redes ocultas y ordena por RSSI.
static void doScan(bool& wifiStarted) {
    s_scanBusy = true;
    s_scanErr  = 0;
    int n = 0;

    if (!wifiStarted) {
        // Inicializa esp-hosted (C6) también sin credenciales. Si falla, el
        // barrido es imposible y queremos saberlo (antes se tomaba por "0 redes").
        if (!WiFi.mode(WIFI_STA)) {
            log_e("[WLED] WiFi.mode(STA) falló: el C6 no responde (¿firmware esp-hosted? ¿pines SDIO?)");
            s_scanErr = -1;
            n = -1;
        } else {
            WiFi.setAutoReconnect(true);
            wifiStarted = true;
        }
    }

    if (s_scanErr == 0) {
        // (2026-10-12 14:56) Un barrido FALLA (esp_wifi_scan_start → ESP_ERR_WIFI_STATE, -2) si la
        // STA está asociando o reintentando (setAutoReconnect + clave errónea / red
        // fuera de alcance: el C6 reintenta en bucle). Se corta la conexión en curso
        // y el auto-reintento antes de barrer; la tarea reconecta al terminar.
        wl_status_t st = WiFi.status();
        log_i("[WLED] barriendo redes (estado WiFi=%d)...", (int)st);
        if (st != WL_CONNECTED) {
            WiFi.setAutoReconnect(false);
            WiFi.disconnect();
            vTaskDelay(pdMS_TO_TICKS(400));
        }

        // Asíncrono + espera propia: la librería bloqueante espera hasta 60 s.
        // Un reintento si el primer lanzamiento falla.
        for (int attempt = 0; attempt < 2; attempt++) {
            WiFi.scanDelete();
            n = WiFi.scanNetworks(true, false);
            uint32_t t0 = millis();
            while (n == WIFI_SCAN_RUNNING && (millis() - t0) < WLED_SCAN_TIMEOUT_MS) {
                vTaskDelay(pdMS_TO_TICKS(100));
                n = WiFi.scanComplete();
            }
            if (n != WIFI_SCAN_FAILED) break;
            log_w("[WLED] barrido: no se lanzó (intento %d), reintentando", attempt + 1);
            WiFi.disconnect();
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        WiFi.setAutoReconnect(true);
        if (n == WIFI_SCAN_RUNNING) {
            log_e("[WLED] barrido: timeout a los %u ms (el C6 no devuelve el fin de barrido)", (unsigned)WLED_SCAN_TIMEOUT_MS);
            s_scanErr = -3;
        } else if (n < 0) {
            log_e("[WLED] barrido falló (código %d): el C6 no lanzó el escaneo", n);
            s_scanErr = -2;
        }
    }

    WledNet tmp[WLED_SCAN_MAX];
    int cnt = 0;
    for (int i = 0; i < n; i++) {
        String ss = WiFi.SSID(i);
        if (ss.length() == 0 || ss.length() > WIFI_SSID_MAX) continue;
        int8_t rssi = (int8_t)WiFi.RSSI(i);
        bool secure = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
        int dup = -1;
        for (int k = 0; k < cnt; k++) if (ss.equals(tmp[k].ssid)) { dup = k; break; }
        if (dup >= 0) {
            if (rssi > tmp[dup].rssi) tmp[dup].rssi = rssi;
        } else if (cnt < WLED_SCAN_MAX) {
            strlcpy(tmp[cnt].ssid, ss.c_str(), sizeof(tmp[cnt].ssid));
            tmp[cnt].rssi   = rssi;
            tmp[cnt].secure = secure;
            cnt++;
        }
    }
    WiFi.scanDelete();
    for (int a2 = 1; a2 < cnt; a2++) {                // inserción: más fuerte primero
        WledNet key = tmp[a2];
        int b2 = a2 - 1;
        while (b2 >= 0 && tmp[b2].rssi < key.rssi) { tmp[b2 + 1] = tmp[b2]; b2--; }
        tmp[b2 + 1] = key;
    }

    if (s_netMux && xSemaphoreTake(s_netMux, portMAX_DELAY) == pdTRUE) {
        memcpy(s_nets, tmp, sizeof(WledNet) * (size_t)cnt);
        s_netCount = cnt;
        xSemaphoreGive(s_netMux);
    }
    log_i("[WLED] barrido: %d red(es) vistas por el C6, %d válidas (err=%d)", n < 0 ? 0 : n, cnt, (int)s_scanErr);
    s_scanSeq++;
    s_scanBusy = false;
}

static void wledTask(void*) {
    vTaskDelay(pdMS_TO_TICKS(WLED_START_DELAY_MS));

    bool     wifiStarted = false;
    uint32_t lastPoll    = 0;
    bool     polledOnce  = false;

    for (;;) {
        if (s_scanReq) {                            // barrido pedido desde la pantalla WiFi
            s_scanReq = false;
            doScan(wifiStarted);
        }
        if (s_credsChanged) {                       // credenciales nuevas: reconectar limpio
            s_credsChanged = false;
            if (wifiStarted) WiFi.disconnect();
            polledOnce = false;
        }

        if (!loadCreds()) {
            s_wifiState = WLED_WIFI_NOCREDS;
            vTaskDelay(pdMS_TO_TICKS(500));         // espera a que el usuario las introduzca
            continue;
        }

        if (WiFi.status() != WL_CONNECTED) {
            s_wifiState = WLED_WIFI_CONNECTING;
            log_i("[WLED] conectando a '%s' (C6 esp-hosted)...", s_ssid);
            if (!wifiStarted) {
                WiFi.mode(WIFI_STA);                // dispara la inicialización de esp-hosted
                WiFi.setAutoReconnect(true);
                wifiStarted = true;
            }
            WiFi.begin(s_ssid, s_pass);

            uint32_t t0 = millis();
            while (WiFi.status() != WL_CONNECTED && (millis() - t0) < WLED_WIFI_TIMEOUT_MS && !s_credsChanged && !s_scanReq)
                vTaskDelay(pdMS_TO_TICKS(250));

            if (WiFi.status() != WL_CONNECTED) {
                if (s_scanReq) continue;          // barrido pedido: atenderlo antes de seguir reintentando
                if (!s_credsChanged) {
                    s_wifiState = WLED_WIFI_FAILED;
                    log_e("[WLED] no se pudo asociar a '%s' en %u ms (¿clave? ¿C6 sin firmware esp-hosted?)",
                          s_ssid, (unsigned)WLED_WIFI_TIMEOUT_MS);
                    waitInterruptible(10000);       // reintenta a los 10 s o antes si hay credenciales nuevas
                }
                continue;
            }
            s_wifiState = WLED_WIFI_CONNECTED;
            log_i("[WLED] WiFi conectado, IP=%s RSSI=%d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
            polledOnce = false;
        }

        s_wifiState = WLED_WIFI_CONNECTED;
        if (!polledOnce || (millis() - lastPoll) >= WLED_POLL_MS) {
            for (int d = 0; d < WLED_DEV_COUNT; d++) {
                if (WiFi.status() != WL_CONNECTED) break;   // (2026-10-12 15:14) cayó a mitad del bucle: sin 'Host is unreachable'
                if (fetchState((WledDev)d)) {
                    const WledState& st = s_state[d];
                    log_i("[WLED] %s on=%d bri=%u ps=%d", kDevName[d], (int)st.on, (unsigned)st.bri, (int)st.ps);
                } else {
                    s_state[d].valid = false;
                }
                vTaskDelay(pdMS_TO_TICKS(100));     // una petición cada vez: el 195 (S2) tiene poca RAM
            }
            lastPoll   = millis();
            polledOnce = true;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void wledInit() {
    if (!s_netMux) s_netMux = xSemaphoreCreateMutex();
#if WLED_ENABLE
    xTaskCreatePinnedToCore(wledTask, "WLED", WLED_TASK_STACK, NULL, 1, NULL, 0);
#else
    log_i("[WLED] desactivado (WLED_ENABLE=0)");
#endif
}
