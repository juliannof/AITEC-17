// src/display/Display.cpp
#include <Arduino.h>
#include "Display.h"
#include "hardware/encoder/Encoder.h"
#include "../hardware/Hardware.h"
#include <Preferences.h>
#include "SpriteUtils.h"           // en Display.cpp
#include "../config.h"
// #include "../nvs/NVSValidator.h"  // DESACTIVADO

extern LGFX        tft;
extern LGFX_Sprite header;
extern LGFX_Sprite mainArea;
extern LGFX_Sprite vPotSprite;

static uint8_t _trackId = 0;

namespace {
    uint8_t screenBrightness = 255;
}

void drawMainArea();
void drawVUMeters();
void drawHeaderSprite();
void drawOfflineScreen();
void drawInitializingScreen();
void drawVPotDisplay();
void drawButton(LGFX_Sprite &sprite, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char* label, bool active, uint16_t activeColor);
void setTrackId(uint8_t id) { _trackId = id; }


bool needsTOTALRedraw    = true;
bool needsMainAreaRedraw = false;
bool needsHeaderRedraw   = false;
bool needsVUMetersRedraw = false;
bool needsVPotRedraw     = false;
volatile ConnectionState logicConnectionState = ConnectionState::DISCONNECTED;


static uint8_t currentVPotRaw = 0;
static int8_t currentVPotLevel = VPOT_DEFAULT_LEVEL;
AutoMode currentAutoMode = AUTO_OFF;

// ════════════════════════════════════════════════════════════
//  initDisplay
// ════════════════════════════════════════════════════════════
void initDisplay(bool otaOnlyMode) {
    esp_reset_reason_t reason = esp_reset_reason();
    Serial.printf("Reset reason: %d\n", (int)reason);

    // Pulso de reset obligatorio GPIO33 antes de tft.init()
    pinMode(33, OUTPUT);
    digitalWrite(33, LOW);
    delay(100);
    digitalWrite(33, HIGH);
    delay(50);

    tft.init();
    tft.setRotation(0);
    tft.setBrightness(screenBrightness);
    tft.fillScreen(TFT_BG_COLOR);

    // En OTA-only mode, NO crear sprites para liberar heap
    if (!otaOnlyMode) {
        mainArea.setColorDepth(16);
        mainArea.setPsram(true);
        mainArea.createSprite(MAINAREA_WIDTH, MAINAREA_HEIGHT);

        header.setColorDepth(16);
        header.setPsram(true);
        header.createSprite(TFT_WIDTH, HEADER_HEIGHT);

        vPotSprite.setColorDepth(16);
        vPotSprite.setPsram(true);
        vPotSprite.createSprite(TFT_WIDTH, VPOT_HEIGHT);
    }

    Serial.printf("tft       : %d x %d\n", tft.width(), tft.height());
    Serial.printf("header    : %d x %d  → pushSprite(0, 0)\n",   header.width(),   header.height());
    Serial.printf("mainArea  : %d x %d  → pushSprite(0, %d)\n",  mainArea.width(),  mainArea.height(), HEADER_HEIGHT);
    Serial.printf("vPotSprite: %d x %d  → pushSprite(0, %d)\n",  vPotSprite.width(),vPotSprite.height(), MAINAREA_HEIGHT + HEADER_HEIGHT);
    Serial.printf("Layout total: %d px alto (pantalla: %d)\n", HEADER_HEIGHT + MAINAREA_HEIGHT + VPOT_HEIGHT, tft.height());

    setVPotLevel(VPOT_DEFAULT_LEVEL);

    Serial.printf("header     creado: %s\n", header.width()    > 0 ? "OK" : "FALLO");
    Serial.printf("mainArea   creado: %s\n", mainArea.width()  > 0 ? "OK" : "FALLO");
    Serial.printf("vPotSprite creado: %s\n", vPotSprite.width()> 0 ? "OK" : "FALLO");
    Serial.printf("Heap libre:  %u bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM libre: %u bytes\n", ESP.getFreePsram());
    Serial.println("[SETUP] Display iniciado - LovyanGFX");

    _logSpriteAlloc("header",    header);
    _logSpriteAlloc("mainArea",  mainArea);
    _logSpriteAlloc("vPotSprite",vPotSprite);

    needsTOTALRedraw = true;
}

// ════════════════════════════════════════════════════════════
//  setScreenBrightness / setVPotLevel
// ════════════════════════════════════════════════════════════
void setScreenBrightness(uint8_t brightness) {
    screenBrightness = brightness;
    tft.setBrightness(brightness);
}

// ════════════════════════════════════════════════════════════
//  drawSplashScreen
// ════════════════════════════════════════════════════════════

void drawSplashScreen() {
    Preferences prefs;
    prefs.begin("ptxx", true);
    uint8_t trackId = prefs.getUChar("trackId", 0);
    prefs.end();
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);

    tft.setFont(&fonts::FreeSans24pt7b);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("iMakie", TFT_WIDTH / 2, 70);

    tft.setFont(&fonts::FreeSans12pt7b);
    tft.setTextColor(TFT_MCU_GRAY);
    char buf[16];
    snprintf(buf, sizeof(buf), "Track %d", trackId);
    tft.drawString(buf, TFT_WIDTH / 2, 105);

    tft.setFont(&fonts::FreeSans9pt7b);
    tft.setTextColor(TFT_WHITE);
    char verBuf[32];
    snprintf(verBuf, sizeof(verBuf), "FW %s", FW_VERSION);
    tft.drawString(verBuf, TFT_WIDTH / 2, 130);
    tft.drawString(FW_BUILD_ID, TFT_WIDTH / 2, 160);

    // DESACTIVADO: círculo NVS validator
    // uint16_t circleColor = (NVSValidator::getLastStatus() == NVSStatus::VALID) ? TFT_GREEN : TFT_RED;
    // tft.fillCircle(TFT_WIDTH / 2, 200, 6, circleColor);
}



// ════════════════════════════════════════════════════════════
//  updateDisplay  —  redraw total + redraws incrementales
// ════════════════════════════════════════════════════════════
void updateDisplay() {
    // Detectar transición CONNECTED → DISCONNECTED
    static ConnectionState lastState = ConnectionState::DISCONNECTED;
    if (logicConnectionState == ConnectionState::DISCONNECTED &&
        lastState == ConnectionState::CONNECTED) {
        tft.fillScreen(TFT_BG_COLOR);
        drawOfflineScreen();
    }
    lastState = logicConnectionState;

    // Sin conexión RS485: pantalla offline
    if (logicConnectionState != ConnectionState::CONNECTED) {
        return;
    }

    // Primera vez o redraw forzado
    if (needsTOTALRedraw) {
        tft.setBrightness(screenBrightness);   // ← AÑADIR ESTO
        tft.fillScreen(TFT_BG_COLOR);
        drawHeaderSprite();
        drawMainArea();
        VU::lastActive = -1;   // fuerza fondo completo del VU tras fillScreen
        drawVUMeters();
        drawVPotDisplay();
        needsTOTALRedraw    = false;
        needsMainAreaRedraw = false;
        needsHeaderRedraw   = false;
        needsVUMetersRedraw = false;
        needsVPotRedraw     = false;
        Serial.println("[Display] Redraw total OK");
        return;
    }

    // Redraws incrementales: solo lo que cambió
    if (needsHeaderRedraw) {
        drawHeaderSprite();
        needsHeaderRedraw = false;
    }
    if (needsMainAreaRedraw) {
        drawMainArea();
        needsMainAreaRedraw = false;
    }
    if (needsVUMetersRedraw) {
        drawVUMeters();
        needsVUMetersRedraw = false;
    }
    if (needsVPotRedraw) {
        drawVPotDisplay();
        needsVPotRedraw = false;
    }
}

// ════════════════════════════════════════════════════════════
//  Pantallas de estado
// ════════════════════════════════════════════════════════════
void drawOfflineScreen() {
    static bool splashDrawn = false;
    if (!splashDrawn) {
        tft.setBrightness(100);
        drawSplashScreen();  // necesita el trackId
        splashDrawn = true;
    }
}

void drawInitializingScreen() {
    tft.fillScreen(TFT_BLUE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("MIDI Handshake OK!", tft.width() / 2, tft.height() / 2 - 30);
    tft.setTextColor(TFT_DARKGREY);
    tft.drawString("Esperando DAW...",   tft.width() / 2, tft.height() / 2 + 10);
}

// ════════════════════════════════════════════════════════════
//  drawHeaderSprite
// ════════════════════════════════════════════════════════════

// Función helper — atenuar color RGB565 a ~25%
static uint16_t dimColor565(uint16_t color) {
    uint8_t r = (color >> 11) & 0x1F;
    uint8_t g = (color >> 5)  & 0x3F;
    uint8_t b =  color        & 0x1F;
    return ((r / 4) << 11) | ((g / 4) << 5) | (b / 4);
}


void drawHeaderSprite() {
    static const uint16_t AUTO_COLORS[] = {
        TFT_AUTO_OFF,    // AUTO_OFF
        TFT_AUTO_READ,   // AUTO_READ
        TFT_AUTO_WRITE,  // AUTO_WRITE
        TFT_AUTO_OFF,    // AUTO_TRIM — reservado
        TFT_AUTO_TOUCH,  // AUTO_TOUCH
        TFT_AUTO_LATCH,  // AUTO_LATCH
    };

    auto dimColor565 = [](uint16_t color) -> uint16_t {
        uint8_t r = (color >> 11) & 0x1F;
        uint8_t g = (color >> 5)  & 0x3F;
        uint8_t b =  color        & 0x1F;
        return ((r / 4) << 11) | ((g / 4) << 5) | (b / 4);
    };

    uint16_t baseColor   = AUTO_COLORS[currentAutoMode];
    uint16_t headerColor = selectStates ? baseColor : dimColor565(baseColor);

    header.fillSprite(headerColor);

    screenBrightness = selectStates ? 255 : 70;
    setScreenBrightness(screenBrightness);
    header.pushSprite(0, 0);
}

// ════════════════════════════════════════════════════════════
//  drawButton
// ════════════════════════════════════════════════════════════
void drawButton(LGFX_Sprite &sprite, uint16_t x, uint16_t y,
                uint16_t w, uint16_t h,
                const char* label, bool active, uint16_t activeColor) {
    uint16_t btnColor  = active ? activeColor : TFT_MCU_DARKGRAY;
    uint16_t textColor = active ? TFT_BLACK   : TFT_WHITE;
    sprite.fillRoundRect(x, y, w, h, 5, btnColor);
    sprite.setTextFont(2);
    sprite.setTextSize(1);
    sprite.setTextColor(textColor, btnColor);
    sprite.setTextDatum(MC_DATUM);
    sprite.drawString(label, x + w / 2, y + h / 2);
}

// ════════════════════════════════════════════════════════════
//  drawMainArea
// ════════════════════════════════════════════════════════════
void drawMainArea() {
    mainArea.fillSprite(TFT_BG_COLOR);

    drawButton(mainArea, 10,                        7, BUTTON_WIDTH, BUTTON_HEIGHT, "REC",  recStates,  TFT_REC_COLOR);
    drawButton(mainArea, 10 + BUTTON_WIDTH + 2,     7, BUTTON_WIDTH, BUTTON_HEIGHT, "SOLO", soloStates, TFT_SOLO_COLOR);
    drawButton(mainArea, 10 + BUTTON_WIDTH * 2 + 4, 7, BUTTON_WIDTH, BUTTON_HEIGHT, "MUTE", muteStates, TFT_MUTE_COLOR);


    // --- Track name ---
    mainArea.setTextDatum(TL_DATUM);
    mainArea.setTextColor(TFT_WHITE, TFT_BG_COLOR);
    mainArea.setFont(&fonts::FreeSans24pt7b);
    mainArea.setTextSize(1);
    mainArea.setCursor(7, 90);
    mainArea.print(trackName);

    // --- Fader dB ---
    char faderDbStr[12];
    if (faderPositions < 0.001f) {
        snprintf(faderDbStr, sizeof(faderDbStr), "-inf");
    } else if (faderPositions < 0.75f) {
        float db = (faderPositions / 0.75f) * 60.0f - 60.0f;
        snprintf(faderDbStr, sizeof(faderDbStr), "%.1f dB", db);
    } else {
        float db = ((faderPositions - 0.75f) / 0.25f) * 10.0f;
        snprintf(faderDbStr, sizeof(faderDbStr), "+%.1f dB", db);
    }
    mainArea.setFont(&fonts::FreeSans12pt7b);
    mainArea.setTextColor(TFT_DARKGREY, TFT_BG_COLOR);
    mainArea.setCursor(7, 145);
    mainArea.print(faderDbStr);

    mainArea.pushSprite(0, HEADER_HEIGHT);
    
    
}

// ════════════════════════════════════════════════════════════
//  VU — geometría y estado para dibujo diferencial en tft
// ════════════════════════════════════════════════════════════
namespace VU {
    static constexpr int X      = MAINAREA_WIDTH + 3;
    static constexpr int Y_TOP  = HEADER_HEIGHT + 4;
    static constexpr int W      = 42;
    static constexpr int H      = MAINAREA_HEIGHT - 10;
    static constexpr int SEGS   = 12;
    static constexpr int PAD    = 2;
    static constexpr int CORNER = 2;
    static constexpr int SEG_H  = (H - PAD * (SEGS - 1)) / SEGS;

    int8_t lastActive = -1;   // -1 = nunca dibujado → fuerza fondo completo
    int8_t lastPeak   = -1;
    bool   lastClip   = false;
}

static uint16_t vuSegColor(int i, int active, int peak, bool clip) {
    if (i == VU::SEGS - 1 && clip) return VU_RED_ON;
    if (peak >= 0 && i == peak)    return (i<8)?VU_GREEN_OFF:(i<10)?VU_YELLOW_OFF:VU_RED_OFF;
    if (i < active)                return (i<8)?VU_GREEN_ON :(i<10)?VU_YELLOW_ON :VU_RED_ON;
    return                                (i<8)?VU_GREEN_OFF:(i<10)?VU_YELLOW_OFF:VU_RED_OFF;
}

static void vuDrawSeg(int i, uint16_t fill, bool peakBorder) {
    int y = VU::Y_TOP + VU::H - (i + 1) * VU::SEG_H - i * VU::PAD;
    tft.fillRoundRect(VU::X, y, VU::W, VU::SEG_H, VU::CORNER, fill);
    if (peakBorder) {
        tft.drawRoundRect(VU::X,   y,   VU::W,   VU::SEG_H,   VU::CORNER,   VU_PEAK_COLOR);
        tft.drawRoundRect(VU::X+1, y+1, VU::W-2, VU::SEG_H-2, VU::CORNER-1, VU_PEAK_COLOR);
    }
}

// ════════════════════════════════════════════════════════════
//  drawVUMeters — diferencial: solo segmentos que cambiaron
// ════════════════════════════════════════════════════════════
void drawVUMeters() {
    int  active   = (int)round(vuLevels     * VU::SEGS);
    int  peak     = (int)round(vuPeakLevels * VU::SEGS);
    if (peak > 0) peak--;
    bool showPeak = (vuPeakLevels > vuLevels + 0.001f);
    if (showPeak && active > 0 && peak < active - 1) peak = active - 1;
    if (!showPeak) peak = -1;
    bool clip = vuClipState;

    if (VU::lastActive < 0) {
        // Primera vez o tras TOTAL redraw: fondo gris + todos los segmentos
        tft.fillRect(MAINAREA_WIDTH, HEADER_HEIGHT,
                     TFT_WIDTH - MAINAREA_WIDTH, MAINAREA_HEIGHT, TFT_MCU_DARKGRAY);
        for (int i = 0; i < VU::SEGS; i++) {
            bool isPeak = (i == peak);
            vuDrawSeg(i, vuSegColor(i, active, isPeak ? peak : -1, clip), isPeak);
        }
    } else {
        // Diferencial: solo los segmentos cuyo color o estado peak cambió
        bool prevShowPeak = (VU::lastPeak >= 0);
        for (int i = 0; i < VU::SEGS; i++) {
            bool isPeakNow  = showPeak    && (i == peak);
            bool isPeakPrev = prevShowPeak && (i == VU::lastPeak);
            uint16_t cNow  = vuSegColor(i, active,        isPeakNow  ? peak        : -1, clip);
            uint16_t cPrev = vuSegColor(i, VU::lastActive, isPeakPrev ? VU::lastPeak : -1, VU::lastClip);
            if (cNow != cPrev || isPeakNow != isPeakPrev)
                vuDrawSeg(i, cNow, isPeakNow);
        }
    }

    VU::lastActive = (int8_t)active;
    VU::lastPeak   = (int8_t)(showPeak ? peak : -1);
    VU::lastClip   = clip;
}

// ════════════════════════════════════════════════════════════
//  handleVUMeterDecay — lógica S3 + bug fix timer peak
// ════════════════════════════════════════════════════════════
void handleVUMeterDecay() {
    const unsigned long DECAY_INTERVAL_MS = 100;
    const unsigned long PEAK_HOLD_TIME_MS = 2000;
    const float DECAY_AMOUNT = 1.0f / 12.0f;

    unsigned long now = millis();
    bool changed = false;

    // 1. Decaimiento del nivel normal
    if (vuLevels > 0 && now - vuLastUpdateTime > DECAY_INTERVAL_MS) {
        float old = vuLevels;
        vuLevels -= DECAY_AMOUNT;
        if (vuLevels < 0.01f) vuLevels = 0.0f;
        vuLastUpdateTime = now;
        changed = true;
        log_v("VU Level decayed %.3f → %.3f", old, vuLevels);
    }

    // 2. Decaimiento del peak tras hold time
    if (vuPeakLevels > 0 && now - vuPeakLastUpdateTime > PEAK_HOLD_TIME_MS) {
        if (vuPeakLevels > vuLevels) {
            float old = vuPeakLevels;
            vuPeakLevels = vuLevels;
            vuPeakLastUpdateTime = now;     // ← BUG FIX: sin esto se dispara cada ciclo
            changed = true;
            log_v("VU Peak decayed %.3f → %.3f (jump to current level)", old, vuPeakLevels);
        }
    }

    // 3. Seguridad: peak nunca puede ser menor que el nivel actual
    if (vuPeakLevels < vuLevels) {
        log_w("VU Peak (%.3f) < VU Level (%.3f). Corrigiendo.", vuPeakLevels, vuLevels);
        vuPeakLevels = vuLevels;
        vuPeakLastUpdateTime = now;
        changed = true;
    }

    if (changed) needsVUMetersRedraw = true;
}

static uint16_t autoModeColor(AutoMode mode) {
    static const uint16_t AUTO_COLORS[] = {
        TFT_AUTO_OFF,    // 0 — OFF
        TFT_AUTO_READ,   // 1 — READ
        TFT_AUTO_WRITE,  // 2 — WRITE
        TFT_AUTO_OFF,    // 3 — TRIM
        TFT_AUTO_TOUCH,  // 4 — TOUCH
        TFT_AUTO_LATCH,  // 5 — LATCH
    };
    uint8_t idx = (uint8_t)mode;
    if (idx >= 6) idx = 0;
    return AUTO_COLORS[idx];
}

// ════════════════════════════════════════════════════════════
//  drawVPotDisplay
// ════════════════════════════════════════════════════════════
void drawVPotDisplay() {
    vPotSprite.fillSprite(TFT_MCU_DARKGRAY);

    const uint8_t raw    = currentVPotRaw;
    const uint8_t pos    = raw & 0x0F;         // 0=off, 1-11
    const uint8_t mode   = (raw >> 4) & 0x03;  // 0=dot 1=boost/cut 2=bar 3=spread
    const bool    center = (raw >> 6) & 0x01;
    const uint16_t litColor = autoModeColor(currentAutoMode);

    const int N      = 5;   // segmentos por lado
    const int gapV   = 3;
    const int cornerR= 4;
    const int seg_y  = 3;
    const int seg_h  = vPotSprite.height() - 6;

    // Distribuir ancho: N segs izq + centro + N segs der
    const int totalW = vPotSprite.width() - 4;
    const int segW   = (totalW - (2*N + 1) * gapV) / (2*N + 1);
    const int ctrW   = segW;  // centro = mismo ancho
    const int startX = (vPotSprite.width() - (segW * (2*N+1) + gapV * (2*N))) / 2;

    // pos 1-5 = izquierda (índice 4..0), pos 6 = centro, pos 7-11 = derecha (índice 0..4)
    for (int i = 0; i < N; i++) {
        int seg = N - 1 - i;  // seg 4=más lejano, 0=más cercano al centro
        int posL = i + 1;;   // posición Mackie correspondiente (1-5)
        bool lit = false;
        if (pos > 0) {
            switch (mode) {
                case 0: lit = (pos == posL);              break; // dot
                case 1: lit = (pos <= 5 && posL >= pos); break; // boost/cut izq
                case 2: lit = false;                      break; // bar: solo derecha
                case 3: lit = (pos >= 6 && (6 - posL) <= (int)pos - 6) ||
                              (pos <= 5 && posL >= pos);  break; // spread
            }
        }
        int x = startX + i * (segW + gapV);
        vPotSprite.fillRoundRect(x, seg_y, segW, seg_h, cornerR, lit ? litColor : TFT_MCU_GRAY);
    }

    // Centro (pos 6)
    int cx = startX + N * (segW + gapV);
    bool ctrLit = (pos > 0) && (center || pos == 6 ||
                  mode == 2 || (mode == 1 && pos == 6));
    vPotSprite.fillRoundRect(cx, seg_y, ctrW, seg_h, cornerR,
                          ctrLit ? litColor : TFT_MCU_GRAY);

    // Derecha (pos 7-11)
    for (int i = 0; i < N; i++) {
        int posR = i + 7;  // posición Mackie 7-11
        bool lit = false;
        if (pos > 0) {
            switch (mode) {
                case 0: lit = (pos == posR);         break; // dot
                case 1: lit = (pos >= 7 && posR <= pos); break; // boost/cut der
                case 2: lit = (posR <= pos);         break; // bar
                case 3: lit = (pos >= 6 && posR <= pos) ||
                              (pos <= 5 && posR <= (12 - pos)); break; // spread
            }
        }
        int x = cx + ctrW + gapV + i * (segW + gapV);
        vPotSprite.fillRoundRect(x, seg_y, segW, seg_h, cornerR, lit ? litColor : TFT_MCU_GRAY);
    }

    vPotSprite.pushSprite(0, MAINAREA_HEIGHT + HEADER_HEIGHT);
}

void setVPotRaw(uint8_t raw) {
    if ((raw & 0x7F) != currentVPotRaw) {
        currentVPotRaw  = raw & 0x7F;
        needsVPotRedraw = true;
    }
}

void setAutoMode(uint8_t mode) {
    AutoMode m = (AutoMode)(mode & 0x07);
    if (m != currentAutoMode) {
        currentAutoMode   = m;
        needsVPotRedraw   = true;
        needsHeaderRedraw = true;
    }
}

void setVPotLevel(int8_t level) {
    if (level < VPOT_MIN_LEVEL) level = VPOT_MIN_LEVEL;
    if (level > VPOT_MAX_LEVEL) level = VPOT_MAX_LEVEL;
    if (level != currentVPotLevel) {
        currentVPotLevel = level;
        needsVPotRedraw = true;
    }
}