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
#include "src/audio/audio_mutation_gate.h"
#include "src/platform/cardputer_adv_hardware.h"
#include "src/platform/cardputer_smf_player_registry.h"
#include "src/platform/cardputer_usb_midi_service.h"
#include "src/platform/cardputer_wdt_diagnostics.h"
#include "src/ui/key_normalize.h"
#include "src/ui/ui_common.h"
#include "src/input/performance_keyboard.h"
#include "src/input/cardputer_input_edges.h"
#include "src/input/internal_synth_output.h"
#include "src/input/musical_event_queue.h"
#include "src/midi/external_midi_clock_follower.h"
#include "src/midi/external_midi_transport_event_queue.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/ui/workflow_mode.h"
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
static bool g_audioOutputReady = false;
static uint32_t g_audioMidiBlockSequence = 0;

TaskHandle_t g_audioTaskHandle = nullptr;
static AudioMutationGate g_audioMutationGate;
static uint32_t g_lastUiDrawUs = 0;
static uint32_t g_peakUiDrawUs = 0;

// Static engine instance to avoid heap fragmentation
static MiniAcid g_miniAcidInstance(kSampleRate, &g_sceneStorage);
static MusicalEventRouter g_musicalEventRouter;
static MusicalEventQueue g_patternMusicalEventQueue;
static ExternalMidiTransportEventQueue g_externalMidiTransportQueue;
static GroovePuterMidi::ExternalMidiClockFollower g_externalClockFollower;
static PerformanceKeyboard g_performanceKeyboard(g_musicalEventRouter);
static InternalSynthOutput g_internalSynthOutput(g_miniAcidInstance, g_audioMutationGate);
static uint32_t g_lastLiveInputEpoch = 0;
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

static float readPatternSequencerPhase(void* context) {
  auto* engine = static_cast<MiniAcid*>(context);
  return engine ? engine->transportPhaseSteps() : 0.0f;
}

void audioTask(void *param) {
  Serial.println("AudioTask: Starting...");

  if (!g_audioOutputReady) {
    Serial.println("[FATAL] I2S audio init failed");
    while (true) { delay(1000); }
  }
  Serial.println("AudioTask: Loop start");
  
  while (true) {
    g_audioMutationGate.waitAtAudioBoundary();
    uint32_t now = micros();
    uint32_t start = now;
    static uint32_t warmupBlocks = 32; // ~743ms at 22.05kHz/512 for codec/DMA stability

    if (warmupBlocks > 0) {
      std::fill(g_audioBuffer, g_audioBuffer + kBlockFrames, 0);
      warmupBlocks--;
      if (warmupBlocks == 0) Serial.println("AudioTask: Warmup complete");
    } else if (g_miniAcid) {
      const uint32_t midiBlockSequence = g_audioMidiBlockSequence++;
      const auto clockSource =
          GroovePuterMidi::transportClockRuntime().source();
      const auto externalClock = g_externalClockFollower.processBlock(
          g_externalMidiTransportQueue, clockSource, now);
      GroovePuterMidi::transportClockRuntime().publishExternalEstimate(
          externalClock.estimate, g_externalClockFollower.failureCount());

      bool restartFromBeginning = true;
      if (clockSource ==
          GroovePuterMidi::TransportClockSource::SeqtrakExternal) {
        if (externalClock.estimate.validTempo) {
          const float externalBpm =
              static_cast<float>(externalClock.estimate.bpmQ16) / 65536.0f;
          const float bpmDelta = externalBpm - g_miniAcid->bpm();
          if (bpmDelta > 0.001f || bpmDelta < -0.001f) {
            g_miniAcid->setExternalClockBpm(externalBpm);
          }
        }
      }
      switch (externalClock.command) {
        case GroovePuterMidi::ExternalTransportCommand::Start:
          g_miniAcid->start();
          break;
        case GroovePuterMidi::ExternalTransportCommand::Continue:
          g_miniAcid->continueTransport();
          restartFromBeginning = false;
          break;
        case GroovePuterMidi::ExternalTransportCommand::Stop:
          g_miniAcid->pauseTransport();
          break;
        case GroovePuterMidi::ExternalTransportCommand::None:
          break;
      }

      const float phaseAtBlockStart = readPatternSequencerPhase(g_miniAcid);
      g_patternMusicalEventQueue.beginMidiRenderBlock(
          midiBlockSequence,
          static_cast<uint16_t>(kBlockFrames),
          phaseAtBlockStart,
          g_miniAcid->bpm(),
          static_cast<float>(kSampleRate),
          g_miniAcid->isPlaying(),
          GroovePuterMidi::transportClockSourcePublishesOutboundClock(
              clockSource),
          restartFromBeginning);
      g_miniAcid->generateAudioBuffer(g_audioBuffer, kBlockFrames);
      g_patternMusicalEventQueue.endMidiRenderBlock();
      publishCardputerUsbMidiBlockAnchor(midiBlockSequence, now);
    } else {
      std::fill(g_audioBuffer, g_audioBuffer + kBlockFrames, 0);
    }
    
    uint32_t dsp_time = micros() - start;
    
    // Publish one coherent cross-core telemetry snapshot.
    if (g_miniAcid) {
      auto& stats = g_miniAcid->perfStats;
      constexpr uint32_t idealPeriodUs =
          (1000000UL * kBlockFrames) / kSampleRate;
      const uint32_t previousCallback =
          stats.lastCallbackMicros.load(std::memory_order_relaxed);
      uint32_t actualPeriodUs = previousCallback > 0
          ? now - previousCallback
          : idealPeriodUs;
      if (actualPeriodUs == 0) actualPeriodUs = idealPeriodUs;

      const float idealCpu =
          static_cast<float>(dsp_time) * 100.0f /
          static_cast<float>(idealPeriodUs);
      const float actualCpu =
          static_cast<float>(dsp_time) * 100.0f /
          static_cast<float>(actualPeriodUs);

      stats.beginWrite();
      stats.cpuAudioPctIdeal.store(idealCpu, std::memory_order_relaxed);
      stats.cpuAudioPctActual.store(actualCpu, std::memory_order_relaxed);
      stats.dspTimeUs.store(dsp_time, std::memory_order_relaxed);
      stats.lastCallbackMicros.store(now, std::memory_order_relaxed);
      const float previousPeak =
          stats.cpuAudioPeakPct.load(std::memory_order_relaxed);
      if (idealCpu > previousPeak) {
        stats.cpuAudioPeakPct.store(idealCpu, std::memory_order_relaxed);
      }
      stats.endWrite();
    }

    if (g_audioRecorder) {
      g_audioRecorder->writeSamples(g_audioBuffer, kBlockFrames);
    }
    
    if (AudioDiagnostics::instance().isEnabled()) {
      AudioDiagnostics::instance().flushIfReady(millis());
    }

    // Write to I2S. Failed writes are real output underruns and must be
    // visible to diagnostics and the adaptive FX safety path.
    if (!g_audioOut.writeMono16(g_audioBuffer, kBlockFrames)) {
      if (g_miniAcid) {
        g_miniAcid->perfStats.audioUnderruns.fetch_add(
            1, std::memory_order_relaxed);
      }
      static uint32_t lastErrorLog = 0;
      if (millis() - lastErrorLog > 1000) {
        Serial.println("[I2S] Write Timeout / Error");
        lastErrorLog = millis();
      }
      taskYIELD();
    }
  }
}

void drawUI() {
  const uint32_t startedAt = micros();
  if (g_miniDisplay) g_miniDisplay->update();
  g_lastUiDrawUs = micros() - startedAt;
  if (g_lastUiDrawUs > g_peakUiDrawUs) {
    g_peakUiDrawUs = g_lastUiDrawUs;
  }
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

static void startAudioTask() {
  // Reserve the realtime stack before allocating lazy UI pages. Reducing this
  // stack masks the allocation failure but risks an audio-task overflow.
  Serial.println("8. Creating AudioTask...");
  markBootStage(80, "before AudioTask create");
  g_audioTaskHandle = nullptr;
  const BaseType_t taskOk = xTaskCreatePinnedToCore(
      audioTask, "AudioTask", 8192, nullptr, 3, &g_audioTaskHandle, 1);
  if (taskOk != pdPASS) {
    Serial.printf("[FATAL] AudioTask create failed: %d\n", (int)taskOk);
    markBootStage(902, "fatal-audio-task");
    while (true) { delay(1000); }
  }

  Serial.printf("[DEBUG] AudioTask created successful, handle: %p\n",
                (void*)g_audioTaskHandle);
  g_audioMutationGate.setAudioTaskActive(true);
  markBootStage(81, "after AudioTask create");
}

void setup() {
  // Enable the Cardputer ADV power amplifier. This pin is not RGB data.
  pinMode(GroovePuterHardware::kPowerAmplifierEnablePin, OUTPUT);
  digitalWrite(GroovePuterHardware::kPowerAmplifierEnablePin, HIGH);
  // pinMode(42, OUTPUT); digitalWrite(42, LOW); // Possible I2S conflict
  
#if ARDUINO_USB_CDC_ON_BOOT
  Serial.begin(115200);
#endif
  // Keep diagnostics off by default on hardware; detailed profiling can exceed
  // the real-time audio budget and cause underruns.
  AudioDiagnostics::instance().enable(false);
  delay(500);
  Serial.println("\n\n!! BOOTING !!");
  uint32_t prevBootStage = g_bootStage;
  Serial.printf("[BOOT] Previous stage retained: %u\n", (unsigned)prevBootStage);
  markBootStage(1, "setup-entry");
  auto cfg = M5.config();
  // GroovePuter owns the ES8311 I2S bus. Keep M5Unified from creating either
  // audio channel; the codec is configured directly below over I2C.
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  markBootStage(10, "before M5Cardputer.begin");
  M5Cardputer.begin(cfg);
  markBootStage(11, "after M5Cardputer.begin");

  // Configure ES8311 without creating a temporary M5Unified I2S channel.
  // These values match M5Unified's Cardputer ADV speaker callback.
  markBootStage(20, "before ES8311 config");
  const uint8_t es8311_addr = GroovePuterHardware::kEs8311I2cAddress;
  M5Cardputer.In_I2C.writeRegister8(es8311_addr, 0x00, 0x80, 100000); // RESET / CSM POWER ON
  M5Cardputer.In_I2C.writeRegister8(es8311_addr, 0x01, 0xB5, 100000); // CLOCK_MANAGER: MCLK=BCLK
  M5Cardputer.In_I2C.writeRegister8(es8311_addr, 0x02, 0x18, 100000); // CLOCK_MANAGER/ MULT_PRE=3
  M5Cardputer.In_I2C.writeRegister8(es8311_addr, 0x0D, 0x01, 100000); // SYSTEM: Power up analog circuitry
  M5Cardputer.In_I2C.writeRegister8(es8311_addr, 0x12, 0x00, 100000); // SYSTEM: power-up DAC
  M5Cardputer.In_I2C.writeRegister8(es8311_addr, 0x13, 0x10, 100000); // SYSTEM: Enable output to HP drive
  M5Cardputer.In_I2C.writeRegister8(es8311_addr, 0x32, 0xBF, 100000); // DAC: DAC volume max (0xBF == +/-0 dB)
  M5Cardputer.In_I2C.writeRegister8(es8311_addr, 0x37, 0x08, 100000); // DAC: Bypass DAC equalizer
  
  Serial.println("[Audio] ES8311 custom I2C Config complete (MCLK derived from BCLK)");

  // Reserve the I2S channel and DMA while internal RAM is still contiguous.
  // Starting the audio task later keeps setup and driver initialization
  // serialized on one task.
  markBootStage(21, "before direct I2S init");
  g_audioOutputReady = g_audioOut.begin(kSampleRate, kBlockFrames);
  markBootStage(g_audioOutputReady ? 22 : 922,
                g_audioOutputReady ? "after direct I2S init"
                                   : "direct I2S init failed");
  
  // Seed random number generator with hardware RNG
  srand(esp_random());
  
  logHeapCaps("after-m5-begin");
  
  esp_reset_reason_t reason = esp_reset_reason();
  Serial.println("\n\n=== MiniAcid STARTUP DIAGNOSTICS ===");
  Serial.printf("Reset Reason: %d\n", (int)reason);
  printAndClearCardputerWdtDiagnostic();

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

  screenLog("4. Engine Static OK");
  // Reserve the realtime stack while the display allocation is still the only
  // large heap consumer. audioTask outputs silence until g_miniAcid is set
  // after MiniAcid::init() completes below.
  logHeapCaps("before-audio-task");
  startAudioTask();
  logHeapCaps("after-audio-task");

  // Each TempoDelay needs one contiguous 8.6KB block. Reserve both before SD
  // and SMF runtime fragment the DRAM-only Cardputer ADV heap.
  screenLog("4a. DSP Buffers...");
  markBootStage(86, "before critical DSP buffers");
  g_miniAcidInstance.preallocateConstrainedDelayBuffers();
  markBootStage(87, "after critical DSP buffers");
  logHeapCaps("after-critical-dsp-buffers");

  // Mount SD while enough contiguous internal memory remains. MiniAcid::init()
  // calls initializeStorage() again, but SceneStorageCardputer treats that as
  // an idempotent readiness check instead of remounting the shared SD object.
  screenLog("4b. SD Init...");
  markBootStage(82, "before early SD init");
  g_sceneStorage.initializeStorage();
  markBootStage(83, "after early SD init");

  // Reserve the SMF task stack and bounded timing buffers before DSP and lazy
  // UI allocations fragment the DRAM-only Cardputer ADV heap.
  screenLog("4c. SMF Runtime...");
  markBootStage(84, "before SMF runtime init");
  if (!beginCardputerSmfPlayerService()) {
    Serial.println("[WARN] SMF runtime unavailable; groovebox remains usable");
  }
  markBootStage(85, "after SMF runtime init");

  // Start the dispatcher before engine/UI activity. Its full 4KB stack is
  // statically reserved, so startup no longer depends on the largest free heap
  // block left by SD and SMF initialization.
  g_musicalEventRouter.addSink(g_internalSynthOutput);
  screenLog("4d. USB MIDI Runtime...");
  markBootStage(52, "before USB MIDI sink");
  if (!registerCardputerUsbMidiSink(
          g_musicalEventRouter,
          g_patternMusicalEventQueue,
          g_externalMidiTransportQueue)) {
    Serial.println("[ERROR] USB MIDI runtime unavailable");
    markBootStage(952, "USB MIDI runtime unavailable");
  } else {
    markBootStage(53, "after USB MIDI sink");
  }

  screenLog("5. Creating Encoder8");
  markBootStage(40, "before Encoder8 alloc");
  g_encoder8 = new (std::nothrow) Encoder8Miniacid(g_miniAcidInstance);
  if (!g_encoder8) {
    Serial.println("[FATAL] Encoder8 allocation failed");
    markBootStage(900, "fatal-encoder8-alloc");
    while (true) { delay(1000); }
  }
  markBootStage(41, "after Encoder8 alloc");

  screenLog("6. Engine Init...");
  // Link sample store before init
  g_miniAcidInstance.sampleStore = &g_sampleStore;
  markBootStage(50, "before MiniAcid::init");
  g_miniAcidInstance.init();
  g_miniAcid = &g_miniAcidInstance;
  g_patternMusicalEventQueue.setPhaseReader(
      readPatternSequencerPhase, g_miniAcid);
  g_miniAcid->setPatternEventQueue(&g_patternMusicalEventQueue);
  g_lastLiveInputEpoch = g_miniAcid->liveInputEpoch();
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
  g_miniDisplay = new (std::nothrow) MiniAcidDisplay(
      g_display, *g_miniAcid, g_performanceKeyboard);
  if (!g_miniDisplay) {
    Serial.println("[FATAL] MiniAcidDisplay allocation failed");
    markBootStage(901, "fatal-display-alloc");
    while (true) { delay(1000); }
  }
  markBootStage(71, "after MiniAcidDisplay alloc");
  Serial.println("7b. UI setAudioGuard");
  
  // Pause the renderer only at a block boundary while existing UI mutation
  // lambdas update engine state. No mutex is held while DSP is rendering.
  AudioGuard guard;
  guard.context = &g_audioMutationGate;
  guard.lock = [](void* context) {
      static_cast<AudioMutationGate*>(context)->lockControl();
  };
  guard.unlock = [](void* context) {
      static_cast<AudioMutationGate*>(context)->unlockControl();
  };
  g_miniDisplay->setAudioGuard(guard);
  
  Serial.println("7c. UI setAudioRecorder");
  // Initialize audio recorder (done after other initialization to avoid boot issues)
  // DISABLING FOR CRASH DEBUGGING
  // g_audioRecorder = new CardputerAudioRecorder();
  // g_miniDisplay->setAudioRecorder(g_audioRecorder);

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

  if (g_miniAcid && g_miniDisplay) {
    g_performanceKeyboard.setEnabled(
        WorkflowPages::allowsPerformanceKeyboard(g_miniDisplay->currentPageIndex()));
    g_performanceKeyboard.setTransportPlaying(g_miniAcid->isPlaying());
    const uint32_t epoch = g_miniAcid->liveInputEpoch();
    if (epoch != g_lastLiveInputEpoch) {
      g_performanceKeyboard.panic();
      g_lastLiveInputEpoch = epoch;
    }
  }

  if (g_encoder8) g_encoder8->update();

  if (M5Cardputer.BtnA.wasClicked()) {
    if (GroovePuterMidi::transportClockRuntime().source() ==
        GroovePuterMidi::TransportClockSource::SeqtrakExternal) {
      UI::showToast("SEQ MASTER: USE SEQTRAK", 900);
    } else {
      AudioMutationScope mutationScope(g_audioMutationGate);
      if (g_miniAcid->isPlaying()) {
        g_miniAcid->stop();
      } else {
        g_performanceKeyboard.setTransportPlaying(true);
        g_miniAcid->start();
      }
    }
    g_performanceKeyboard.setTransportPlaying(g_miniAcid->isPlaying());
    drawUI();
  }

  static constexpr unsigned long KEY_REPEAT_DELAY_MS = 350;
  static constexpr unsigned long KEY_REPEAT_INTERVAL_MS = 80;
  static Keyboard_Class::KeysState previousKeysState{};
  static bool hasPreviousKeysState = false;
  static UIEvent repeatEvent{};
  static uint8_t repeatHid = 0;
  static bool repeatActive = false;
  static uint32_t repeatPressId = 0;
  static uint32_t nextPressId = 1;
  static unsigned long nextRepeatAt = 0;

  auto handleWithFallback = [&](UIEvent evt,
                                const char* source,
                                uint32_t pressId,
                                bool repeat) {
    Serial.printf("[KEY] press=%u src=%s repeat=%d fn=%d alt=%d ctrl=%d shift=%d key=0x%02X sc=%d\n",
      (unsigned)pressId, source, repeat ? 1 : 0,
      evt.meta ? 1 : 0, evt.alt ? 1 : 0, evt.ctrl ? 1 : 0,
      evt.shift ? 1 : 0, (uint8_t)evt.key, evt.scancode);
    evt.event_type = GROOVEPUTER_KEY_DOWN;

    bool handled = false;
    {
      AudioMutationScope mutationScope(g_audioMutationGate);
      handled = g_miniDisplay ? g_miniDisplay->handleEvent(evt) : false;
    }
    g_performanceKeyboard.setTransportPlaying(g_miniAcid->isPlaying());
    if (handled) {
      drawUI();
      return;
    }

    bool needsDraw = false;
    {
      // Guard only control-plane mutations. Releasing the gate before drawUI()
      // prevents a full display redraw from intentionally pausing audio output.
      AudioMutationScope mutationScope(g_audioMutationGate);
      char c = evt.key;
      if (c == '\t' && g_miniDisplay) {
        UIEvent app_evt{};
        app_evt.event_type = GROOVEPUTER_APPLICATION_EVENT;
        app_evt.app_event_type = GROOVEPUTER_APP_EVENT_MULTIPAGE_DOWN;
        needsDraw = g_miniDisplay->handleEvent(app_evt);
      } else if (c == '\n' || c == '\r') {
        if (g_miniDisplay) g_miniDisplay->dismissSplash();
        needsDraw = true;
      } else if (c == '[') {
        if (g_miniDisplay) g_miniDisplay->previousPage();
        needsDraw = true;
      } else if (c == ']') {
        if (g_miniDisplay) g_miniDisplay->nextPage();
        needsDraw = true;
      } else if (c == 'i' || c == 'I') {
        g_miniAcid->randomize303Pattern(0);
        needsDraw = true;
      } else if (c == 'o' || c == 'O') {
        g_miniAcid->randomize303Pattern(1);
        needsDraw = true;
      } else if (c == 'p' || c == 'P') {
        g_miniAcid->randomizeDrumPattern();
        needsDraw = true;
      } else if (c == '1') {
        g_miniAcid->toggleMute303(0);
        needsDraw = true;
      } else if (c == '2') {
        g_miniAcid->toggleMute303(1);
        needsDraw = true;
      } else if (c == '3') {
        g_miniAcid->toggleMuteKick();
        needsDraw = true;
      } else if (c == '4') {
        g_miniAcid->toggleMuteSnare();
        needsDraw = true;
      } else if (c == '5') {
        g_miniAcid->toggleMuteHat();
        needsDraw = true;
      } else if (c == '6') {
        g_miniAcid->toggleMuteOpenHat();
        needsDraw = true;
      } else if (c == '7') {
        g_miniAcid->toggleMuteMidTom();
        needsDraw = true;
      } else if (c == '8') {
        g_miniAcid->toggleMuteHighTom();
        needsDraw = true;
      } else if (c == '9') {
        if (g_miniAcid->currentDrumEngineName() == "SP12") g_miniAcid->toggleMuteClap();
        else g_miniAcid->toggleMuteRim();
        needsDraw = true;
      } else if (c == '0') {
        if (g_miniAcid->currentDrumEngineName() == "SP12") g_miniAcid->toggleMuteRim();
        else g_miniAcid->toggleMuteClap();
        needsDraw = true;
      } else if (c == 'k' || c == 'K') {
        if (GroovePuterMidi::transportClockRuntime().source() ==
            GroovePuterMidi::TransportClockSource::GroovePuterInternal) {
          g_miniAcid->setBpm(g_miniAcid->bpm() - 2.5f);
        } else {
          UI::showToast("SEQ MASTER BPM", 700);
        }
        needsDraw = true;
      } else if (c == 'l' || c == 'L') {
        if (GroovePuterMidi::transportClockRuntime().source() ==
            GroovePuterMidi::TransportClockSource::GroovePuterInternal) {
          g_miniAcid->setBpm(g_miniAcid->bpm() + 2.5f);
        } else {
          UI::showToast("SEQ MASTER BPM", 700);
        }
        needsDraw = true;
      } else if (c == '-' || c == '_') {
        g_miniAcid->adjustParameter(MiniAcidParamId::MainVolume, -3);
        needsDraw = true;
      } else if (c == '=' || c == '+') {
        g_miniAcid->adjustParameter(MiniAcidParamId::MainVolume, 3);
        needsDraw = true;
      } else if (c == ';' || c == '\'') {
        needsDraw = true;
      } else if (c == ' ') {
        if (GroovePuterMidi::transportClockRuntime().source() ==
            GroovePuterMidi::TransportClockSource::SeqtrakExternal) {
          UI::showToast("SEQ MASTER: USE SEQTRAK", 900);
        } else if (g_miniAcid->isPlaying()) {
          g_miniAcid->stop();
        } else {
          g_miniAcid->start();
        }
        needsDraw = true;
      }
    }

    if (needsDraw) drawUI();
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

  auto reconcilePerformanceKeys = [&](const Keyboard_Class::KeysState& ks) {
    char pressed[PerformanceKeyboard::kMaxHeldNotes]{};
    size_t count = 0;
    if (!ks.alt && !ks.ctrl && !ks.shift && !ks.fn) {
      for (auto hid : ks.hid_keys) {
        if (hid < 0x04 || hid > 0x1D) continue;
        const char key = static_cast<char>('a' + (hid - 0x04));
        uint8_t degree = 0;
        if (PerformanceKeyboard::scaleDegreeForKey(key, degree) &&
            count < PerformanceKeyboard::kMaxHeldNotes) {
          pressed[count++] = key;
        }
      }
    }
    g_performanceKeyboard.releaseMissingKeys(pressed, count);
  };

  auto processKeyEdges = [&](const Keyboard_Class::KeysState& ks,
                             const Keyboard_Class::KeysState& previous,
                             bool hadPrevious,
                             uint32_t pressId) -> bool {
    bool dispatched = false;
    bool sawEdge = false;
    bool armedRepeat = false;

    for (auto hid : ks.hid_keys) {
      if (!GroovePuterInput::shouldDispatchHid(
              ks, previous, hadPrevious, static_cast<uint8_t>(hid))) {
        continue;
      }
      sawEdge = true;

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
      } else if (hid == 0x28 || hid == 0x58) {
        evt.key = '\n';
        shouldSend = true;
      } else if (hid == KEY_BACKSPACE) {
        evt.key = '\b';
        shouldSend = true;
      } else if (hid == KEY_TAB || hid == 0x2B) {
        evt.key = '\t';
        evt.scancode = GROOVEPUTER_TAB;
        shouldSend = true;
      } else if (hid >= 0x1E && hid <= 0x27) {
        evt.key = hid == 0x27 ? '0' : static_cast<char>('1' + (hid - 0x1E));
        shouldSend = true;
      } else if (applyCtrlLetter(ks, hid, evt)) {
        mapHidLetterScancode(hid, evt.scancode);
        shouldSend = true;
      } else if (applyAltLetter(ks, hid, evt)) {
        mapHidLetterScancode(hid, evt.scancode);
        shouldSend = true;
      }

      if (!shouldSend) continue;
      handleWithFallback(evt, "HID", pressId, false);
      dispatched = true;

      if (GroovePuterInput::mayRepeat(evt) &&
          ks.hid_keys.size() == 1 && ks.word.empty()) {
        repeatEvent = evt;
        repeatHid = static_cast<uint8_t>(hid);
        repeatPressId = pressId;
        nextRepeatAt = millis() + KEY_REPEAT_DELAY_MS;
        armedRepeat = true;
      }
    }

    const bool suppressWordAfterModifierRelease =
        hadPrevious && GroovePuterInput::modifierReleased(ks, previous);
    if (!suppressWordAfterModifierRelease) {
      for (auto inputChar : ks.word) {
        if (!GroovePuterInput::shouldDispatchWord(
                ks, previous, hadPrevious, inputChar)) {
          continue;
        }
        sawEdge = true;

        if (inputChar != 0) {
          const unsigned char u = static_cast<unsigned char>(inputChar);
          if (u >= '0' && u <= '9') continue;
          if (u == '\n' || u == '\r' || u == '\b' || u == '\t') continue;

          if (ks.ctrl || ks.alt) {
            const bool isLetter =
                (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z');
            const bool isCtrlChar = u >= 1 && u <= 26;
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
        if (evt.key == '`' || evt.key == '~') {
          evt.scancode = GROOVEPUTER_ESCAPE;
        }
        handleWithFallback(evt, "WORD", pressId, false);
        dispatched = true;
      }
    }

    if (sawEdge) {
      repeatActive = armedRepeat;
      if (!repeatActive) {
        repeatHid = 0;
        repeatPressId = 0;
      }
    }
    return dispatched;
  };

  const Keyboard_Class::KeysState currentKeysState =
      M5Cardputer.Keyboard.keysState();
  reconcilePerformanceKeys(currentKeysState);

  const uint32_t candidatePressId = nextPressId;
  const bool dispatched = processKeyEdges(
      currentKeysState,
      previousKeysState,
      hasPreviousKeysState,
      candidatePressId);
  if (dispatched) {
    ++nextPressId;
    if (g_miniDisplay) g_miniDisplay->dismissSplash();
  }

  if (repeatActive &&
      !GroovePuterInput::repeatKeyStillHeld(
          currentKeysState, repeatHid, repeatEvent)) {
    Serial.printf("[KEY] press=%u src=REPEAT blocked=1 hid=0x%02X\n",
                  (unsigned)repeatPressId, (unsigned)repeatHid);
    repeatActive = false;
  }

  const unsigned long nowMs = millis();
  if (repeatActive &&
      static_cast<int32_t>(nowMs - nextRepeatAt) >= 0) {
    handleWithFallback(repeatEvent, "REPEAT", repeatPressId, true);
    nextRepeatAt = nowMs + KEY_REPEAT_INTERVAL_MS;
  }

  previousKeysState = currentKeysState;
  hasPreviousKeysState = true;

  static unsigned long lastUIUpdate = 0;
  if (millis() - lastUIUpdate > 40) {
    lastUIUpdate = millis();
    if (g_miniDisplay) g_miniDisplay->update();
  }

  static unsigned long lastMemLog = 0;
  if (millis() - lastMemLog > 5000) {
    lastMemLog = millis();
    // logHeapCaps("periodic");
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
       const uint32_t freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
       const uint32_t largestInt = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
       Serial.printf("[PERF] audio=%.1f%% peak=%.1f%% underruns=%u ui=%uus uiPeak=%uus freeInt=%u largest=%u dsp=%u/%u/%u/%u\n",
           cpuAvg, cpuPeak, (unsigned)underruns,
           (unsigned)g_lastUiDrawUs, (unsigned)g_peakUiDrawUs,
           (unsigned)freeInt, (unsigned)largestInt,
           (unsigned)dv, (unsigned)dd, (unsigned)ds, (unsigned)df);
       g_peakUiDrawUs = g_lastUiDrawUs;
    }
  }

  delay(5);
}
