#include "SampleBank.h"

bool SampleBank::begin() {
    SD_MMC.setPins(kSD_CLK, kSD_CMD, kSD_D0, kSD_D1, kSD_D2, kSD_D3);
    if (!SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_52M)) {
        Serial.println("[SampleBank] SD_MMC init échoué");
        return false;
    }
    SD_MMC.mkdir("/samples");
    SD_MMC.mkdir("/loops");
    Serial.printf("[SampleBank] SD prête — %llu MB\n",
        SD_MMC.cardSize() / (1024 * 1024));
    _ready = true;
    return true;
}

String SampleBank::listBanksJson() {
    String out = "[";
    File root = SD_MMC.open("/samples");
    if (!root) return "[]";
    File entry = root.openNextFile();
    bool first = true;
    while (entry) {
        if (entry.isDirectory()) {
            if (!first) out += ",";
            out += "\"";
            out += entry.name();
            out += "\"";
            first = false;
        }
        entry = root.openNextFile();
    }
    out += "]";
    return out;
}

String SampleBank::listSamplesJson(const char* bank) {
    char path[48];
    snprintf(path, sizeof(path), "/samples/%s", bank);
    File dir = SD_MMC.open(path);
    if (!dir) return "[]";
    String out = "[";
    File entry = dir.openNextFile();
    bool first = true;
    while (entry) {
        String name = entry.name();
        if (name.endsWith(".wav")) {
            if (!first) out += ",";
            /* extrait la note du nom de fichier (ex: "60.wav" → 60) */
            out += name.substring(0, name.lastIndexOf('.'));
            first = false;
        }
        entry = dir.openNextFile();
    }
    out += "]";
    return out;
}

bool SampleBank::saveSample(const char* bank, uint8_t note, const uint8_t* data, size_t len) {
    char dirPath[40];
    snprintf(dirPath, sizeof(dirPath), "/samples/%s", bank);
    SD_MMC.mkdir(dirPath);

    char filePath[56];
    samplePath(filePath, sizeof(filePath), bank, note);
    File f = SD_MMC.open(filePath, FILE_WRITE);
    if (!f) return false;
    f.write(data, len);
    f.close();
    return true;
}

bool SampleBank::deleteSample(const char* bank, uint8_t note) {
    char path[56];
    samplePath(path, sizeof(path), bank, note);
    return SD_MMC.remove(path);
}

void SampleBank::samplePath(char* out, size_t sz, const char* bank, uint8_t note) {
    snprintf(out, sz, "/samples/%s/%u.wav", bank, note);
}
