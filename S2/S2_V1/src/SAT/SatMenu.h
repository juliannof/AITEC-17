#pragma once
// ============================================================
//  SatMenu.h  –  iMakie PTxx Track S2
// ============================================================
#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include "../config.h"

#define C_BG      0x0000
#define C_TEXT    0xFFFF
#define C_ACCENT  0xF800
#define C_GRAY    0x4208
#define C_DARK    0x2104
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_GREEN   0x07E0
#define C_RED     0xF800
#define C_YELLOW  0xFFE0
#define C_BLUE    0x001F
#define C_CYAN    0x07FF
#define C_ORANGE  0xFD20

#define SAT_HDR_H   36
#define SAT_HINT_H  18
#define SAT_ROW_H   36
#define SAT_BADGE_W 28
#define SAT_BADGE_H 20
#define SAT_DEB_MS  160

struct SatConfig {
    uint8_t trackId;
    char    label[7];
    uint8_t pwmMin;
    uint8_t pwmMax;
    bool    touchEnabled;
    bool    motorDisabled; 
};

using CbVoid     = std::function<void()>;
using CbMotor    = std::function<void(int pwm)>;
using CbConfig   = std::function<void(const SatConfig&)>;
using CbLedsTest = std::function<void(int idx, uint8_t r, uint8_t g, uint8_t b)>;

class SatMenu {
public:
    explicit SatMenu(LovyanGFX* tft);

    void onMotorOff       (CbVoid     cb) { _cbMotorOff   = cb; }
    void onMotorOn        (CbVoid     cb) { _cbMotorOn    = cb; }
    void onMotorDrive     (CbMotor    cb) { _cbMotorDrv   = cb; }
    void onRS485Off       (CbVoid     cb) { _cbRS485Off   = cb; }
    void onRS485On        (CbVoid     cb) { _cbRS485On    = cb; }
    void onLedsOff        (CbVoid     cb) { _cbLedsOff    = cb; }
    void onLedsRestore    (CbVoid     cb) { _cbLedsRestore = cb; }
    void onReboot         (CbVoid     cb) { _cbReboot     = cb; }
    void onWiFiOta        (CbVoid     cb) { _cbWiFiOta    = cb; }
    void onConfigSaved    (CbConfig   cb) { _cbSaved      = cb; }
    void onSuspendSprites (CbVoid     cb) { _cbSuspend    = cb; }
    void onRestoreSprites (CbVoid     cb) { _cbRestore    = cb; }
    void onBrightness     (std::function<void(uint8_t)> cb) { _cbBrightness = cb; }
    void onLedsTest       (CbLedsTest cb) { _cbLedsTest   = cb; }

    void update();
    void open();
    void close();
    void showStatus(const char* msg);
    bool isOpen() const { return _open; }
    bool isEncoderConsumed() const { return _encoderConsumed; }
    const SatConfig& getConfig() const { return _cfg; }
    bool isMotorCalibScreen() const { return _scr == Scr::MOTOR_CALIB; }

private:
    enum class Scr {
        MAIN, MOTOR, TOUCH, DIAG,
        OTA, REINICIAR,
        EDIT_TRACKID,
        EDIT_PWMMIN, EDIT_PWMMAX,
        CONFIRM, TOAST,
        TEST_DISPLAY, TEST_ENCODER, TEST_FADER, TEST_NEOPIXEL, TEST_TOUCH,
        MOTOR_CALIB,
        MOTOR_POS,
        MOTOR_TEST,
    };

    struct Item { const char* badge; const char* label; Scr target; };

    LovyanGFX*   _tft;
    LGFX_Sprite  _spr;
    bool         _open  = false;
    bool         _encoderConsumed = false;
    Scr          _scr   = Scr::MAIN;
    Scr          _prev  = Scr::MAIN;
    int          _cur   = 0;
    int          _scrl  = 0;
    bool         _dirty = true;
    SatConfig    _cfg, _tmp;

    int         _eVal=0, _eMin=0, _eMax=255;
    const char* _eTitle="";

    unsigned long _toastT   = 0;
    const char*   _toastMsg = "";
    Scr           _toastRet = Scr::MAIN;
    const char*   _confMsg  = "";
    Scr           _confYes  = Scr::MAIN;

    unsigned long _debT = 0;
    Preferences   _prefs;

    CbVoid     _cbMotorOff, _cbMotorOn, _cbRS485Off, _cbRS485On, _cbReboot, _cbLedsOff, _cbLedsRestore;
    CbVoid     _cbWiFiOta, _cbSuspend, _cbRestore;
    CbMotor    _cbMotorDrv;
    CbConfig   _cbSaved;
    CbLedsTest _cbLedsTest;
    std::function<void(uint8_t)> _cbBrightness;

    int           _dPat   = 0;
    unsigned long _dAutoT = 0;

    long          _encCnt     = 0;
    int           _encLastA   = HIGH;
    int           _encLastB   = HIGH;
    bool          _encSW      = false;
    unsigned long _encDebT    = 0;
    static const int ENC_HIST = 20;
    int8_t        _encHist[ENC_HIST] = {};
    int           _encHistIdx = 0;

    int           _fadRaw    = 0;
    float         _fadPct    = 0.0f;
    int           _tchRaw    = 0;
    bool          _tchOn     = false;
    int           _motPWM    = 0;
    unsigned long _fadT      = 0;
    static const int FAD_HIST = 80;
    uint8_t       _fadHist[FAD_HIST] = {};
    int           _fadHistIdx = 0;
    unsigned long _fadHistT   = 0;

    bool _neoPressed[4] = {};

    enum class Btn { NONE, UP, DOWN, BACK, ENTER };
    Btn _readBtn();

    void _render();
    void _push() { _spr.pushSprite(0, 0); }

    void _drawHdr(const char* t);
    void _drawHints(const char* u, const char* d, const char* b, const char* e);
    void _drawList(const Item* items, int n);
    void _drawValEdit(const char* t, int v, int mn, int mx, const char* u="");
    void _drawLblEdit();
    void _drawConfirm(const char* msg);
    void _drawToast(const char* msg);
    void _drawBadge(int x, int y, const char* t, uint16_t bg, uint16_t fg);
    void _drawHBar(int x, int y, int w, int h, float pct, uint16_t col);
    void _drawDivider(int y);

    void _hMain(Btn b);
    void _hIdent(Btn b);
    void _hMotor(Btn b);
    void _hTouch(Btn b);
    void _hDiag(Btn b);
    void _hEditVal(Btn b);
    void _hEditLbl(Btn b);
    void _hConfirm(Btn b);
    void _hToast(Btn b);

    void _tickTestDisplay(Btn b);
    void _tickTestEncoder(Btn b);
    void _tickTestFader(Btn b);
    void _tickTestNeopixel(Btn b);
    void _tickTestTouch(Btn b);

    void _load();
    void _save();

    void _goto(Scr s) { _prev=_scr; _scr=s; _dirty=true; _cur=0; _scrl=0; }
    void _back()      { _goto(_prev); }
    void _toast(const char* msg, Scr ret);
    void _confirm(const char* msg, Scr yes);

    void _tickMotorCalib(Btn b);
    void _tickMotorPos(Btn b);
    void _tickMotorTest(Btn b);

    int           _fadCalMin  = 8191;
    int           _fadCalMax  = 0;
    uint16_t _noiseMin = 8191;
    uint16_t _noiseMax = 0;
    int           _lastRaw    = 0;
    unsigned long _stopT      = 0;
    bool          _reported   = false;

    uint32_t      _calibRecalib_ms = 0;    // Timestamp para mostrar "(recalibrado)" 600ms (2026-05-12 18:36)

    static const Item _mainItems[];
    static const Item _identItems[];
    static const Item _motorItems[];
    static const Item _touchItems[];
    static const Item _diagItems[];
    static const int  _mainN, _identN, _motorN, _touchN, _diagN;
    unsigned long _neoMuteHoldT = 0;
    uint16_t      _motorTarget = 4096;   // posición manual 0–8191
};