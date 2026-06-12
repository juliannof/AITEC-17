// src/midi/MIDIProcessor.cpp
#include "MIDIProcessor.h"
#include "../config.h"
#include <USBMIDI.h>
#include "../RS485/RS485.h"

extern USBMIDI MIDI;
extern void updateLeds();
extern uint8_t g_logicConnected;

namespace {
    byte midi_buffer[512];
    int midi_idx = 0;
    bool in_sysex = false;
    byte last_status_byte = 0;

    static uint16_t fadersAtMinMask = 0;
    static unsigned long firstFaderMinTime = 0;
    static const uint16_t ALL_FADERS_MIN_MASK = 0x01FF;
    static unsigned long lastMidiActivityTime = 0;
    
    static const unsigned long MIDI_TIMEOUT_MS = 0;
    static const int DISCONNECT_THRESHOLD = 9;
    static const unsigned long DISCONNECT_WINDOW_MS = 150;
    static const int16_t PITCHBEND_DEADBAND = 150;
    static int16_t lastSentPitchBend[9] = {INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN};

    static int8_t  g_selectedChannel    = -1;
    static unsigned long connectedSinceTime  = 0;
    static const unsigned long CONNECT_GRACE_MS = 1500;
    static uint8_t  _calibPendingFrom = 0;
    static uint32_t _calibNextTime    = 0;
}

void processMidiByte(byte b);
void processMackieSysEx(byte* payload, int len);
void processNote(byte status, byte note, byte velocity);
void processChannelPressure(byte channel, byte value);
void processControlChange(byte channel, byte controller, byte value);
void processPitchBend(byte channel, int bendValue);
void processMackieFader(byte channel, int value);

float masterMeterLevel = 0.0f;
float masterPeakLevel = 0.0f;
bool masterClip = false;
unsigned long masterMeterDecayTimer = 0;

String vpotAssignNames[8];

uint8_t g_channelAutoMode[8] = {};

void tickCalibracion() {
    if (_calibPendingFrom == 0) return;
    if (millis() < _calibNextTime) return;
    rs485.setCalibrate(_calibPendingFrom);
    log_i("[CALIB] Slave %d disparado", _calibPendingFrom);
    _calibPendingFrom++;
    if (_calibPendingFrom > NUM_SLAVES) {
        _calibPendingFrom = 0;
    } else {
        _calibNextTime = millis() + 4000;
    }
}

void sendMIDIBytes(const byte* data, size_t len) {
    log_v("[MIDI OUT] Enviando %d bytes", len);

    if (data[0] == 0xF0) {
        size_t i = 0;
        while (i < len) {
            midiEventPacket_t packet;
            size_t remaining = len - i;
            if (remaining >= 3 && (i + 3) < len) {
                packet.header = 0x04;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = data[i+2];
                i += 3;
            } else if (remaining == 1) {
                packet.header = 0x05;
                packet.byte1  = data[i];
                packet.byte2  = 0x00;
                packet.byte3  = 0x00;
                i += 1;
            } else if (remaining == 2) {
                packet.header = 0x06;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = 0x00;
                i += 2;
            } else {
                packet.header = 0x07;
                packet.byte1  = data[i];
                packet.byte2  = data[i+1];
                packet.byte3  = data[i+2];
                i += 3;
            }
            MIDI.writePacket(&packet);
        }
        return;
    }

    if (len == 3) {
        byte status  = data[0] & 0xF0;
        byte channel = (data[0] & 0x0F) + 1;
        byte byte1   = data[1];
        byte byte2   = data[2];
        switch (status) {
            case 0x90:
                if (byte2 > 0) MIDI.noteOn(byte1, byte2, channel);
                else           MIDI.noteOff(byte1, 0, channel);
                break;
            case 0x80:
                MIDI.noteOff(byte1, byte2, channel);
                break;
            case 0xB0:
                MIDI.controlChange(byte1, byte2, channel);
                break;
            case 0xE0: {
                midiEventPacket_t packet;
                packet.header = 0x0E | ((data[0] & 0x0F) << 4);
                packet.byte1  = data[0];
                packet.byte2  = data[1];
                packet.byte3  = data[2];
                MIDI.writePacket(&packet);
                break;
            }
            default:
                log_d("[MIDI OUT] Mensaje no soportado: 0x%02X", status);
                break;
        }
    }
}

void processMidiByte(byte b) {
    if (b >= 0xF8) return;

    if (b & 0x80) {
        if (b == 0xF0) {
            in_sysex = true;
            midi_idx = 0;
            return;
        }
        if (b == 0xF7) {
            if (in_sysex) { in_sysex = false; processMackieSysEx(midi_buffer, midi_idx); }
            return;
        }
        if (in_sysex) {
            in_sysex = false;
        }
        last_status_byte = b;
        midi_idx = 0;
        return;
    }

    if (in_sysex) {
        if (midi_idx < (int)sizeof(midi_buffer)) midi_buffer[midi_idx++] = b;
        return;
    }

    if (last_status_byte != 0) {
        if (midi_idx >= 250) midi_idx = 0;
        midi_buffer[midi_idx++] = b;

        byte cmd_type = last_status_byte & 0xF0;
        int msg_len_expected = 0;
        switch (cmd_type) {
            case 0xC0: case 0xD0: msg_len_expected = 1; break;
            case 0xF0: msg_len_expected = 0; break;
            default:   msg_len_expected = 2; break;
        }

        if (midi_idx == msg_len_expected) {
            byte data1 = midi_buffer[0];
            byte data2 = (msg_len_expected > 1) ? midi_buffer[1] : 0;
            switch (cmd_type) {
                case 0x90: case 0x80: processNote(last_status_byte, data1, data2); break;
                case 0xD0: processChannelPressure(last_status_byte & 0x0F, data1); break;
                case 0xB0: processControlChange(last_status_byte & 0x0F, data1, data2); break;
                case 0xE0: {
                    int bendValue = (data2 << 7) | data1;
                    processPitchBend(last_status_byte & 0x0F, bendValue);
                    break;
                }
                default: break;
            }
            midi_idx = 0;
        }
    }
}


void processControlChange(byte channel, byte controller, byte value) {
    log_i("CC CH=%d, CC=%d, Val=0x%02X", channel, controller, value);
    if (channel != 0 && channel != 15) return;

    if (controller >= 48 && controller <= 55) {
        uint8_t strip = controller - 48;
        rs485.setVPotValue(strip + 1, value);
        vpotValues[strip + P4_CH_OFFSET] = value;
        needsButtonsRedraw = true;
        log_i("[VPot] CC%d strip=%u raw=0x%02X pos=%u",
              controller, strip, value, value & 0x0F);
        return;
    }

    if (controller < 64 || controller > 73) return;

    int digit_index  = 73 - controller;
    byte char_code   = value & 0x3F;
    char ascii_char  = (char_code < 64) ? MACKIE_CHAR_MAP[char_code] : '?';
    byte char_to_store = (byte)ascii_char;
    if (value & 0x40) char_to_store |= 0x80;

    beatsChars_clean[digit_index]    = char_to_store;
    timeCodeChars_clean[digit_index] = char_to_store;

    needsHeaderRedraw   = true;
    needsTimecodeRedraw = true;
}

String formatTimecodeString() {
    char formatted[14];
    int pos = 0;
    for (int i = 0; i < 10; i++) {
        byte b = timeCodeChars_clean[i];
        char c = b & 0x7F;
        if (c == 0 || c < 32) c = ':';
        if (c == ';') c = ':';
        formatted[pos++] = c;
        if (b & 0x80) formatted[pos++] = ':';
    }
    formatted[pos] = '\0';
    String result = String(formatted);
    result.trim();
    if (result.length() == 0) return "--:--:--:--";
    // Si hay más de 4 grupos (4+ colones), quitar el primer grupo
    int colonCount = 0;
    for (int j = 0; j < (int)result.length(); j++) if (result[j] == ':') colonCount++;
    if (colonCount >= 4) {
        int firstColon = result.indexOf(':');
        if (firstColon >= 0) result = result.substring(firstColon + 1);
    }
    return result;
}

String formatBeatString() {
    // Mackie MCU BEATS: posiciones fijas en buffer de 10 chars
    // [0-3]=barras(4) [4]=beat(1) [5]=subdiv(1) [6-8]=ticks(3)
    // Resultado: 12 chars "0000.0.0.000", right-align por bloque

    bool anyDigit = false;
    for (int i = 0; i < 10; i++) {
        char c = beatsChars_clean[i] & 0x7F;
        if (c >= '0' && c <= '9') { anyDigit = true; break; }
    }
    if (!anyDigit) return "   1. 1. 1.  1";

    const int starts[4] = {0, 6, 4, 7};
    const int counts[4] = {4, 1, 1, 3};
    const int widths[4] = {4, 1, 1, 3};

    char result[16] = {};
    int  pos        = 0;

    for (int b = 0; b < 4; b++) {
        char tmp[8] = {};
        int  len    = 0;
        for (int i = starts[b]; i < starts[b] + counts[b]; i++) {
            char c = (char)(beatsChars_clean[i] & 0x7F);
            if (c < 32) c = ' ';
            tmp[len++] = c;
        }
        char* src = tmp;
        while (len > 0 && *src == ' ') { src++; len--; }
        for (int p = 0; p < widths[b] - len; p++) result[pos++] = ' ';
        for (int p = 0; p < len;             p++) result[pos++] = src[p];
        if (b < 3) result[pos++] = '.';
    }
    result[pos] = '\0';
    return String(result);
}

void processChannelPressure(byte channel, byte value) {
    log_v(">> CP IN: Ch=%d, Val=%d", channel, value);

    float normalizedLevel = 0.0f;
    int targetChannel = -1;
    bool newClipState = false;
    bool clearClip = false;
    uint8_t vuLevel7bit = 0;

    if (channel == 0) {
        targetChannel = (value >> 4) & 0x0F;
        byte mcu_level = value & 0x0F;
        if (targetChannel >= 8) return;
        switch (mcu_level) {
            case 0x0F: clearClip = true; normalizedLevel = vuLevels[targetChannel]; break;
            case 0x0E: newClipState = true; normalizedLevel = 1.0f; vuLevel7bit = 127; break;
            case 0x0D: case 0x0C: normalizedLevel = 1.0f; vuLevel7bit = 120; break;
            default:
                normalizedLevel = (mcu_level <= 11) ? (float)mcu_level / 11.0f : 0.0f;
                vuLevel7bit = (uint8_t)(normalizedLevel * 127.0f);
                break;
        }
        rs485.setVuLevel(targetChannel + 1, vuLevel7bit);
    } else if (channel >= 1 && channel <= 7) {
        targetChannel = channel;
        normalizedLevel = (float)value / 127.0f;
        if (value >= 127) newClipState = true;
        rs485.setVuLevel(targetChannel + 1, value);
    } else {
        return;
    }

    if (targetChannel != -1) {
        int dispCh = targetChannel + P4_CH_OFFSET;
        bool stateChanged = false;
        if (clearClip) {
            if (vuClipState[dispCh]) { vuClipState[dispCh] = false; stateChanged = true; }
        } else {
            if (normalizedLevel > 0.0f) vuLastUpdateTime[dispCh] = millis();
            if (newClipState) {
                if (!vuClipState[dispCh]) { vuClipState[dispCh] = true; stateChanged = true; }
            }
            if (normalizedLevel != vuLevels[dispCh]) {
                vuLevels[dispCh] = normalizedLevel;
                stateChanged = true;
            }
            if (normalizedLevel > vuPeakLevels[dispCh]) {
                vuPeakLevels[dispCh] = normalizedLevel;
                vuPeakLastUpdateTime[dispCh] = millis();
                stateChanged = true;
            }
        }
        if (stateChanged) needsVUMetersRedraw = true;
    }
}

void processMackieSysEx(byte* payload, int len) {
    if (len < 5) return;

    byte device_family = payload[3];
    byte command = payload[4];

    log_v("[SYSEX] family=0x%02X cmd=0x%02X len=%d", device_family, command, len);

    // Fase 0: sondeo — responder a cmd 0x00 y 0x13 en cualquier familia
    if (command == 0x00) {
        byte reply[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x01,
                        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xF7};
        sendMIDIBytes(reply, sizeof(reply));
        return;
    }
    if (command == 0x13) {
        byte reply[] = {0xF0, 0x00, 0x00, 0x66, 0x14, 0x14, 0x00, 0xF7};
        sendMIDIBytes(reply, sizeof(reply));
        return;
    }

    // Fase 1+: solo familia 0x14
    if (device_family != 0x14) return;

    switch (command) {

        case 0x0F: {
            logicConnectionState = ConnectionState::DISCONNECTED;
            g_logicConnected     = 0;
            fadersAtMinMask      = 0;
            firstFaderMinTime    = 0;
            g_switchToOffline    = true;
            memset(recStates,    0, sizeof(recStates));
            memset(soloStates,   0, sizeof(soloStates));
            memset(muteStates,   0, sizeof(muteStates));
            memset(selectStates, 0, sizeof(selectStates));
            memset(vuLevels,     0, sizeof(vuLevels));
            memset(vuClipState,  0, sizeof(vuClipState));
            memset(vuPeakLevels, 0, sizeof(vuPeakLevels));
            memset(vuPeakFadeTime, 0, sizeof(vuPeakFadeTime));
            memset(vuPeakAlpha, 255, sizeof(vuPeakAlpha));
            memset(faderPositions, 0, sizeof(faderPositions));
            memset(btnStatePG1,  0, sizeof(bool) * BTN_PG1_COUNT);
            memset(btnFlashPG1,  0, sizeof(bool) * BTN_PG1_COUNT);
            memset(g_channelAutoMode, 0, sizeof(g_channelAutoMode));
            g_selectedChannel = -1;
            rudeSoloActive = false;
            cycleActive    = false;
            g_clickActive  = false;
            for (int i = 0; i < 8; i++) trackNames[P4_CH_OFFSET + i] = "";
            _calibPendingFrom = 0;
            for (uint8_t i = 1; i <= NUM_SLAVES; i++) rs485.setFlags(i, 0);
            rs485.beginDisconnectSequence();
            log_i("[MCU] GoOffline recibido — iniciando DISCONNECT SEQUENCE");
            break;
        }

        case 0x21: {
            // Fase 2 CRÍTICA — echo inmediato
            byte echo[] = {0xF0, 0x00, 0x00, 0x66, DEVICE_FAMILY, 0x21, 0x01, 0xF7};
            sendMIDIBytes(echo, sizeof(echo));
            if (logicConnectionState != ConnectionState::CONNECTED) {
                logicConnectionState = ConnectionState::CONNECTED;
                g_logicConnected     = 1;
                connectedSinceTime   = millis();
                needsTOTALRedraw     = true;
                fadersAtMinMask      = 0;
                for (uint8_t i = 0; i < 8; i++) {
                    if (selectStates[P4_CH_OFFSET + i]) {
                        byte offMsg[3] = { 0x80, (uint8_t)(24 + i), 0x00 };
                        sendMIDIBytes(offMsg, 3);
                        selectStates[P4_CH_OFFSET + i] = false;
                    }
                }
                _calibPendingFrom = 1;
                _calibNextTime    = millis();
                g_switchToPage3   = true;
                log_i("[MCU] 0x21 — CONNECTED");
            }
            break;
        }

        case 0x0C: {
            // Surface Type + suscripción a feedback
            byte echo[] = {0xF0, 0x00, 0x00, 0x66, DEVICE_FAMILY, 0x0C, 0x00, 0xF7};
            sendMIDIBytes(echo, sizeof(echo));
            byte sub[]  = {0xF0, 0x00, 0x00, 0x66, DEVICE_FAMILY, 0x10, 0x00, 0xF7};
            sendMIDIBytes(sub, sizeof(sub));
            log_v("[MCU] 0x0C echo + 0x10 suscripcion feedback");
            break;
        }

        case 0x20:
        case 0x0A: case 0x0B: {
            byte echo[32];
            int  elen = len + 2;
            if (elen <= (int)sizeof(echo)) {
                echo[0] = 0xF0;
                memcpy(echo + 1, payload, len);
                echo[len + 1] = 0xF7;
                sendMIDIBytes(echo, elen);
            }
            break;
        }

        case 0x12: {
            if (len < 6) break;
            byte startOffset = payload[5];
            int  text_len    = len - 6;
            if (text_len <= 0) break;

            auto trimRight = [](char* s) {
                for (int j = 6; j >= 0; j--) {
                    if (s[j] == ' ' || s[j] == '\0') s[j] = '\0';
                    else break;
                }
            };

            char nameBufs[8][8] = {};
            bool nameChanged[8] = {};
            char vpotBufs[8][8] = {};
            bool vpotChanged[8] = {};

            for (int t = 0; t < 8; t++) {
                strncpy(nameBufs[t], trackNames[P4_CH_OFFSET + t].c_str(), 7);
                nameBufs[t][7] = '\0';
                strncpy(vpotBufs[t], vpotAssignNames[t].c_str(), 7);
                vpotBufs[t][7] = '\0';
            }

            for (int i = 0; i < text_len; i++) {
                byte offset = startOffset + i;
                if (offset < 56) {
                    nameBufs[offset / 7][offset % 7] = (char)payload[6 + i];
                    nameChanged[offset / 7] = true;
                } else if (offset < 112) {
                    int s = (offset - 56) / 7;
                    int p = (offset - 56) % 7;
                    vpotBufs[s][p] = (char)payload[6 + i];
                    vpotChanged[s] = true;
                }
            }

            for (int t = 0; t < 8; t++) {
                if (!nameChanged[t]) continue;
                trimRight(nameBufs[t]);
                if (nameBufs[t][0] == '\0') continue;
                if (trackNames[P4_CH_OFFSET + t] == nameBufs[t]) continue;
                trackNames[P4_CH_OFFSET + t] = String(nameBufs[t]);
                needsMainAreaRedraw = true;
                needsButtonsRedraw  = true;
                rs485.setTrackName(t + 1, nameBufs[t]);
            }

            for (int t = 0; t < 8; t++) {
                if (!vpotChanged[t]) continue;
                trimRight(vpotBufs[t]);
                String newName = String(vpotBufs[t]);
                if (vpotAssignNames[t] != newName) {
                    vpotAssignNames[t] = newName;
                    needsHeaderRedraw = true;
                }
            }
            break;
        }

        case 0x11: {
            if (len < 7) break;
            byte b1 = payload[5], b2 = payload[6];
            char c1 = (b1 >= 32 && b1 <= 126) ? (char)b1 : '?';
            char c2 = (b2 >= 32 && b2 <= 126) ? (char)b2 : '?';
            char assign_buf[3] = {c1, c2, '\0'};
            if (assignmentString != assign_buf) {
                assignmentString = String(assign_buf);
                needsHeaderRedraw = true;
            }
            break;
        }

        case 0x61: {
            for (uint8_t i = 1; i <= NUM_SLAVES; i++)
                rs485.setFaderTarget(i, 0);
            log_i("[MCU] AllFaderstoMinimum — faders a 0");
            break;
        }

        case 0x72: {
            if (len < 13) break;
            lastMidiActivityTime = millis();
            for (int i = 0; i < 8; i++) {
                byte mcu_level = payload[5 + i] & 0x0F;
                int  dispCh    = i + P4_CH_OFFSET;
                bool stateChanged = false;
                bool clearClip = (mcu_level == 0x0F);
                bool newClip   = (mcu_level == 0x0E);
                if (clearClip) {
                    if (vuClipState[dispCh]) { vuClipState[dispCh] = false; stateChanged = true; }
                } else {
                    float normalized = (newClip || mcu_level >= 0x0C) ? 1.0f
                                     : (mcu_level <= 11) ? (float)mcu_level / 11.0f : 0.0f;
                    if (normalized > 0.0f) vuLastUpdateTime[dispCh] = millis();
                    if (newClip && !vuClipState[dispCh]) { vuClipState[dispCh] = true; stateChanged = true; }
                    if (normalized != vuLevels[dispCh]) { vuLevels[dispCh] = normalized; stateChanged = true; }
                    if (normalized > vuPeakLevels[dispCh]) {
                        vuPeakLevels[dispCh] = normalized;
                        vuPeakLastUpdateTime[dispCh] = millis();
                        stateChanged = true;
                    }
                    rs485.setVuLevel(i + 1, (uint8_t)(normalized * 127.0f));
                }
                if (stateChanged) needsVUMetersRedraw = true;
            }
            break;
        }

        case 0x0E: {
            if (len < 7) break;
            byte channel = payload[5];
            byte mode    = payload[6];
            if (channel < 8) {
                g_channelAutoMode[channel] = mode;
                needsButtonsRedraw = true;
            }
            break;
        }

        default:
            log_v("processMackieSysEx: Comando 0x%02X no manejado.", command);
            break;
    }
}

void processNote(byte status, byte note, byte velocity) {
    bool is_on       = ((status & 0xF0) == 0x90 && velocity > 0);
    bool is_flashing = ((status & 0xF0) == 0x90 && velocity == 1);

    if (note == 113) { if (is_on) { currentTimecodeMode = MODE_SMPTE; needsHeaderRedraw = true; needsTimecodeRedraw = true; } return; }
    if (note == 114) { if (is_on) { currentTimecodeMode = MODE_BEATS; needsHeaderRedraw = true; needsTimecodeRedraw = true; } return; }
    if (note == 0x73) { rudeSoloActive = is_on; needsTimecodeRedraw = true; return; }
    if (note == 0x56) { cycleActive    = is_on; needsTimecodeRedraw = true; return; }
    if (note == 0x59) { g_clickActive  = is_on; needsTimecodeRedraw = true; return; }

    if (note <= 31) {
        int group     = note / 8;
        int track_idx = note % 8;
        bool stateChanged = false;
        switch (group) {
            case 0: if (recStates[track_idx + P4_CH_OFFSET]    != is_on) { recStates[track_idx + P4_CH_OFFSET]    = is_on; stateChanged = true; } break;
            case 1: if (soloStates[track_idx + P4_CH_OFFSET]   != is_on) { soloStates[track_idx + P4_CH_OFFSET]   = is_on; stateChanged = true; } break;
            case 2: if (muteStates[track_idx + P4_CH_OFFSET]   != is_on) { muteStates[track_idx + P4_CH_OFFSET]   = is_on; stateChanged = true; } break;
            case 3:
                if (selectStates[track_idx + P4_CH_OFFSET] != is_on) { selectStates[track_idx + P4_CH_OFFSET] = is_on; stateChanged = true; }
                if (is_on) g_selectedChannel = track_idx;
                else if (g_selectedChannel == track_idx) g_selectedChannel = -1;
                break;
        }
        if (stateChanged) {
            needsMainAreaRedraw = true;
            needsButtonsRedraw  = true;
            uint8_t slaveId = track_idx + 1;
            uint8_t flags = 0;
            if (recStates[track_idx + P4_CH_OFFSET])    flags |= FLAG_REC;
            if (soloStates[track_idx + P4_CH_OFFSET])   flags |= FLAG_SOLO;
            if (muteStates[track_idx + P4_CH_OFFSET])   flags |= FLAG_MUTE;
            if (selectStates[track_idx + P4_CH_OFFSET]) flags |= FLAG_SELECT;
            flags = setAutoMode(flags, (AutoMode)g_channelAutoMode[track_idx]);
            rs485.setFlags(slaveId, flags);
        }
        return;
    }

    if (note >= 74 && note <= 78 && is_on && g_selectedChannel >= 0) {
        const AutoMode modeMap[] = {
            AUTO_READ, AUTO_WRITE, AUTO_TRIM, AUTO_TOUCH, AUTO_LATCH
        };
        AutoMode mode = modeMap[note - 74];
        g_channelAutoMode[g_selectedChannel] = (uint8_t)mode;
        rs485.setAutoMode(g_selectedChannel + 1, mode);
        for (int key = 0; key < BTN_PG1_COUNT; key++) {
            if (MIDI_NOTES_PG1[key] != 0x00 && MIDI_NOTES_PG1[key] == note) {
                btnStatePG1[key]  = is_on;
                btnFlashPG1[key]  = is_flashing;
            }
        }
        needsMainAreaRedraw = true;
        needsButtonsRedraw  = true;
        return;
    }

    bool stateChanged = false;
    for (int key = 0; key < BTN_PG1_COUNT; key++) {
        if (MIDI_NOTES_PG1[key] != 0x00 && MIDI_NOTES_PG1[key] == note) {
            if (btnStatePG1[key] != is_on || btnFlashPG1[key] != is_flashing) {
                btnStatePG1[key]  = is_on;
                btnFlashPG1[key]  = is_flashing;
                stateChanged = true;
            }
        }
    }
    if (stateChanged) {
        needsMainAreaRedraw = true;
        needsButtonsRedraw  = true;
    }
}

void processPitchBend(byte channel, int bendValue) {
    log_v("PB ch%d raw:%d", channel, bendValue);
    if (channel > 9) return;

    if (bendValue == 0) {
        if (logicConnectionState == ConnectionState::CONNECTED) {
            if (millis() - connectedSinceTime < CONNECT_GRACE_MS) return;
            unsigned long now = millis();
            if (fadersAtMinMask == 0) firstFaderMinTime = now;
            fadersAtMinMask |= (1 << channel);
            int bitsSet = __builtin_popcount(fadersAtMinMask);
            if (bitsSet >= DISCONNECT_THRESHOLD &&
                (now - firstFaderMinTime) <= DISCONNECT_WINDOW_MS) {
                unsigned long elapsed = now - firstFaderMinTime;
                logicConnectionState = ConnectionState::DISCONNECTED;
                g_logicConnected     = 0;
                fadersAtMinMask      = 0;
                firstFaderMinTime    = 0;
                for (uint8_t i = 1; i <= NUM_SLAVES; i++)
                    rs485.setFaderTarget(i, rs485.getChannel(i).faderPos);
                g_switchToOffline = true;
                log_d("[DISCONNECT] %d faders en 0 en %lums.", bitsSet, elapsed);
                return;
            }
            if ((now - firstFaderMinTime) > DISCONNECT_WINDOW_MS) {
                fadersAtMinMask   = (1 << channel);
                firstFaderMinTime = now;
            }
        }
    } else {
        fadersAtMinMask &= ~(1 << channel);
    }

    if (channel < 9) {
        int bendClamped = (bendValue < 0) ? 0 : bendValue;
        if (channel < 8) {
            if (abs(bendClamped - (int)lastSentPitchBend[channel]) > PITCHBEND_DEADBAND) {
                rs485.setFaderTarget(channel + 1, (uint16_t)bendClamped);
                lastSentPitchBend[channel] = (int16_t)bendClamped;
            }
        }
        if (channel < 8) {
            float faderPositionNormalized = (float)bendClamped / (float)LOGIC_PITCHBEND_MAX;
            if (abs(faderPositions[channel + P4_CH_OFFSET] - faderPositionNormalized) > 0.001f) {
                faderPositions[channel + P4_CH_OFFSET] = faderPositionNormalized;
                needsMainAreaRedraw = true;
            }
        }
    }
}

void checkMidiTimeout() {
    if (logicConnectionState == ConnectionState::CONNECTED) {
        if (millis() - lastMidiActivityTime > MIDI_TIMEOUT_MS) {
            logicConnectionState = ConnectionState::DISCONNECTED;
            needsTOTALRedraw     = true;
            fadersAtMinMask      = 0;
            g_switchToOffline    = true;
        }
    }
}

bool isLogicConnected() {
    return (logicConnectionState == ConnectionState::CONNECTED);
}