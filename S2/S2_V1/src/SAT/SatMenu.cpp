// ============================================================
//  SatMenu.cpp  –  iMakie PTxx Track S2
// ============================================================
#include "SatMenu.h"
#include "../hardware/encoder/Encoder.h"
#include "../config.h"
#include "../hardware/Hardware.h"  // Para buttonRec, buttonMute (2026-05-10 19:54)

#include "../hardware/fader/FaderADC.h"   // ← añadir
#include "../hardware/fader/FaderTouch.h"
#include "../hardware/Motor/Motor.h"

extern FaderADC faderADC;                  // ← añadir

// ─────────────────────────────────────────────────────────────
//  Tablas de menú
// ─────────────────────────────────────────────────────────────
const SatMenu::Item SatMenu::_mainItems[] = {
    {"ID","Track ID (1-9)",   Scr::EDIT_TRACKID },
    {"OT","Activar OTA", Scr::OTA       },
    {"MT","Motor",       Scr::MOTOR     },
    {"TC","Touch",       Scr::TOUCH     },
    {"DG","Diagnostico", Scr::DIAG      },
    {"RB","Reiniciar",   Scr::REINICIAR },
};
const SatMenu::Item SatMenu::_motorItems[] = {
    {"MX","PWM Maximo",    Scr::EDIT_PWMMAX},
    {"MN","PWM Minimo",    Scr::EDIT_PWMMIN},
    {"PO","Tiempo Min/Max",Scr::MOTOR_POS},
};

const SatMenu::Item SatMenu::_touchItems[] = {
    {"EN","Habilitado",  Scr::TOUCH        },
};
const SatMenu::Item SatMenu::_diagItems[] = {
    {"DP","Test Display",  Scr::TEST_DISPLAY  },
    {"EC","Test Encoder",  Scr::TEST_ENCODER  },
    {"FD","Test Fader",    Scr::TEST_FADER    },
    {"NP","Test Botones",  Scr::TEST_NEOPIXEL },
    {"TC","Test Touch",    Scr::TEST_TOUCH    },
};
const int SatMenu::_mainN  = 6;
const int SatMenu::_motorN = 3;  // PWM Max, PWM Min, Tiempo Min/Max (2026-08-23 — Calibrar y Test Mode eliminados, ya no necesarios)
const int SatMenu::_touchN = 2;
const int SatMenu::_diagN  = 5;

// ─────────────────────────────────────────────────────────────
//  Constructor
// ─────────────────────────────────────────────────────────────
SatMenu::SatMenu(LovyanGFX* tft)
    : _tft(tft),
      _spr(tft)
{
    pinMode(BUTTON_PIN_REC,    INPUT_PULLUP);
    pinMode(BUTTON_PIN_SOLO,   INPUT_PULLUP);
    pinMode(BUTTON_PIN_MUTE,   INPUT_PULLUP);
    pinMode(BUTTON_PIN_SELECT, INPUT_PULLUP);
    pinMode(ENCODER_PIN_A,  INPUT);
    pinMode(ENCODER_PIN_B,  INPUT);
    pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
    // Motor pines controlados SOLO por Motor::init(), NO por SAT (2026-05-10 20:30)
    _load();
    _tmp = _cfg;
}

// ─────────────────────────────────────────────────────────────
//  open / close
// ─────────────────────────────────────────────────────────────
void SatMenu::open() {
    if (_open) return;
    _open = true;
    _encoderConsumed = true;
    _scr = Scr::MAIN; _cur = 0; _scrl = 0; _dirty = true; _tmp = _cfg;

    if (_cbMotorOff)   _cbMotorOff();
    if (_cbRS485Off)   _cbRS485Off();
    if (_cbLedsOff)    _cbLedsOff();
    if (_cbBrightness) _cbBrightness(255);
    if (_cbSuspend)    _cbSuspend();

    _spr.setColorDepth(16);
    _spr.setPsram(true);
    _spr.createSprite(_tft->width(), _tft->height());
    _spr.setTextFont(1);

    _spr.fillScreen(C_BG);
    _push();
}

void SatMenu::close() {
    if (!_open) return;
    if (_cfg.trackId == 0) return;
    _open = false;
    _encoderConsumed = false;

    // Motor controlado SOLO por Motor.cpp, NO por SAT (2026-05-10 20:30)
    _spr.deleteSprite();

    if (_cbMotorOn)     _cbMotorOn();
    if (_cbRS485On)     _cbRS485On();
    if (_cbRestore)     _cbRestore();
    if (_cbLedsRestore) _cbLedsRestore();
    _tft->fillScreen(C_BLACK);
}

// ─────────────────────────────────────────────────────────────
//  update
// ─────────────────────────────────────────────────────────────
void SatMenu::update() {
    if (!_open) return;

    bool live = (_scr == Scr::TEST_DISPLAY   ||
             _scr == Scr::TEST_ENCODER   ||
             _scr == Scr::TEST_FADER     ||
             _scr == Scr::TEST_NEOPIXEL  ||
             _scr == Scr::TEST_TOUCH     ||
             _scr == Scr::MOTOR_POS);

    // ADC + Motor frescos en pantallas de motor
    if (_scr == Scr::MOTOR_POS) {
        faderADC.update();
        /* Motor::setADC(faderADC.getFaderPos()); */
    }

    Btn b = _readBtn();

    if (_scr == Scr::TOAST) {
        if (millis() - _toastT > 1300 || b != Btn::NONE) _goto(_toastRet);
        else { _render(); }
        return;
    }

    if (!live && !_dirty && b == Btn::NONE) return;
    if (_dirty || live) { _render(); _dirty = false; }

    switch (_scr) {
        case Scr::MAIN:          _hMain(b);            break;
        case Scr::MOTOR:         _hMotor(b);           break;
        case Scr::MOTOR_POS:     _tickMotorPos(b);   break;
        case Scr::TOUCH:         _hTouch(b);           break;
        case Scr::DIAG:          _hDiag(b);            break;
        case Scr::EDIT_TRACKID:
        case Scr::EDIT_PWMMIN:
        case Scr::EDIT_PWMMAX:   _hEditVal(b);         break;
        case Scr::CONFIRM:       _hConfirm(b);         break;
        case Scr::TOAST:         _hToast(b);           break;
        case Scr::TEST_DISPLAY:  _tickTestDisplay(b);  break;
        case Scr::TEST_ENCODER:  _tickTestEncoder(b);  break;
        case Scr::TEST_FADER:    _tickTestFader(b);    break;
        case Scr::TEST_NEOPIXEL: _tickTestNeopixel(b); break;
        case Scr::TEST_TOUCH:    _tickTestTouch(b);    break;
        case Scr::REINICIAR:
            _confirm("Reiniciar dispositivo?", Scr::REINICIAR); break;
        default: break;
    }
}

// ─────────────────────────────────────────────────────────────
//  Lectura botones
// ─────────────────────────────────────────────────────────────
SatMenu::Btn SatMenu::_readBtn() {
    // Leer encoder si SAT está consumiendo
    if (_encoderConsumed) {
        static int pendingEvents = 0;
        static int encoderDir = 0;  // 1=DOWN, -1=UP

        // Emitir eventos pendientes por aceleración
        if (pendingEvents > 0) {
            pendingEvents--;
            return (encoderDir > 0) ? Btn::DOWN : Btn::UP;
        }

        // Leer nuevo delta
        int delta = Encoder::getCount();
        if (delta > 0) {
            int detents = delta / 4;
            if (detents < 1) detents = 1;  // giro parcial (transición incompleta) cuenta como 1 paso
            pendingEvents = detents - 1;   // el evento de este detent ya se devuelve abajo — resto queda pendiente
            encoderDir = 1;
            Encoder::reset();  // consumir delta — evita reprocesarlo en el próximo tick (2026-08-13 09:08)
            return Btn::DOWN;
        }
        if (delta < 0) {
            int detents = (-delta) / 4;
            if (detents < 1) detents = 1;
            pendingEvents = detents - 1;
            encoderDir = -1;
            Encoder::reset();  // consumir delta — evita reprocesarlo en el próximo tick (2026-08-13 09:08)
            return Btn::UP;
        }

        // Botón encoder = ENTER (como SELECT)
        if (digitalRead(ENCODER_SW_PIN) == LOW) { _debT=millis(); return Btn::ENTER; }
    }

    unsigned long n = millis();
    if (n - _debT < SAT_DEB_MS) return Btn::NONE;
    if (digitalRead(BUTTON_PIN_REC)    == LOW) { _debT=n; return Btn::UP;    }
    if (digitalRead(BUTTON_PIN_SOLO)   == LOW) { _debT=n; return Btn::DOWN;  }
    if (digitalRead(BUTTON_PIN_MUTE)   == LOW) { _debT=n; return Btn::BACK;  }
    if (digitalRead(BUTTON_PIN_SELECT) == LOW) { _debT=n; return Btn::ENTER; }
    return Btn::NONE;
}

// ─────────────────────────────────────────────────────────────
//  Render dispatcher
// ─────────────────────────────────────────────────────────────
void SatMenu::_render() {
    switch (_scr) {
        case Scr::TEST_DISPLAY:  _tickTestDisplay(Btn::NONE);  return;
        case Scr::TEST_ENCODER:  _tickTestEncoder(Btn::NONE);  return;
        case Scr::TEST_FADER:    _tickTestFader(Btn::NONE);    return;
        case Scr::TEST_NEOPIXEL: _tickTestNeopixel(Btn::NONE); return;
        case Scr::TEST_TOUCH:    _tickTestTouch(Btn::NONE);    return;
        case Scr::MOTOR_POS:     _tickMotorPos(Btn::NONE);   return;
        default: break;
    }

    _spr.fillScreen(C_BG);

    switch (_scr) {
        case Scr::MAIN:
            _drawHdr(_cfg.trackId == 0 ? "!SIN CONFIGURAR!" : "iMakie SAT");
            _drawList(_mainItems, _mainN);
            _drawHints("","","Salir","Entrar");
            if (_cfg.trackId == 0) {
                int W = _spr.width();
                _spr.fillRect(0, SAT_HDR_H, W, 20, C_ACCENT);
                _spr.setTextColor(C_WHITE, C_ACCENT);
                _spr.setTextSize(1);
                _spr.setTextDatum(textdatum_t::middle_center);
                _spr.drawString("Configura Track ID antes de usar", W/2, SAT_HDR_H+10);
            }
            break;
        case Scr::MOTOR: {
            _drawHdr("Motor");

            // Verificar si PWM es válido (2026-05-10 20:20)
            if (Motor::getPWMMin() == 0 || Motor::getPWMMax() == 0) {
                int W = _spr.width(), H = _spr.height();
                _spr.setTextColor(C_RED, C_BG);
                _spr.setTextSize(2);
                _spr.setTextDatum(textdatum_t::middle_center);
                _spr.drawString("PWM invalido", W/2, H/2);
                _spr.setTextSize(1);
                _spr.drawString("Configura PWM Min/Max", W/2, H/2+40);
                _drawHints("","","Atras","");
            } else {
                _drawList(_motorItems, _motorN);
                _drawHints("","","Atras","Entrar");
            }
            break;
        }
        case Scr::TOUCH: {
            _drawHdr("Touch");
            int W = _spr.width();
            int y = SAT_HDR_H + 4;
            bool en = _tmp.touchEnabled;
            _spr.fillRect(0, y, W, SAT_ROW_H, _cur==0 ? C_ACCENT : C_BG);
            _drawBadge(6, y+(SAT_ROW_H-SAT_BADGE_H)/2, "EN",
                _cur==0?C_WHITE:C_ACCENT, _cur==0?C_ACCENT:C_WHITE);
            _spr.setTextColor(_cur==0?C_WHITE:C_TEXT, _cur==0?C_ACCENT:C_BG);
            _spr.setTextSize(1); _spr.setTextDatum(textdatum_t::middle_left);
            _spr.drawString("Habilitado", SAT_BADGE_W+14, y+SAT_ROW_H/2);
            _spr.setTextDatum(textdatum_t::middle_right);
            _spr.setTextColor(en?C_GREEN:C_GRAY, _cur==0?C_ACCENT:C_BG);
            _spr.drawString(en?"ON":"OFF", W-10, y+SAT_ROW_H/2);
            _drawHints("","","Atras","Activar/Desactivar");
            break;
        }
        case Scr::DIAG:
            _drawHdr("Diagnostico");
            _drawList(_diagItems, _diagN);
            _drawHints("","","Atras","Entrar");
            break;
        case Scr::EDIT_TRACKID:
        case Scr::EDIT_PWMMIN:
        case Scr::EDIT_PWMMAX:
            _drawValEdit(_eTitle, _eVal, _eMin, _eMax, "");
            break;
        case Scr::CONFIRM: _drawConfirm(_confMsg); break;
        case Scr::TOAST:   _drawToast(_toastMsg);  break;
        default: break;
    }

    _push();
}

// ─────────────────────────────────────────────────────────────
//  TEST DISPLAY
// ─────────────────────────────────────────────────────────────
static const uint16_t DISP_SOLID_COLORS[] = {
    C_RED,C_GREEN,C_BLUE,C_WHITE,C_BLACK,C_YELLOW,C_CYAN,C_ORANGE,C_ACCENT
};
static const char* DISP_SOLID_NAMES[] = {
    "Rojo","Verde","Azul","Blanco","Negro","Amarillo","Cyan","Naranja","Acento"
};
static const int DISP_SOLID_N = 9;

void SatMenu::_tickTestDisplay(Btn b) {
    int W = _spr.width(), H = _spr.height();

    if (b == Btn::BACK) { _goto(Scr::DIAG); return; }
    if (b == Btn::ENTER || b == Btn::UP) _dPat = (_dPat+1) % 7;
    if (b == Btn::DOWN)                  _dPat = (_dPat+6) % 7;

    switch (_dPat) {
        case 0: {
            static int ci = 0;
            if (b == Btn::ENTER || b == Btn::UP) ci = (ci+1) % DISP_SOLID_N;
            if (b == Btn::DOWN)                  ci = (ci+DISP_SOLID_N-1) % DISP_SOLID_N;
            _spr.fillScreen(DISP_SOLID_COLORS[ci]);
            uint16_t inv = ~DISP_SOLID_COLORS[ci];
            _spr.setTextColor(inv); _spr.setTextSize(2);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString(DISP_SOLID_NAMES[ci], W/2, H/2);
            _spr.setTextSize(1);
            _spr.drawString("SEL/REC/SOL=color  MUT=salir", W/2, H-14);
            break;
        }
        case 1: {
            for (int x=0;x<W;x++) {
                uint8_t r=(x*31)/W, g=(x*63)/W, bv=31-(x*31)/W;
                _spr.drawFastVLine(x,0,H,(uint16_t)((r<<11)|(g<<5)|bv));
            }
            _spr.setTextColor(C_WHITE); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString("Gradiente RGB — SEL=sig  MUT=salir",W/2,H/2);
            break;
        }
        case 2: {
            _spr.fillScreen(C_BLACK);
            for (int y=0;y<H;y+=8) for (int x=0;x<W;x+=8)
                if (((x/8)+(y/8))%2==0) _spr.fillRect(x,y,8,8,C_WHITE);
            _spr.setTextColor(C_ACCENT); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString("Checker 8px — SEL=sig  MUT=salir",W/2,H/2);
            break;
        }
        case 3: {
            _spr.fillScreen(C_WHITE);
            int y=4;
            _spr.setTextColor(C_BLACK); _spr.setTextDatum(textdatum_t::top_left);
            _spr.setTextSize(1); _spr.drawString("Texto tam 1 — ABCDEFGabcdefg0123",2,y); y+=14;
            _spr.setTextSize(2); _spr.drawString("Tam 2 ABCabc123",2,y); y+=22;
            _spr.setTextSize(3); _spr.drawString("Tam 3 ABC",2,y); y+=30;
            _spr.setTextColor(C_ACCENT);
            _spr.setTextSize(2); _spr.drawString("Acento rojo",2,y); y+=22;
            _spr.setTextColor(C_DARK); _spr.setTextSize(1);
            _spr.drawString("SEL=sig  MUT=salir",2,H-14);
            break;
        }
        case 4: {
            _spr.fillScreen(C_BLACK);
            _spr.fillRect(10,40,60,40,C_RED);
            _spr.fillCircle(W/2,80,30,C_GREEN);
            _spr.fillTriangle(10,H-20,W/2-10,H-80,W-10,H-20,C_BLUE);
            _spr.drawRect(W-80,40,60,40,C_YELLOW);
            _spr.drawCircle(W/2,H/2,20,C_CYAN);
            _spr.drawRoundRect(20,H/2-20,80,40,8,C_ACCENT);
            _spr.setTextColor(C_WHITE); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::bottom_center);
            _spr.drawString("Geometria — SEL=sig  MUT=salir",W/2,H-2);
            break;
        }
        case 5: {
            int bars=32, bw=W/bars;
            for (int i=0;i<bars;i++) {
                uint8_t v=(i*255)/(bars-1);
                uint16_t col=((v>>3)<<11)|((v>>2)<<5)|(v>>3);
                _spr.fillRect(i*bw,0,bw,H-SAT_HINT_H,col);
            }
            _spr.fillRect(0,H-SAT_HINT_H,W,SAT_HINT_H,C_BLACK);
            _spr.setTextColor(C_WHITE); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString("Grises — SEL=sig  MUT=salir",W/2,H-SAT_HINT_H/2);
            break;
        }
        case 6: {
            static int lx=0; static uint16_t lcol=C_ACCENT;
            _spr.drawFastVLine((lx+W-4)%W,0,H-SAT_HINT_H,C_BLACK);
            _spr.drawFastVLine(lx,0,H-SAT_HINT_H,lcol);
            lx=(lx+1)%W;
            if (lx==0) {
                static int ci=0;
                const uint16_t cols[]={C_RED,C_GREEN,C_BLUE,C_CYAN,C_ACCENT};
                ci=(ci+1)%5; lcol=cols[ci];
            }
            _spr.fillRect(0,H-SAT_HINT_H,W,SAT_HINT_H,C_BLACK);
            _spr.setTextColor(C_WHITE); _spr.setTextSize(1);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString("Barrido — SEL=sig  MUT=salir",W/2,H-SAT_HINT_H/2);
            break;
        }
    }
    _push();
}

// ─────────────────────────────────────────────────────────────
//  TEST ENCODER
// ─────────────────────────────────────────────────────────────
void SatMenu::_tickTestEncoder(Btn b) {
    int W = _spr.width(), H = _spr.height();

    if (b == Btn::BACK) { _goto(Scr::DIAG); return; }
    if (b == Btn::UP || b == Btn::DOWN) { _encCnt=0; memset(_encHist,0,ENC_HIST); }
    if (b == Btn::ENTER) { _encCnt=0; memset(_encHist,0,ENC_HIST); }

    // Lógica de encoder está en Encoder.cpp (única fuente de verdad)
    _encCnt = Encoder::getCount();
    _encHist[_encHistIdx] = (int8_t)constrain(_encCnt,-63,63);
    _encHistIdx = (_encHistIdx+1) % ENC_HIST;
    
    int A = digitalRead(ENCODER_PIN_A);
    int B = digitalRead(ENCODER_PIN_B);
    bool sw = (digitalRead(ENCODER_SW_PIN) == LOW);
    _encSW = sw;

    _spr.fillScreen(C_BG);
    _spr.fillRect(0,0,W,SAT_HDR_H,C_BG);
    _spr.setTextColor(C_WHITE,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("TEST ENCODER",W/2,SAT_HDR_H/2);
    _spr.drawFastHLine(0,SAT_HDR_H-2,W,C_ACCENT);

    int y = SAT_HDR_H+6;
    _spr.setTextDatum(textdatum_t::top_left); _spr.setTextSize(1);
    _spr.setTextColor(A==LOW?C_GREEN:C_GRAY,C_BG);
    _spr.drawString(A==LOW?"A = LOW ":"A = HIGH",6,y);
    _spr.setTextColor(B==LOW?C_GREEN:C_GRAY,C_BG);
    _spr.drawString(B==LOW?"B = LOW ":"B = HIGH",W/2,y); y+=16;
    _spr.setTextColor(sw?C_YELLOW:C_GRAY,C_BG);
    _spr.drawString(sw?"SW = PULSADO  ":"SW = libre    ",6,y); y+=18;

    char cbuf[8]; snprintf(cbuf,8,"%+ld",_encCnt);
    _spr.setTextSize(4); _spr.setTextDatum(textdatum_t::middle_center);
    _spr.setTextColor(C_WHITE,C_BG);
    _spr.drawString(cbuf,W/2,y+28); y+=64;

    _spr.setTextSize(2);
    int p2=(_encHistIdx+ENC_HIST-1)%ENC_HIST, p3=(_encHistIdx+ENC_HIST-2)%ENC_HIST;
    const char* dir="  =  ";
    if (_encHist[p2]>_encHist[p3]) dir=" >>  ";
    else if (_encHist[p2]<_encHist[p3]) dir=" <<  ";
    _spr.setTextColor(C_CYAN,C_BG);
    _spr.drawString(dir,W/2,y); y+=22;

    int gW=W-12,gH=40,gX=6,gY=y;
    _spr.drawRect(gX-1,gY-1,gW+2,gH+2,C_DARK);
    int bw2=gW/ENC_HIST, cy2=gY+gH/2;
    _spr.drawFastHLine(gX,cy2,gW,C_DARK);
    for (int i=0;i<ENC_HIST;i++) {
        int idx=(_encHistIdx+i)%ENC_HIST, v=_encHist[idx];
        int barH=abs(v)*(gH/2)/63;
        uint16_t col=v>=0?C_CYAN:C_ACCENT;
        int bx=gX+i*bw2;
        if (v>=0) _spr.fillRect(bx,cy2-barH,bw2>1?bw2-1:1,barH,col);
        else      _spr.fillRect(bx,cy2,bw2>1?bw2-1:1,barH,col);
    }

    _spr.fillRect(0,H-SAT_HINT_H,W,SAT_HINT_H,C_BG);
    _spr.drawFastHLine(0,H-SAT_HINT_H,W,C_DARK);
    _spr.setTextSize(1); _spr.setTextColor(C_GRAY,C_BG);
    _spr.setTextDatum(textdatum_t::middle_left);
    _spr.drawString("REC/SOL=reset  SEL=hist  MUT=salir",4,H-SAT_HINT_H/2);
    _push();
}

// ─────────────────────────────────────────────────────────────
//  TEST FADER
// ─────────────────────────────────────────────────────────────
void SatMenu::_tickTestFader(Btn b) {
    if (b == Btn::BACK) { _goto(Scr::DIAG); return; }
    if (b == Btn::ENTER) {
        _fadCalMin=8191; _fadCalMax=0;
        _noiseMin=8191;  _noiseMax=0;
        _reported=false; _stopT=0;
        Motor::startCalib();  // Dispara calibración automática del motor (2026-05-11 17:45)
    }

    unsigned long now=millis();
    if (now-_fadT>25) {
        _fadT=now;
        faderADC.update();
        _fadRaw = faderADC.getRawLast();
        uint16_t fadEma = faderADC.getFaderPos();

        if ((int)fadEma < _fadCalMin) { _fadCalMin = fadEma; _reported = false; }
        if ((int)fadEma > _fadCalMax) { _fadCalMax = fadEma; _reported = false; }

        if (abs((int)fadEma - (int)_lastRaw) > 15) {
            _stopT       = now;
            _lastRaw     = (int)fadEma;
            _noiseMin    = fadEma;
            _noiseMax    = fadEma;
            _reported    = false;
        } else {
            if (fadEma < _noiseMin) _noiseMin = fadEma;
            if (fadEma > _noiseMax) _noiseMax = fadEma;
            if (!_reported && now - _stopT > 500 && _fadCalMax > _fadCalMin) {
                _reported = true;
            }
        }

        _fadPct = (_fadCalMax > _fadCalMin) ?
                  constrain((float)((int)fadEma - _fadCalMin) / (_fadCalMax - _fadCalMin), 0.0f, 1.0f) :
                  constrain((float)fadEma / 8191.0f, 0.0f, 1.0f);

        if (now-_fadHistT>50) {
            _fadHistT=now;
            _fadHist[_fadHistIdx]=(uint8_t)(_fadPct*255);
            _fadHistIdx=(_fadHistIdx+1)%FAD_HIST;
        }
    }

    int W = _spr.width();
    _spr.fillScreen(C_BG);
    _drawHdr("TEST FADER");
    int y=SAT_HDR_H+6;
    char buf[32];

    _spr.setTextColor(C_CYAN,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::top_left);
    _spr.drawString("FADER",4,y);
    snprintf(buf,32,"raw=%4d  [%d-%d]", _fadRaw, _fadCalMin, _fadCalMax);
    _spr.setTextColor(C_TEXT,C_BG); _spr.drawString(buf,44,y); y+=14;
    _drawHBar(4,y,W-8,14,_fadPct,C_CYAN);
    int ugX=4+(int)((W-8)*0.75f);
    _spr.drawFastVLine(ugX,y,14,C_YELLOW);
    y+=20;
    _spr.setTextColor(C_YELLOW,C_BG); _spr.setTextDatum(textdatum_t::top_center);
    _spr.drawString("0dB",ugX,y); y+=12;

    _drawDivider(y+2); y+=8;
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextDatum(textdatum_t::top_left);
    for (int i=0; i<FAD_HIST-1; i++) {
        int h1=(int)_fadHist[i]*100/256;
        int h2=(int)_fadHist[(i+1)%FAD_HIST]*100/256;
        _spr.drawLine(4+i,y+100-h1, 4+i+1,y+100-h2, C_CYAN);
    }
    y+=108;
    _drawHints("","","Atras","Calibrar");
    _push();
}

void SatMenu::_tickTestTouch(Btn b) {
    if (b == Btn::BACK) { _goto(Scr::DIAG); return; }

    FaderTouch::update();

    unsigned long now = millis();
    if (now - _fadT > 50) {
        _fadT = now;
        
        uint32_t raw = FaderTouch::getRaw();
        uint32_t base = FaderTouch::getBase();
        bool touched = FaderTouch::isTouched();
        
        int W = _spr.width();
        _spr.fillScreen(C_BG);
        _drawHdr("TEST TOUCH");
        int y = SAT_HDR_H + 6;
        char buf[40];

        _spr.setTextColor(C_CYAN, C_BG); _spr.setTextSize(1);
        _spr.setTextDatum(textdatum_t::top_left);
        _spr.drawString("BASELINE", 4, y);
        snprintf(buf, 40, "%5lu", base);
        _spr.setTextColor(C_TEXT, C_BG); _spr.drawString(buf, 80, y); y += 14;

        _spr.setTextColor(C_YELLOW, C_BG);
        _spr.drawString("RAW", 4, y);
        snprintf(buf, 40, "%5lu", raw);
        _spr.setTextColor(C_TEXT, C_BG); _spr.drawString(buf, 80, y); y += 14;

        _drawDivider(y + 2); y += 8;

        if (base > 0) {
            float ratio = constrain(1.0f - (float)raw / base, 0.0f, 1.0f);
            _spr.setTextColor(touched ? C_GREEN : C_GRAY, C_BG);
            _spr.drawString(touched ? "TOCADO" : "LIBRE", 4, y); y += 14;
            _drawHBar(4, y, W - 8, 12, ratio, touched ? C_GREEN : C_DARK);
            y += 16;
        } else {
            _spr.setTextColor(C_GRAY, C_BG);
            _spr.drawString("Calibrando...", 4, y); y += 14;
        }

        _drawHints("", "", "Atras", "");
    }
    _push();
}

void SatMenu::_hMain(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)        { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_mainN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) { close(); return; }
    if (b == Btn::ENTER) {
        switch (_mainItems[_cur].target) {
            case Scr::EDIT_TRACKID: _eTitle="Track ID"; _eVal=max((int)_tmp.trackId,1); _eMin=1; _eMax=9; _goto(Scr::EDIT_TRACKID); break;
            case Scr::OTA:
                if (_cbWiFiOta) _cbWiFiOta();
                _toast("Activando OTA...", Scr::MAIN);
                break;
            case Scr::MOTOR:     _goto(Scr::MOTOR);     break;
            case Scr::TOUCH:     _goto(Scr::TOUCH);     break;
            case Scr::DIAG:      _goto(Scr::DIAG);      break;
            case Scr::REINICIAR: _confirm("Reiniciar el dispositivo?", Scr::REINICIAR); break;
            default: break;
        }
    }
}
void SatMenu::_hMotor(Btn b) {
    if (b == Btn::UP)   { if (_cur > 0) { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur < _motorN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) { _goto(Scr::MAIN); return; }
    if (b == Btn::ENTER) {
        switch (_cur) {
            case 0:
                _eTitle="PWM Maximo"; _eVal=_tmp.pwmMax; _eMin=50; _eMax=255;
                _goto(Scr::EDIT_PWMMAX);
                break;
            case 1:
                _eTitle="PWM Minimo"; _eVal=_tmp.pwmMin; _eMin=0; _eMax=200;
                _goto(Scr::EDIT_PWMMIN);
                break;
            case 2:
                // Test automático total (2026-08-23): arranca solo al entrar, sin
                // teclas salvo BACK — ver _tickMotorPos().
                _goto(Scr::MOTOR_POS);
                _testPhase      = 0;
                _toMaxMs        = -1;
                _toMinMs        = -1;
                _testPhaseStart = millis();
                Motor::setTargetForced(Motor::getADCMax());
                break;
        }
    }
}
void SatMenu::_hTouch(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)         { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_touchN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        if (_cur==0) {
            _tmp.touchEnabled=!_tmp.touchEnabled;
            _cfg.touchEnabled=_tmp.touchEnabled;
            _save(); if (_cbSaved) _cbSaved(_cfg);
            _toast(_tmp.touchEnabled?"Touch: ON":"Touch: OFF", Scr::TOUCH);
        } else {
        }
    }
}
void SatMenu::_hDiag(Btn b) {
    if (b == Btn::UP)   { if (_cur>0)        { _cur--; _dirty=true; } }
    if (b == Btn::DOWN) { if (_cur<_diagN-1) { _cur++; _dirty=true; } }
    if (b == Btn::BACK) _goto(Scr::MAIN);
    if (b == Btn::ENTER) {
        switch (_cur) {
            case 0: _dPat=0; _goto(Scr::TEST_DISPLAY); break;
            case 1: _encCnt=0; memset(_encHist,0,ENC_HIST); _goto(Scr::TEST_ENCODER); break;
            case 2: _motPWM=0;
                    _fadHistIdx=0; memset(_fadHist,0,FAD_HIST);
                    _goto(Scr::TEST_FADER); break;
            case 3: memset(_neoPressed,0,sizeof(_neoPressed));
                    _goto(Scr::TEST_NEOPIXEL); break;
            case 4: _fadHistIdx=0; memset(_fadHist,0,FAD_HIST); _fadT=0;
                    _goto(Scr::TEST_TOUCH); break;
        }
    }
}
void SatMenu::_hEditVal(Btn b) {
    if (b == Btn::UP)   { if (_eVal<_eMax) { _eVal++; _dirty=true; } }
    if (b == Btn::DOWN) { if (_eVal>_eMin) { _eVal--; _dirty=true; } }
    if (b == Btn::BACK) _back();
    if (b == Btn::ENTER) {
        switch (_scr) {
            case Scr::EDIT_TRACKID:  _cfg.trackId=_tmp.trackId=(uint8_t)_eVal; break;
            case Scr::EDIT_PWMMIN:   _cfg.pwmMin =_tmp.pwmMin =(uint8_t)_eVal; break;
            case Scr::EDIT_PWMMAX:   _cfg.pwmMax =_tmp.pwmMax =(uint8_t)_eVal; break;
            default: break;
        }
        _save(); if (_cbSaved) _cbSaved(_cfg);
        _toast("Guardado!", _prev);
    }
}
void SatMenu::_hConfirm(Btn b) {
    if (b == Btn::ENTER) {
        if (_confYes==Scr::REINICIAR) {
            _toast("Reiniciando...",Scr::MAIN); delay(900);
            if (_cbReboot) _cbReboot(); else ESP.restart();
        } else _goto(_confYes);
    }
    if (b == Btn::BACK) _back();
}
void SatMenu::_hToast(Btn b) {
    if (millis()-_toastT>1300||b!=Btn::NONE) _goto(_toastRet);
}

// ─────────────────────────────────────────────────────────────
//  Primitivas de dibujo
// ─────────────────────────────────────────────────────────────
void SatMenu::_drawHdr(const char* t) {
    int W=_spr.width();
    _spr.fillRect(0,0,W,SAT_HDR_H,C_BG);
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(t,W/2,SAT_HDR_H/2);
    _spr.fillRect(0,SAT_HDR_H-2,W,2,C_ACCENT);
}
void SatMenu::_drawHints(const char* u,const char* d,const char* b,const char* e) {
    int W=_spr.width(),H=_spr.height(),y=H-SAT_HINT_H;
    _spr.fillRect(0,y,W,SAT_HINT_H,C_BG);
    _spr.drawFastHLine(0,y,W,C_DARK);
    _spr.setTextSize(1); _spr.setTextColor(C_GRAY,C_BG);
    const char* lbs[4]={u,d,b,e};
    const char* bns[4]={"REC","SOL","MUT","SEL"};
    int w4=W/4;
    for (int i=0;i<4;i++) {
        if (lbs[i]&&lbs[i][0]) {
            char buf[16]; snprintf(buf,16,"%s:%s",bns[i],lbs[i]);
            _spr.setTextDatum(textdatum_t::middle_center);
            _spr.drawString(buf,w4*i+w4/2,y+SAT_HINT_H/2);
        }
    }
}
void SatMenu::_drawList(const Item* items,int n) {
    int W=_spr.width(),H=_spr.height();
    int aH=H-SAT_HDR_H-SAT_HINT_H;
    int vis=aH/SAT_ROW_H; if (vis>n) vis=n;
    if (_cur<_scrl) _scrl=_cur;
    if (_cur>=_scrl+vis) _scrl=_cur-vis+1;
    for (int i=0;i<vis;i++) {
        int idx=i+_scrl; if (idx>=n) break;
        int y=SAT_HDR_H+i*SAT_ROW_H;
        bool sel=(idx==_cur);
        uint16_t bg=sel?C_ACCENT:C_BG;
        _spr.fillRect(0,y,W,SAT_ROW_H,bg);
        _drawBadge(6,y+(SAT_ROW_H-SAT_BADGE_H)/2,items[idx].badge,
            sel?C_WHITE:C_ACCENT, sel?C_ACCENT:C_WHITE);
        _spr.setTextColor(sel?C_WHITE:C_TEXT,bg);
        _spr.setTextSize(1); _spr.setTextDatum(textdatum_t::middle_left);
        _spr.drawString(items[idx].label,SAT_BADGE_W+14,y+SAT_ROW_H/2);
        _spr.setTextDatum(textdatum_t::middle_right);
        _spr.drawString(">",W-8,y+SAT_ROW_H/2);
        if (!sel) _drawDivider(y+SAT_ROW_H-1);
    }
    if (n>vis) {
        int bH=(aH*vis)/n, bY=SAT_HDR_H+(aH*_scrl)/n;
        _spr.fillRect(W-4,SAT_HDR_H,4,aH,C_DARK);
        _spr.fillRect(W-4,bY,4,bH,C_ACCENT);
    }
}
void SatMenu::_drawValEdit(const char* t,int v,int mn,int mx,const char* u) {
    int W=_spr.width(),H=_spr.height();
    _drawHdr(t);
    int cx=W/2,cy=H/2;
    _spr.setTextColor(C_ACCENT,C_BG); _spr.setTextSize(3);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("^",cx,cy-46); _spr.drawString("v",cx,cy+46);
    char buf[12]; snprintf(buf,12,"%d%s",v,u);
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(4);
    _spr.drawString(buf,cx,cy);
    char rb[16]; snprintf(rb,16,"[%d - %d]",mn,mx);
    _spr.setTextSize(1); _spr.setTextColor(C_GRAY,C_BG);
    _spr.drawString(rb,cx,cy+28);
    _drawHints("","","Cancel","Guardar");
}
void SatMenu::_drawConfirm(const char* msg) {
    int W=_spr.width(),H=_spr.height();
    _drawHdr("Confirmar");
    int cx=W/2,cy=H/2;
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(msg,cx,cy-18);
    _spr.fillRoundRect(cx-68,cy+8,54,28,6,C_ACCENT);
    _spr.fillRoundRect(cx+14,cy+8,54,28,6,C_DARK);
    _spr.setTextColor(C_WHITE,C_ACCENT); _spr.drawString("SI (SEL)",cx-41,cy+22);
    _spr.setTextColor(C_TEXT, C_DARK);   _spr.drawString("NO (MUT)",cx+41,cy+22);
}
void SatMenu::_drawToast(const char* msg) {
    int W=_spr.width(),H=_spr.height();
    int tw=W-40,th=40,tx=20,ty=H/2-20;
    _spr.fillRoundRect(tx,ty,tw,th,8,C_BG);
    _spr.drawRoundRect(tx,ty,tw,th,8,C_ACCENT);
    _spr.setTextColor(C_TEXT,C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(msg,W/2,ty+th/2);
}
void SatMenu::_drawBadge(int x,int y,const char* t,uint16_t bg,uint16_t fg) {
    _spr.fillRoundRect(x,y,SAT_BADGE_W,SAT_BADGE_H,3,bg);
    _spr.setTextColor(fg,bg); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString(t,x+SAT_BADGE_W/2,y+SAT_BADGE_H/2);
}
void SatMenu::_drawHBar(int x,int y,int w,int h,float pct,uint16_t col) {
    _spr.fillRect(x,y,w,h,C_DARK);
    int fw=(int)(w*constrain(pct,0.0f,1.0f));
    if (fw>0) _spr.fillRect(x,y,fw,h,col);
    _spr.drawRect(x,y,w,h,C_GRAY);
}
void SatMenu::_drawDivider(int y) {
    _spr.drawFastHLine(0,y,_spr.width(),C_GRAY);
}

// Motor controlado SOLO por Motor.cpp — estas funciones ya no existen (2026-05-10 20:30)

// ─────────────────────────────────────────────────────────────
//  NVS
// ─────────────────────────────────────────────────────────────
void SatMenu::_load() {
    _prefs.begin("ptxx",true);
    _cfg.trackId        = _prefs.getUChar("trackId", 0);
    _cfg.pwmMin         = _prefs.getUChar("pwmMin", PWM_MIN);   // Fallback a config.h (2026-05-10 20:10)
    _cfg.pwmMax         = _prefs.getUChar("pwmMax", PWM_MAX);   // Fallback a config.h (2026-05-10 20:10)
    _cfg.touchEnabled   = _prefs.getBool ("touchEn", true);
    _cfg.motorDisabled  = _prefs.getBool("motorDis", false);
    _prefs.end();
}
void SatMenu::_save() {
    _prefs.begin("ptxx",false);
    _prefs.putUChar("trackId", _cfg.trackId);
    _prefs.putUChar("pwmMin",  _cfg.pwmMin);
    _prefs.putUChar("pwmMax",  _cfg.pwmMax);
    _prefs.putBool ("touchEn", _cfg.touchEnabled);
    _prefs.putBool("motorDis", _cfg.motorDisabled);
    _prefs.end();

    // Actualizar Motor con nuevos valores de NVS sin reiniciar (2026-05-10 20:45)
    if (_cfg.pwmMin > 0 && _cfg.pwmMax > 0) {
        Motor::initPWM();
    }
}

// ─────────────────────────────────────────────────────────────
//  Toast / Confirm
// ─────────────────────────────────────────────────────────────
void SatMenu::_toast(const char* msg,Scr ret) {
    _toastMsg=msg; _toastRet=ret; _toastT=millis();
    _scr=Scr::TOAST; _dirty=true;
}
void SatMenu::_confirm(const char* msg,Scr yes) {
    _confMsg=msg; _confYes=yes; _prev=_scr;
    _scr=Scr::CONFIRM; _dirty=true;
}

// ─────────────────────────────────────────────────────────────
//  TEST NEOPIXEL + BOTONES
// ─────────────────────────────────────────────────────────────
void SatMenu::_tickTestNeopixel(Btn b) {
    int W = _spr.width(), H = _spr.height();

    bool p[4] = {
        digitalRead(BUTTON_PIN_REC)    == LOW,
        digitalRead(BUTTON_PIN_SOLO)   == LOW,
        digitalRead(BUTTON_PIN_MUTE)   == LOW,
        digitalRead(BUTTON_PIN_SELECT) == LOW
    };

    if (p[2]) {
        if (_neoMuteHoldT == 0) _neoMuteHoldT = millis();
        if (millis() - _neoMuteHoldT >= 1000) {
            if (_cbLedsTest) for (int i = 0; i < 4; i++) _cbLedsTest(i, 0, 0, 0);
            _neoMuteHoldT = 0;
            _goto(Scr::DIAG);
            return;
        }
    } else {
        _neoMuteHoldT = 0;
    }

    static const uint8_t COLORS[4][3] = {
        {255,   0,   0},
        {255, 200,   0},
        {  0, 255,   0},
        {200, 200, 200},
    };
    static const char* NAMES[4] = {"REC", "SOLO", "MUTE", "SEL"};

    if (_cbLedsTest) {
        for (int i = 0; i < 4; i++) {
            if (p[i]) _cbLedsTest(i, COLORS[i][0], COLORS[i][1], COLORS[i][2]);
            else      _cbLedsTest(i, 0, 0, 0);
        }
    }

    _spr.fillScreen(C_BG);
    _drawHdr("TEST BOTONES + LEDS");

    int y = SAT_HDR_H + 10;
    for (int i = 0; i < 4; i++) {
        uint16_t col = p[i] ?
            _spr.color565(COLORS[i][0], COLORS[i][1], COLORS[i][2]) : C_DARK;
        int bx = 10 + i * 56, bw = 50, bh = 50;
        _spr.fillRoundRect(bx, y, bw, bh, 6, col);
        _spr.setTextColor(p[i] ? C_BLACK : C_GRAY, col);
        _spr.setTextSize(1);
        _spr.setTextDatum(textdatum_t::middle_center);
        _spr.drawString(NAMES[i], bx + bw/2, y + bh/2);
    }

    y += 70;
    _spr.setTextColor(C_TEXT, C_BG);
    _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("Pulsa cada boton", W/2, y); y += 18;
    _spr.setTextColor(C_GRAY, C_BG);
    _spr.drawString("Se enciende su LED", W/2, y);

    _spr.fillRect(0, H - SAT_HINT_H, W, SAT_HINT_H, C_BG);
    _spr.drawFastHLine(0, H - SAT_HINT_H, W, C_DARK);
    _spr.setTextColor(C_GRAY, C_BG);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.drawString("MUT solo = salir", W/2, H - SAT_HINT_H/2);

    _push();
}

void SatMenu::showStatus(const char* msg) {
    if (!_open || !_spr.width()) return;
    _spr.fillRect(0, 120, _spr.width(), 40, C_BG);
    _spr.setFont(&fonts::FreeSans9pt7b);
    _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::middle_center);
    _spr.setTextColor(C_TEXT, C_BG);   // ← añadir antes de drawString
    _spr.drawString(msg, _spr.width()/2, 140);
    _push();
}


// ─────────────────────────────────────────────────────────────
//  MOTOR POSICIÓN
// ─────────────────────────────────────────────────────────────
// MOTOR POS — sweep MIN/MAX calibrado (NVS) + cronómetro de tránsito (2026-08-23)
// WHAT: envía el motor al mínimo o máximo ya calibrados (Motor::getADCMin/Max,
//       origen NVS) y mide cuánto tarda en llegar, para diagnosticar si el PWM
//       mínimo actual es insuficiente en algún tramo del recorrido real.
// WHY:  usa API pública de Motor (setTargetForced/getState/getRawADC). Llama
//       Motor::update() explícitamente (2026-08-23) — main.cpp NO lo hace
//       mientras el SAT está abierto (return temprano en loop()), así que sin
//       esto el target armado por setTargetForced() nunca movía el PWM real.
//       Seguro aquí (a diferencia del extinto Test Mode) porque este test solo
//       usa la máquina de estados normal, no control manual de bajo nivel.
void SatMenu::_tickMotorPos(Btn b) {
    int W = _spr.width(), H = _spr.height();

    if (b == Btn::BACK) { _goto(Scr::MOTOR); return; }

    // main.cpp NO llama Motor::update() mientras el SAT está abierto (return
    // temprano en loop()) — faderADC.update()/Motor::setADC() sí siguen corriendo
    // ahí (ver comentario "SIEMPRE, incluso en SAT" en main.cpp), pero sin esta
    // llamada el target armado por setTargetForced() nunca se traduce en PWM real.
    // Único caso hoy que necesita el motor realmente en movimiento dentro del SAT.
    Motor::update();

    uint16_t adcMin = Motor::getADCMin();
    uint16_t adcMax = Motor::getADCMax();
    uint16_t pos    = Motor::getRawADC();
    uint16_t span   = (adcMax > adcMin) ? (adcMax - adcMin) : 1;
    float    pct    = constrain((float)(pos - adcMin) / span, 0.f, 1.f);

    Motor::MotorState st       = Motor::getState();
    bool               atTarget = (st == Motor::MotorState::AT_TARGET);
    bool               timedOut = (millis() - _testPhaseStart > SAT_MOTOR_SWEEP_TIMEOUT_MS);

    // Máquina de 3 fases, 100% automática — solo BACK interrumpe (2026-08-23).
    if (_testPhase == 0) {              // yendo a MAX
        Motor::setTargetForced(adcMax);
        if (atTarget || timedOut) {
            _toMaxMs        = timedOut ? -1 : (long)(millis() - _testPhaseStart);
            _testPhase      = 1;
            _testPhaseStart = millis();
            Motor::setTargetForced(adcMin);
        }
    } else if (_testPhase == 1) {       // yendo a MIN
        Motor::setTargetForced(adcMin);
        if (atTarget || timedOut) {
            _toMinMs   = timedOut ? -1 : (long)(millis() - _testPhaseStart);
            _testPhase = 2;              // terminado
        }
    }

    _spr.fillScreen(C_BG);
    _drawHdr("TIEMPO MIN/MAX");

    int y = SAT_HDR_H + 8;
    char buf[48];

    _spr.setTextColor(C_CYAN, C_BG); _spr.setTextSize(1);
    _spr.setTextDatum(textdatum_t::top_left);
    snprintf(buf, 48, "Min=%u Max=%u  pos=%u", adcMin, adcMax, pos);
    _spr.drawString(buf, 4, y); y+=14;
    _drawHBar(4, y, W-8, 12, pct, C_GREEN); y+=18;

    _spr.setTextColor(C_YELLOW, C_BG);
    if (_testPhase == 0) {
        snprintf(buf, 48, "-> MAX: en curso %lu ms", millis() - _testPhaseStart);
        _spr.drawString(buf, 4, y); y+=16;
    } else {
        if (_toMaxMs >= 0) snprintf(buf, 48, "-> MAX: %ld ms", _toMaxMs);
        else               snprintf(buf, 48, "-> MAX: TIMEOUT");
        _spr.drawString(buf, 4, y); y+=16;
    }
    if (_testPhase == 1) {
        snprintf(buf, 48, "MAX -> MIN: en curso %lu ms", millis() - _testPhaseStart);
        _spr.setTextColor(C_YELLOW, C_BG);
        _spr.drawString(buf, 4, y); y+=16;
    } else if (_testPhase == 2) {
        if (_toMinMs >= 0) snprintf(buf, 48, "MAX -> MIN: %ld ms", _toMinMs);
        else               snprintf(buf, 48, "MAX -> MIN: TIMEOUT");
        _spr.setTextColor(C_YELLOW, C_BG);
        _spr.drawString(buf, 4, y); y+=16;
    }
    if (_testPhase == 2) {
        _spr.setTextColor(C_GREEN, C_BG);
        _spr.drawString("TEST COMPLETO", 4, y); y+=16;
    }

    if (!Motor::isCalibrated()) {
        _spr.setTextColor(C_ACCENT, C_BG);
        _spr.drawString("!! SIN CALIBRAR !!", 4, y);
    }

    _drawHints("","","Atras","");
    _push();
}
