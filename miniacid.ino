#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include "dsp_engine.h"
#include "cardputer_display.h"
#include <cstdarg>
#include <cstdio>
#include "miniacid_display.h"
#include "miniacid_encoder8.h"
#include "scene_storage_cardputer.h"

static constexpr IGfxColor CP_BLACK = IGfxColor::Black();

CardputerDisplay g_display;
MiniAcidDisplay* g_miniDisplay = nullptr;
SceneStorageCardputer g_sceneStorage;

int16_t g_audioBuffer[AUDIO_BUFFER_SAMPLES];

TaskHandle_t g_audioTaskHandle = nullptr;

MiniAcid g_miniAcid(SAMPLE_RATE, &g_sceneStorage);
Encoder8Miniacid g_encoder8(g_miniAcid);

void audioTask(void *param) {
  while (true) {
    if (!g_miniAcid.isPlaying()) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    while (M5Cardputer.Speaker.isPlaying()) {
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    g_miniAcid.generateAudioBuffer(g_audioBuffer, AUDIO_BUFFER_SAMPLES);

    M5Cardputer.Speaker.playRaw(g_audioBuffer, AUDIO_BUFFER_SAMPLES,
                                SAMPLE_RATE, false);
  }
}


void drawUI() {
  if (g_miniDisplay) g_miniDisplay->update();
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);

  Serial.begin(115200);

  g_display.setRotation(1);
  g_display.begin();
  g_display.clear(CP_BLACK);

  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(200); // 0-255

  g_miniAcid.init();
  g_miniDisplay = new MiniAcidDisplay(g_display, g_miniAcid);

  xTaskCreatePinnedToCore(audioTask, "AudioTask",
                          4096, // stack
                          nullptr,
                          3, // priority
                          &g_audioTaskHandle,
                          1 // core
  );

  g_encoder8.initialize();

  drawUI();
}

void loop() {
  M5Cardputer.update();

  g_encoder8.update();

  if (M5Cardputer.BtnA.wasClicked()) {
    if (g_miniAcid.isPlaying()) {
      g_miniAcid.stop();
    } else {
      g_miniAcid.start();
    }
    drawUI();
  }

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    if (g_miniDisplay) g_miniDisplay->dismissSplash();
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();

    auto handleWithFallback = [&](UIEvent evt) {
      evt.event_type = MINIACID_KEY_DOWN;
      bool handled = g_miniDisplay ? g_miniDisplay->handleEvent(evt) : false;
      if (handled) return;

      char c = evt.key;
      if (c == '\n' || c == '\r') {
        if (g_miniDisplay) g_miniDisplay->dismissSplash();
        drawUI();
      } else if (c == '[') {
        if (g_miniDisplay) g_miniDisplay->previousPage();
        drawUI();
      } else if (c == ']') {
        if (g_miniDisplay) g_miniDisplay->nextPage();
        drawUI();
      } else if (c == 'i' || c == 'I') {
        g_miniAcid.randomize303Pattern(0);
        drawUI();
      } else if (c == 'o' || c == 'O') {
        g_miniAcid.randomize303Pattern(1);
        drawUI();
      } else if (c == 'p' || c == 'P') {
        g_miniAcid.randomizeDrumPattern();
        drawUI();
      } else if (c == '1') {
        g_miniAcid.toggleMute303(0);
        drawUI();
      } else if (c == '2') {
        g_miniAcid.toggleMute303(1);
        drawUI();
      } else if (c == '3') {
        g_miniAcid.toggleMuteKick();
        drawUI();
      } else if (c == '4') {
        g_miniAcid.toggleMuteSnare();
        drawUI();
      } else if (c == '5') {
        g_miniAcid.toggleMuteHat();
        drawUI();
      } else if (c == '6') {
        g_miniAcid.toggleMuteOpenHat();
        drawUI();
      } else if (c == '7') {
        g_miniAcid.toggleMuteMidTom();
        drawUI();
      } else if (c == '8') {
        g_miniAcid.toggleMuteHighTom();
        drawUI();
      } else if (c == '9') {
        g_miniAcid.toggleMuteRim();
        drawUI();
      } else if (c == '0') {
        g_miniAcid.toggleMuteClap();
        drawUI();
      } else if (c == 'k' || c == 'K') {
        g_miniAcid.setBpm(g_miniAcid.bpm() - 5.0f);
        drawUI();
      } else if (c == 'l' || c == 'L') {
        g_miniAcid.setBpm(g_miniAcid.bpm() + 5.0f);
        drawUI();
      } else if (c == ' ') {
        if (g_miniAcid.isPlaying()) {
          g_miniAcid.stop();
        } else {
          g_miniAcid.start();
        }
        drawUI();
      }
    };

    for (auto hid : ks.hid_keys) {
      UIEvent evt{};
      evt.alt = ks.alt;
      bool shouldSend = false;
      if (hid == 0x33) {
        evt.scancode = MINIACID_UP;
        shouldSend = true;
      } else if (hid == 0x37) {
        evt.scancode = MINIACID_DOWN;
        shouldSend = true;
      } else if (hid == 0x36) {
        evt.scancode = MINIACID_LEFT;
        shouldSend = true;
      } else if (hid == 0x38) {
        evt.scancode = MINIACID_RIGHT;
        shouldSend = true;
      } else if (hid == 0x28 || hid == 0x58) { // enter
        evt.key = '\n';
        shouldSend = true;
      } else if (hid == KEY_BACKSPACE) {
        evt.key = '\b';
        shouldSend = true;
      }
      if (shouldSend) {
        handleWithFallback(evt);
      }
    }

    for (auto inputChar : ks.word) {
      UIEvent evt{};
      evt.alt = ks.alt;
      evt.key = inputChar;
      handleWithFallback(evt);
    }
  }

  static unsigned long lastUIUpdate = 0;
  if (millis() - lastUIUpdate > 80) {
    lastUIUpdate = millis();
    if (g_miniDisplay) g_miniDisplay->update();
  }

  delay(5);
}
