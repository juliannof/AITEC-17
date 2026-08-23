// src/midi/MIDIProcessor.h
#pragma once
#include <Arduino.h>
#include "../config.h"

extern volatile ConnectionState logicConnectionState;
extern uint8_t  g_logicConnected;
extern volatile bool g_switchToOffline;
extern volatile bool g_switchToPage3;
extern DisplayMode currentTimecodeMode;
extern char timeCodeChars_clean[13];
extern char beatsChars_clean[13];
extern bool recStates[9];
extern bool soloStates[9];
extern bool muteStates[9];
extern bool selectStates[9];
extern float vuLevels[9];
extern float vuPeakLevels[9];
extern bool  vuClipState[9];
extern unsigned long vuLastUpdateTime[9];
extern unsigned long vuPeakLastUpdateTime[9];
extern float faderPositions[9];
extern bool btnStatePG1[32];
extern bool btnStatePG2[32];
extern bool btnFlashPG1[32];
extern bool btnFlashPG2[32];
extern String trackNames[8];
extern String assignmentString;
extern uint8_t vpotValues[8];
extern bool needsTOTALRedraw;
extern bool needsMainAreaRedraw;
extern bool needsHeaderRedraw;
extern bool needsVUMetersRedraw;
extern bool needsButtonsRedraw;
extern bool needsTimecodeRedraw;

bool isLogicConnected();
void sendMIDIBytes(const byte* data, size_t len);
void processMidiByte(byte b);
void processMackieSysEx(byte* payload, int len);
void processNote(byte status, byte note, byte velocity);
void handleMcuHandshake(byte* challenge_code);
void processChannelPressure(byte channel, byte value);
void processControlChange(byte channel, byte controller, byte value);
void processPitchBend(byte channel, int bendValue);
void checkUsbLink();       // detecta desconexión física real vía tud_mounted() (2026-08-18)
void checkLogicCloseSignature();  // detecta cierre "silencioso" de Logic sin 0x0F ni faders-a-0 (2026-08-23)
void tickCalibracion();    // ← AÑADIR
void tickTrackNameDebounce();  // debounce anti-flash de nombres de pista (2026-08-13 15:10)
String formatBeatString();
String formatTimecodeString();

extern uint8_t g_channelAutoMode[8];
