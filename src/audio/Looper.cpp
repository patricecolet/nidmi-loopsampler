#include "Looper.h"
#include <esp_heap_caps.h>

bool Looper::begin() {
    /* Alloue le buffer d'enregistrement en PSRAM */
    _recBuf = (int16_t*)ps_malloc(kRecCapacity * sizeof(int16_t));
    if (!_recBuf) {
        Serial.println("[Looper] ERREUR: ps_malloc échoué — PSRAM disponible ?");
        return false;
    }
    _recCapacity = kRecCapacity;
    Serial.printf("[Looper] buffer PSRAM: %.1f MB (max %us stéréo 44.1k)\n",
        (kRecCapacity * sizeof(int16_t)) / 1048576.0f, kMaxRecSec);
    return true;
}

void Looper::startRecord() {
    if (_state == LooperState::Recording || _state == LooperState::Overdub) return;

    if (_layerCount == 0) {
        /* Premier enregistrement : démarre + définit la durée */
        _recPos = 0;
        _state  = LooperState::Recording;
        Serial.println("[Looper] enregistrement démarré");
    } else if (_layerCount < kMaxLayers) {
        /* Overdub par-dessus les couches existantes */
        _recPos = 0;
        _state  = LooperState::Overdub;
        /* Remet les couches au début */
        for (uint8_t i = 0; i < _layerCount; i++) {
            _playFiles[i].seek(_playDataStart[i]);
        }
        _playPos = 0;
        Serial.printf("[Looper] overdub démarré (couche %u/%u)\n", _layerCount + 1, kMaxLayers);
    } else {
        Serial.println("[Looper] max couches atteint");
    }
}

void Looper::stopRecord() {
    if (_state != LooperState::Recording && _state != LooperState::Overdub) return;

    if (_state == LooperState::Recording) {
        _loopSamples = _recPos;  /* définit la durée du loop */
    }

    if (saveLayerToSD(_layerCount)) {
        /* Ouvre le fichier sauvegardé pour la lecture */
        char path[40];
        snprintf(path, sizeof(path), "/loops/layer%u.wav", _layerCount);
        _playFiles[_layerCount] = SD_MMC.open(path, FILE_READ);
        size_t dummy;
        /* parseWavHeader minimal : skip les 44 premiers octets */
        _playFiles[_layerCount].seek(44);
        _playDataStart[_layerCount] = 44;
        _layerCount++;
    }

    _state  = LooperState::Playing;
    _playPos = 0;
    for (uint8_t i = 0; i < _layerCount; i++) {
        _playFiles[i].seek(_playDataStart[i]);
    }
    Serial.printf("[Looper] enregistrement terminé — %u couche(s)\n", _layerCount);
}

void Looper::play() {
    if (_layerCount == 0) return;
    _state   = LooperState::Playing;
    _playPos = 0;
    for (uint8_t i = 0; i < _layerCount; i++) {
        _playFiles[i].seek(_playDataStart[i]);
    }
}

void Looper::stop() {
    _state = LooperState::Idle;
}

void Looper::clearAll() {
    _state      = LooperState::Idle;
    _layerCount = 0;
    _loopSamples = 0;
    _playPos    = 0;
    _recPos     = 0;
    for (uint8_t i = 0; i < kMaxLayers; i++) {
        if (_playFiles[i]) _playFiles[i].close();
    }
    /* Supprime les fichiers sur SD */
    for (uint8_t i = 0; i < kMaxLayers; i++) {
        char path[40];
        snprintf(path, sizeof(path), "/loops/layer%u.wav", i);
        SD_MMC.remove(path);
    }
}

bool Looper::saveLayerToSD(uint8_t layerIdx) {
    char path[40];
    snprintf(path, sizeof(path), "/loops/layer%u.wav", layerIdx);

    SD_MMC.mkdir("/loops");
    File f = SD_MMC.open(path, FILE_WRITE);
    if (!f) {
        Serial.printf("[Looper] impossible d'écrire %s\n", path);
        return false;
    }

    size_t samples = (_state == LooperState::Overdub) ? _loopSamples : _recPos;
    writeWavHeader(f, samples);
    f.write((uint8_t*)_recBuf, samples * sizeof(int16_t));
    f.close();

    Serial.printf("[Looper] sauvegardé: %s (%.1fs)\n",
        path, (float)samples / (kSampleRate * 2));
    return true;
}

bool Looper::writeWavHeader(File& f, size_t samples) {
    uint32_t dataSize   = samples * sizeof(int16_t);
    uint32_t fileSize   = dataSize + 36;
    uint32_t sampleRate = kSampleRate;
    uint32_t byteRate   = kSampleRate * 2 * kBitsPerSample / 8;
    uint16_t blockAlign = 2 * kBitsPerSample / 8;
    uint16_t bitsPS     = kBitsPerSample;
    uint16_t numChan    = 2;
    uint16_t audioFmt   = 1;  /* PCM */

    f.write((uint8_t*)"RIFF", 4);
    f.write((uint8_t*)&fileSize, 4);
    f.write((uint8_t*)"WAVE", 4);
    f.write((uint8_t*)"fmt ", 4);
    uint32_t fmtSize = 16;
    f.write((uint8_t*)&fmtSize, 4);
    f.write((uint8_t*)&audioFmt, 2);
    f.write((uint8_t*)&numChan, 2);
    f.write((uint8_t*)&sampleRate, 4);
    f.write((uint8_t*)&byteRate, 4);
    f.write((uint8_t*)&blockAlign, 2);
    f.write((uint8_t*)&bitsPS, 2);
    f.write((uint8_t*)"data", 4);
    f.write((uint8_t*)&dataSize, 4);
    return true;
}

void Looper::update() {
    if (_state == LooperState::Idle) return;

    size_t rd;
    _codec.read(_recInput, kProcessBlock * 2, &rd);

    if (_state == LooperState::Recording) {
        size_t space = _recCapacity - _recPos;
        size_t copy  = min(rd, space);
        memcpy(_recBuf + _recPos, _recInput, copy * sizeof(int16_t));
        _recPos += copy;
        if (_recPos >= _recCapacity) stopRecord();  /* buffer plein → stop auto */
    }

    mixLayersToOutput();

    if (_state == LooperState::Overdub) {
        /* Mix input + playback → buffer d'enregistrement */
        size_t copy = min(rd, _loopSamples - _recPos);
        for (size_t s = 0; s < copy; s++) {
            int32_t v = (int32_t)_recInput[s] + _mixBuf[s];
            if (v > 32767)  v = 32767;
            if (v < -32768) v = -32768;
            _recBuf[_recPos + s] = (int16_t)v;
        }
        _recPos += copy;
        if (_recPos >= _loopSamples) stopRecord();
    }

    /* Loop : remet les fichiers au début si fin atteinte */
    _playPos += kProcessBlock;
    if (_loopSamples > 0 && _playPos >= _loopSamples) {
        _playPos = 0;
        for (uint8_t i = 0; i < _layerCount; i++) {
            _playFiles[i].seek(_playDataStart[i]);
        }
    }
}

void Looper::mixLayersToOutput() {
    memset(_mixBuf, 0, sizeof(_mixBuf));

    for (uint8_t i = 0; i < _layerCount; i++) {
        if (!_playFiles[i]) continue;
        size_t got = _playFiles[i].read((uint8_t*)_rdBuf, kProcessBlock * 2 * sizeof(int16_t));
        size_t s = got / sizeof(int16_t);
        for (size_t j = 0; j < s; j++) {
            _mixBuf[j] += _rdBuf[j];
        }
    }

    int16_t outBuf[kProcessBlock * 2];
    for (size_t j = 0; j < kProcessBlock * 2; j++) {
        int32_t v = _mixBuf[j];
        if (v > 32767)  v = 32767;
        if (v < -32768) v = -32768;
        outBuf[j] = (int16_t)v;
    }

    size_t written;
    _codec.write(outBuf, kProcessBlock * 2, &written);
}
