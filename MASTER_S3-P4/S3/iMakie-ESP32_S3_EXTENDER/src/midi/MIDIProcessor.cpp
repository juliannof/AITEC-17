// src/midi/MIDIProcessor.cpp
#include "MIDIProcessor.h"
#include "../config.h"
#include <USBMIDI.h>
#include "../RS485/RS485.h"
#include "../S3Link/S3Link.h"
#include "../hardware/Transporte.h"  // ← AÑADIDO

extern USBMIDI MIDI;
extern void updateLeds();
extern bool btnStatePG1[32];
extern bool btnStatePG2[32];
extern uint8_t g_logicConnected;

namespace {
    byte midi_buffer[512];
    int midi_idx = 0;
    bool in_sysex = false;
    byte last_status_byte = 0;

    static uint16_t fadersAtMinMask = 0;
    static unsigned long firstFaderMinTime = 0;
    static const uint16_t ALL_FADERS_MIN_MASK = 0x01FF;
    // MIDI_TIMEOUT_MS/lastMidiActivityTime (timeout por silencio MIDI) descartado
    // (2026-08-18) — mismo defecto estructural diagnosticado en el P4 el mismo día:
    // Logic no garantiza tráfico periódico en reposo genuino, generaba falsos
    // positivos de desconexión. Sustituido por checkUsbLink() (tud_mounted()).
    static const int DISCONNECT_THRESHOLD = 9;
    static const unsigned long DISCONNECT_WINDOW_MS = 150;
    static const int16_t PITCHBEND_DEADBAND = 80;  // ~0.5% rango, filtra ruido ±5 con margen seguro

    static int8_t  g_selectedChannel    = -1;
    static unsigned long connectedSinceTime  = 0;
    static const unsigned long CONNECT_GRACE_MS = 1500;
    // Firma de cierre real de Logic = faders-a-0 + 0x21 pegados (2026-08-18 20:26).
    // Logic manda GoOnline (0x21) unos ms después del burst de faders a 0 al cerrar
    // la app — sin esto, case 0x21 reconectaba siempre, deshaciendo la desconexión
    // que la heurística de faders acababa de marcar. Ver case 0x21 más abajo.
    static unsigned long lastFaderDisconnectTime = 0;
    static const unsigned long DISCONNECT_CONFIRM_WINDOW_MS = 500;
    static uint8_t  _calibPendingFrom = 0;
    static uint32_t _calibNextTime    = 0;
    static int16_t lastSentPitchBend[9] = {INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN, INT16_MIN};

    // Debounce anti-flash de nombres de pista (2026-08-13 15:10) — WHY: Logic
    // escribe texto transitorio más ancho que un slot de 7 caracteres (ej.
    // "Seleccionar"/"Selecting" al seleccionar canal), que se desborda sobre
    // el canal vecino y se restaura ~34ms después. Sin retener el cambio, ese
    // intermedio se aplica y se manda a S2 antes de la restauración.
    static char     _pendingName[8][8]  = {};
    static uint32_t _pendingSince[8]    = {};
    static bool     _pendingDirty[8]    = {};

    // Estado de las 5 notas de automodo (notas 74-78 → índices 0-4). (2026-06-14)
    // WHAT: rastrea qué nota del grupo está actualmente On según Logic.
    // WHY:  AUTO_OFF se deriva de la ausencia de cualquier nota activa — no existe
    //       una nota dedicada para OFF. Este array permite sobrevivir refreshes
    //       masivos donde las 5 notas llegan intercaladas con ~80 notas ajenas.
    // Ciclo de vida: activado por Note On; borrado por Note Off del mismo índice
    //   O por Note On de otro índice del grupo (mutual exclusion).
    static bool _autoNoteState[5] = {};
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

extern bool btnFlashPG1[32];
extern bool btnFlashPG2[32];
// default boot: READ, no OFF, en los 8 canales (2026-07-30)
uint8_t g_channelAutoMode[8] = {AUTO_READ, AUTO_READ, AUTO_READ, AUTO_READ,
                                 AUTO_READ, AUTO_READ, AUTO_READ, AUTO_READ};

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

// Debounce anti-flash de nombres de pista (2026-08-13 15:10) — aplica un
// cambio pendiente solo si sigue igual tras TRACK_NAME_DEBOUNCE_MS sin más
// cambios para ese canal. Ver case 0x12 (arma el pendiente) y el WHY arriba.
void tickTrackNameDebounce() {
    uint32_t now = millis();
    for (int t = 0; t < 8; t++) {
        if (!_pendingDirty[t]) continue;
        if (now - _pendingSince[t] < TRACK_NAME_DEBOUNCE_MS) continue;
        _pendingDirty[t] = false;
        if (trackNames[t] == _pendingName[t]) continue;  // defensivo, no debería pasar
        trackNames[t] = String(_pendingName[t]);
        rs485.setTrackName(t + 1, _pendingName[t]);
        s3Link.setTrackName(t, _pendingName[t]);  // reenvía al P4 (2026-08-16)
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
    log_d("CC CH=%d, CC=%d, Val=0x%02X", channel, controller, value);
    if (channel != 0 && channel != 15) return;

    if (controller >= 48 && controller <= 55) {
        uint8_t strip = controller - 48;
        // Gate de conexión (2026-08-02): mismo criterio que el fader — no propagar CC
        // recibidos antes del handshake real.
        if (logicConnectionState == ConnectionState::CONNECTED) {
            rs485.setVPotValue(strip + 1, value);
            vpotValues[strip] = value;
        }
        return;
    }

    if (controller < 64 || controller > 73) return;

    int digit_index  = 73 - controller;
    byte char_code   = value & 0x3F;
    char ascii_char  = (char_code < 64) ? MACKIE_CHAR_MAP[char_code] : '?';
    byte char_to_store = (byte)ascii_char;
    if (value & 0x40) char_to_store |= 0x80;

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
    return (result.length() == 0) ? "--:--:--:--" : result;
}

String formatBeatString() {
    char formatted[14];
    int pos = 0;
    for (int i = 0; i < 10; i++) {
        byte b = beatsChars_clean[i];
        char c = b & 0x7F;
        if (c == 0 || c < 32) c = '.';
        if (c == ';') c = '.';
        formatted[pos++] = c;
        if (b & 0x80) formatted[pos++] = '.';
    }
    formatted[pos] = '\0';
    String result = String(formatted);
    result.trim();
    if (result.length() == 0) return "  1.  1.  1.  1";
    while ((int)result.length() < 13) result += " ";
    return result;
}

void processChannelPressure(byte channel, byte value) {
    // Gate de conexión (2026-08-02): mismo criterio que el fader — no propagar VU
    // recibido antes del handshake real.
    if (logicConnectionState != ConnectionState::CONNECTED) return;

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
        s3Link.setVuLevel(targetChannel, vuLevel7bit);  // reenvía al P4 (2026-08-16)
    } else if (channel >= 1 && channel <= 7) {
        targetChannel = channel;
        normalizedLevel = (float)value / 127.0f;
        if (value >= 127) newClipState = true;
        rs485.setVuLevel(targetChannel + 1, value);
        s3Link.setVuLevel(targetChannel, value);  // reenvía al P4 (2026-08-16)
    } else {
        return;
    }

    if (targetChannel != -1) {
        bool stateChanged = false;
        if (normalizedLevel >= vuLevels[targetChannel] || normalizedLevel == 0.0f)
            vuLastUpdateTime[targetChannel] = millis();
        if (clearClip) {
            if (vuClipState[targetChannel]) { vuClipState[targetChannel] = false; stateChanged = true; }
        } else if (newClipState) {
            if (!vuClipState[targetChannel]) { vuClipState[targetChannel] = true; stateChanged = true; }
        }
        if (normalizedLevel > vuLevels[targetChannel]) {
            vuLevels[targetChannel] = normalizedLevel;
            if (normalizedLevel > vuPeakLevels[targetChannel]) {
                vuPeakLevels[targetChannel] = normalizedLevel;
                vuPeakLastUpdateTime[targetChannel] = millis();
            }
            stateChanged = true;
        } else if (normalizedLevel == 0.0f && vuLevels[targetChannel] != 0.0f) {
            vuLevels[targetChannel] = 0.0f;
            stateChanged = true;
        }
        
    }
}

void processMackieSysEx(byte* payload, int len) {
    if (len < 5) return;

    byte device_family = payload[3];
    byte command = payload[4];

    log_v("[SYSEX] family=0x%02X cmd=0x%02X len=%d", device_family, command, len);

    // Fase 0: sondeo — responder a cmd 0x00 y 0x13 en cualquier familia
    if (command == 0x00) {
        // Diagnóstico (2026-08-02): Logic reinicia el handshake completo mandando
        // este query — loguear con timestamp para correlacionar con reset del S3
        // (ver esp_reset_reason() en setup()) y descartar/confirmar reboot físico.
        log_e("[HANDSHAKE] Device Query 0x00 recibido — Logic reinicia negociacion (t=%lu ms)", millis());
        // FIX 2026-08-07: familia hardcodeada a 0x14 aquí — el S3 respondía SIEMPRE
        // "soy familia 0x14" (identidad del P4) sin importar qué familia sondeara
        // Logic (0x10/0x11/0x17/0x14/0x15). Cuando Logic buscaba específicamente el
        // extender (familia 0x15), el S3 igual contestaba 0x14 — Logic nunca lograba
        // identificar el extender como device distinto del P4, y reiniciaba la
        // negociación en bucle. Usar DEVICE_FAMILY (0x15 en el S3) en vez del literal.
        // Serial 7 bytes ASCII "AITEC-X" (X = Extender), único frente al P4.
        // Challenge 4 bytes fijos "AITX". Logic responde con 0x02; confirmamos con 0x03.
        byte reply[] = {0xF0, 0x00, 0x00, 0x66, DEVICE_FAMILY, 0x01,
                        'A','I','T','E','C','-','X', 'A','I','T','X', 0xF7};
        sendMIDIBytes(reply, sizeof(reply));
        return;
    }
    if (command == 0x02) {
        // Fase 2: Logic confirma el challenge. No lo validamos (patrón BCF, confianza B1).
        byte reply[] = {0xF0, 0x00, 0x00, 0x66, DEVICE_FAMILY, 0x03,
                        'A','I','T','E','C','-','X', 0xF7};
        sendMIDIBytes(reply, sizeof(reply));
        return;
    }
    if (command == 0x13) {
        // FIX 2026-08-07: mismo bug — familia hardcodeada a 0x14 en vez de DEVICE_FAMILY.
        // Version reply: 5 bytes ASCII "V1.00" (antes 2 bytes, incorrecto).
        byte reply[] = {0xF0, 0x00, 0x00, 0x66, DEVICE_FAMILY, 0x14,
                        'V','1','.','0','0', 0xF7};
        sendMIDIBytes(reply, sizeof(reply));
        return;
    }

    // Fase 1+: solo la familia propia del dispositivo (0x15 en el S3).
    // FIX 2026-08-07: comparaba contra 0x14 literal (identidad del P4). Antes esto
    // "funcionaba por accidente" porque la Fase 0 también respondía 0x14 siempre,
    // así que Logic seguía direccionando todo a 0x14. Al corregir la Fase 0 para
    // que el S3 se identifique como 0x15 (DEVICE_FAMILY), Logic empezó a mandar
    // los comandos de la Fase 1+ (incluido 0x21, que marca CONNECTED) direccionados
    // a 0x15 — y esta guarda los descartaba todos, dejando el dispositivo sin
    // conectar nunca. Debe comparar contra DEVICE_FAMILY, no un literal.
    if (device_family != DEVICE_FAMILY) return;

    switch (command) {

        case 0x0F: {
            logicConnectionState = ConnectionState::DISCONNECTED;
            g_logicConnected     = 0;   // Todos los slaves recibirán connected=0
            fadersAtMinMask      = 0;
            firstFaderMinTime    = 0;
            // Fijar target a la posición actual del fader (2026-08-02): igual que ya hacía
            // la desconexión automática por fadersAtMinMask — evita dejar armado un target
            // viejo/potencialmente contaminado para cuando Logic reconecte.
            for (uint8_t i = 1; i <= NUM_SLAVES; i++)
                rs485.setFaderTarget(i, rs485.getChannel(i).faderPos);
            // Reset AutoMode a READ en desconexión (2026-08-16): sin esto, los 8
            // canales se quedaban con el último modo real recibido por nota, incluso
            // con Logic desconectado — mostrando un estado obsoleto. READ es el
            // default seguro (igual que el boot, línea 71-73) hasta que Logic
            // reconecte y mande el modo real por nota.
            for (uint8_t i = 0; i < 8; i++) g_channelAutoMode[i] = AUTO_READ;
            for (uint8_t i = 1; i <= NUM_SLAVES; i++) rs485.setAutoMode(i, AUTO_READ);
            Transporte::setAllLedsOff();  // LEDs transport off al desconectar (2026-05-27)
            rs485.beginDisconnectSequence();
            g_switchToOffline    = true;
            log_i("[MCU] GoOffline recibido — iniciando DISCONNECT SEQUENCE");
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

            for (int t = 0; t < 8; t++) {
                strncpy(nameBufs[t], trackNames[t].c_str(), 7);
                nameBufs[t][7] = '\0';
            }

            for (int i = 0; i < text_len; i++) {
                byte offset = startOffset + i;
                if (offset >= 56) break;
                nameBufs[offset / 7][offset % 7] = (char)payload[6 + i];
                nameChanged[offset / 7] = true;
            }

            for (int t = 0; t < 8; t++) {
                if (!nameChanged[t]) continue;
                trimRight(nameBufs[t]);
                if (nameBufs[t][0] == '\0') continue;  // row 1 vacía (modo plugin/Atmos) — conservar nombre previo

                if (trackNames[t] == nameBufs[t]) {
                    // Coincide con lo ya mostrado — cancela cualquier cambio pendiente
                    // (ej. el flash "Seleccionar" restaurándose al valor original: nunca
                    // llegó a mandarse, así que no hace falta "deshacerlo").
                    _pendingDirty[t] = false;
                    continue;
                }
                // Arma/reemplaza el pendiente con timer fresco — se aplica de verdad
                // en tickTrackNameDebounce() si sigue igual tras TRACK_NAME_DEBOUNCE_MS.
                strncpy(_pendingName[t], nameBufs[t], 8);
                _pendingSince[t] = millis();
                _pendingDirty[t] = true;
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
            }
            break;
        }

        case 0x61: {
            // AllFadersToMinimum — Logic inicializa faders al conectar (parte de GoOnline)
            // NO cambiar g_logicConnected — S2s deben quedarse CONNECTED (2026-05-27)
            for (uint8_t i = 1; i <= NUM_SLAVES; i++)
                rs485.setFaderTarget(i, 0);
            log_i("[MCU] AllFaderstoMinimum — faders a 0");
            break;
        }

        case 0x72: {
            if (len < 13) break;
            for (int i = 0; i < 8; i++) {
                byte raw     = payload[5 + i];
                byte channel = raw & 0x0F;
                byte level   = (raw >> 4);
                bool clip    = (raw == 0x0F);
                if (channel < 8) {
                    float normalized = level / 7.0f;
                    if (vuLevels[channel] != normalized) {
                        vuLevels[channel]    = normalized;
                        vuClipState[channel] = clip;
                    }
                    rs485.setVuLevel(channel + 1, (uint8_t)(normalized * 127.0f));
                }
            }
            break;
        }

        case 0x0E: {
            // AutoMode NO se procesa desde este SysEx (2026-08-16). Confirmado en
            // banco: Logic lo manda como refresco periódico con valor idéntico para
            // los 8 canales (no refleja el modo real por pista), y su orden de bytes
            // resultó ambiguo/no validado frente al de las notas 74-78. Tras el
            // cambio de DEVICE_FAMILY (0x15→0x14, este mismo config.h) el S3 pasó a
            // recibir las notas 74-78 igual que el P4 — esas SÍ reflejan el modo real
            // por pista y ya usan un mapeo correcto (noteToMode[], más abajo). Dejar
            // este SysEx activo solo arriesgaba pisar con un valor placeholder el
            // modo correcto que acababa de llegar por nota. Ver CHANGELOG 2026-08-16.
            break;
        }

        case 0x21: {
            // Fase 2 CRÍTICA — echo inmediato
            byte echo[] = {0xF0, 0x00, 0x00, 0x66, DEVICE_FAMILY, 0x21, 0x01, 0xF7};
            sendMIDIBytes(echo, sizeof(echo));
            // Firma de cierre real de Logic = faders-a-0 + 0x21 pegados (2026-08-18 20:26).
            // Si este 0x21 llega dentro de DISCONNECT_CONFIRM_WINDOW_MS desde la última
            // desconexión por heurística de faders, es la confirmación del cierre — no
            // se reconecta. Ver DISCONNECT_CONFIRM_WINDOW_MS arriba y el bloque de
            // processPitchBend que fija lastFaderDisconnectTime.
            if (logicConnectionState != ConnectionState::CONNECTED &&
                millis() - lastFaderDisconnectTime <= DISCONNECT_CONFIRM_WINDOW_MS) {
                log_i("[MCU] 0x21 tras faders-a-0 (firma cierre) — permanece DISCONNECTED");
                break;
            }
            if (logicConnectionState != ConnectionState::CONNECTED) {
                logicConnectionState = ConnectionState::CONNECTED;
                g_logicConnected     = 1;
                connectedSinceTime   = millis();
                // _calibPendingFrom = 1;   // ELIMINADO — boot auto-calib ya lo hizo (2026-05-19)
                // _calibNextTime    = millis();
                // Diagnóstico (2026-08-02): timestamp para correlacionar con [HANDSHAKE] 0x00
                // y con [BOOT] reset reason — ver nota en processMackieSysEx case 0x00.
                log_e("[HANDSHAKE] 0x21 — CONNECTED (t=%lu ms)", millis());
            }
            break;
        }

        case 0x0C: {
            // Echo Surface Type + suscripción a feedback
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
                log_v("[MCU] Echo cmd=0x%02X (%d bytes)", command, elen);
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

    log_v("[NOTE] st=0x%02X note=0x%02X(%d) vel=%d is_on=%d", status, note, note, velocity, is_on);

    if (note <= 31) {
        int group     = note / 8;
        int track_idx = note % 8;
        bool stateChanged = false;
        switch (group) {
            case 0: if (recStates[track_idx]    != is_on) { recStates[track_idx]    = is_on; stateChanged = true; } break;
            case 1: if (soloStates[track_idx]   != is_on) { soloStates[track_idx]   = is_on; stateChanged = true; } break;
            case 2: if (muteStates[track_idx]   != is_on) { muteStates[track_idx]   = is_on; stateChanged = true; } break;
            case 3:
                if (selectStates[track_idx] != is_on) { selectStates[track_idx] = is_on; stateChanged = true; }
                if (is_on) g_selectedChannel = track_idx;
                else if (g_selectedChannel == track_idx) g_selectedChannel = -1;
                break;
        }
        if (stateChanged) {
            uint8_t slaveId = track_idx + 1;
            uint8_t flags = 0;
            if (recStates[track_idx])    flags |= FLAG_REC;
            if (soloStates[track_idx])   flags |= FLAG_SOLO;
            if (muteStates[track_idx])   flags |= FLAG_MUTE;
            if (selectStates[track_idx]) flags |= FLAG_SELECT;
            flags = setAutoMode(flags, (AutoMode)g_channelAutoMode[track_idx]);
            rs485.setFlags(slaveId, flags);
            s3Link.setFlags(track_idx, flags);  // reenvía al P4 (2026-08-16); enmascara AutoMode internamente
        }
        return;
    }

    // ── Automodo — notas 74-78 (2026-06-14) ─────────────────────────────────
    // WHAT: detecta transiciones de modo de automatización por Note On/Off.
    // WHY:  AUTO_OFF se deriva de la AUSENCIA de cualquier nota activa en el
    //       grupo (74-78) — no existe nota dedicada. Nota 79 ("Automation Group"
    //       en la spec Mackie) NO se mapea a AutoMode.
    // ASSUMES: las 5 notas son mutuamente excluyentes. Un Note On en el grupo
    //          implica que las otras 4 deben ser Off. El estado se rastrea en
    //          _autoNoteState[5] para sobrevivir refreshes masivos donde las
    //          notas llegan intercaladas con ~80 eventos no relacionados.
    if (note >= 74 && note <= 78 && g_selectedChannel >= 0) {
        uint8_t idx = note - 74;  // 0=READ 1=WRITE 2=TRIM 3=TOUCH 4=LATCH
        AutoMode prevMode = (AutoMode)g_channelAutoMode[g_selectedChannel];

        if (is_on) {
            // Note On: este modo es activo. Borrar las otras 4 (mutual exclusion
            // garantizada por el protocolo). Esto resuelve la anomalía TRIM —
            // Note On sin Note Off previo — sin lógica adicional.
            for (uint8_t i = 0; i < 5; i++) _autoNoteState[i] = false;
            _autoNoteState[idx] = true;
        } else {
            // Note Off: desactivar esta nota. Si el resto siguen Off → AUTO_OFF.
            // Si alguno quedó On por anomalía de protocolo, ese modo prevalece.
            _autoNoteState[idx] = false;
        }

        // Derivar modo activo: primer índice true, o AUTO_OFF si ninguno.
        static const AutoMode noteToMode[5] = {AUTO_READ, AUTO_WRITE, AUTO_TRIM, AUTO_TOUCH, AUTO_LATCH};
        AutoMode newMode = AUTO_OFF;
        for (uint8_t i = 0; i < 5; i++) {
            if (_autoNoteState[i]) { newMode = noteToMode[i]; break; }
        }

        if (newMode != prevMode) {
            log_i("[AUTOMODE] S3 nota=%d vel=%d canal=%d %d→%d",
                  note, velocity, g_selectedChannel, (int)prevMode, (int)newMode);
            g_channelAutoMode[g_selectedChannel] = (uint8_t)newMode;
            rs485.setAutoMode(g_selectedChannel + 1, newMode);
        }

        // Actualizar estado visual del botón en el mapa de NeoTrellis/PG1.
        for (int key = 0; key < 32; key++) {
            if (MIDI_NOTES_PG1[key] != 0x00 && MIDI_NOTES_PG1[key] == note) {
                btnStatePG1[key]  = is_on;
                btnFlashPG1[key]  = is_flashing;
            }
        }
        return;
    }

    bool stateChanged = false;
    for (int key = 0; key < 32; key++) {
        if (MIDI_NOTES_PG1[key] != 0x00 && MIDI_NOTES_PG1[key] == note) {
            if (btnStatePG1[key] != is_on || btnFlashPG1[key] != is_flashing) {
                btnStatePG1[key]  = is_on;
                btnFlashPG1[key]  = is_flashing;
                stateChanged = true;
            }
        }
        if (MIDI_NOTES_PG2[key] != 0x00 && MIDI_NOTES_PG2[key] == note) {
            if (btnStatePG2[key] != is_on || btnFlashPG2[key] != is_flashing) {
                btnStatePG2[key]  = is_on;
                btnFlashPG2[key]  = is_flashing;
                stateChanged = true;
            }
        }
    }
   
    Transporte::setLedByNote(note, is_on);  // ← AÑADIDO
}

void processPitchBend(byte channel, int bendValue) {
    // Diagnóstico RX (2026-08-07) — qué recibe el S3 de Logic, para comparar
    // directamente contra el log [RS485-TX] (qué sale hacia el S2) en el mismo monitor.
    // Restringido a channel < 8 (2026-08-07): solo hay 8 slaves aquí (canales 0-7).
    // ch=8 es el master fader de Logic (no se reenvía a ningún slave) y ch=9 no se
    // procesa en ningún sitio — loguearlos era ruido sin relación con los slaves.
    if (channel < 8) {
        log_i("[MIDI-RX] PB ch=%d raw=%d connected=%d", channel, bendValue, (int)(logicConnectionState == ConnectionState::CONNECTED));
    }
    if (channel > 9) return;

    if (bendValue == 0) {
        // Bug B3 fix (2026-08-13): el guard de grace period ya NO hace return
        // aquí — antes se comía el resto del mensaje (incluido el reenvío del
        // target más abajo) para cualquier canal procesado tras el flip a
        // CONNECTED dentro de la misma ráfaga de conexión. Ahora solo evita
        // que esos mensajes cuenten para la heurística de desconexión.
        if (logicConnectionState == ConnectionState::CONNECTED &&
            millis() - connectedSinceTime >= CONNECT_GRACE_MS) {
            unsigned long now = millis();
            if (fadersAtMinMask == 0) firstFaderMinTime = now;
            fadersAtMinMask |= (1 << channel);
            int bitsSet = __builtin_popcount(fadersAtMinMask);
            if (bitsSet >= DISCONNECT_THRESHOLD &&
                (now - firstFaderMinTime) <= DISCONNECT_WINDOW_MS) {
                logicConnectionState = ConnectionState::DISCONNECTED;
                g_logicConnected     = 0;
                fadersAtMinMask      = 0;
                firstFaderMinTime    = 0;
                lastFaderDisconnectTime = now;
                Transporte::setAllLedsOff();  // LEDs transport off al desconectar (2026-05-27)
                for (uint8_t i = 1; i <= NUM_SLAVES; i++)
                    rs485.setFaderTarget(i, rs485.getChannel(i).faderPos);
                g_switchToOffline = true;
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

    if (logicConnectionState == ConnectionState::MIDI_HANDSHAKE_COMPLETE) {
        logicConnectionState = ConnectionState::CONNECTED;
        g_logicConnected     = 1;
        connectedSinceTime   = millis();
        fadersAtMinMask      = 0;
        for (uint8_t i = 0; i < 8; i++) {
            if (selectStates[i]) {
                byte offMsg[3] = { 0x80, (uint8_t)(24 + i), 0x00 };
                sendMIDIBytes(offMsg, 3);
                selectStates[i] = false;
            }
        }
        // _calibPendingFrom = 1;   // ELIMINADO — boot auto-calib ya lo hizo (2026-05-19)
        // _calibNextTime    = millis();
        g_switchToPage3 = true;
    }

    if (channel < 9) {
        // Logic raw 0-16383 (14-bit MIDI completo, confirmado MIDI Monitor 2026-07-20)
        // setFaderTarget espera este rango y hace el mapeo a ADC internamente
        int bendClamped = (bendValue < 0) ? 0 : bendValue;

        if (channel < 8) {
            // Gate de conexión (2026-08-02): sin esto, cualquier PitchBend que llegue por
            // USB-MIDI antes del handshake real (SysEx 0x21) queda armado en _ch[id].faderTarget
            // del S3 y se aplica de golpe al motor del S2 en cuanto CONNECTED pasa a 1 —
            // provocaba el motor persiguiendo un target no confirmado por Logic tras conectar.
            // Bug B3 fix (2026-08-13): también bloqueado durante CONNECT_GRACE_MS
            // — sin esto, el canal que dispara el flip a CONNECTED (procesado
            // ANTES de este guard) sí reenviaba su target, mientras el resto de
            // canales de la misma ráfaga inicial lo perdían por el return de
            // arriba — asimetría que causaba que un fader se moviera y otros no
            // al abrir Logic sin proyecto.
            // Ajuste (2026-08-13, misma sesión): bloquear TODO reenvío durante el
            // grace period tenía un efecto secundario grave — confirmado con MIDI
            // Monitor sin proyecto abierto: si un fader NO estaba ya en 0 (sesión
            // de prueba anterior, STALL a medio camino, etc.), el target=0 de esa
            // única ráfaga se perdía y como Logic no lo repite, el fader quedaba
            // huérfano en su posición física real para siempre. Ahora el grace
            // period solo suprime cuando el fader YA está donde Logic quiere
            // (según el último faderPos reportado por el S2) — si difiere de
            // verdad, se deja pasar aunque siga dentro del grace period.
            bool alreadyThere = abs(bendClamped - (int)rs485.getChannel(channel + 1).faderPos) <= PITCHBEND_DEADBAND;
            if (logicConnectionState == ConnectionState::CONNECTED &&
                (millis() - connectedSinceTime >= CONNECT_GRACE_MS || !alreadyThere) &&
                abs(bendClamped - (int)lastSentPitchBend[channel]) > PITCHBEND_DEADBAND) {
                rs485.setFaderTarget(channel + 1, (uint16_t)bendClamped);
                lastSentPitchBend[channel] = (int16_t)bendClamped;
                log_i("[MIDI-RX] → RS485 slave=%d bendClamped=%d (forwarded)", channel + 1, bendClamped);
            } else if (logicConnectionState != ConnectionState::CONNECTED) {
                // Solo avisa cuando se descarta por NO estar conectado — el filtro de
                // deadband rutinario NO se loguea aquí porque dispara constantemente
                // en movimiento normal de fader y ahogaría el monitor. (2026-08-07)
                log_i("[MIDI-RX] slave=%d bendClamped=%d DESCARTADO (no conectado)", channel + 1, bendClamped);
            }
        }
        float faderPositionNormalized = (float)bendClamped / (float)LOGIC_PITCHBEND_MAX;
        if (abs(faderPositions[channel] - faderPositionNormalized) > 0.001f) {
            faderPositions[channel] = faderPositionNormalized;
        }
    }
}

// Detección de desconexión física real (2026-08-18) — reemplaza el intento anterior
// de timeout por silencio MIDI (MIDI_TIMEOUT_MS/lastMidiActivityTime, nunca llegó a
// llamarse desde main.cpp). Mismo criterio aplicado al P4 el mismo día: Logic no
// manda tráfico MIDI garantizado en reposo genuino, así que cualquier timeout fijo
// por silencio genera falsos positivos. tud_mounted() consulta el estado real del
// enlace USB (TinyUSB) — exacto, inmediato, sin heurísticas de tiempo.
void checkUsbLink() {
    if (logicConnectionState == ConnectionState::CONNECTED && !tud_mounted()) {
        logicConnectionState = ConnectionState::DISCONNECTED;
        fadersAtMinMask      = 0;
        g_switchToOffline    = true;
        log_i("[MCU] USB desmontado — forzando DISCONNECTED");
    }
}

bool isLogicConnected() {
    return (logicConnectionState == ConnectionState::CONNECTED);
}