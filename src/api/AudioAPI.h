#pragma once
#include <ESPAsyncWebServer.h>
#include "../audio/WavPlayer.h"
#include "../audio/Looper.h"
#include "../storage/SampleBank.h"

class AudioAPI {
public:
    AudioAPI(AsyncWebServer& server, WavPlayer& player, Looper& looper, SampleBank& bank)
        : _server(server), _player(player), _looper(looper), _bank(bank) {}

    void registerRoutes();

private:
    AsyncWebServer& _server;
    WavPlayer&      _player;
    Looper&         _looper;
    SampleBank&     _bank;

    /* GET /api/banks → liste des banques JSON */
    void handleListBanks(AsyncWebServerRequest* req);

    /* GET /api/banks/:bank/samples → notes disponibles */
    void handleListSamples(AsyncWebServerRequest* req);

    /* POST /api/banks/:bank/samples/:note → upload WAV (multipart) */
    void handleUploadSample(AsyncWebServerRequest* req, const String& filename,
                            size_t index, uint8_t* data, size_t len, bool final);

    /* POST /api/looper/record → startRecord / stopRecord */
    void handleLooperRecord(AsyncWebServerRequest* req);

    /* POST /api/looper/play */
    void handleLooperPlay(AsyncWebServerRequest* req);

    /* POST /api/looper/stop */
    void handleLooperStop(AsyncWebServerRequest* req);

    /* POST /api/looper/clear */
    void handleLooperClear(AsyncWebServerRequest* req);

    /* GET /api/looper/status → JSON état + nb couches */
    void handleLooperStatus(AsyncWebServerRequest* req);

    /* POST /api/player/bank → change la banque active */
    void handleSetBank(AsyncWebServerRequest* req);
};
