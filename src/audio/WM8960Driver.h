#pragma once
#include <Arduino.h>
#include <SparkFun_WM8960_Arduino_Library.h>
#include <driver/i2s.h>

/* Broches I2C vers WM8960 (adapter selon câblage) */
static constexpr int kWM_SDA  = 5;
static constexpr int kWM_SCL  = 6;

/* Broches I2S (adapter selon câblage) */
static constexpr int kI2S_BCLK = 7;
static constexpr int kI2S_WS   = 44;
static constexpr int kI2S_DOUT = 43;  /* S3 → WM8960 DAC */
static constexpr int kI2S_DIN  = 2;   /* WM8960 ADC → S3 */

static constexpr uint32_t kSampleRate   = 44100;
static constexpr uint8_t  kBitsPerSample = 16;
static constexpr i2s_port_t kI2SPort    = I2S_NUM_0;

class WM8960Driver {
public:
    bool begin();

    /* Écriture vers le DAC (sortie audio) */
    bool write(const int16_t* buf, size_t samples, size_t* written);

    /* Lecture depuis l'ADC (entrée micro/ligne) */
    bool read(int16_t* buf, size_t samples, size_t* read);

    void setOutputVolume(uint8_t vol);  /* 0–63 */
    void setInputGain(uint8_t gain);    /* 0–63 */

private:
    WM8960 _codec;

    bool initCodec();
    bool initI2S();
};
