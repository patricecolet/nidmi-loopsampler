#include "WavPlayer.h"
#include <string.h>

bool WavPlayer::begin() {
    memset(_voices, 0, sizeof(_voices));
    memset(_mixBuf, 0, sizeof(_mixBuf));
    return true;
}

void WavPlayer::noteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (velocity == 0) { noteOff(channel, note); return; }
    if (note >= 128) return;

    const VoiceConfig& cfg = _configs[note];

    uint8_t idx = allocVoice(note, cfg.exclGroup);
    Voice& v = _voices[idx];

    /* Chemin WAV : <bank>/<note>.wav */
    char path[80];
    snprintf(path, sizeof(path), "%s/%d.wav", _bankPath, note);

    if (v.file) v.file.close();
    v.file = SD_MMC.open(path, FILE_READ);
    if (!v.file) {
        Serial.printf("[WavPlayer] introuvable: %s\n", path);
        v.active = false;
        return;
    }

    if (!parseWavHeader(v.file, v.dataStart)) {
        v.file.close();
        v.active = false;
        return;
    }

    v.active    = true;
    v.note      = note;
    v.velocity  = velocity;
    v.mode      = cfg.mode;
    v.exclGroup = cfg.exclGroup;
}

void WavPlayer::noteOff(uint8_t channel, uint8_t note) {
    for (uint8_t i = 0; i < kMaxVoices; i++) {
        Voice& v = _voices[i];
        if (v.active && v.note == note && v.mode != PlayMode::Loop) {
            v.active = false;
            if (v.file) v.file.close();
        }
    }
}

void WavPlayer::setBank(const char* bankPath) {
    strncpy(_bankPath, bankPath, sizeof(_bankPath) - 1);
    _bankPath[sizeof(_bankPath) - 1] = '\0';
}

void WavPlayer::setVoiceConfig(uint8_t voiceIdx, const VoiceConfig& cfg) {
    /* voiceIdx ici = note MIDI (0–127) */
    if (voiceIdx < 128) _configs[voiceIdx] = cfg;
}

uint8_t WavPlayer::allocVoice(uint8_t note, uint8_t exclGroup) {
    /* Coupe les voix du même groupe exclusif */
    if (exclGroup != 0) {
        for (uint8_t i = 0; i < kMaxVoices; i++) {
            if (_voices[i].active && _voices[i].exclGroup == exclGroup) {
                _voices[i].active = false;
                if (_voices[i].file) _voices[i].file.close();
            }
        }
    }
    /* Cherche une voix libre */
    for (uint8_t i = 0; i < kMaxVoices; i++) {
        if (!_voices[i].active) return i;
    }
    /* Toutes occupées → vole la première (oldest-steal) */
    if (_voices[0].file) _voices[0].file.close();
    _voices[0].active = false;
    return 0;
}

bool WavPlayer::parseWavHeader(File& f, size_t& dataStart) {
    /* Lecture minimale du header WAV RIFF PCM */
    uint8_t hdr[44];
    if (f.read(hdr, 44) != 44) return false;
    /* Vérification "RIFF" et "WAVE" */
    if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) return false;
    /* Cherche le chunk "data" (peut ne pas être exactement à 36) */
    f.seek(12);
    while (f.available() > 8) {
        uint8_t chunk[8];
        f.read(chunk, 8);
        uint32_t chunkSize = chunk[4] | (chunk[5] << 8) | (chunk[6] << 16) | (chunk[7] << 24);
        if (memcmp(chunk, "data", 4) == 0) {
            dataStart = f.position();
            return true;
        }
        f.seek(f.position() + chunkSize);
    }
    return false;
}

void WavPlayer::update() {
    mixAndSend();
}

void WavPlayer::mixAndSend() {
    bool anyActive = false;
    for (uint8_t i = 0; i < kMaxVoices; i++) {
        if (_voices[i].active) { anyActive = true; break; }
    }
    if (!anyActive) return;

    memset(_mixBuf, 0, sizeof(_mixBuf));

    for (uint8_t i = 0; i < kMaxVoices; i++) {
        Voice& v = _voices[i];
        if (!v.active) continue;

        int16_t readBuf[kMixBufSamples * 2];
        size_t toRead = kMixBufSamples * 2 * sizeof(int16_t);
        size_t got = v.file.read((uint8_t*)readBuf, toRead);
        size_t samples = got / sizeof(int16_t);

        float gain = v.velocity / 127.0f;
        for (size_t s = 0; s < samples; s++) {
            _mixBuf[s] += (int32_t)(readBuf[s] * gain);
        }

        if (got < toRead) {
            if (v.mode == PlayMode::Loop) {
                v.file.seek(v.dataStart);
            } else {
                v.active = false;
                v.file.close();
            }
        }
    }

    /* Clamp int32 → int16 */
    for (size_t s = 0; s < kMixBufSamples * 2; s++) {
        int32_t val = _mixBuf[s];
        if (val > 32767)  val = 32767;
        if (val < -32768) val = -32768;
        _outBuf[s] = (int16_t)val;
    }

    size_t written;
    _dac.write(_outBuf, kMixBufSamples * 2, &written);
}
