#pragma once
#include <Arduino.h>
#include <SD_MMC.h>

/* Broches SDMMC 4-bit sur XIAO ESP32-S3 (adapter selon câblage) */
static constexpr int kSD_CLK  = 7;
static constexpr int kSD_CMD  = 9;
static constexpr int kSD_D0   = 8;
static constexpr int kSD_D1   = 21;
static constexpr int kSD_D2   = 20;
static constexpr int kSD_D3   = 10;

class SampleBank {
public:
    bool begin();

    /* Liste les banques disponibles sous /samples/ → JSON array */
    String listBanksJson();

    /* Liste les samples d'une banque → JSON array de notes */
    String listSamplesJson(const char* bank);

    /* Sauve un sample uploadé (appelé depuis AudioAPI) */
    bool saveSample(const char* bank, uint8_t note, const uint8_t* data, size_t len);

    /* Supprime un sample */
    bool deleteSample(const char* bank, uint8_t note);

    bool available() const { return _ready; }

private:
    bool _ready = false;

    /* Construit le chemin : /samples/<bank>/<note>.wav */
    void samplePath(char* out, size_t sz, const char* bank, uint8_t note);
};
