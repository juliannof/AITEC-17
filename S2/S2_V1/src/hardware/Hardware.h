#pragma once
#include <Arduino.h>
#include <Button2.h>

enum class ButtonId {
    REC = 0,
    SOLO,
    MUTE,
    SELECT,
    ENCODER_SELECT,
    FADER_TOUCH,
    UNKNOWN
};

typedef void (*ButtonPressCallback)();
typedef void (*ButtonEventCallback)(ButtonId buttonId);

extern Button2 buttonRec;
extern Button2 buttonSolo;
extern Button2 buttonMute;
extern Button2 buttonSelect;
extern Button2 buttonEncoderSelect;

void initHardware();
void updateButtons();
void registerButtonPressCallback(ButtonId id, ButtonPressCallback callback);
void registerButtonEventCallback(ButtonEventCallback callback);
void setLedBuiltin(bool state);
void toggleLedBuiltin();