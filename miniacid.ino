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
#include "src/ui/key_normalize.h"
#include <new>

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
static MiniAcid g_miniAcidInstance(kSampleRate, &g_sceneStorage);
MiniAcid* volatile g_miniAcid = nullptr;
Encoder8Miniacid* g_encoder8 = nullptr;

#if defined(ESP32) || defined(ESP_PLATFORM)
RTC_DATA_ATTR static uint32_t g_bootStage = 0;
#else
static uint32_t g_bootStage = 0;
#endif

static void markBootStage(uint32_t stage, const char* msg = nullptr) {
  g_bootStage = stage;
  if (msg) {
    Serial.printf("[BOOT-STAGE] %u %s\n", (unsigned)stage, msg);
  } else {
    Serial.printf("[BOOT-STAGE] %u\n", (unsigned)stage);
  }
}

void audioTask(void *param) {
  Serial.println("AudioTask: Starting begin()...");
  // Initialize I2S audio output
  if (!g_audioOut.begin(kSampleRate, kBlockFrames)) {
    Serial.println("[FATAL] I2S audio init failed");
    while (true) { delay(1000); }
  }
  Serial.println("AudioTask: Loop start");
  
  while (true) {
    // Serial.println("A1"); Serial.flush();
    // Generate audio
    uint32_t now = micros();
    uint32_t start = now;
    static uint32_t warmupBlocks = 32; // ~90ms at 44.1kHz/128 for hardware stability
    if (warmupBlocks > 0) {
      std::fill(g_audioBuffer, g_audioBuffer + kBlockFrames, 0);
      warmupBlocks--;
      if (warmupBlocks == 0) {
        Serial.println("AudioTask: Warmup complete, audio active");
      }
    } else if (g_miniAcid) {
      g_miniAcid->generateAudioBuffer(g_audioBuffer, kBlockFrames);
    } else {
      std::fill(g_audioBuffer, g_audioBuffer + kBlockFrames, 0);
    }
    uint32_t dsp_time = micros() - start;
    
    // Update performance stats with lock-free snapshot
    constexpr uint32_t ideal_period_us = (1000000UL * kBlockFrames) / kSampleRate;
    if (g_miniAcid) {
        auto& stats = g_miniAcid->perfStats;
        
        // Measure actual callback period
        uint32_t actual_period_us = ideal_period_us;  // default
        if (stats.lastCallbackMicros > 0) {
            actual_period_us = now - stats.lastCallbackMicros;
        }
        
        // Start snapshot write (seq becomes odd)
        stats.seq++;
        
        // Update peak
        static uint32_t peakBlockCounter = 0;
        static float peakAccum = 0;
        float currentPct = (float)dsp_time * 100.0f / (float)ideal_period_us;
        if (currentPct > peakAccum) peakAccum = currentPct;
        
        if (++peakBlockCounter >= 22 || stats.cpuAudioPeakPct == 0) {
            stats.cpuAudioPeakPct = peakAccum;
            peakAccum = 0;
            peakBlockCounter = 0;
        }

        // Core stats
        stats.audioUnderruns = (dsp_time > ideal_period_us) ? stats.audioUnderruns + 1 : stats.audioUnderruns;
        stats.cpuAudioPctIdeal = currentPct;
        stats.cpuAudioPctActual = (float)dsp_time * 100.0f / (float)actual_period_us;
        stats.dspTimeUs = dsp_time;
        stats.lastCallbackMicros = now;

        // End snapshot write (seq becomes even)
        stats.seq++;
        
        // Log underruns
        if (dsp_time > ideal_period_us) {
           // Serial.printf("[UNDERRUN] dsp:%uus ideal:%uus actual:%uus total:%u\n", dsp_time, ideal_period_us, actual_period_us, (unsigned)stats.audioUnderruns);
        }
    }

    // Write to recorder if recording
    if (g_audioRecorder) {
      // Serial.println("A4"); Serial.flush();
      g_audioRecorder->writeSamples(g_audioBuffer, kBlockFrames);
    }
    
    // Flush diagnostics periodically
    if (AudioDiagnostics::instance().isEnabled()) {
      // Serial.println("A5"); Serial.flush();
      AudioDiagnostics::instance().flushIfReady(millis());
    }

    // Write to I2S (blocks until DMA accepts the buffer)
    if (!g_audioOut.writeMono16(g_audioBuffer, kBlockFrames)) {
      // Serial.println("A6"); Serial.flush();
      // If write fails (usually because it was never initialized),
      // we MUST sleep to avoid starving lower priority tasks (like the UI/Keys!)
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    // Serial.println("A7"); Serial.flush();
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
  // ENABLE hardware amplifier (PA_EN = G21)
  pinMode(21, OUTPUT); digitalWrite(21, HIGH);
  pinMode(42, OUTPUT); digitalWrite(42, LOW);
  
  Serial.begin(115200);
  // Keep diagnostics off by default on hardware; detailed profiling can exceed
  // the real-time audio budget and cause underruns.
  AudioDiagnostics::instance().enable(false);
  delay(500);
  Serial.println("\n\n!! BOOTING !!");
  uint32_t prevBootStage = g_bootStage;
  Serial.printf("[BOOT] Previous stage retained: %u\n", (unsigned)prevBootStage);
  markBootStage(1, "setup-entry");
  auto cfg = M5.config();
  markBootStage(10, "before M5Cardputer.begin");
  M5Cardputer.begin(cfg);
  markBootStage(11, "after M5Cardputer.begin");
  
  // 1. Let M5Unified initialize the ES8311 codec over I2C.
  // This wakes up the codec, sets output volumes, etc.
  markBootStage(20, "before Speaker.begin");
  M5Cardputer.Speaker.setVolume(220); 
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(220);
  
  // 2. IMPORTANT: Release Port 0 immediately.
  // This uninstalls M5's I2S driver but keeps the ES8311 codec registers intact.
  // This avoids the "I2S driver conflict" and allows us to use Port 0.
  M5Cardputer.Speaker.end();
  markBootStage(21, "after Speaker.end");
  Serial.println("[Audio] ES8311 codec initialized, Speaker released Port 0");
  
  // Small delay to ensure the driver is fully uninstalled before our custom one starts
  delay(50);
  
  // Seed random number generator with hardware RNG
  srand(esp_random());
  
  logHeapCaps("after-m5-begin");
  
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.println("\n\n=== MiniAcid STARTUP DIAGNOSTICS ===");
  Serial.printf("Reset Reason: %d\n", (int)reason);

  Serial.println("Creating Display...");
  logHeapCaps("before-display");
  markBootStage(30, "before display.begin");
  g_display.setRotation(1);
  g_display.begin();
  markBootStage(31, "after display.begin");
  
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
  Serial.printf("[DEBUG] g_miniAcid addr: %p (instance: %p)\n", (void*)g_miniAcid, (void*)&g_miniAcidInstance);
  Serial.printf("[DEBUG] MiniAcid size: %u bytes\n", (unsigned)sizeof(MiniAcid));
  
  screenLog("4. Engine Static OK");
  
  // Previously we tried PSRAM allocation here, but now we use BSS
  // to avoid 'largest free block' issues on DRAM-only usage.

  screenLog("5. Creating Encoder8");
  markBootStage(40, "before Encoder8 alloc");
  g_encoder8 = new (std::nothrow) Encoder8Miniacid(*g_miniAcid);
  if (!g_encoder8) {
    Serial.println("[FATAL] Encoder8 allocation failed");
    markBootStage(900, "fatal-encoder8-alloc");
    while (true) { delay(1000); }
  }
  markBootStage(41, "after Encoder8 alloc");

  screenLog("6. Engine Init...");
  // Link sample store before init
  g_miniAcid->sampleStore = &g_sampleStore;
  markBootStage(50, "before MiniAcid::init");
  g_miniAcid->init();
  markBootStage(51, "after MiniAcid::init");
  
  // Scan samples from SD card (SD initialized by engine->init->sceneStorage)
  screenLog("6b. Scan /sd/samples...");
  markBootStage(60, "before sample scan");
  g_miniAcid->sampleIndex.scanDirectory("/sd/samples");
  
  if (g_miniAcid->sampleIndex.getFiles().empty()) {
     // Fallback: try different path if /sd/samples is not right
     g_miniAcid->sampleIndex.scanDirectory("/samples");
  }
  markBootStage(61, "after sample scan");

  for (const auto& file : g_miniAcid->sampleIndex.getFiles()) {
      Serial.printf("Found sample: %s (id=%u)\n", file.filename.c_str(), file.id.value);
      // Register with RamSampleStore. 
      // Note: "registerFile" just stores the path for lazy loading.
      g_sampleStore.registerFile(file.id, file.fullPath);
  }

  
  Serial.println("7a. UI Instance Created");
  markBootStage(70, "before MiniAcidDisplay alloc");
  g_miniDisplay = new (std::nothrow) MiniAcidDisplay(g_display, *g_miniAcid);
  if (!g_miniDisplay) {
    Serial.println("[FATAL] MiniAcidDisplay allocation failed");
    markBootStage(901, "fatal-display-alloc");
    while (true) { delay(1000); }
  }
  markBootStage(71, "after MiniAcidDisplay alloc");
  Serial.println("7b. UI setAudioGuard");
  
  // Set audio guard to protect audio task from concurrent access
  // Configure Audio Guard for synchronization
  AudioGuard guard;
  guard.lock = [](void*) {
      // In a real dual-core ESP32 setup, we could use a mutex here
      // For now, grooveputer uses a single-threaded DSP model with volatile flags
  };
  guard.unlock = [](void*) {};
  g_miniDisplay->setAudioGuard(guard);
  
  Serial.println("7c. UI setAudioRecorder");
  // Initialize audio recorder (done after other initialization to avoid boot issues)
  // DISABLING FOR CRASH DEBUGGING
  // g_audioRecorder = new CardputerAudioRecorder();
  // g_miniDisplay->setAudioRecorder(g_audioRecorder);

  Serial.println("8. Creating AudioTask...");
  markBootStage(80, "before AudioTask create");
  BaseType_t taskOk = xTaskCreatePinnedToCore(audioTask, "AudioTask",
                                              8192, // stack
                                              nullptr,
                                              3, // priority
                                              &g_audioTaskHandle,
                                              1 // core
  );
  if (taskOk != pdPASS) {
    Serial.println("[FATAL] AudioTask create failed");
    markBootStage(902, "fatal-audio-task");
    while (true) { delay(1000); }
  }
  markBootStage(81, "after AudioTask create");

  Serial.println("9. Final Init...");
  markBootStage(90, "before encoder init");
  g_encoder8->initialize();
  markBootStage(91, "after encoder init");
  markBootStage(92, "before led init");
  LedManager::instance().init();
  markBootStage(93, "after led init");

  Serial.println("10. First drawUI...");
  markBootStage(94, "before first drawUI");
  drawUI();
  markBootStage(95, "after first drawUI");
  Serial.println("setup() complete");
  markBootStage(100, "setup-complete");
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
    evt.event_type = GROOVEPUTER_KEY_DOWN;
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
      if (g_miniAcid->currentDrumEngineName() == "SP12") g_miniAcid->toggleMuteClap();
      else g_miniAcid->toggleMuteRim();
      drawUI();
    } else if (c == '0') {
      if (g_miniAcid->currentDrumEngineName() == "SP12") g_miniAcid->toggleMuteRim();
      else g_miniAcid->toggleMuteClap();
      drawUI();
    } else if (c == 'k' || c == 'K') {
      g_miniAcid->setBpm(g_miniAcid->bpm() - 2.5f);
      drawUI();
    } else if (c == 'l' || c == 'L') {
      g_miniAcid->setBpm(g_miniAcid->bpm() + 2.5f);
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
     // g_miniAcid->toggleAudioDiag();
     // Serial.println("[UI] Toggled Audio Diagnostics");
      drawUI();
    } else if (c == '\'') {
     // bool newState = !g_miniAcid->isTestToneEnabled();
      //g_miniAcid->setTestTone(newState);
      //Serial.printf("[UI] Test Tone: %s\n", newState ? "ON" : "OFF");
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
  auto mapHidLetterScancode = [](uint8_t hid, KeyScanCode& sc) -> bool {
    constexpr uint8_t HID_KEY_A = 0x04;
    constexpr uint8_t HID_KEY_Z = 0x1D;
    if (hid < HID_KEY_A || hid > HID_KEY_Z) return false;
    sc = static_cast<KeyScanCode>(GROOVEPUTER_A + (hid - HID_KEY_A));
    return true;
  };
  auto mapAsciiLetterScancode = [](char c) -> KeyScanCode {
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
    if (c < 'a' || c > 'z') return GROOVEPUTER_NO_SCANCODE;
    return static_cast<KeyScanCode>(GROOVEPUTER_A + (c - 'a'));
  };

  auto processKeys = [&](const Keyboard_Class::KeysState& ks) {
    for (auto hid : ks.hid_keys) {
      UIEvent evt{};
      evt.alt = ks.alt;
      evt.ctrl = ks.ctrl;
      evt.shift = ks.shift;
      evt.meta = ks.fn;
      bool shouldSend = false;
      auto mapFKey = [&](uint8_t h, KeyScanCode& sc) -> bool {
        if (h >= 0x3A && h <= 0x41) {
            sc = static_cast<KeyScanCode>(GROOVEPUTER_F1 + (h - 0x3A));
            return true;
        }
        return false;
      };

      if (mapFKey(hid, evt.scancode)) {
        shouldSend = true;
      } else if (hid == 0x33) {
        evt.scancode = GROOVEPUTER_UP;
        shouldSend = true;
      } else if (hid == 0x37) {
        evt.scancode = GROOVEPUTER_DOWN;
        shouldSend = true;
      } else if (hid == 0x36) {
        evt.scancode = GROOVEPUTER_LEFT;
        shouldSend = true;
      } else if (hid == 0x38) {
        evt.scancode = GROOVEPUTER_RIGHT;
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
      } else if (hid >= 0x1E && hid <= 0x27) { // numbers 1..0
        if (hid == 0x27) evt.key = '0';
        else evt.key = '1' + (hid - 0x1E);
        shouldSend = true;
      } else if (applyCtrlLetter(ks, hid, evt)) {
        mapHidLetterScancode(hid, evt.scancode);
        shouldSend = true;
      } else if (applyAltLetter(ks, hid, evt)) {
        mapHidLetterScancode(hid, evt.scancode);
        shouldSend = true;
      }
      if (shouldSend) {
        handleWithFallback(evt);
      }
    }

    // do not send events only for modifier changes (ctrl/alt/shift/fn alone)
    if (ks.hid_keys.empty() && ks.word.empty()) {
      return;
    }
    for (auto inputChar : ks.word) {
      if (inputChar != 0) {
        unsigned char u = static_cast<unsigned char>(inputChar);
        // Skip keys already handled via HID loop (numbers, navigation, special keys)
        // This prevents "double-toggle" where a key on/off cycle happens in a single frame.
        if (u >= '0' && u <= '9') continue;
        if (u == '\n' || u == '\r' || u == '\b' || u == '\t') continue;

        // Skip letters if Ctrl/Alt held (handled via HID path)
        if (ks.ctrl || ks.alt) {
          bool isLetter = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
          bool isCtrlChar = (u >= 1 && u <= 26);
          if (isLetter || isCtrlChar) continue;
        }
      }

      UIEvent evt{};
      evt.alt = ks.alt;
      evt.ctrl = ks.ctrl;
      evt.shift = ks.shift;
      evt.meta = ks.fn;
      evt.key = normalizeKeyChar(inputChar);
      evt.scancode = mapAsciiLetterScancode(evt.key);

      if (evt.key == '`' || evt.key == '~') { // escape
        evt.scancode = GROOVEPUTER_ESCAPE;
      }
      handleWithFallback(evt);
    }
  };

  bool keyChanged = M5Cardputer.Keyboard.isChange();
  bool keyPressed = M5Cardputer.Keyboard.isPressed();
  static uint32_t repeatCount = 0;
  
  if (keyChanged && keyPressed) {
    if (g_miniDisplay) g_miniDisplay->dismissSplash();
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    processKeys(ks);
    lastKeysState = ks;
    hasLastKeys = true;
    nextRepeatAt = millis() + KEY_REPEAT_DELAY_MS;
    repeatCount = 0;
  } else if (!keyPressed) {
    if (hasLastKeys) {
        // Serial.println("[Keys] Released");
    }
    hasLastKeys = false;
    repeatCount = 0;
  } else if (hasLastKeys && millis() >= nextRepeatAt) {
    if (g_miniDisplay) g_miniDisplay->dismissSplash();
    // Only repeat alphabetical keys or specific navigational keys to avoid spamming
    // Serial.printf("[Keys] Repeating (HID=0x%02x)\n", lastKeysState.hid_keys[0]);
    processKeys(lastKeysState);
    nextRepeatAt = millis() + KEY_REPEAT_INTERVAL_MS;
    
    // Safety: if a key is held for more than 10 seconds, stop repeating until re-pressed
    if (++repeatCount > 100) { // 10s / 80ms approx
        hasLastKeys = false;
        repeatCount = 0;
        Serial.println("[Keys] Safety: Stopping long repeat");
    }
  }

  static unsigned long lastUIUpdate = 0;
  if (millis() - lastUIUpdate > 40) {
    lastUIUpdate = millis();
    if (g_miniDisplay) g_miniDisplay->update();
  }

  static unsigned long lastMemLog = 0;
  if (millis() - lastMemLog > 5000) {
    lastMemLog = millis();
    logHeapCaps("periodic");
    if (g_miniAcid) {
       auto& stats = g_miniAcid->perfStats;
       uint32_t s1 = 0, s2 = 0;
       uint32_t underruns = 0;
       float cpuAvg = 0.0f;
       float cpuPeak = 0.0f;
       uint32_t dv = 0, dd = 0, ds = 0, df = 0;
       do {
           s1 = stats.seq;
           underruns = stats.audioUnderruns;
           cpuAvg = stats.cpuAudioPctIdeal;
           cpuPeak = stats.cpuAudioPeakPct;
           dv = stats.dspVoicesUs;
           dd = stats.dspDrumsUs;
           ds = stats.dspSamplerUs;
           df = stats.dspFxUs;
           s2 = stats.seq;
       } while (s1 != s2 || (s1 & 1));
       Serial.printf("[PERF] CPU: avg %.1f%% / peak %.1f%% (underruns %u)\n",
           cpuAvg, cpuPeak, (unsigned)underruns);
       Serial.printf("       DSP: v:%uus d:%uus s:%uus f:%uus\n",
           (unsigned)dv, (unsigned)dd, (unsigned)ds, (unsigned)df);
    }
  }

  delay(5);
}
