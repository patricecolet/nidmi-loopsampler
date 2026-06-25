#include "WM8960Driver.h"
#include <Wire.h>

bool WM8960Driver::begin() {
    return initCodec() && initI2S();
}

bool WM8960Driver::initCodec() {
    Wire.begin(kWM_SDA, kWM_SCL);
    if (!_codec.begin(Wire)) {
        Serial.println("[WM8960] codec non détecté sur I2C");
        return false;
    }

    _codec.enableVREF();
    _codec.enableVMID();

    /* Sortie casque/ligne */
    _codec.enableHeadphones();
    _codec.enableOUT3MIX();
    _codec.setHeadphoneVolumeDB(0.0f);

    /* Entrée micro/ligne (LINE IN gauche + droit) */
    _codec.enableLMIC();
    _codec.enableRMIC();
    _codec.connectLMN1();
    _codec.connectRMN1();
    _codec.setLINVOLDB(0.0f);
    _codec.setRINVOLDB(0.0f);

    /* I2S 16-bit, maître sur le codec */
    _codec.enablePLL();
    _codec.setPLLPRESCALE(WM8960_PLLPRESCALE_DIV_2);
    _codec.setSMD(WM8960_PLL_MODE_FRACTIONAL);
    _codec.setCLKSEL(WM8960_CLKSEL_PLL);
    _codec.setSYSCLKDIV(WM8960_SYSCLK_DIV_BY_2);
    _codec.setBCLKDIV(4);
    _codec.setDCLKDIV(WM8960_DCLKDIV_16);
    _codec.setPLLN(7);
    _codec.setPLLK(0x86, 0xC2, 0x26);  /* 44100 Hz */
    _codec.setADCDIV(0);
    _codec.setDACDIV(0);
    _codec.setWL(WM8960_WL_16BIT);

    _codec.enableAdcLeft();
    _codec.enableAdcRight();
    _codec.enableDacLeft();
    _codec.enableDacRight();
    _codec.enableDacMute(false);

    _codec.enableLoopBack(false);
    _codec.disableDacMute();

    _codec.enableLOMix();
    _codec.enableROMix();

    Serial.println("[WM8960] codec initialisé (44100 Hz / 16-bit / stéréo)");
    return true;
}

bool WM8960Driver::initI2S() {
    const i2s_config_t cfg = {
        .mode             = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
        .sample_rate      = kSampleRate,
        .bits_per_sample  = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format   = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count    = 8,
        .dma_buf_len      = 512,
        .use_apll         = true,
        .tx_desc_auto_clear = true,
    };

    const i2s_pin_config_t pins = {
        .bck_io_num   = kI2S_BCLK,
        .ws_io_num    = kI2S_WS,
        .data_out_num = kI2S_DOUT,
        .data_in_num  = kI2S_DIN,
    };

    if (i2s_driver_install(kI2SPort, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println("[WM8960] erreur installation driver I2S");
        return false;
    }
    if (i2s_set_pin(kI2SPort, &pins) != ESP_OK) {
        Serial.println("[WM8960] erreur configuration broches I2S");
        return false;
    }
    i2s_zero_dma_buffer(kI2SPort);
    Serial.println("[WM8960] I2S initialisé");
    return true;
}

bool WM8960Driver::write(const int16_t* buf, size_t samples, size_t* written) {
    size_t bytes;
    esp_err_t err = i2s_write(kI2SPort, buf, samples * sizeof(int16_t), &bytes, portMAX_DELAY);
    if (written) *written = bytes / sizeof(int16_t);
    return err == ESP_OK;
}

bool WM8960Driver::read(int16_t* buf, size_t samples, size_t* rd) {
    size_t bytes;
    esp_err_t err = i2s_read(kI2SPort, buf, samples * sizeof(int16_t), &bytes, portMAX_DELAY);
    if (rd) *rd = bytes / sizeof(int16_t);
    return err == ESP_OK;
}

void WM8960Driver::setOutputVolume(uint8_t vol) {
    /* vol 0–63 → DB mapping WM8960 */
    _codec.setHeadphoneVolumeDB((float)vol - 40.0f);
}

void WM8960Driver::setInputGain(uint8_t gain) {
    float db = (float)gain * 0.75f - 17.25f;  /* 0–63 → -17dB..+30dB */
    _codec.setLINVOLDB(db);
    _codec.setRINVOLDB(db);
}
