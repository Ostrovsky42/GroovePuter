#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "src/dsp/miniacid_engine.h"
#include "cardputer_display.h"
#include <cstdarg>
#include <cstdio>
#include "src/ui/miniacid_display.h"
#include "src/audio/cardputer_audio_recorder.h"
#include "miniacid_encoder8.h"
#include "scene_storage_cardputer.h"
#include "src/ui/led_manager.h"
#include "src/audio/audio_diagnostics.h"

static constexpr IGfxColor CP_BLACK = IGfxColor::Black();

CardputerDisplay g_display;
MiniAcidDisplay* g_miniDisplay = nullptr;
SceneStorageCardputer g_sceneStorage;
CardputerAudioRecorder* g_audioRecorder = nullptr;
#include "src/sampler/ram_sample_store.h"
#include "src/audio/audio_out_i2s.h"
RamSampleStore g_sampleStore;

static AudioOutI2S g_audioOut;
static int16_t g_audioBuffer[kBlockFrames];

TaskHandle_t g_audioTaskHandle = nullptr;

// Static engine instance to avoid heap fragmentation
static MiniAcid g_miniAcidInstance(SAMPLE_RATE, &g_sceneStorage);
MiniAcid* g_miniAcid = nullptr;
Encoder8Miniacid* g_encoder8 = nullptr;

void audioTask(void *param) {
  // Initialize I2S audio output
  if (!g_audioOut.begin(kSampleRate, kBlockFrames)) {
    Serial.println("[FATAL] I2S audio init failed");
    while (true) { delay(1000); }
  }
  
  while (true) {
    if (!g_miniAcid || !g_miniAcid->isPlaying()) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    // Generate audio
    uint32_t start = micros();
    g_miniAcid->generateAudioBuffer(g_audioBuffer, kBlockFrames);
    uint32_t dsp_time = micros() - start;
    
    // Check for underruns
    constexpr uint32_t max_allowed = (1000000UL * kBlockFrames) / kSampleRate;
    if (dsp_time > max_allowed) {
      Serial.printf("[UNDERRUN] dsp:%uus max:%uus\n", dsp_time, max_allowed);
    }

    // Write to recorder if recording
    if (g_audioRecorder) {
      g_audioRecorder->writeSamples(g_audioBuffer, kBlockFrames);
    }
    
    // Flush diagnostics periodically
    if (AudioDiagnostics::instance().isEnabled()) {
      AudioDiagnostics::instance().flushIfReady(millis());
    }

    // Write to I2S (blocks until DMA accepts the buffer)
    g_audioOut.writeMono16(g_audioBuffer, kBlockFrames);
  }
}


void drawUI() {
  if (g_miniDisplay) g_miniDisplay->update();
}

static void logHeapCaps(const char* tag) {
  auto freeInt  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  auto largInt  = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  auto free8    = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  auto larg8    = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  Serial.printf("[%s] freeInt=%u largInt=%u free8=%u larg8=%u\n",
                tag, (unsigned)freeInt, (unsigned)largInt,
                (unsigned)free8, (unsigned)larg8);
}

void setup() {
  Serial.begin(115200);
  // При желании сразу включить
  AudioDiagnostics::instance().enable(true);
  delay(500);
  Serial.println("\n\n!! BOOTING !!");
  logHeapCaps("boot-start");
  
  auto cfg = M5.config();
  
  // Disable M5 audio to verify our custom I2S driver takes precedence
  cfg.internal_spk = false;
  cfg.external_spk = false;
  cfg.internal_mic = false;
  
  M5Cardputer.begin(cfg);
  
  // Seed random number generator with hardware RNG
  srand(esp_random());
  
  logHeapCaps("after-m5-begin");
  
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.println("\n\n=== MiniAcid STARTUP DIAGNOSTICS ===");
  Serial.printf("Reset Reason: %d\n", (int)reason);

  Serial.println("Creating Display...");
  logHeapCaps("before-display");
  g_display.setRotation(1);
  g_display.begin();
  
  Serial.println("Clearing Display...");
  g_display.clear(CP_BLACK);
  g_display.setTextColor(IGfxColor::White());
  logHeapCaps("after-display");
  
  
  // Speaker initialization removed - using direct I2S in audioTask
  // Serial.println("Init Speaker...");
  // M5Cardputer.Speaker.begin();
  // M5Cardputer.Speaker.setVolume(160);
  // Serial.println("Speaker OK");
  // logHeapCaps("after-speaker");
    
  auto screenLog = [&](const char* msg) {
    static int logY = 0;
    Serial.println(msg);
    g_display.drawText(0, logY, msg);
    logY += 10;
    if (logY > 120) { g_display.clear(CP_BLACK); logY = 0; }
  };

  screenLog("1. M5 Hardware OK");
  char buf[64];
  snprintf(buf, sizeof(buf), "2. PSRAM: %d KB free", (int)(ESP.getFreePsram() / 1024));
  screenLog(buf);

  // Points to static instance
  g_miniAcid = &g_miniAcidInstance;
  
  screenLog("4. Engine Static OK");
  
  // Previously we tried PSRAM allocation here, but now we use BSS
  // to avoid 'largest free block' issues on DRAM-only usage.

  screenLog("5. Creating Encoder8");
  g_encoder8 = new Encoder8Miniacid(*g_miniAcid);

  screenLog("6. Engine Init...");
  // Link sample store before init
  g_miniAcid->sampleStore = &g_sampleStore;
  g_miniAcid->init();
  
  // Scan samples from SD card (SD initialized by engine->init->sceneStorage)
  screenLog("6b. Scan /sd/samples...");
  g_miniAcid->sampleIndex.scanDirectory("/sd/samples");
  
  if (g_miniAcid->sampleIndex.getFiles().empty()) {
     // Fallback: try different path if /sd/samples is not right
     g_miniAcid->sampleIndex.scanDirectory("/samples");
  }

  for (const auto& file : g_miniAcid->sampleIndex.getFiles()) {
      Serial.printf("Found sample: %s (id=%u)\n", file.filename.c_str(), file.id.value);
      // Register with RamSampleStore. 
      // Note: "registerFile" just stores the path for lazy loading.
      g_sampleStore.registerFile(file.id, file.fullPath);
  }

  
  screenLog("7. UI Setup...");
  g_miniDisplay = new MiniAcidDisplay(g_display, *g_miniAcid);
  
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

  g_encoder8->initialize();
  LedManager::instance().init();

  drawUI();
}


void loop() {
  M5Cardputer.update();
  LedManager::instance().update();

  if (g_encoder8) g_encoder8->update();

  if (M5Cardputer.BtnA.wasClicked()) {
    if (g_miniAcid->isPlaying()) {
      g_miniAcid->stop();
    } else {
      g_miniAcid->start();
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
    if (handled) {
      drawUI();
      return;
    }

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
      g_miniAcid->randomize303Pattern(0);
      drawUI();
    } else if (c == 'o' || c == 'O') {
      g_miniAcid->randomize303Pattern(1);
      drawUI();
    } else if (c == 'p' || c == 'P') {
      g_miniAcid->randomizeDrumPattern();
      drawUI();
    } else if (c == '1') {
      g_miniAcid->toggleMute303(0);
      drawUI();
    } else if (c == '2') {
      g_miniAcid->toggleMute303(1);
      drawUI();
    } else if (c == '3') {
      g_miniAcid->toggleMuteKick();
      drawUI();
    } else if (c == '4') {
      g_miniAcid->toggleMuteSnare();
      drawUI();
    } else if (c == '5') {
      g_miniAcid->toggleMuteHat();
      drawUI();
    } else if (c == '6') {
      g_miniAcid->toggleMuteOpenHat();
      drawUI();
    } else if (c == '7') {
      g_miniAcid->toggleMuteMidTom();
      drawUI();
    } else if (c == '8') {
      g_miniAcid->toggleMuteHighTom();
      drawUI();
    } else if (c == '9') {
      g_miniAcid->toggleMuteRim();
      drawUI();
    } else if (c == '0') {
      g_miniAcid->toggleMuteClap();
      drawUI();
    } else if (c == 'k' || c == 'K') {
      g_miniAcid->setBpm(g_miniAcid->bpm() - 5.0f);
      drawUI();
    } else if (c == 'l' || c == 'L') {
      g_miniAcid->setBpm(g_miniAcid->bpm() + 5.0f);
      drawUI();
    } else if (c == '-' || c == '_') {
      // Volume down (larger step: 3 * 1/64 ≈ 5%)
      g_miniAcid->adjustParameter(MiniAcidParamId::MainVolume, -3);
      drawUI();
    } else if (c == '=' || c == '+') {
      // Volume up (larger step: 3 * 1/64 ≈ 5%)
      g_miniAcid->adjustParameter(MiniAcidParamId::MainVolume, 3);
      drawUI();

    } else if (c == ';') {
      g_miniAcid->toggleAudioDiag();
      Serial.println("[UI] Toggled Audio Diagnostics");
      drawUI();
    } else if (c == '\'') {
      bool newState = !g_miniAcid->isTestToneEnabled();
      g_miniAcid->setTestTone(newState);
      Serial.printf("[UI] Test Tone: %s\n", newState ? "ON" : "OFF");
      drawUI();
    } else if (c == ' ') {
      if (g_miniAcid->isPlaying()) {
        g_miniAcid->stop();
      } else {
        g_miniAcid->start();
      }
      drawUI();
    }
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
