#pragma once
#include <Arduino.h>
#include <SD_MMC.h>
#include "WM8960Driver.h"

/* Nombre max de couches superposées */
static constexpr uint8_t kMaxLayers = 4;

enum class LooperState : uint8_t {
    Idle,       /* aucune boucle active */
    Recording,  /* premier enregistrement — définit la durée du loop */
    Overdub,    /* enregistrement par-dessus une couche existante */
    Playing,    /* lecture seule */
};

class Looper {
public:
    explicit Looper(WM8960Driver& codec) : _codec(codec) {}

    bool begin();

    /* Transport — déclenché par MIDI CC ou GPIO */
    void startRecord();   /* premier enregistrement ou overdub si loop existe */
    void stopRecord();    /* ferme l'enregistrement, sauve sur SD */
    void play();
    void stop();
    void clearAll();      /* supprime toutes les couches */

    LooperState state() const { return _state; }
    uint8_t     layerCount() const { return _layerCount; }

    /* Appelé depuis AudioTask sur Core 0 */
    void update();

private:
    WM8960Driver& _codec;
    LooperState   _state      = LooperState::Idle;
    uint8_t       _layerCount = 0;

    /* Lecture des couches existantes */
    File    _playFiles[kMaxLayers];
    size_t  _playDataStart[kMaxLayers] = {};
    size_t  _loopSamples = 0;   /* durée du loop en samples stéréo */
    size_t  _playPos     = 0;   /* position courante en samples stéréo */

    /* Enregistrement vers PSRAM puis SD */
    int16_t* _recBuf      = nullptr;  /* alloué en PSRAM (ps_malloc) */
    size_t   _recCapacity = 0;        /* max samples stéréo dans _recBuf */
    size_t   _recPos      = 0;        /* samples écrits dans _recBuf */

    static constexpr size_t kMaxRecSec    = 60;
    static constexpr size_t kRecCapacity  = kMaxRecSec * kSampleRate * 2;  /* stéréo */
    static constexpr size_t kProcessBlock = 512;  /* samples stéréo par cycle */

    bool saveLayerToSD(uint8_t layerIdx);
    bool writeWavHeader(File& f, size_t samples);
    void mixLayersToOutput();

    int16_t _rdBuf[kProcessBlock * 2];  /* lecture d'une couche */
    int32_t _mixBuf[kProcessBlock * 2]; /* mix en int32 */
    int16_t _recInput[kProcessBlock * 2]; /* capture ADC */
};
