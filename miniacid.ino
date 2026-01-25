#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include "src/dsp/miniacid_engine.h"
#include "cardputer_display.h"
#include <cstdarg>
#include <cstdio>
#include "src/ui/miniacid_display.h"
#include "src/audio/cardputer_audio_recorder.h"
#include "miniacid_encoder8.h"
#include "scene_storage_cardputer.h"

static constexpr IGfxColor CP_BLACK = IGfxColor::Black();

CardputerDisplay g_display;
MiniAcidDisplay* g_miniDisplay = nullptr;
SceneStorageCardputer g_sceneStorage;
CardputerAudioRecorder* g_audioRecorder = nullptr;

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

    // Write to recorder if recording
    if (g_audioRecorder) {
      g_audioRecorder->writeSamples(g_audioBuffer, AUDIO_BUFFER_SAMPLES);
    }

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
#if defined(MINIACID_SCENE_DEBUG)
  Serial.printf("sizeof(AutomationLane)=%u sizeof(SynthPattern)=%u sizeof(Scene)=%u\n",
                static_cast<unsigned>(sizeof(AutomationLane)),
                static_cast<unsigned>(sizeof(SynthPattern)),
                static_cast<unsigned>(sizeof(Scene)));
  Serial.printf("free heap at boot: %u\n", static_cast<unsigned>(ESP.getFreeHeap()));
#endif

  g_display.setRotation(1);
  g_display.begin();
  g_display.clear(CP_BLACK);

  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(200); // 0-255

  g_miniAcid.init();
#if defined(MINIACID_SCENE_DEBUG)
  Serial.printf("free heap after MiniAcid init: %u\n", static_cast<unsigned>(ESP.getFreeHeap()));
#endif
  g_miniDisplay = new MiniAcidDisplay(g_display, g_miniAcid);
  
  // Set audio guard to protect audio task from concurrent access
  g_miniDisplay->setAudioGuard([](const std::function<void()>& fn) {
    // On Cardputer, we need to suspend/resume the audio task or use a mutex
    // For now, just call the function directly since FreeRTOS handles this
    fn();
  });
  
  // Initialize audio recorder (done after other initialization to avoid boot issues)
  g_audioRecorder = new CardputerAudioRecorder();
  g_miniDisplay->setAudioRecorder(g_audioRecorder);

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

  static constexpr unsigned long KEY_REPEAT_DELAY_MS = 350;
  static constexpr unsigned long KEY_REPEAT_INTERVAL_MS = 80;
  static Keyboard_Class::KeysState lastKeysState{};
  static bool hasLastKeys = false;
  static unsigned long nextRepeatAt = 0;

  auto handleWithFallback = [&](UIEvent evt) {
    evt.event_type = MINIACID_KEY_DOWN;
    bool handled = g_miniDisplay ? g_miniDisplay->handleEvent(evt) : false;
    if (handled) drawUI();
  };

  auto applyCtrlLetter = [](const Keyboard_Class::KeysState& ks, uint8_t hid, UIEvent& evt) -> bool {
    constexpr uint8_t HID_KEY_A = 0x04;
    constexpr uint8_t HID_KEY_Z = 0x1D;
    if (!ks.ctrl || hid < HID_KEY_A || hid > HID_KEY_Z) return false;
    evt.key = static_cast<char>('a' + (hid - HID_KEY_A));
    return true;
  };
  auto applyAltLetter = [](const Keyboard_Class::KeysState& ks, uint8_t hid, UIEvent& evt) -> bool {
    constexpr uint8_t HID_KEY_A = 0x04;
    constexpr uint8_t HID_KEY_Z = 0x1D;
    if (!ks.alt || hid < HID_KEY_A || hid > HID_KEY_Z) return false;
    evt.key = static_cast<char>('a' + (hid - HID_KEY_A));
    return true;
  };

  auto processKeys = [&](const Keyboard_Class::KeysState& ks) {
    for (auto hid : ks.hid_keys) {
      UIEvent evt{};
      evt.alt = ks.alt;
      evt.ctrl = ks.ctrl;
      evt.shift = ks.shift;
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
      } else if (hid == KEY_TAB) {
        evt.key = '\t';
        shouldSend = true;
      } else if (applyCtrlLetter(ks, hid, evt)) {
        shouldSend = true;
      } else if (applyAltLetter(ks, hid, evt)) {
        shouldSend = true;
      }
      if (shouldSend) {
        handleWithFallback(evt);
      }
    }

    for (auto inputChar : ks.word) {
      UIEvent evt{};
      evt.alt = ks.alt;
      evt.ctrl = ks.ctrl;
      evt.shift = ks.shift;
      evt.key = inputChar;
      if (evt.key == '`' || evt.key == '~') { // escape
        evt.scancode = MINIACID_ESCAPE;
      }
      handleWithFallback(evt);
    }
  };

  bool keyChanged = M5Cardputer.Keyboard.isChange();
  bool keyPressed = M5Cardputer.Keyboard.isPressed();
  if (keyChanged && keyPressed) {
    if (g_miniDisplay) g_miniDisplay->dismissSplash();
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    processKeys(ks);
    lastKeysState = ks;
    hasLastKeys = true;
    nextRepeatAt = millis() + KEY_REPEAT_DELAY_MS;
  } else if (!keyPressed) {
    hasLastKeys = false;
  } else if (hasLastKeys && millis() >= nextRepeatAt) {
    if (g_miniDisplay) g_miniDisplay->dismissSplash();
    processKeys(lastKeysState);
    nextRepeatAt = millis() + KEY_REPEAT_INTERVAL_MS;
  }

  static unsigned long lastUIUpdate = 0;
  if (millis() - lastUIUpdate > 80) {
    lastUIUpdate = millis();
    if (g_miniDisplay) g_miniDisplay->update();
  }

  delay(5);
}
