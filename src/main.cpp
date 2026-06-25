/*
 * nidmi-loopsampler : sampler polyphonique + loopstation audio
 *
 * Hardware : XIAO ESP32-S3 + WM8960 (I2S) + micro SD (SDMMC 4-bit)
 * Transport MIDI : RTP-MIDI (WiFi AP) via nidmi-core
 * Contrôle looper : MIDI CC ou API HTTP
 *
 * Core 0 : tâche audio (WavPlayer + Looper)
 * Core 1 : WiFi / HTTP / RTP-MIDI (Arduino loop)
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <nidmi_core/NetBootstrap.h>
#include <nidmi_core/RtpMidiService.h>
#include <nidmi_core/Version.h>

#include "audio/WM8960Driver.h"
#include "audio/WavPlayer.h"
#include "audio/Looper.h"
#include "storage/SampleBank.h"
#include "api/AudioAPI.h"

static constexpr const char* kApSsid   = "nidmi-loopsampler";
static constexpr const char* kApPass   = "nidmipass";
static constexpr const char* kMdnsHost = "nidmilooper";
static constexpr const char* kRtpName  = "nidmi-loopsampler";

/* MIDI CC pour contrôle looper (canal 1) */
static constexpr uint8_t kCC_Record = 64;  /* pédale sustain → record/stop */
static constexpr uint8_t kCC_Play   = 65;
static constexpr uint8_t kCC_Clear  = 66;

static WM8960Driver      g_codec;
static WavPlayer         g_player(g_codec);
static Looper            g_looper(g_codec);
static SampleBank        g_bank;
static AsyncWebServer    g_server(80);
static nidmi::RtpMidiService g_rtp;
static AudioAPI          g_audioApi(g_server, g_player, g_looper, g_bank);

/* -------------------------------------------------------------------------
 * Tâche audio sur Core 0
 * ------------------------------------------------------------------------- */
static void audioTask(void*) {
    for (;;) {
        g_player.update();
        g_looper.update();
    }
}

/* -------------------------------------------------------------------------
 * Callbacks RTP-MIDI
 * ------------------------------------------------------------------------- */
static void onNoteOn(byte ch, byte note, byte vel) {
    g_player.noteOn(ch, note, vel);
}

static void onNoteOff(byte ch, byte note, byte vel) {
    g_player.noteOff(ch, note);
}

static void onControlChange(byte ch, byte cc, byte val) {
    if (val < 64) return;  /* only on (≥64) */
    switch (cc) {
        case kCC_Record: {
            LooperState s = g_looper.state();
            if (s == LooperState::Recording || s == LooperState::Overdub)
                g_looper.stopRecord();
            else
                g_looper.startRecord();
            break;
        }
        case kCC_Play:  g_looper.play();     break;
        case kCC_Clear: g_looper.clearAll(); break;
    }
}

/* -------------------------------------------------------------------------
 * Setup / Loop
 * ------------------------------------------------------------------------- */
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.printf("[nidmi-loopsampler] nidmi-core %s\n", nidmi::version());

    /* SD */
    if (!g_bank.begin()) {
        Serial.println("[setup] ERREUR: SD — vérifier câblage et carte");
    }

    /* Codec I2S */
    if (!g_codec.begin()) {
        Serial.println("[setup] ERREUR: WM8960 — vérifier câblage I2C/I2S");
    }

    /* Sampler + looper */
    g_player.begin();
    g_player.setBank("/samples/default");
    g_looper.begin();

    /* WiFi AP */
    if (!nidmi::netBeginSoftAp(kApSsid, kApPass, kMdnsHost)) {
        Serial.println("[setup] ERREUR: WiFi AP");
    }
    Serial.printf("[setup] AP IP: %s\n", WiFi.softAPIP().toString().c_str());

    /* RTP-MIDI */
    if (g_rtp.begin(kRtpName)) {
        g_rtp.setHandleNoteOn(onNoteOn);
        g_rtp.setHandleNoteOff(onNoteOff);
        g_rtp.setHandleControlChange(onControlChange);
        Serial.printf("[setup] RTP-MIDI prêt (port %u)\n", g_rtp.port());
    }

    /* API HTTP */
    g_audioApi.registerRoutes();
    g_server.begin();
    Serial.println("[setup] WebServer démarré");

    /* Tâche audio sur Core 0, priorité haute */
    xTaskCreatePinnedToCore(audioTask, "audio", 8192, nullptr, 10, nullptr, 0);

    Serial.println("[setup] prêt");
}

void loop() {
    g_rtp.update();
    delay(1);
}
