// src/midi/MIDIProcessor.h
#pragma once
#include <Arduino.h>

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
String formatBeatString();
String formatTimecodeString();

extern uint8_t g_channelAutoMode[8];
