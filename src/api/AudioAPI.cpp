#include "AudioAPI.h"

static const char* looperStateName(LooperState s) {
    switch (s) {
        case LooperState::Idle:      return "idle";
        case LooperState::Recording: return "recording";
        case LooperState::Overdub:   return "overdub";
        case LooperState::Playing:   return "playing";
    }
    return "unknown";
}

void AudioAPI::registerRoutes() {
    _server.on("/api/banks", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleListBanks(req);
    });

    _server.on("/api/banks/samples", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleListSamples(req);
    });

    _server.on("/api/banks/upload", HTTP_POST,
        [](AsyncWebServerRequest* req) { req->send(200); },
        [this](AsyncWebServerRequest* req, const String& fn, size_t idx,
               uint8_t* data, size_t len, bool final) {
            handleUploadSample(req, fn, idx, data, len, final);
        }
    );

    _server.on("/api/looper/record", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleLooperRecord(req);
    });
    _server.on("/api/looper/play", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleLooperPlay(req);
    });
    _server.on("/api/looper/stop", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleLooperStop(req);
    });
    _server.on("/api/looper/clear", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleLooperClear(req);
    });
    _server.on("/api/looper/status", HTTP_GET, [this](AsyncWebServerRequest* req) {
        handleLooperStatus(req);
    });
    _server.on("/api/player/bank", HTTP_POST, [this](AsyncWebServerRequest* req) {
        handleSetBank(req);
    });
}

void AudioAPI::handleListBanks(AsyncWebServerRequest* req) {
    req->send(200, "application/json", _bank.listBanksJson());
}

void AudioAPI::handleListSamples(AsyncWebServerRequest* req) {
    if (!req->hasParam("bank")) { req->send(400, "text/plain", "missing bank"); return; }
    req->send(200, "application/json",
        _bank.listSamplesJson(req->getParam("bank")->value().c_str()));
}

void AudioAPI::handleUploadSample(AsyncWebServerRequest* req, const String& filename,
                                   size_t index, uint8_t* data, size_t len, bool final) {
    if (!req->hasParam("bank", true) || !req->hasParam("note", true)) return;
    uint8_t note = req->getParam("note", true)->value().toInt();
    const char* bank = req->getParam("bank", true)->value().c_str();

    /* Sauve uniquement à la fin (fichiers courts — samples, pas stems) */
    if (final && index == 0) {
        _bank.saveSample(bank, note, data, len);
    }
}

void AudioAPI::handleLooperRecord(AsyncWebServerRequest* req) {
    LooperState s = _looper.state();
    if (s == LooperState::Recording || s == LooperState::Overdub) {
        _looper.stopRecord();
    } else {
        _looper.startRecord();
    }
    req->send(200, "application/json", "{\"ok\":true}");
}

void AudioAPI::handleLooperPlay(AsyncWebServerRequest* req) {
    _looper.play();
    req->send(200, "application/json", "{\"ok\":true}");
}

void AudioAPI::handleLooperStop(AsyncWebServerRequest* req) {
    _looper.stop();
    req->send(200, "application/json", "{\"ok\":true}");
}

void AudioAPI::handleLooperClear(AsyncWebServerRequest* req) {
    _looper.clearAll();
    req->send(200, "application/json", "{\"ok\":true}");
}

void AudioAPI::handleLooperStatus(AsyncWebServerRequest* req) {
    char buf[80];
    snprintf(buf, sizeof(buf), "{\"state\":\"%s\",\"layers\":%u}",
        looperStateName(_looper.state()), _looper.layerCount());
    req->send(200, "application/json", buf);
}

void AudioAPI::handleSetBank(AsyncWebServerRequest* req) {
    if (!req->hasParam("bank", true)) { req->send(400); return; }
    _player.setBank(req->getParam("bank", true)->value().c_str());
    req->send(200, "application/json", "{\"ok\":true}");
}
