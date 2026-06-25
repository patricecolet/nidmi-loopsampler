#pragma once
#include <Arduino.h>
#include <SD_MMC.h>
#include "WM8960Driver.h"

static constexpr uint8_t kMaxVoices = 6;

/* Modes de lecture par voix */
enum class PlayMode : uint8_t {
    OneShot,    /* joue une fois et s'arrête */
    Loop,       /* boucle indéfiniment */
    Exclusive,  /* coupe les autres voix du même groupe */
};

struct VoiceConfig {
    PlayMode mode        = PlayMode::OneShot;
    uint8_t  exclGroup   = 0;  /* 0 = pas de groupe exclusif */
    uint8_t  midiChannel = 0;  /* 0 = tous les canaux */
};

class WavPlayer {
public:
    explicit WavPlayer(WM8960Driver& dac) : _dac(dac) {}

    bool begin();

    /* Appelé par RTP-MIDI sur réception noteOn */
    void noteOn(uint8_t channel, uint8_t note, uint8_t velocity);
    void noteOff(uint8_t channel, uint8_t note);

    /* Mapping note → fichier WAV (ex: note 60 → "/samples/bank0/60.wav") */
    void setBank(const char* bankPath);
    void setVoiceConfig(uint8_t voiceIdx, const VoiceConfig& cfg);

    /* Appelé depuis AudioTask sur Core 0 */
    void update();

private:
    struct Voice {
        File     file;
        bool     active    = false;
        uint8_t  note      = 0;
        uint8_t  velocity  = 0;
        uint8_t  exclGroup = 0;
        PlayMode mode      = PlayMode::OneShot;
        size_t   dataStart = 0;  /* offset après header WAV */
    };

    WM8960Driver& _dac;
    Voice         _voices[kMaxVoices];
    VoiceConfig   _configs[128];  /* un config par note MIDI */
    char          _bankPath[64] = "/samples/default";

    /* Renvoie l'index d'une voix libre (oldest-steal si toutes actives) */
    uint8_t allocVoice(uint8_t note, uint8_t exclGroup);

    /* Lit et valide le header WAV, positionne file après le header */
    bool parseWavHeader(File& f, size_t& dataStart);

    /* Mixe les voix actives et envoie au DAC par blocs */
    void mixAndSend();

    static constexpr size_t kMixBufSamples = 512;  /* par canal → 1024 int16 stéréo */
    int32_t _mixBuf[kMixBufSamples * 2];            /* mix en int32 pour éviter overflow */
    int16_t _outBuf[kMixBufSamples * 2];
};
