#include "miniacid_engine.h"
#include "src/dsp/musical_development.h"
#include "src/phrase/runtime_phrase_edit.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"
#include "src/state/material_version.h"
#include "src/state/melody_promotion.h"
#include "song_cycle_boundary.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#include "wav_header.h"
#else
#include "../../platform_sdl/arduino_compat.h"
#include "wav_header.h"
#endif
#include <algorithm>
#include <cmath>
#include <cctype>
#include <new>
#include <string>

#include "../audio/audio_diagnostics.h"
#include "../audio/pattern_paging.h"
#include "../input/musical_event_queue.h"
#include "../generation/migration/quantized_generation_commit.h"

#include "../sampler/sample_index.h"
#include "../ui/led_manager.h"

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#include <esp_psram.h>
#endif

#include "../platform/log.h"
#include "swappable_synth_voice.h"
#include "advanced_pattern_generator.h"
#include "atlas_runtime.h"

namespace {
constexpr int kDrumKickVoice = 0;
constexpr int kDrumSnareVoice = 1;
constexpr int kDrumHatVoice = 2;
constexpr int kDrumOpenHatVoice = 3;
constexpr int kDrumMidTomVoice = 4;
constexpr int kDrumHighTomVoice = 5;
constexpr int kDrumRimVoice = 6;
constexpr int kDrumClapVoice = 7;

SynthPattern makeEmptySynthPattern() {
  SynthPattern pattern{};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i].note = -1;
    pattern.steps[i].accent = false;
    pattern.steps[i].slide = false;
  }
  return pattern;
}

DrumPatternSet makeEmptyDrumPatternSet() {
  DrumPatternSet set{};
  for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
    for (int s = 0; s < DrumPattern::kSteps; ++s) {
      set.voices[v].steps[s].hit = false;
      set.voices[v].steps[s].accent = false;
    }
  }
  return set;
}

const SynthPattern kEmptySynthPattern = makeEmptySynthPattern();
const DrumPatternSet kEmptyDrumPatternSet = makeEmptyDrumPatternSet();

std::string toLowerCopy(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}
}

TempoDelay::TempoDelay(float sampleRate)
  : buffer(),
    writeIndex(0),
    delaySamples(1),
    sampleRate(0.0f),
    maxDelaySamples(0),
    beats(0.25f),
    mix(0.35f),
    feedback(0.45f),
    enabled(false) {
  // Defer allocation to init()
  // setSampleRate(sampleRate); 
  // Store sampleRate for later use
  if (sampleRate <= 0.0f) this->sampleRate = 44100.0f;
  else this->sampleRate = sampleRate;
  
  // DONT calculate maxDelaySamples here, wait for init()
  maxDelaySamples = 0;
}

void TempoDelay::init(float maxSeconds) {
  if (maxSeconds <= 0.0f) maxSeconds = 1.0f;
  
  // Calculate required
  int newMaxSamples = static_cast<int>(this->sampleRate * maxSeconds);
  if (newMaxSamples < 1) newMaxSamples = 44100;
  
  // Prevent double allocation if already sufficient
  if (!buffer.empty() && maxDelaySamples == newMaxSamples) {
     LOG_PRINTLN("TempoDelay::init: already initialized, skipping allocation");
     return;
  }

  LOG_DEBUG("TempoDelay::init: sr=%.1f maxSeconds=%.3f => samples=%d\n",
          sampleRate, maxSeconds, newMaxSamples);
  
  const size_t required = static_cast<size_t>(newMaxSamples);
  
  // Log allocation attempt
  LOG_DEBUG("TempoDelay::init: Allocating %d samples (%.1f KB)...\n", 
                newMaxSamples, (newMaxSamples * sizeof(float)) / 1024.0f);

  try {
    buffer.assign(required, 0.0f);
  } catch (const std::bad_alloc&) {
    buffer.clear();
    maxDelaySamples = 0;
    LOG_PRINTLN("TempoDelay::init: allocation failed; delay disabled");
    return;
  }
  maxDelaySamples = newMaxSamples;
  
  reset();
}

void TempoDelay::reset() {
  if (buffer.empty())
    return;
  std::fill(buffer.begin(), buffer.end(), 0.0f);
  writeIndex = 0;
  if (delaySamples < 1)
    delaySamples = 1;
  if (delaySamples >= maxDelaySamples)
    delaySamples = maxDelaySamples - 1;
}

void TempoDelay::setSampleRate(float sr) {
  if (sr <= 0.0f) sr = 44100.0f;
  sampleRate = sr;
  maxDelaySamples = static_cast<int>(sampleRate * kMaxDelaySeconds);
  if (maxDelaySamples < 1)
    maxDelaySamples = 1;
  
  // Resize only if we are initialized (buffer not empty)
  // or if we are explicitly re-configuring
  if (!buffer.empty()) {
    buffer.assign(static_cast<size_t>(maxDelaySamples), 0.0f);
  }
  if (delaySamples >= maxDelaySamples)
    delaySamples = maxDelaySamples - 1;
  if (delaySamples < 1)
    delaySamples = 1;
}

void TempoDelay::setBpm(float bpm) {
  if (bpm < 40.0f)
    bpm = 40.0f;
  float secondsPerBeat = 60.0f / bpm;
  float delaySeconds = secondsPerBeat * beats;
  int samples = static_cast<int>(delaySeconds * sampleRate);
  if (samples < 1)
    samples = 1;
  if (samples >= maxDelaySamples)
    samples = maxDelaySamples - 1;
  delaySamples = samples;
}

void TempoDelay::setBeats(float b) {
  if (b < 0.125f)
    b = 0.125f;
  beats = b;
}

void TempoDelay::setMix(float m) {
  if (m < 0.0f)
    m = 0.0f;
  if (m > 1.0f)
    m = 1.0f;
  mix = m;
}

void TempoDelay::setFeedback(float fb) {
  if (fb < 0.0f)
    fb = 0.0f;
  if (fb > 0.95f)
    fb = 0.95f;
  feedback = fb;
}

void TempoDelay::setEnabled(bool on) { enabled = on; }

bool TempoDelay::isEnabled() const { return enabled; }

float TempoDelay::process(float input) {
  if (!enabled || buffer.empty()) {
    return input;
  }

  int readIndex = writeIndex - delaySamples;
  if (readIndex < 0)
    readIndex += maxDelaySamples;

  float delayed = buffer[readIndex];
  // Soft limit feedback sum to prevent accumulation/runaway
  float fbSum = input + delayed * feedback;
  fbSum = fbSum / (1.0f + fabsf(fbSum) * 0.8f);  // gentle limiting
  buffer[writeIndex] = fbSum;

  writeIndex++;
  if (writeIndex >= maxDelaySamples)
    writeIndex = 0;

  return input + delayed * mix;
}

MiniAcid::MiniAcid(float sampleRate, SceneStorage* sceneStorage)
  : drums(std::make_unique<TR808DrumSynthVoice>(sampleRate)),
    sampleRateValue(sampleRate),
    drumEngineName_("808"),
    sceneStorage_(sceneStorage),
    samplerTrack(std::make_unique<DrumSamplerTrack>()),
    tapeFX(std::make_unique<TapeFX>()),
    tapeLooper(std::make_unique<TapeLooper>()),
    playing(false),
    mute303(false),
    mute303_2(false),
    muteKick(false),
    muteSnare(false),
    muteHat(false),
    muteOpenHat(false),
    muteMidTom(false),
    muteHighTom(false),
    muteRim(false),
    muteClap(false),
    delay303Enabled(false),
    delay3032Enabled(false),
    distortion303Enabled(false),
    distortion3032Enabled(false),
    bpmValue(100.0f),
    projectBpmValue(100.0f),
    currentStepIndex(-1),
    tickPhaseAccum_(0),
    tickPhaseInc_(0),
    currentTick_(0),
    samplesPerStep_(10000.0f),
    songMode_(false),
    drumCycleIndex_(0),
    songPlayheadPosition_(0),
    songPlaybackSlot_(0),
    liveMixMode_(false),
    patternModeDrumPatternIndex_(0),
    patternModeDrumBankIndex_(0),
    patternModeSynthPatternIndex_{0, 0},
    patternModeSynthBankIndex_{0, 0},
    delay303(sampleRate),
    delay3032(sampleRate),
    distortion303(),
    distortion3032(),
    currentTimingOffset_(0) {
  if (sampleRateValue <= 0.0f) sampleRateValue = 44100.0f;

  // One bounded reservation, at construction, for the session-only NEXT and
  // source-anchor history. It never occurs on the audio path.
  (void)initPendingMaterial();
  
  // Initialize Drum FX
  drumReverb.setSampleRate(sampleRateValue);
  drumTransientShaper.setSampleRate(sampleRateValue);
  
  // NEW: Configure voice processing chain
  // HPF @ 150Hz is built-in to compressor
  voiceCompressor_.setThreshold(0.3f);      // -10dB
  voiceCompressor_.setRatio(4.0f);          // 4:1 compression
  voiceCompressor_.setMakeupGain(2.8f);     // +9dB boost
  voiceCompressor_.setPresenceBoost(0.5f);  // +3dB @ 2kHz

  // Initialize synth engines (default to TB303 for both)
  synthVoices_[0] = std::make_unique<SwappableSynthVoice>(sampleRate, SynthEngineType::TB303);
  synthEngineNames_[0] = "TB303";
  synthVoices_[1] = std::make_unique<SwappableSynthVoice>(sampleRate, SynthEngineType::TB303);
  synthEngineNames_[1] = "TB303";
}

void MiniAcid::preallocateConstrainedDelayBuffers() {
  // Cardputer ADV has no PSRAM. Reserve both equal-sized vectors before SD and
  // SMF task allocations split the remaining internal heap into small blocks.
  delay303.init(0.1f);
  delay3032.init(0.1f);
}


void MiniAcid::init() {
  bool hasPsram = false;
#if defined(ESP32) || defined(ESP_PLATFORM)
  // Check for ACTUAL usable PSRAM, not just if it was detected
  // psramFound() can return true even if init failed
  size_t freePsram = ESP.getFreePsram();
  hasPsram = (freePsram > 512 * 1024); // Require at least 512KB usable
  LOG_DEBUG("  - MiniAcid::init: freePsram=%u, hasPsram=%d", (unsigned)freePsram, hasPsram);
#endif

  if (hasPsram) {
    LOG_PRINTLN("  - MiniAcid::init: PSRAM mode (high performance)");
    // PSRAM: High-performance mode (44.1kHz = ~176KB per second float)
    if (tapeLooper) tapeLooper->init(8);           // 8s looper (~1.4MB)
    if (sampleStore) sampleStore->setPoolSize(2 * 1024 * 1024); // 2MB pool
    delay303.init(1.0f);
    delay3032.init(1.0f);
  } else {
    LOG_PRINTLN("  - MiniAcid::init: DRAM-only mode (constrained)");
    // DRAM: Constrained mode (44.1kHz is expensive!)
    // Keep a practical looper length so REC/PLAY is musically usable without PSRAM.
    // 1.0s mono int16 looper ~= 88KB.
    if (tapeLooper) tapeLooper->init(0.5f);
    if (sampleStore) sampleStore->setPoolSize(32 * 1024); // 32KB sampler pool
    delay303.init(0.1f);
    delay3032.init(0.1f);
    
    // TAPE FX DISABLED BY DEFAULT IN DRAM MODE
    if (tapeFX) tapeFX->setEnabled(false);
    
    LOG_PRINTLN("  - MiniAcid::init: DRAM MODE ACTIVE (Reduced buffers)");
  }

  LOG_PRINTLN("  - MiniAcid::init: Memory strategy applied");

  // Replaces existing default params initialization
  // Allow extra headroom for quiet built-in speakers; final stage is soft-limited.
  params[static_cast<int>(MiniAcidParamId::MainVolume)] = Parameter("vol", "", 0.0f, 1.8f, 0.6f, 1.0f / 64);
  params[static_cast<int>(MiniAcidParamId::VoicePitch)] = Parameter("v_pch", "Hz", 60.0f, 400.0f, 150.0f, 1.0f); // Was 120
  params[static_cast<int>(MiniAcidParamId::VoiceSpeed)] = Parameter("v_spd", "x", 0.5f, 2.0f, 1.2f, 0.1f);   // Was 1.0
  params[static_cast<int>(MiniAcidParamId::VoiceRobotness)] = Parameter("v_rob", "%", 0.0f, 1.0f, 0.7f, 0.05f); // Was 0.8
  params[static_cast<int>(MiniAcidParamId::VoiceVolume)] = Parameter("v_vol", "%", 0.0f, 1.0f, 0.8f, 0.05f); // Was 1.0
  setMasterOutputHighCutHz(kMasterHighCutHz);
  


  if (sceneStorage_) {
    LOG_PRINTLN("  - MiniAcid::init: Initializing scene storage...");
    sceneStorage_->initializeStorage();
    
    // Initialize voice cache (SD card)
    if (voiceCache_.init()) {
        LOG_PRINTLN("  - MiniAcid::init: Voice cache initialized");
    }
    
    LOG_PRINTLN("  - MiniAcid::init: Loading scene from storage...");
    loadSceneFromStorage();
  }
  
  // Initialize FX buffers (deferred allocation) - done in if/else above now
  // delay303.init();
  // delay3032.init();

  // Ensure drums are allocated before reset
  if (!drums) {
    LOG_PRINTLN("  - MiniAcid::init: Allocating default drum engine (909)...");
    setDrumEngine("909"); 
  }

  playing = false; // Safety
  LOG_PRINTLN("  - MiniAcid::init: reset()...");
  reset();
  AudioDiagnostics::instance().enable(false);
  LOG_PRINTLN("  - MiniAcid::init: applySceneStateFromManager()...");
  applySceneStateFromManager();
  // P2 startup publication is a playback precondition. Playback is still
  // stopped here: publish a complete resident bank and only then mirror
  // the actual paging identity into MiniAcid runtime selection.
  const int residentPage = PatternPagingService::activePageIndex();
  if (rebuildPatternRuntimeEventBank() &&
      patternRuntimeBank_.pageIdentity() == residentPage) {
    setCurrentPage(static_cast<int8_t>(residentPage));
  } else {
    patternRuntimeBank_.invalidatePageIdentity();
    LOG_PRINTLN("  - MiniAcid::init: Pattern runtime bank publication failed");
  }
  LOG_PRINTLN("  - MiniAcid::init: Done");
}

void MiniAcid::reset() {
  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
  LOG_PRINTLN("    - MiniAcid::reset: Start");
  if (synthVoices_[0]) synthVoices_[0]->reset();
  if (synthVoices_[1]) synthVoices_[1]->reset();
  liveNotes_[0] = -1;
  liveNotes_[1] = -1;
  publishPatternAllNotesOff_();
  ++liveInputEpoch_;
  LOG_PRINTLN("    - MiniAcid::reset: voices reset");
  
  // Make the second voice have different params (intentional base offset)
  // TB303 Param mapping: 0=Cutoff, 1=Res, 2=Env, 3=Decay
  if (TB303Voice* v303 = tb303Voice(1)) {
      v303->adjustParameter(TB303ParamId::Cutoff, -3);
      v303->adjustParameter(TB303ParamId::Resonance, -3);
      v303->adjustParameter(TB303ParamId::EnvAmount, -1);
  }
  
  if (drums) {
    LOG_PRINTLN("    - MiniAcid::reset: resetting drums...");
    drums->reset();
  } else {
    LOG_PRINTLN("    - MiniAcid::reset: ERROR: drums is NULL!");
  }
  playing = false;
  mute303 = false;
  mute303_2 = false;
  muteKick = false;
  muteSnare = false;
  muteHat = false;
  muteOpenHat = false;
  muteMidTom = false;
  muteHighTom = false;
  muteRim = false;
  muteClap = false;
  delay303Enabled = false; // Forced OFF for performance
  delay3032Enabled = false;
  distortion303Enabled = false;
  distortion3032Enabled = false;
  bpmValue = 100.0f;
  projectBpmValue = 100.0f;
  currentStepIndex = -1;
  tickPhaseAccum_ = 0;
  currentTick_ = 0;
  songBarIndex_ = -1;
  currentTimingOffset_ = 0;
  updateTickIncrement();
  masterOutputLpState_ = 0.0f;
  
  delay303.reset();
  delay303.setBeats(0.5f); // eighth note
  delay303.setMix(0.25f);
  delay303.setFeedback(0.35f);
  delay303.setEnabled(delay303Enabled);
  delay303.setBpm(bpmValue);
  
  delay3032.reset();
  delay3032.setBeats(0.5f);
  delay3032.setMix(0.22f);
  delay3032.setFeedback(0.32f);
  delay3032.setEnabled(delay3032Enabled);
  delay3032.setBpm(bpmValue);
  
  vocalMixer_.setDuckAmount(0.0f);
  voiceCompressor_.reset();
  vocalSynth_.reset();
  
  drumCompressor.reset();
  drumTransientShaper.reset();
  drumReverb.reset();
  
  updateDrumCompression(0.0f);
  updateDrumTransientAttack(0.0f);
  updateDrumTransientSustain(0.0f);
  updateDrumReverbMix(0.0f);
  updateDrumReverbDecay(0.5f);
  
  distortion303.setEnabled(distortion303Enabled);
  distortion3032.setEnabled(distortion3032Enabled);
  
  // Initialize waveform buffers
  for (int b = 0; b < 2; ++b) {
    waveformBuffers_[b].count = 0;
    for (int i = 0; i < AUDIO_BUFFER_SAMPLES; ++i) {
      waveformBuffers_[b].data[i] = 0;
    }
  }
  
  songMode_ = false;
  songPlayheadPosition_ = 0;
  songPlaybackSlot_ = sceneManager_.activeSongSlot();
  liveMixMode_ = false;
  patternModeDrumPatternIndex_ = 0;
  patternModeSynthPatternIndex_[0] = 0;
  patternModeSynthPatternIndex_[1] = 0;
  
  // Reset Retrig States
  retrigA_ = {};
  retrigB_ = {};
  patternPlaybackState_[0] = {};
  patternPlaybackState_[1] = {};
  patternRetrigEvent_[0] = {};
  patternRetrigEvent_[1] = {};
  for(int i=0; i<NUM_DRUM_VOICES; ++i) retrigDrums_[i] = {};

  LOG_PRINTLN("    - MiniAcid::reset: Done");
}

void MiniAcid::start() {
  LOG_PRINTLN("[DSP] START command received");
  // PatternPlayer takes exclusive ownership of the monophonic voices.
  allLiveNotesOff();
  publishPatternAllNotesOff_();
  playing = true;
  currentStepIndex = -1;
  // Force immediate first step trigger.
  tickPhaseAccum_ = 0x100000000ULL; // Trigger advance on first sample
  currentTick_ = 383; // Set to end of bar so first modulo triggers step 0
  currentTimingOffset_ = 0;
  songBarIndex_ = -1;
  if (songMode_) {
    if (!liveMixMode_) {
      songPlaybackSlot_ = sceneManager_.activeSongSlot();
    }
    songPlayheadPosition_ = clampSongPosition(sceneManager_.getSongPosition());
    sceneManager_.setSongPosition(songPlayheadPosition_);
    applySongPositionSelection();
  }
}

void MiniAcid::stop() {
  const uint8_t patternAuthorityAtEntry =
      patternOwnedMask_.load(std::memory_order_acquire);
  // Phrase D2 has already committed persistent Song/Pattern truth. STOP must
  // settle that exact pending destination immediately instead of discarding the
  // activation and leaving the next START on the old runtime row. Ordinary C
  // generation keeps its existing cancel+runtime-settlement behavior.
  const bool songSettled =
      GroovePuterRhythm::LiveSongArrangementDetail::
          settlePendingSongArrangementOnStop(*this);
  const bool phraseSettled = !songSettled &&
      GroovePuterRhythm::PhraseLiveArrangementDetail::
          settlePendingPhraseArrangementOnStop(*this);
  if (!songSettled && !phraseSettled &&
      GroovePuterRhythm::QuantizedGenerationDetail::
          cancelPendingGenerationActivation(*this)) {
    GroovePuterRhythm::QuantizedGenerationDetail::
        synchronizeCommittedGenerationRuntime(*this);
  }
  LOG_PRINTLN("[DSP] STOP command received");
  hardBarrierPatternPlayback_();
  playing = false;
  currentStepIndex = -1;
  tickPhaseAccum_ = 0;
  currentTick_ = 0;
  songBarIndex_ = -1;
  gateCountdownA_ = 0;
  gateCountdownB_ = 0;
  retrigA_ = {};
  retrigB_ = {};
  for (int i = 0; i < NUM_DRUM_VOICES; ++i) retrigDrums_[i] = {};
  cleanupLiveNotesForTransportBarrier_(patternAuthorityAtEntry);
  drums->reset();
  if (songMode_) {
    sceneManager_.setSongPosition(clampSongPosition(songPlayheadPosition_));
  }
}

void MiniAcid::pauseTransport() {
  if (!playing) return;
  const uint8_t patternAuthorityAtEntry =
      patternOwnedMask_.load(std::memory_order_acquire);
  LOG_PRINTLN("[DSP] PAUSE command received");
  hardBarrierPatternPlayback_();
  playing = false;
  currentStepIndex = -1;
  gateCountdownA_ = 0;
  gateCountdownB_ = 0;
  retrigA_ = {};
  retrigB_ = {};
  for (int i = 0; i < NUM_DRUM_VOICES; ++i) retrigDrums_[i] = {};
  cleanupLiveNotesForTransportBarrier_(patternAuthorityAtEntry);
  drums->reset();
  if (songMode_) {
    sceneManager_.setSongPosition(clampSongPosition(songPlayheadPosition_));
  }
}

void MiniAcid::continueTransport() {
  if (playing) return;
  LOG_PRINTLN("[DSP] CONTINUE command received");
  allLiveNotesOff();
  publishPatternAllNotesOff_();
  // Continue from a never-started engine behaves like MIDI Continue at song
  // position zero: step zero must still fire on the first rendered sample.
  if (currentTick_ == 0 && tickPhaseAccum_ == 0) {
    currentTick_ = 383;
    tickPhaseAccum_ = 0x100000000ULL;
  }
  playing = true;
}

void MiniAcid::liveNoteOn(int synthIndex, uint8_t midiNote, uint8_t velocity) {
  const int idx = clamp303Voice(synthIndex);
  const int note = clamp303Note(static_cast<int>(midiNote));
  if (!synthVoices_[idx]) return;
  if (velocity < 1) velocity = 1;
  if (velocity > 127) velocity = 127;

  synthVoices_[idx]->startNote(noteToFreq(note), false, false, velocity);
  liveNotes_[idx] = static_cast<int16_t>(note);
  if (idx == 0) gateCountdownA_ = 0;
  else gateCountdownB_ = 0;
}

void MiniAcid::liveNoteOff(int synthIndex, uint8_t midiNote) {
  const int idx = clamp303Voice(synthIndex);
  if (liveNotes_[idx] != static_cast<int16_t>(midiNote)) return;
  if (synthVoices_[idx]) synthVoices_[idx]->release();
  liveNotes_[idx] = -1;
}

void MiniAcid::allLiveNotesOff() {
  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {
    if (synthVoices_[idx]) synthVoices_[idx]->release();
    liveNotes_[idx] = -1;
  }
  gateCountdownA_ = 0;
  gateCountdownB_ = 0;
}

void MiniAcid::suspendLiveNoteProjection(int synthIndex) {
  liveNotes_[clamp303Voice(synthIndex)] = -1;
}

bool MiniAcid::patternOwnsInternalSynth(int synthIndex) const {
  const int idx = clamp303Voice(synthIndex);
  const uint8_t mask = static_cast<uint8_t>(1u << idx);
  return (patternOwnedMask_.load(std::memory_order_acquire) & mask) != 0u;
}

void MiniAcid::setPatternEventQueue(MusicalEventQueue* queue) {
  patternEventQueue_ = queue;
  patternMidiNotes_[0] = -1;
  patternMidiNotes_[1] = -1;
}

void MiniAcid::publishPatternNoteOn_(int synthIdx,
                                     uint8_t note,
                                     uint8_t velocity) {
  const int idx = clamp303Voice(synthIdx);
  patternOwnedMask_.fetch_or(
      static_cast<uint8_t>(1u << idx), std::memory_order_release);
  if (!patternEventQueue_) return;
  if (velocity < 1) velocity = 1;
  if (velocity > 127) velocity = 127;
  const MusicalEventTarget target = idx == 0
      ? MusicalEventTarget::SynthA
      : MusicalEventTarget::SynthB;
  const MusicalEvent event{
      MusicalEventType::NoteOn,
      MusicalEventSource::PatternPlayer,
      target,
      0,
      note,
      velocity,
  };
  if (patternEventQueue_->tryPush(event)) {
    patternMidiNotes_[idx] = static_cast<int16_t>(note);
  }
}

void MiniAcid::publishPatternNoteOff_(int synthIdx, uint8_t velocity) {
  const int idx = clamp303Voice(synthIdx);
  const uint8_t clearMask =
      static_cast<uint8_t>(~static_cast<uint8_t>(1u << idx));
  patternOwnedMask_.fetch_and(clearMask, std::memory_order_release);
  const int16_t note = patternMidiNotes_[idx];
  if (note < 0) return;
  if (patternEventQueue_) {
    const MusicalEventTarget target = idx == 0
        ? MusicalEventTarget::SynthA
        : MusicalEventTarget::SynthB;
    patternEventQueue_->tryPush(MusicalEvent{
        MusicalEventType::NoteOff,
        MusicalEventSource::PatternPlayer,
        target,
        0,
        static_cast<uint8_t>(note),
        velocity,
    });
  }
  // A failed critical enqueue records a target-scoped panic in the queue.
  patternMidiNotes_[idx] = -1;
}

void MiniAcid::publishPatternAllNotesOff_() {
  patternOwnedMask_.store(0u, std::memory_order_release);
  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {
    if (patternEventQueue_) {
      patternEventQueue_->tryPush(MusicalEvent{
          MusicalEventType::AllNotesOff,
          MusicalEventSource::PatternPlayer,
          idx == 0 ? MusicalEventTarget::SynthA : MusicalEventTarget::SynthB,
          0,
          0,
          0,
      });
    }
    patternMidiNotes_[idx] = -1;
  }
}

int MiniAcid::liveNote(int synthIndex) const {
  return liveNotes_[clamp303Voice(synthIndex)];
}

void MiniAcid::setBpm(float bpm) {
  projectBpmValue = bpm;
  if (projectBpmValue < 10.0f)
    projectBpmValue = 10.0f;
  if (projectBpmValue > 250.0f)
    projectBpmValue = 250.0f;
  bpmValue = projectBpmValue;
  updateTickIncrement();
  delay303.setBpm(bpmValue);
  delay3032.setBpm(bpmValue);
}

void MiniAcid::setExternalClockBpm(float bpm) {
  bpmValue = bpm;
  if (bpmValue < 5.0f) bpmValue = 5.0f;
  if (bpmValue > 300.0f) bpmValue = 300.0f;
  updateTickIncrement();
  delay303.setBpm(bpmValue);
  delay3032.setBpm(bpmValue);
}

void MiniAcid::restoreProjectBpm() {
  bpmValue = projectBpmValue;
  updateTickIncrement();
  delay303.setBpm(bpmValue);
  delay3032.setBpm(bpmValue);
}

void MiniAcid::setMasterOutputHighCutHz(float hz) {
  float nyquist = sampleRateValue * 0.5f - 200.0f;
  if (nyquist < 4000.0f) nyquist = 4000.0f;
  if (hz < 4000.0f) hz = 4000.0f;
  if (hz > nyquist) hz = nyquist;
  masterOutputHighCutHz_ = hz;
  const float omega = 2.0f * 3.14159265f * masterOutputHighCutHz_ / sampleRateValue;
  masterOutputLpAlpha_ = 1.0f - expf(-omega);
  if (masterOutputLpAlpha_ < 0.0f) masterOutputLpAlpha_ = 0.0f;
  if (masterOutputLpAlpha_ > 1.0f) masterOutputLpAlpha_ = 1.0f;
}

float MiniAcid::bpm() const { return bpmValue; }
float MiniAcid::sampleRate() const { return sampleRateValue; }

bool MiniAcid::isPlaying() const { return playing; }

int MiniAcid::currentStep() const { 
    if (!playing) return 0;
    return (currentTick_ % 384) / 24; 
}

float MiniAcid::getStepProgress() const {
    if (!playing) return 0.0f;
    uint32_t barTick = currentTick_ % 384;
    uint32_t tickInStep = barTick % 24;
    // Account for fractional phase for sub-tick visual smoothness
    double frac = (double)(tickPhaseAccum_ & 0xFFFFFFFF) / 4294967296.0;
    return (float)(tickInStep + frac) / 24.0f;
}

float MiniAcid::transportPhaseSteps() const {
    const uint32_t barTick = currentTick_ % 384;
    const double fractionalTick =
        static_cast<double>(tickPhaseAccum_ & 0xFFFFFFFFULL) /
        4294967296.0;
    return static_cast<float>(
        (static_cast<double>(barTick) + fractionalTick) / 24.0);
}

int MiniAcid::cycleBarCount() const {
  int bars = sceneManager_.currentScene().feel.patternBars;
  if (bars != 1 && bars != 2 && bars != 4 && bars != 8) bars = 1;
  return bars;
}

int MiniAcid::cycleBarIndex() const {
  int bars = cycleBarCount();
  int bar = songBarIndex_;
  if (bar < 0) bar = 0;
  if (bar >= bars) bar = bars - 1;
  return bar;
}

int16_t MiniAcid::currentDrumPatternIndex() const {
  return sceneManager_.getCurrentDrumPatternIndex();
}

int16_t MiniAcid::current303PatternIndex(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  return sceneManager_.getCurrentSynthPatternIndex(idx);
}

int16_t MiniAcid::currentDrumBankIndex() const {
  return sceneManager_.getCurrentBankIndex(0);
}

int16_t MiniAcid::current303BankIndex(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  return sceneManager_.getCurrentBankIndex(idx + 1);
}

bool MiniAcid::is303Muted(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  return idx == 0 ? mute303 : mute303_2;
}
bool MiniAcid::isKickMuted() const { return muteKick; }
bool MiniAcid::isSnareMuted() const { return muteSnare; }
bool MiniAcid::isHatMuted() const { return muteHat; }
bool MiniAcid::isOpenHatMuted() const { return muteOpenHat; }
bool MiniAcid::isMidTomMuted() const { return muteMidTom; }
bool MiniAcid::isHighTomMuted() const { return muteHighTom; }
bool MiniAcid::isRimMuted() const { return muteRim; }
bool MiniAcid::isClapMuted() const { return muteClap; }
bool MiniAcid::is303DelayEnabled(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  return idx == 0 ? delay303Enabled : delay3032Enabled;
}
bool MiniAcid::is303DistortionEnabled(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  return idx == 0 ? distortion303Enabled : distortion3032Enabled;
}
const Parameter& MiniAcid::parameter303(TB303ParamId id, int voiceIndex) const {
  static Parameter dummyParam("dummy", "", 0, 1, 0);
  const TB303Voice* v303 = tb303Voice(voiceIndex);
  if (v303) return v303->parameter(id);
  return dummyParam;
}
const int8_t* MiniAcid::pattern303Steps(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  refreshSynthCaches(idx);
  return synthNotesCache_[idx];
}
const bool* MiniAcid::pattern303AccentSteps(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  refreshSynthCaches(idx);
  return synthAccentCache_[idx];
}
const bool* MiniAcid::pattern303SlideSteps(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  refreshSynthCaches(idx);
  return synthSlideCache_[idx];
}
const bool* MiniAcid::patternKickSteps() const {
  refreshDrumCache(kDrumKickVoice);
  return drumHitCache_[kDrumKickVoice];
}
const bool* MiniAcid::patternSnareSteps() const {
  refreshDrumCache(kDrumSnareVoice);
  return drumHitCache_[kDrumSnareVoice];
}
const bool* MiniAcid::patternHatSteps() const {
  refreshDrumCache(kDrumHatVoice);
  return drumHitCache_[kDrumHatVoice];
}
const bool* MiniAcid::patternOpenHatSteps() const {
  refreshDrumCache(kDrumOpenHatVoice);
  return drumHitCache_[kDrumOpenHatVoice];
}
const bool* MiniAcid::patternMidTomSteps() const {
  refreshDrumCache(kDrumMidTomVoice);
  return drumHitCache_[kDrumMidTomVoice];
}
const bool* MiniAcid::patternHighTomSteps() const {
  refreshDrumCache(kDrumHighTomVoice);
  return drumHitCache_[kDrumHighTomVoice];
}
const bool* MiniAcid::patternRimSteps() const {
  refreshDrumCache(kDrumRimVoice);
  return drumHitCache_[kDrumRimVoice];
}
const bool* MiniAcid::patternClapSteps() const {
  refreshDrumCache(kDrumClapVoice);
  return drumHitCache_[kDrumClapVoice];
}
const bool* MiniAcid::patternDrumAccentSteps() const {
  int pat = songPatternIndexForTrack(SongTrack::Drums);
  const DrumPatternSet& set = pat >= 0 ? sceneManager_.getDrumPatternSet(pat)
                                       : kEmptyDrumPatternSet;
  for (int i = 0; i < SEQ_STEPS; ++i) {
    bool accent = false;
    for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
      if (set.voices[v].steps[i].accent) {
        accent = true;
        break;
      }
    }
    drumStepAccentCache_[i] = accent;
  }
  return drumStepAccentCache_;
}
const bool* MiniAcid::patternKickAccentSteps() const {
  refreshDrumCache(kDrumKickVoice);
  return drumAccentCache_[kDrumKickVoice];
}
const bool* MiniAcid::patternSnareAccentSteps() const {
  refreshDrumCache(kDrumSnareVoice);
  return drumAccentCache_[kDrumSnareVoice];
}
const bool* MiniAcid::patternHatAccentSteps() const {
  refreshDrumCache(kDrumHatVoice);
  return drumAccentCache_[kDrumHatVoice];
}
const bool* MiniAcid::patternOpenHatAccentSteps() const {
  refreshDrumCache(kDrumOpenHatVoice);
  return drumAccentCache_[kDrumOpenHatVoice];
}
const bool* MiniAcid::patternMidTomAccentSteps() const {
  refreshDrumCache(kDrumMidTomVoice);
  return drumAccentCache_[kDrumMidTomVoice];
}
const bool* MiniAcid::patternHighTomAccentSteps() const {
  refreshDrumCache(kDrumHighTomVoice);
  return drumAccentCache_[kDrumHighTomVoice];
}
const bool* MiniAcid::patternRimAccentSteps() const {
  refreshDrumCache(kDrumRimVoice);
  return drumAccentCache_[kDrumRimVoice];
}
const bool* MiniAcid::patternClapAccentSteps() const {
  refreshDrumCache(kDrumClapVoice);
  return drumAccentCache_[kDrumClapVoice];
}

bool MiniAcid::songModeEnabled() const { return songMode_; }

void MiniAcid::setSongMode(bool enabled) {
  if (enabled == songMode_) return;
  songBarIndex_ = -1;
  hardBarrierPatternPlayback_();
  if (enabled) {
    patternModeDrumPatternIndex_ = sceneManager_.getCurrentDrumPatternIndex();
    patternModeSynthPatternIndex_[0] = sceneManager_.getCurrentSynthPatternIndex(0);
    patternModeSynthPatternIndex_[1] = sceneManager_.getCurrentSynthPatternIndex(1);
    patternModeDrumBankIndex_ = sceneManager_.getCurrentBankIndex(0);
    patternModeSynthBankIndex_[0] = sceneManager_.getCurrentBankIndex(1);
    patternModeSynthBankIndex_[1] = sceneManager_.getCurrentBankIndex(2);
    songPlayheadPosition_ = clampSongPosition(sceneManager_.getSongPosition());
    sceneManager_.setSongPosition(songPlayheadPosition_);
    applySongPositionSelection();
  } else {
    sceneManager_.setCurrentDrumPatternIndex(patternModeDrumPatternIndex_);
    sceneManager_.setCurrentSynthPatternIndex(0, patternModeSynthPatternIndex_[0]);
    sceneManager_.setCurrentSynthPatternIndex(1, patternModeSynthPatternIndex_[1]);
    sceneManager_.setCurrentBankIndex(0, patternModeDrumBankIndex_);
    sceneManager_.setCurrentBankIndex(1, patternModeSynthBankIndex_[0]);
    sceneManager_.setCurrentBankIndex(2, patternModeSynthBankIndex_[1]);
  }
  songMode_ = enabled;
  sceneManager_.setSongMode(songMode_);
}

void MiniAcid::toggleSongMode() { setSongMode(!songMode_); }

bool MiniAcid::loopModeEnabled() const { return sceneManager_.loopMode(); }

void MiniAcid::setLoopMode(bool enabled) { sceneManager_.setLoopMode(enabled); }

void MiniAcid::setLoopRange(int startRow, int endRow) {
  sceneManager_.setLoopRange(startRow, endRow);
}

int MiniAcid::loopStartRow() const { return sceneManager_.loopStartRow(); }

int MiniAcid::loopEndRow() const { return sceneManager_.loopEndRow(); }

int MiniAcid::songLength() const { return sceneManager_.songLength(); }
void MiniAcid::setSongLength(int length) { sceneManager_.setSongLength(length); }

int MiniAcid::currentSongPosition() const { return sceneManager_.getSongPosition(); }

int MiniAcid::songPlayheadPosition() const { return songPlayheadPosition_; }

void MiniAcid::setSongPosition(int position) {
  int pos = clampSongPosition(position);
  sceneManager_.setSongPosition(pos);
  songBarIndex_ = -1;
  if (!playing) songPlayheadPosition_ = pos;
  if (songMode_) applySongPositionSelection();
}

void MiniAcid::setSongPattern(int position, SongTrack track, int16_t patternIndex) {
  sceneManager_.setSongPattern(position, track, patternIndex);
  if (songMode_ && position == currentSongPosition() &&
      activeSongSlot() == songPlaybackSlot_) {
    applySongPositionSelection();
  }
}

void MiniAcid::clearSongPattern(int position, SongTrack track) {
  sceneManager_.clearSongPattern(position, track);
  int pos = clampSongPosition(sceneManager_.getSongPosition());
  sceneManager_.setSongPosition(pos);
  if (songMode_ && position == pos &&
      activeSongSlot() == songPlaybackSlot_) {
    applySongPositionSelection();
  }
}

int16_t MiniAcid::songPatternAt(int position, SongTrack track) const {
  return sceneManager_.songPattern(position, track);
}

int16_t MiniAcid::songPatternAtSlot(int slot, int position, SongTrack track) const {
  return sceneManager_.songPatternAtSlot(slot, position, track);
}

const Song& MiniAcid::song() const { return sceneManager_.song(); }
int MiniAcid::activeSongSlot() const { return sceneManager_.activeSongSlot(); }
void MiniAcid::setActiveSongSlot(int slot) {
  sceneManager_.setActiveSongSlot(slot);
  // D3 separates persistent EDIT selection from runtime PLAY selection. During
  // transport, changing EDIT:A/B must never redirect the audible Song.
  if (!playing) {
    songBarIndex_ = -1;
    if (!liveMixMode_) {
      songPlaybackSlot_ = sceneManager_.activeSongSlot();
    }
    if (songMode_ && songPlaybackSlot_ == sceneManager_.activeSongSlot()) {
      applySongPositionSelection();
    }
  }
}
int MiniAcid::songPlaybackSlot() const { return songPlaybackSlot_; }
void MiniAcid::setSongPlaybackSlot(int slot) {
  if (slot < 0) slot = 0;
  if (slot > 1) slot = 1;
  if (songPlaybackSlot_ == slot) return;
  songPlaybackSlot_ = slot;
  songBarIndex_ = -1;
  if (songMode_) applySongPositionSelection();
}
bool MiniAcid::liveMixModeEnabled() const { return liveMixMode_; }
void MiniAcid::setLiveMixMode(bool enabled) {
  if (liveMixMode_ == enabled) return;
  liveMixMode_ = enabled;
  songBarIndex_ = -1;
  if (!liveMixMode_ && !playing) {
    songPlaybackSlot_ = sceneManager_.activeSongSlot();
    if (songMode_) applySongPositionSelection();
  }
}
void MiniAcid::toggleLiveMixMode() { setLiveMixMode(!liveMixMode_); }
void MiniAcid::mergeSongs() { sceneManager_.mergeSongs(); }
void MiniAcid::alternateSongs() { sceneManager_.alternateSongs(); }
void MiniAcid::insertSongRow(int position) { sceneManager_.insertSongRow(position); }
void MiniAcid::deleteSongRow(int position) { sceneManager_.deleteSongRow(position); }
void MiniAcid::setSongReverse(bool reverse) { sceneManager_.setSongReverse(reverse); }
bool MiniAcid::isSongReverse() const { return sceneManager_.isSongReverse(); }
void MiniAcid::queueSongReverseToggle() {
  if (playing && songMode_) return;
  sceneManager_.setSongReverse(!sceneManager_.isSongReverse());
}
bool MiniAcid::hasPendingSongReverseToggle() const { return false; }

int16_t MiniAcid::display303PatternIndex(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  if (songMode_) {
    int pos = clampSongPosition(sceneManager_.getSongPosition());
    int combined = sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos,
                                                   idx == 0 ? SongTrack::SynthA : SongTrack::SynthB);
    return combined; // Return global ID
  }
  // Return global ID for pattern mode too
  return songPatternFromPageBankIndex(currentPageIndex(), sceneManager_.getCurrentBankIndex(idx + 1), sceneManager_.getCurrentSynthPatternIndex(idx));
}

int16_t MiniAcid::displayDrumPatternIndex() const {
  if (songMode_) {
    int pos = clampSongPosition(sceneManager_.getSongPosition());
    int combined = sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, SongTrack::Drums);
    return combined; // Return global ID
  }
  // Return global ID for pattern mode too
  return songPatternFromPageBankIndex(currentPageIndex(), sceneManager_.getCurrentBankIndex(0), sceneManager_.getCurrentDrumPatternIndex());
}

int MiniAcid::display303LocalPatternIndex(int voiceIndex) const {
  int16_t global = display303PatternIndex(voiceIndex);
  if (global < 0) return -1;
  if (songPatternPage(global) != currentPageIndex()) return -1;
  if (songPatternBank(global) != current303BankIndex(voiceIndex)) return -1;
  return songPatternIndexInBank(global);
}

int MiniAcid::displayDrumLocalPatternIndex() const {
  int16_t global = displayDrumPatternIndex();
  if (global < 0) return -1;
  if (songPatternPage(global) != currentPageIndex()) return -1;
  if (songPatternBank(global) != currentDrumBankIndex()) return -1;
  return songPatternIndexInBank(global);
}

const Parameter& MiniAcid::synthParameter(int voiceIndex, int knobIndex) const {
  static Parameter dummyParam("dummy", "", 0, 1, 0);
  int idx = clamp303Voice(voiceIndex);
  if (synthVoices_[idx]) {
    return synthVoices_[idx]->getParameter(knobIndex);
  }
  return dummyParam;
}

uint8_t MiniAcid::synthParameterCount(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  if (synthVoices_[idx]) return synthVoices_[idx]->parameterCount();
  return 0;
}

void MiniAcid::adjustSynthParameter(int voiceIndex, int knobIndex, int steps) {
  int idx = clamp303Voice(voiceIndex);
  if (synthVoices_[idx]) {
    // We get a non-const copy or pointer if possible? 
    // No, we use setParameterNormalized.
    const Parameter& p = synthVoices_[idx]->getParameter(knobIndex);
    float val = p.value();
    float step = p.step();
    // If step is 0, we use a default 1/128 increment
    if (step <= 0.00001f) step = (p.max() - p.min()) / 128.0f;
    
    float newValue = val + step * steps;
    // Normalized calculation
    float norm = (newValue - p.min()) / (p.max() - p.min());
    if (p.max() <= p.min()) norm = 0.0f;
    synthVoices_[idx]->setParameterNormalized(knobIndex, norm);
  }
}

void MiniAcid::setSynthEngine(int voiceIndex, const std::string& engineName) {
  int idx = clamp303Voice(voiceIndex);
  if (!synthVoices_[idx]) {
    synthVoices_[idx] = std::make_unique<SwappableSynthVoice>(sampleRateValue, SynthEngineType::TB303);
    synthVoices_[idx]->setMode(sceneManager_.getMode());
    const float loFiAmt = sceneManager_.currentScene().feel.lofiAmount / 100.0f;
    synthVoices_[idx]->setLoFiAmount(loFiAmt);
  }

  std::string name = engineName;
  for (auto& c : name) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  LOG_DEBUG("[Synth] %d -> %s\n", idx, name.c_str());

  SynthEngineType target = SynthEngineType::TB303;
  const char* targetName = "TB303";
  if (name.find("WAVEMORPH") != std::string::npos ||
      name.find("WAVE MORPH") != std::string::npos) {
    target = SynthEngineType::WAVEMORPH;
    targetName = "WAVEMORPH";
  } else if (name.find("SH101") != std::string::npos ||
             name.find("SH-101") != std::string::npos ||
             name.find("MC202") != std::string::npos ||
             name.find("MC-202") != std::string::npos) {
    target = SynthEngineType::SH101;
    targetName = "SH101";
  } else if (name.find("SN76489") != std::string::npos ||
             name.find("SEGA") != std::string::npos) {
    target = SynthEngineType::SN76489;
    targetName = "SN76489";
  } else if (name.find("SID") != std::string::npos) {
    target = SynthEngineType::SID;
    targetName = "SID";
  } else if (name.find("OPL2") != std::string::npos ||
             name.find("YM3812") != std::string::npos ||
             name.find("FM") != std::string::npos) {
    target = SynthEngineType::TB303;
    targetName = "TB303";
  } else if (name.find("AY") != std::string::npos ||
             name.find("YM2149") != std::string::npos ||
             name.find("PSG") != std::string::npos) {
    target = SynthEngineType::AY;
    targetName = "AY";
  } else if (name.find("TB303") != std::string::npos ||
             name.find("303") != std::string::npos) {
    target = SynthEngineType::TB303;
    targetName = "TB303";
  }


  if (synthEngineNames_[idx] == targetName) {
    return;
  }

  const uint8_t patternAuthorityAtEntry =
      patternOwnedMask_.load(std::memory_order_acquire);
  if ((patternAuthorityAtEntry & static_cast<uint8_t>(1u << idx)) != 0u) {
    hardBarrierPatternPlayback_(idx);
  } else if (synthVoices_[idx]) {
    synthVoices_[idx]->release();
  }
  liveNotes_[idx] = -1;
  ++liveInputEpoch_;

  if (!playing) {
    SynthVoiceState blankState;
    blankState.engineType = target;
    blankState.paramCount = 0;
    synthVoices_[idx]->setState(blankState);
  } else {
    synthVoices_[idx]->setEngineType(target);
  }
  synthEngineNames_[idx] = synthVoices_[idx]->getEngineName();
  sceneManager_.setSynthEngineName(idx, synthEngineNames_[idx]);

  if (TB303Voice* v303 = tb303Voice(idx)) {
    const ModeConfig& cfg = modeManager_.config();
    v303->setSubOscillator(cfg.dsp.subOscillator);
    v303->setNoiseAmount(cfg.dsp.noiseAmount);
  }
}

std::vector<std::string> MiniAcid::getAvailableDrumEngines() const {
  return {"808", "909", "606", "CR78", "KPR77", "SP12"};
}

std::vector<std::string> MiniAcid::getAvailableSynthEngines() const {
  return {"TB303", "SID", "AY", "SH101", "SN76489", "WAVEMORPH"};
}

std::string MiniAcid::currentSynthEngineName(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  if (synthVoices_[idx]) return synthVoices_[idx]->getEngineName();
  return synthEngineNames_[idx];
}


void MiniAcid::setDrumEngine(const std::string& engineName) {
  std::string name = toLowerCopy(engineName);
  std::string targetEngine;
  if (name.find("909") != std::string::npos) {
    targetEngine = "909";
  } else if (name.find("606") != std::string::npos) {
    targetEngine = "606";
  } else if (name.find("808") != std::string::npos) {
    targetEngine = "808";
  } else if (name.find("cr78") != std::string::npos) {
    targetEngine = "CR78";
  } else if (name.find("kpr77") != std::string::npos) {
    targetEngine = "KPR77";
  } else if (name.find("sp12") != std::string::npos) {
    targetEngine = "SP12";
  }
  LOG_DEBUG("    - MiniAcid::setDrumEngine: setting to %s\n", name.c_str());
  if (targetEngine.empty()) {
    LOG_PRINTLN("    - MiniAcid::setDrumEngine: Unknown engine!");
    return;
  }
  if (drums && drumEngineName_ == targetEngine) {
    return;
  }

  if (targetEngine == "909") {
    drums = std::make_unique<TR909DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "909";
  } else if (targetEngine == "606") {
    drums = std::make_unique<TR606DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "606";
  } else if (targetEngine == "808") {
    drums = std::make_unique<TR808DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "808";
  } else if (targetEngine == "CR78") {
    drums = std::make_unique<CR78DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "CR78";
  } else if (targetEngine == "KPR77") {
    drums = std::make_unique<KPR77DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "KPR77";
  } else if (targetEngine == "SP12") {
    drums = std::make_unique<SP12DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "SP12";
  }
  if (drums) {
    LOG_PRINTLN("    - MiniAcid::setDrumEngine: resetting drums...");
    drums->reset();
  }
}

std::string MiniAcid::currentDrumEngineName() const {
  return drumEngineName_;
}

// Thread-safe waveform buffer access for UI
const MiniAcid::WaveformBuffer& MiniAcid::getWaveformBuffer() const {
  int idx = displayBufferIndex_.load(std::memory_order_acquire);
  return waveformBuffers_[idx];
}

void MiniAcid::toggleMute303(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  bool muted;
  if (idx == 0) {
    mute303 = !mute303;
    muted = mute303;
  } else {
    mute303_2 = !mute303_2;
    muted = mute303_2;
  }
  if (muted) hardBarrierPatternPlayback_(idx);
  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);
}
void MiniAcid::toggleMuteKick() {
  muteKick = !muteKick;
  LedManager::instance().onMuteChanged(muteKick, sceneManager_.currentScene().led);
}
void MiniAcid::toggleMuteSnare() {
  muteSnare = !muteSnare;
  LedManager::instance().onMuteChanged(muteSnare, sceneManager_.currentScene().led);
}
void MiniAcid::toggleMuteHat() {
  muteHat = !muteHat;
  LedManager::instance().onMuteChanged(muteHat, sceneManager_.currentScene().led);
}
void MiniAcid::toggleMuteOpenHat() {
  muteOpenHat = !muteOpenHat;
  LedManager::instance().onMuteChanged(muteOpenHat, sceneManager_.currentScene().led);
}
void MiniAcid::toggleMuteMidTom() {
  muteMidTom = !muteMidTom;
  LedManager::instance().onMuteChanged(muteMidTom, sceneManager_.currentScene().led);
}
void MiniAcid::toggleMuteHighTom() {
  muteHighTom = !muteHighTom;
  LedManager::instance().onMuteChanged(muteHighTom, sceneManager_.currentScene().led);
}
void MiniAcid::toggleMuteRim() {
  muteRim = !muteRim;
  LedManager::instance().onMuteChanged(muteRim, sceneManager_.currentScene().led);
}
void MiniAcid::toggleMuteClap() {
  muteClap = !muteClap;
  LedManager::instance().onMuteChanged(muteClap, sceneManager_.currentScene().led);
}

void MiniAcid::setMute303(int voiceIndex, bool muted) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0) mute303 = muted;
  else mute303_2 = muted;
  if (muted) hardBarrierPatternPlayback_(idx);
  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);
}

bool MiniAcid::isTrackActive(int index) const {
  switch (index) {
    case 0: return !mute303;
    case 1: return !mute303_2;
    case 2: return !muteKick;
    case 3: return !muteSnare;
    case 4: return !muteHat;
    case 5: return !muteOpenHat;
    case 6: return !muteMidTom;
    case 7: return !muteHighTom;
    case 8: return !muteRim;
    case 9: return !muteClap;
    default: return false;
  }
}

void MiniAcid::setTrackVolume(VoiceId id, float volume) {
    sceneManager_.setTrackVolume((int)id, volume);
}

float MiniAcid::getTrackVolume(VoiceId id) const {
    return sceneManager_.getTrackVolume((int)id);
}

void MiniAcid::toggleDelay303(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0) {
    delay303Enabled = !delay303Enabled;
    delay303.setEnabled(delay303Enabled);
  } else {
    delay3032Enabled = !delay3032Enabled;
    delay3032.setEnabled(delay3032Enabled);
  }
}
void MiniAcid::toggleDistortion303(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0) {
    distortion303Enabled = !distortion303Enabled;
    distortion303.setEnabled(distortion303Enabled);
  } else {
    distortion3032Enabled = !distortion3032Enabled;
    distortion3032.setEnabled(distortion3032Enabled);
  }
}

void MiniAcid::set303DelayEnabled(int voiceIndex, bool enabled) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0) {
    delay303Enabled = enabled;
    delay303.setEnabled(enabled);
  } else {
    delay3032Enabled = enabled;
    delay3032.setEnabled(enabled);
  }
}

void MiniAcid::set303DistortionEnabled(int voiceIndex, bool enabled) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0) {
    distortion303Enabled = enabled;
    distortion303.setEnabled(enabled);
  } else {
    distortion3032Enabled = enabled;
    distortion3032.setEnabled(enabled);
  }
}

void MiniAcid::setDrumPatternIndex(int16_t patternIndex) {
  sceneManager_.setCurrentDrumPatternIndex(patternIndex);
}

void MiniAcid::shiftDrumPatternIndex(int delta) {
  int current = sceneManager_.getCurrentDrumPatternIndex();
  int next = current + delta;
  if (next < 0) next = Bank<DrumPatternSet>::kPatterns - 1;
  if (next >= Bank<DrumPatternSet>::kPatterns) next = 0;
  sceneManager_.setCurrentDrumPatternIndex(next);
}

void MiniAcid::setDrumBankIndex(int bankIndex) {
  sceneManager_.setCurrentBankIndex(0, bankIndex);
}

void MiniAcid::adjust303Parameter(TB303ParamId id, int steps, int voiceIndex) {
  if (TB303Voice* v303 = tb303Voice(voiceIndex)) v303->adjustParameter(id, steps);
}
void MiniAcid::set303Parameter(TB303ParamId id, float value, int voiceIndex) {
  if (TB303Voice* v303 = tb303Voice(voiceIndex)) v303->setParameter(id, value);
}
void MiniAcid::set303ParameterNormalized(TB303ParamId id, float norm, int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  if (synthVoices_[idx]) {
      synthVoices_[idx]->setParameterNormalized(static_cast<uint8_t>(id), norm);
  }
}
void MiniAcid::set303PatternIndex(int voiceIndex, int16_t patternIndex) {
  int idx = clamp303Voice(voiceIndex);
  hardBarrierPatternPlayback_(idx);
  sceneManager_.setCurrentSynthPatternIndex(idx, patternIndex);
}
void MiniAcid::shift303PatternIndex(int voiceIndex, int delta) {
  int idx = clamp303Voice(voiceIndex);
  hardBarrierPatternPlayback_(idx);
  int current = sceneManager_.getCurrentSynthPatternIndex(idx);
  int next = current + delta;
  if (next < 0) next = Bank<SynthPattern>::kPatterns - 1;
  if (next >= Bank<SynthPattern>::kPatterns) next = 0;
  sceneManager_.setCurrentSynthPatternIndex(idx, next);
}

void MiniAcid::set303BankIndex(int voiceIndex, int bankIndex) {
  int idx = clamp303Voice(voiceIndex);
  hardBarrierPatternPlayback_(idx);
  sceneManager_.setCurrentBankIndex(idx + 1, bankIndex);
}

void MiniAcid::setCurrentPage(int8_t page) {
  hardBarrierPatternPlayback_();
  currentPage_.store(page, std::memory_order_release);
}

void MiniAcid::requestPageSwitch(int pageIndex) {
    if (pageIndex < 0 || pageIndex >= kMaxPages) return;
    if (pageIndex == currentPageIndex() && targetPageIndex() == -1) return;
    
    setTargetPage(pageIndex);
    
    if (!approachB_Enabled_) {
        // Approach A: Synchronous (Risky in audio thread!)
        // ...
    } else {
        // Approach B: Asynchronous (Signal UI thread)
        setPageLoading(true);
    }
}
void MiniAcid::adjust303StepNote(int voiceIndex, int stepIndex, int semitoneDelta) {
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  int note = pattern.steps[step].note;
  
  if (note == -2) {
      if (semitoneDelta > 0) pattern.steps[step].note = -1;
      return;
  }
  if (note == -1) {
      if (semitoneDelta > 0) pattern.steps[step].note = kMin303Note;
      else if (semitoneDelta < 0) pattern.steps[step].note = -2;
      return;
  }

  note += semitoneDelta;
  if (note < kMin303Note) {
    pattern.steps[step].note = -1;
    return;
  }
  note = clamp303Note(note);
  pattern.steps[step].note = static_cast<int8_t>(note);
}
void MiniAcid::adjust303StepOctave(int voiceIndex, int stepIndex, int octaveDelta) {
  adjust303StepNote(voiceIndex, stepIndex, octaveDelta * 12);
}
void MiniAcid::clear303StepNote(int voiceIndex, int stepIndex) {
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  pattern.steps[step].note = -1;
}
void MiniAcid::toggle303AccentStep(int voiceIndex, int stepIndex) {
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  pattern.steps[step].accent = !pattern.steps[step].accent;
}
void MiniAcid::toggle303SlideStep(int voiceIndex, int stepIndex) {
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  pattern.steps[step].slide = !pattern.steps[step].slide;
}

void MiniAcid::toggleDrumStep(int voiceIndex, int stepIndex) {
  int voice = clampDrumVoice(voiceIndex);
  int step = stepIndex;
  if (step < 0) step = 0;
  if (step >= DrumPattern::kSteps) step = DrumPattern::kSteps - 1;
  DrumPattern& pattern = editDrumPattern(voice);
  pattern.steps[step].hit = !pattern.steps[step].hit;
}

void MiniAcid::toggleDrumAccentStep(int stepIndex) {
  int step = stepIndex;
  if (step < 0) step = 0;
  if (step >= DrumPattern::kSteps) step = DrumPattern::kSteps - 1;
  DrumPatternSet& patternSet = sceneManager_.editCurrentDrumPattern();
  bool anyAccent = false;
  for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
    if (patternSet.voices[v].steps[step].accent) {
      anyAccent = true;
      break;
    }
  }
  bool newAccent = !anyAccent;
  for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
    patternSet.voices[v].steps[step].accent = newAccent;
  }
}

void MiniAcid::setDrumAccentStep(int voiceIndex, int stepIndex, bool accent) {
  int voice = clampDrumVoice(voiceIndex);
  int step = stepIndex;
  if (step < 0) step = 0;
  if (step >= DrumPattern::kSteps) step = DrumPattern::kSteps - 1;
  DrumPattern& pattern = editDrumPattern(voice);
  pattern.steps[step].accent = accent;
}

void MiniAcid::cycle303StepFx(int voiceIndex, int stepIndex) {
  constexpr uint8_t kDefaultRetrigCount = 2;
  constexpr uint8_t kMaxRetrigCount = 8;
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  SynthStep& value = pattern.steps[step];

  if (value.fx == static_cast<uint8_t>(StepFx::Retrig) &&
      value.fxParam >= 1 && value.fxParam <= kMaxRetrigCount) {
    value.fx = static_cast<uint8_t>(StepFx::None);
    value.fxParam = 0;
    return;
  }

  // Reverse remains valid for sampled drums, but oscillator synth playback has
  // no defined Reverse consumer. Dead/legacy synth FX states, including R0,
  // normalize to an audible bounded Retrig on the first F press.
  value.fx = static_cast<uint8_t>(StepFx::Retrig);
  value.fxParam = kDefaultRetrigCount;
}

void MiniAcid::adjust303StepFxParam(int voiceIndex, int stepIndex, int delta) {
  constexpr uint8_t kMaxRetrigCount = 8;
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  SynthStep& value = pattern.steps[step];
  if (value.fx != static_cast<uint8_t>(StepFx::Retrig)) return;

  int count = static_cast<int>(value.fxParam) + delta;
  if (count < 1) count = 1;
  if (count > kMaxRetrigCount) count = kMaxRetrigCount;
  value.fxParam = static_cast<uint8_t>(count);
}

TB303Voice* MiniAcid::tb303Voice(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  if (!synthVoices_[idx]) return nullptr;
  if (synthVoices_[idx]->engineType() != SynthEngineType::TB303) return nullptr;
  return static_cast<TB303Voice*>(synthVoices_[idx]->activeVoice());
}

const TB303Voice* MiniAcid::tb303Voice(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  if (!synthVoices_[idx]) return nullptr;
  if (synthVoices_[idx]->engineType() != SynthEngineType::TB303) return nullptr;
  return static_cast<const TB303Voice*>(synthVoices_[idx]->activeVoice());
}

int MiniAcid::clamp303Voice(int voiceIndex) const {
  if (voiceIndex < 0) return 0;
  if (voiceIndex >= NUM_303_VOICES) return NUM_303_VOICES - 1;
  return voiceIndex;
}
int MiniAcid::clampDrumVoice(int voiceIndex) const {
  if (voiceIndex < 0) return 0;
  if (voiceIndex >= NUM_DRUM_VOICES) return NUM_DRUM_VOICES - 1;
  return voiceIndex;
}

// Helper: set up retrig/flam/roll state for a drum voice from its step FX.
// Called from advanceStep after a drum hit triggers.
void MiniAcid::setupDrumStepFx_(int voiceIdx, uint8_t fx, uint8_t fxParam, uint8_t velocity) {
  RetrigState& rs = retrigDrums_[voiceIdx];
  rs.flamGhostVelocity = 0;
  rs.rollTotal = 0;

  if (fx == DRUM_FX_RETRIG && fxParam > 0) {
    rs.countRemaining = fxParam;
    rs.interval = (int)(samplesPerStep_ / (fxParam + 1));
    if (rs.interval < 1) rs.interval = 1;
    rs.counter = rs.interval;
    rs.active = true;
  } else if (fx == DRUM_FX_FLAM) {
    // Flam: one extra hit after fxParam ticks (24 ticks per 16th step).
    int gapTicks = (fxParam > 0) ? fxParam : 2;
    if (gapTicks > 23) gapTicks = 23;
    rs.countRemaining = 1;
    rs.interval = (int)((samplesPerStep_ * gapTicks) / 24.0f);
    if (rs.interval < 1) rs.interval = 1;
    rs.counter = rs.interval;
    rs.flamGhostVelocity = (uint8_t)((int)velocity * 72 / 100);
    rs.active = true;
  } else if (fx == DRUM_FX_ROLL) {
    // Roll: subdivide current step into fxParam total hits with crescendo.
    int hitCount = fxParam;
    if (hitCount < 2) {
      rs.active = false;
      return;
    }
    if (hitCount > 12) hitCount = 12;
    rs.rollTotal = hitCount - 1;   // Base hit already played in advanceStep().
    rs.countRemaining = rs.rollTotal;
    rs.interval = (int)(samplesPerStep_ / hitCount);
    if (rs.interval < 1) rs.interval = 1;
    rs.counter = rs.interval;
    rs.active = true;
  } else {
    rs.active = false;
  }
}
int MiniAcid::clamp303Step(int stepIndex) const {
  if (stepIndex < 0) return 0;
  if (stepIndex >= SEQ_STEPS) return SEQ_STEPS - 1;
  return stepIndex;
}
int MiniAcid::clamp303Note(int note) const {
  if (note < kMin303Note) return kMin303Note;
  if (note > kMax303Note) return kMax303Note;
  return note;
}

const SynthPattern& MiniAcid::synthPattern(int synthIndex) const {
  int idx = clamp303Voice(synthIndex);
  return sceneManager_.getCurrentSynthPattern(idx);
}

SynthPattern& MiniAcid::editSynthPattern(int synthIndex) {
  int idx = clamp303Voice(synthIndex);
  return sceneManager_.editCurrentSynthPattern(idx);
}

const DrumPattern& MiniAcid::drumPattern(int drumVoiceIndex) const {
  int idx = clampDrumVoice(drumVoiceIndex);
  const DrumPatternSet& patternSet = sceneManager_.getCurrentDrumPattern();
  return patternSet.voices[idx];
}

DrumPattern& MiniAcid::editDrumPattern(int drumVoiceIndex) {
  int idx = clampDrumVoice(drumVoiceIndex);
  DrumPatternSet& patternSet = sceneManager_.editCurrentDrumPattern();
  return patternSet.voices[idx];
}

int MiniAcid::songPatternIndexForTrack(SongTrack track) const {
  if (!songMode_) {
    switch (track) {
    case SongTrack::SynthA:
      return sceneManager_.getCurrentSynthPatternIndex(0);
    case SongTrack::SynthB:
      return sceneManager_.getCurrentSynthPatternIndex(1);
    case SongTrack::Drums:
      return sceneManager_.getCurrentDrumPatternIndex();
    default:
      return -1;
    }
  }
  int pos = clampSongPosition(sceneManager_.getSongPosition());
  int combined = sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, track);
  if (combined < 0) return -1;
  return songPatternIndexInBank(combined);
}

const SynthPattern& MiniAcid::activeSynthPattern(int synthIndex) const {
  int idx = clamp303Voice(synthIndex);
  if (const SynthPattern* pending =
          GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleSynthPattern(
              *this, idx)) {
    return *pending;
  }
  SongTrack track = idx == 0 ? SongTrack::SynthA : SongTrack::SynthB;
  int pat = songPatternIndexForTrack(track);
  if (pat < 0) return kEmptySynthPattern;
  return sceneManager_.getSynthPattern(idx, pat);
}

const DrumPattern& MiniAcid::activeDrumPattern(int drumVoiceIndex) const {
  int idx = clampDrumVoice(drumVoiceIndex);
  int pat = songPatternIndexForTrack(SongTrack::Drums);
  if (const DrumPatternSet* pending =
          GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(
              *this)) {
    return pending->voices[idx];
  }
  const DrumPatternSet& set = pat >= 0 ? sceneManager_.getDrumPatternSet(pat)
                                       : kEmptyDrumPatternSet;
  return set.voices[idx];
}

int MiniAcid::clampSongPosition(int position) const {
  int len = songMode_ ? sceneManager_.songLengthAtSlot(songPlaybackSlot_) : sceneManager_.songLength();
  if (len < 1) len = 1;
  if (position < 0) return 0;
  if (position >= len) return len - 1;
  if (position >= Song::kMaxPositions) return Song::kMaxPositions - 1;
  return position;
}

void MiniAcid::applySongPositionSelection() {
  if (!songMode_) return;
  hardBarrierPatternPlayback_();
  int pos = clampSongPosition(sceneManager_.getSongPosition());
  sceneManager_.setSongPosition(pos);
  songPlayheadPosition_ = pos;
  int patA = sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, SongTrack::SynthA);
  int patB = sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, SongTrack::SynthB);
  int patD = sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, SongTrack::Drums);
  int patV = sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, SongTrack::Voice);

  // Check for auto-paging
  int firstGlobal = -1;
  if (patA >= 0) firstGlobal = patA;
  else if (patB >= 0) firstGlobal = patB;
  else if (patD >= 0) firstGlobal = patD;

  if (firstGlobal >= 0) {
      int tPage = songPatternPage(firstGlobal);
      if (tPage != currentPageIndex()) {
          requestPageSwitch(tPage);
      }
  }

  if (playing && patV >= 0) {
    if (patV < 16) {
      speakPhrase(patV);
    } else {
      speakCustomPhrase(patV - 16);
    }
  }

  if (patA < 0) {
    sceneManager_.setCurrentBankIndex(1, patternModeSynthBankIndex_[0]);
    sceneManager_.setCurrentSynthPatternIndex(0, patternModeSynthPatternIndex_[0]);
  } else {
    int bank = songPatternBank(patA);
    int pat = songPatternIndexInBank(patA);
    if (bank < 0) bank = 0;
    if (bank >= kBankCount) bank = kBankCount - 1;
    sceneManager_.setCurrentBankIndex(1, bank);
    sceneManager_.setCurrentSynthPatternIndex(0, pat);
  }

  if (patB < 0) {
    sceneManager_.setCurrentBankIndex(2, patternModeSynthBankIndex_[1]);
    sceneManager_.setCurrentSynthPatternIndex(1, patternModeSynthPatternIndex_[1]);
  } else {
    int bank = songPatternBank(patB);
    int pat = songPatternIndexInBank(patB);
    if (bank < 0) bank = 0;
    if (bank >= kBankCount) bank = kBankCount - 1;
    sceneManager_.setCurrentBankIndex(2, bank);
    sceneManager_.setCurrentSynthPatternIndex(1, pat);
  }

  if (patD < 0) {
    sceneManager_.setCurrentBankIndex(0, patternModeDrumBankIndex_);
    sceneManager_.setCurrentDrumPatternIndex(patternModeDrumPatternIndex_);
  } else {
    int bank = songPatternBank(patD);
    int pat = songPatternIndexInBank(patD);
    if (bank < 0) bank = 0;
    if (bank >= kBankCount) bank = kBankCount - 1;
    sceneManager_.setCurrentBankIndex(0, bank);
    sceneManager_.setCurrentDrumPatternIndex(pat);
  }
}

// REWRITTEN LOGIC
void MiniAcid::acknowledgeRehearsal() {
  if (waitingForRehearsal_) {
    waitingForRehearsal_ = false;
    rehearsalAcknowledged_ = true;
    LOG_PRINTLN("Rehearsal acknowledged, resuming playback");
  }
}

void MiniAcid::advanceSongPlayhead() {
  int len = sceneManager_.songLengthAtSlot(songPlaybackSlot_);
  if (len < 1) len = 1;

  bool rev = sceneManager_.isSongReverseAtSlot(songPlaybackSlot_);
  bool loop = sceneManager_.loopMode();
  int loopStart = sceneManager_.loopStartRow();
  int loopEnd = sceneManager_.loopEndRow();

  // Clamp loop range safety
  if (loopStart < 0) loopStart = 0;
  if (loopEnd < 0) loopEnd = 0;
  if (loopStart >= len) loopStart = len - 1;
  if (loopEnd >= len) loopEnd = len - 1;
  if (loopStart > loopEnd) {
      int tmp = loopStart;
      loopStart = loopEnd;
      loopEnd = tmp;
  }

  // Current position (from SceneManager to be safe, though local should verify)
  int currentPos = sceneManager_.getSongPosition();
  int nextPos = currentPos;

  if (loop) {
      // Loop Mode Rules
      
      // 1. Catch-up: if current is outside loop, jump in immediately
      if (currentPos < loopStart || currentPos > loopEnd) {
          nextPos = rev ? loopEnd : loopStart;
      } else {
          // Inside loop
          if (rev) {
              nextPos--;
              if (nextPos < loopStart) nextPos = loopEnd;
          } else {
              nextPos++;
              if (nextPos > loopEnd) nextPos = loopStart;
          }
      }
  } else {
      // No Loop Mode
      if (rev) {
          nextPos--;
          if (nextPos < 0) nextPos = len - 1;
      } else {
          nextPos++;
          if (nextPos >= len) nextPos = 0;
      }
  }

  // REHEARSAL MODE (Pause Rows)
  // Check if next row contains the pause sentinel (-2) on any track
  bool rowIsPause = false;
  for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      if (sceneManager_.songPatternAtSlot(songPlaybackSlot_, nextPos, (SongTrack)t) == -2) {
          rowIsPause = true;
          break;
      }
  }

  if (rowIsPause && !rehearsalAcknowledged_) {
      waitingForRehearsal_ = true;
      // DO NOT advance nextPos - stay on current row
      return;
  }
  
  // Clear acknowledgment once we move past a pause row or hit a normal row
  rehearsalAcknowledged_ = false;
  waitingForRehearsal_ = false;

  // Final Safety clamp
  if (nextPos < 0) nextPos = 0;
  if (nextPos >= len) nextPos = len - 1;

  // Update SceneManager FIRST so applySongPositionSelection sees new value
  sceneManager_.setSongPosition(nextPos);
  
  // Propagate to UI/Engine state (including playhead local var)
  applySongPositionSelection();
}

void MiniAcid::refreshSynthCaches(int synthIndex) const {
  int idx = clamp303Voice(synthIndex);
  const SynthPattern& pattern = activeSynthPattern(idx);
  for (int i = 0; i < SEQ_STEPS; ++i) {
    synthNotesCache_[idx][i] = static_cast<int8_t>(pattern.steps[i].note);
    synthAccentCache_[idx][i] = pattern.steps[i].accent;
    synthSlideCache_[idx][i] = pattern.steps[i].slide;
  }
}

void MiniAcid::refreshDrumCache(int drumVoiceIndex) const {
  int idx = clampDrumVoice(drumVoiceIndex);
  const DrumPattern& pattern = activeDrumPattern(idx);
  for (int i = 0; i < SEQ_STEPS; ++i) {
    drumHitCache_[idx][i] = pattern.steps[i].hit;
    drumAccentCache_[idx][i] = pattern.steps[i].accent && pattern.steps[i].hit;
  }
}

void MiniAcid::updateTickIncrement() {
  // Q32.32 math: inc = (ticksPerSec * 2^32) / sampleRate
  double ticksPerSec = (double)bpmValue * (double)kPPQN / 60.0;
  tickPhaseInc_ = (uint64_t)((ticksPerSec * 4294967296.0) / (double)SAMPLE_RATE);

  // Legacy fallback for gate lengths and envelope durations expecting sample counts
  float effectiveSteps = 16.0f; 
  samplesPerStep_ = (float)SAMPLE_RATE * 240.0f / (bpmValue * effectiveSteps);
}

float MiniAcid::noteToFreq(int note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

int MiniAcid::grooveOverrideTicksForStep_(const DrumPatternSet& patternSet, int stepIndex) const {
  const PatternGroove& groove = patternSet.groove;
  int ticks = 0;

  if (groove.swing >= 0.0f) {
    float swing = groove.swing;
    if (swing < 0.0f) swing = 0.0f;
    if (swing > 0.66f) swing = 0.66f;
    if ((stepIndex & 1) != 0) {
      ticks += static_cast<int>(std::round(swing * 24.0f));
    }
  }

  if (groove.humanize >= 0.0f) {
    float humanize = groove.humanize;
    if (humanize < 0.0f) humanize = 0.0f;
    if (humanize > 1.0f) humanize = 1.0f;
    const int range = static_cast<int>(std::round(humanize * 6.0f));
    if (range > 0) {
      const int patternIndex = sceneManager_.getCurrentDrumPatternIndex();
      const int bankIndex = sceneManager_.getCurrentBankIndex(0);
      uint32_t seed = static_cast<uint32_t>(stepIndex)
                    ^ (static_cast<uint32_t>(patternIndex + 17) * 2654435761u)
                    ^ (static_cast<uint32_t>(bankIndex + 29) * 2246822519u)
                    ^ (static_cast<uint32_t>(cyclePulseCounter_ + 1) * 3266489917u);
      seed ^= (seed >> 15);
      const int jitter = static_cast<int>(seed % static_cast<uint32_t>(range * 2 + 1)) - range;
      ticks += jitter;
    }
  }

  if (ticks < -63) ticks = -63;
  if (ticks > 63) ticks = 63;
  return ticks;
}

int MiniAcid::timingTicksForStep_(int stepIndex) const {
  int step = stepIndex;
  if (step < 0) step = 0;
  if (step >= SEQ_STEPS) step %= SEQ_STEPS;

  int ticks = static_cast<int>(sceneManager_.getCurrentSynthPattern(0).steps[step].timing);
  ticks += grooveOverrideTicksForStep_(sceneManager_.getCurrentDrumPattern(), step);
  if (ticks < -127) ticks = -127;
  if (ticks > 127) ticks = 127;
  return ticks;
}

float MiniAcid::evaluateAutomationLaneAtStep_(const AutomationLane& lane, int step) const {
  if (lane.nodeCount == 0) return 0.0f;

  int minIdx = 0;
  int maxIdx = 0;
  int minStep = 16;
  int maxStep = -1;
  int prevIdx = -1;
  int nextIdx = -1;
  int prevStep = -1;
  int nextStep = 16;

  const int count = static_cast<int>(lane.nodeCount > AutomationLane::kMaxNodes ? AutomationLane::kMaxNodes : lane.nodeCount);
  for (int i = 0; i < count; ++i) {
    int s = lane.nodes[i].step;
    if (s < 0) s = 0;
    if (s > 15) s = 15;
    if (s < minStep) {
      minStep = s;
      minIdx = i;
    }
    if (s > maxStep) {
      maxStep = s;
      maxIdx = i;
    }
    if (s <= step && s > prevStep) {
      prevStep = s;
      prevIdx = i;
    }
    if (s >= step && s < nextStep) {
      nextStep = s;
      nextIdx = i;
    }
  }

  if (prevIdx < 0) {
    prevIdx = minIdx;
    prevStep = minStep;
  }
  if (nextIdx < 0) {
    nextIdx = maxIdx;
    nextStep = maxStep;
  }

  auto clamp01 = [](float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  };

  const float prevValue = clamp01(lane.nodes[prevIdx].value);
  const float nextValue = clamp01(lane.nodes[nextIdx].value);
  if (prevIdx == nextIdx || prevStep == nextStep) return prevValue;

  float t = static_cast<float>(step - prevStep) / static_cast<float>(nextStep - prevStep);
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  switch (lane.nodes[prevIdx].curveType) {
    case 1: t = t * t; break; // EaseIn
    case 2: t = 1.0f - ((1.0f - t) * (1.0f - t)); break; // EaseOut
    default: break; // Linear
  }

  return clamp01(prevValue + (nextValue - prevValue) * t);
}

void MiniAcid::applyDrumAutomationLanesForStep_(const DrumPatternSet& patternSet, int step) {
  Scene& scene = sceneManager_.currentScene();
  static const char* kEngineByLaneValue[] = {"808", "909", "606", "CR78", "KPR77", "SP12"};
  constexpr int kEngineCount = static_cast<int>(sizeof(kEngineByLaneValue) / sizeof(kEngineByLaneValue[0]));

  for (int i = 0; i < DrumPatternSet::kMaxLanes; ++i) {
    const AutomationLane& lane = patternSet.lanes[i];
    if (lane.nodeCount == 0 || lane.targetParam == DRUM_AUTOMATION_NONE) continue;

    const float value = evaluateAutomationLaneAtStep_(lane, step);
    switch (lane.targetParam) {
      case DRUM_AUTOMATION_REVERB_MIX:
        scene.drumFX.reverbMix = value;
        updateDrumReverbMix(value);
        break;
      case DRUM_AUTOMATION_COMPRESSION:
        scene.drumFX.compression = value;
        updateDrumCompression(value);
        break;
      case DRUM_AUTOMATION_TRANSIENT_ATTACK:
        scene.drumFX.transientAttack = value;
        updateDrumTransientAttack(value);
        break;
      case DRUM_AUTOMATION_ENGINE_SWITCH: {
        int engineIdx = static_cast<int>(value * static_cast<float>(kEngineCount));
        if (engineIdx < 0) engineIdx = 0;
        if (engineIdx >= kEngineCount) engineIdx = kEngineCount - 1;
        setDrumEngine(kEngineByLaneValue[engineIdx]);
        break;
      }
      default:
        break;
    }
  }
}

void MiniAcid::advanceTick() {
  // Dispatch every PPQN tick. processSequencerEvents() keeps logical
  // step and bar-boundary side effects on their exact boundaries.
  processSequencerEvents(currentTick_);
}

void MiniAcid::processSequencerEvents(uint32_t absoluteTick) {
  uint32_t barTick = absoluteTick % 384;
  currentStepIndex = barTick / 24;

  if (barTick == 0) {
    // Musical bar boundary: activate queued GO requests
    for (int synth = 0; synth < NUM_303_VOICES; ++synth) {
      if (goQueued_[synth]) {
        goQueued_[synth] = false;
        if (pendingMaterial_[synth].queued &&
            pendingMaterial_[synth].lifecycleBound &&
            goQueuedGeneration_[synth] == pendingGeneration_[synth]) {
          activateNextMaterialAtBoundary(synth);
        }
      }
    }

    // Keep the accepted BAR_START pending owner and Song ordering intact.
    if (genreManager_.commitPendingRecipe()) {
      regeneratePatternsWithGenre();
    }
    advanceSongBar_();
    LedManager::instance().onBeat(currentStepIndex, sceneManager_.currentScene().led);
  } else if (barTick % 24 == 0) {
    LedManager::instance().onBeat(currentStepIndex, sceneManager_.currentScene().led);
  }

  // P2 changes the Synth material/lifetime source, not legacy trigger/RNG
  // ordering. Keep the physical source-step scan and A -> B -> drums order.
  const uint32_t absoluteStartSubtick =
      absoluteTick * static_cast<uint32_t>(PhraseRuntime::kSubticksPerTick);
  const PhraseRuntime::RuntimePatternEventBuffer& synthAEvents =
      activePatternRuntimeEvents(0);
  const PhraseRuntime::RuntimePatternEventBuffer& synthBEvents =
      activePatternRuntimeEvents(1);

  int swingPct = GroovePuterRhythm::QuantizedGenerationDetail::audibleGenerationSwingPct(
      *this, sceneManager_.currentScene().feel.swingPct);
  if (swingPct < 50) swingPct = 50;
  if (swingPct > 75) swingPct = 75;
  int swingDelay = (int)std::round((swingPct - 50.0f) * 24.0f / 50.0f);
  uint16_t swingMask = sceneManager_.currentScene().feel.swingMask;

  int nominalStep = barTick / 24;
  for (int sIdx = nominalStep - 1; sIdx <= nominalStep + 1; ++sIdx) {
    int s = (sIdx + 16) % 16;
    uint32_t nominalT = s * 24;

    // PHRASE addresses onsets in phrase-relative time and resolves once per
    // tick, at the nominal step, so the per-step A -> B -> drums draw order is
    // untouched. PATTERN keeps its bar-local source-step scan verbatim.
    if (activeMaterial_[0].kind == GroovePuterMaterial::MaterialKind::Melody) {
      if (s == nominalStep) {
        if (const PhraseRuntime::RuntimeSynthEvent* phraseA =
                phraseEventAt_(0, absoluteTick)) {
          triggerSynthStep_(0, *phraseA, absoluteStartSubtick);
        }
      }
    } else if (const PhraseRuntime::RuntimeSynthEvent* eventA =
                   synthAEvents.eventForSourceStep(static_cast<uint8_t>(s));
               eventA != nullptr && eventA->startTick == barTick) {
      triggerSynthStep_(0, *eventA, absoluteStartSubtick);
    }
    if (activeMaterial_[1].kind == GroovePuterMaterial::MaterialKind::Melody) {
      if (s == nominalStep) {
        if (const PhraseRuntime::RuntimeSynthEvent* phraseB =
                phraseEventAt_(1, absoluteTick)) {
          triggerSynthStep_(1, *phraseB, absoluteStartSubtick);
        }
      }
    } else if (const PhraseRuntime::RuntimeSynthEvent* eventB =
                   synthBEvents.eventForSourceStep(static_cast<uint8_t>(s));
               eventB != nullptr && eventB->startTick == barTick) {
      triggerSynthStep_(1, *eventB, absoluteStartSubtick);
    }

    const DrumPatternSet* pendingDrums =
        GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(*this);
    const DrumPatternSet& dSet = pendingDrums
        ? *pendingDrums
        : sceneManager_.getCurrentDrumPattern();
    for (int v = 0; v < 8; ++v) {
      VoiceId vId = (VoiceId)((int)VoiceId::DrumKick + v);
      int swingD = (s % 2 != 0 && (swingMask & (1 << (int)vId))) ? swingDelay : 0;
      int microD = dSet.voices[v].steps[s].timing;
      if ((nominalT + swingD + microD + 384) % 384 == barTick) {
        triggerDrumVoice_(v, s);
      }
    }
  }
}



void MiniAcid::generateAudioBuffer(int16_t *buffer, size_t numSamples) {
  if (!buffer || numSamples == 0) return;

  // Test Tone Mode (Hardware diagnostic)
  if (testToneEnabled_) {
    for (size_t i = 0; i < numSamples; ++i) {
      testTonePhase_ += 440.0f / sampleRateValue;
      if (testTonePhase_ >= 1.0f) testTonePhase_ -= 1.0f;
      float val = sinf(2.0f * 3.14159265f * testTonePhase_) * 0.707f;
      // 1-bit triangular dither for test tone
      ditherState_ = ditherState_ * 1664525u + 1013904223u;
      float r1 = (float)(ditherState_ & 65535) * (1.0f / 65536.0f);
      ditherState_ = ditherState_ * 1664525u + 1013904223u;
      float r2 = (float)(ditherState_ & 65535) * (1.0f / 65536.0f);
      val += (r1 - r2) * (1.0f / 32768.0f);
      if (val > 1.0f) val = 1.0f;
      if (val < -1.0f) val = -1.0f;
      buffer[i] = static_cast<int16_t>(val * 32767.0f);
    }
    // Copy to waveform buffer for UI
    size_t copyCount = std::min(numSamples, (size_t)AUDIO_BUFFER_SAMPLES);
    memcpy(waveformBuffers_[writeBufferIndex_].data, buffer, copyCount * sizeof(int16_t));
    waveformBuffers_[writeBufferIndex_].count = copyCount;
    displayBufferIndex_.store(writeBufferIndex_, std::memory_order_release);
    writeBufferIndex_ = 1 - writeBufferIndex_;
    return;
  }

  updateTickIncrement();
  delay303.setBpm(bpmValue);
  delay3032.setBpm(bpmValue);

  // Update tape controls only on change (avoids per-buffer control overhead spikes).
  const TapeState& tapeState = sceneManager_.currentScene().tape;
  const bool macroChanged =
      (tapeState.macro.wow != lastTapeMacro_.wow) ||
      (tapeState.macro.age != lastTapeMacro_.age) ||
      (tapeState.macro.sat != lastTapeMacro_.sat) ||
      (tapeState.macro.tone != lastTapeMacro_.tone) ||
      (tapeState.macro.crush != lastTapeMacro_.crush);
  const bool minimalChanged =
      (tapeState.space != lastTapeSpace_) ||
      (tapeState.movement != lastTapeMovement_) ||
      (tapeState.groove != lastTapeGroove_);
  const bool looperModeChanged = (tapeState.mode != lastTapeMode_);
  const bool looperSpeedChanged = (tapeState.speed != lastTapeSpeed_);
  const bool looperVolChanged = fabsf(tapeState.looperVolume - lastTapeLooperVolume_) > 0.0005f;

  if (!tapeControlCached_ || macroChanged) {
    tapeFX->applyMacro(tapeState.macro);
    lastTapeMacro_ = tapeState.macro;
  }
  if (!tapeControlCached_ || minimalChanged) {
    tapeFX->applyMinimalParams(tapeState.space, tapeState.movement, tapeState.groove);
    lastTapeSpace_ = tapeState.space;
    lastTapeMovement_ = tapeState.movement;
    lastTapeGroove_ = tapeState.groove;
  }
  if (!tapeControlCached_ || looperModeChanged) {
    tapeLooper->setMode(tapeState.mode);
    lastTapeMode_ = tapeState.mode;
  }
  if (!tapeControlCached_ || looperSpeedChanged) {
    tapeLooper->setSpeed(tapeState.speed);
    lastTapeSpeed_ = tapeState.speed;
  }
  if (!tapeControlCached_ || looperVolChanged) {
    tapeLooper->setVolume(tapeState.looperVolume);
    lastTapeLooperVolume_ = tapeState.looperVolume;
  }
  tapeControlCached_ = true;

  const bool hasSampleStore = (sampleStore != nullptr);

  // Cache immutable-per-buffer flags
  const float* trackVolumes = sceneManager_.currentScene().trackVolumes;
  const bool looperActive = (tapeState.mode != TapeMode::Stop);
  const bool tapeFxEnabled = tapeState.fxEnabled;
  AudioDiagnostics& diag = AudioDiagnostics::instance();
  const bool diagEnabled = diag.isEnabled();
  // Fine-grained profiling is expensive, so we do it periodically.
  const bool detailedProfile = diagEnabled && ((perfDetailCounter_++ & 0x7Fu) == 0);

  // FX safety guard: if previous callback was near/over budget, reduce FX wet path
  // first (Tape/Looper) to avoid audible underruns, then recover gradually.
  const float cpuLoad = perfStats.cpuAudioPctIdeal;
  const uint32_t underrunsNow = perfStats.audioUnderruns;
  const bool underrunAdvanced = (underrunsNow != lastUnderrunCount_);
  lastUnderrunCount_ = underrunsNow;
  const bool hardOverload = underrunAdvanced || (cpuLoad > 99.0f);
  const bool nearOverload = (cpuLoad > 92.0f);
  if (hardOverload) {
    fxSafetyMix_ -= 0.20f;
    fxSafetyHold_ = 80; // hold ~80 buffers before full recovery
  } else if (nearOverload) {
    fxSafetyMix_ -= 0.06f;
    fxSafetyHold_ = 40;
  } else {
    if (fxSafetyHold_ > 0) {
      fxSafetyHold_--;
    } else {
      fxSafetyMix_ += 0.01f;
    }
  }
  if (fxSafetyMix_ < 0.35f) fxSafetyMix_ = 0.35f;
  if (fxSafetyMix_ > 1.0f) fxSafetyMix_ = 1.0f;

  // Profiling accumulators (detailed sections only when diagnostics is enabled)
  uint32_t tVoicesTotal = 0;
  uint32_t tDrumsTotal = 0;
  uint32_t tSamplerTotal = 0;
  uint32_t tFxTotal = 0;
  uint32_t tVocalTotal = 0;
  uint32_t tLoopStart = micros();

  for (size_t i = 0; i < numSamples; ++i) {
    if (playing) {
      tickPhaseAccum_ += tickPhaseInc_;
      if (tickPhaseAccum_ >= 0x100000000ULL) {
        uint32_t ticksToAdvance = (uint32_t)(tickPhaseAccum_ >> 32);
        tickPhaseAccum_ &= 0xFFFFFFFFULL;
        
        while (ticksToAdvance--) {
          ++currentTick_;
          advanceTick();
        }
      }
      const uint32_t absoluteSubtick = currentAbsoluteSubtick_();
      for (int synth = 0; synth < NUM_303_VOICES; ++synth) {
        consumePatternPlaybackActions_(
            synth, patternPlaybackState_[synth].releaseDue(absoluteSubtick));
      }
    }

    float sample = 0.0f;
    float sample303 = 0.0f;
    float drumsMix = 0.0f;
    float samplerSample = 0.0f;

    // Retrig Logic. Legacy sample counters keep only timing authority;
    // RuntimeSynthPlaybackState owns the logical Release/Start decision and
    // the common consumer fans it to both internal synth and Pattern MIDI.
    if (playing && retrigA_.active) {
      if (--retrigA_.counter <= 0 && retrigA_.countRemaining > 0) {
        consumePatternPlaybackActions_(
            0, patternPlaybackState_[0].acceptRetrigger(patternRetrigEvent_[0]));
        retrigA_.counter = retrigA_.interval;
        retrigA_.countRemaining--;
        if (retrigA_.countRemaining <= 0) retrigA_.active = false;
      }
    }
    if (playing && retrigB_.active) {
      if (--retrigB_.counter <= 0 && retrigB_.countRemaining > 0) {
        consumePatternPlaybackActions_(
            1, patternPlaybackState_[1].acceptRetrigger(patternRetrigEvent_[1]));
        retrigB_.counter = retrigB_.interval;
        retrigB_.countRemaining--;
        if (retrigB_.countRemaining <= 0) retrigB_.active = false;
      }
    }
    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
        if (!playing || currentStepIndex < 0) continue;
        if (retrigDrums_[v].active) {
             if (--retrigDrums_[v].counter <= 0 && retrigDrums_[v].countRemaining > 0) {
                 const DrumPattern& pattern = activeDrumPattern(v);
                 const DrumStep& step = pattern.steps[currentStepIndex];
                 bool accent = step.accent;
                 uint8_t trigVelocity = step.velocity;

                 // FLAM: a single lighter secondary hit.
                 if (retrigDrums_[v].flamGhostVelocity > 0 && retrigDrums_[v].rollTotal == 0) {
                     trigVelocity = retrigDrums_[v].flamGhostVelocity;
                 }

                 // ROLL: crescendo across scheduled retrigs.
                 if (retrigDrums_[v].rollTotal > 0) {
                     const int total = retrigDrums_[v].rollTotal;
                     const int done = total - retrigDrums_[v].countRemaining; // 0..total-1
                     const float t = (total <= 1) ? 1.0f : (float)done / (float)(total - 1);
                     const int startV = std::max(1, (int)step.velocity * 60 / 100);
                     const int endV = std::min(127, (int)step.velocity + 20);
                     int vel = startV + (int)((endV - startV) * t + 0.5f);
                     if (vel < 1) vel = 1;
                     if (vel > 127) vel = 127;
                     trigVelocity = (uint8_t)vel;
                 }

                 switch(v) {
                     case kDrumKickVoice: if (!muteKick) { drums->triggerKick(accent, trigVelocity); if(sampleStore) samplerTrack->triggerPad(0, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumSnareVoice: if (!muteSnare) { drums->triggerSnare(accent, trigVelocity); if(sampleStore) samplerTrack->triggerPad(1, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumHatVoice: if (!muteHat) { drums->triggerHat(accent, trigVelocity); if(sampleStore) samplerTrack->triggerPad(2, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumOpenHatVoice: if (!muteOpenHat) { drums->triggerOpenHat(accent, trigVelocity); if(sampleStore) samplerTrack->triggerPad(3, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumMidTomVoice: if (!muteMidTom) { drums->triggerMidTom(accent, trigVelocity); if(sampleStore) samplerTrack->triggerPad(4, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumHighTomVoice: if (!muteHighTom) { drums->triggerHighTom(accent, trigVelocity); if(sampleStore) samplerTrack->triggerPad(5, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumRimVoice: if (!muteRim) { drums->triggerRim(accent, trigVelocity); if(sampleStore) samplerTrack->triggerPad(6, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumClapVoice: if (!muteClap) { drums->triggerClap(accent, trigVelocity); if(sampleStore) samplerTrack->triggerPad(7, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                 }
                 retrigDrums_[v].counter = retrigDrums_[v].interval;
                 retrigDrums_[v].countRemaining--;
                 if (retrigDrums_[v].countRemaining <= 0) retrigDrums_[v].active = false;
             }
        }
    }

    uint32_t tV0 = 0;
    if (detailedProfile) tV0 = micros();
    // Synth voices are instruments as well as sequencer voices. Their envelopes
    // must be rendered while transport is stopped so live NoteOn/NoteOff reaches
    // the audio output and release tails can complete naturally.
    if (!mute303 && synthVoices_[0]) {
      float v = synthVoices_[0]->process() * 0.5f;
      v = distortion303.process(v);
      v *= trackVolumes[(int)VoiceId::SynthA];
      sample303 += delay303.process(v);
    } else delay303.process(0.0f);
    if (!mute303_2 && synthVoices_[1]) {
      float v = synthVoices_[1]->process() * 0.5f;
      v = distortion3032.process(v);
      v *= trackVolumes[(int)VoiceId::SynthB];
      sample303 += delay3032.process(v);
    } else delay3032.process(0.0f);
    if (detailedProfile) tVoicesTotal += (micros() - tV0);

    uint32_t tD0 = 0;
    if (detailedProfile) tD0 = micros();
    if (playing) {
      // Engine-wide drum state must not depend on any individual mute.
      drums->beginSample();
      if (!muteKick)    drumsMix += drums->processKick() * trackVolumes[(int)VoiceId::DrumKick];
      if (!muteSnare)   drumsMix += drums->processSnare() * trackVolumes[(int)VoiceId::DrumSnare];
      if (!muteHat)     drumsMix += drums->processHat() * trackVolumes[(int)VoiceId::DrumHatC];
      if (!muteOpenHat) drumsMix += drums->processOpenHat() * trackVolumes[(int)VoiceId::DrumHatO];
      if (!muteMidTom)  drumsMix += drums->processMidTom() * trackVolumes[(int)VoiceId::DrumTomM];
      if (!muteHighTom) drumsMix += drums->processHighTom() * trackVolumes[(int)VoiceId::DrumTomH];
      if (!muteRim)     drumsMix += drums->processRim() * trackVolumes[(int)VoiceId::DrumRim];
      if (!muteClap)    drumsMix += drums->processClap() * trackVolumes[(int)VoiceId::DrumClap];
      drumsMix *= 0.60f;
      
      // DC blocker: y[n] = x[n] - x[n-1] + 0.995 * y[n-1]
      dcBlockOut_ = drumsMix - dcBlockPrev_ + 0.995f * dcBlockOut_;
      dcBlockPrev_ = drumsMix;
      drumsMix = dcBlockOut_;

      // Drum Bus Processing
      drumsMix = drumTransientShaper.process(drumsMix);
      drumsMix = drumCompressor.process(drumsMix);
      drumsMix = drumReverb.process(drumsMix);
      
      drumsMix = softLimit(drumsMix);
      sample += drumsMix;
    }
    sample += sample303;
    if (detailedProfile) tDrumsTotal += (micros() - tD0);

    uint32_t tS0 = 0;
    if (detailedProfile) tS0 = micros();
    if (hasSampleStore) {
      // Tick/retrig dispatch above can start a sampler voice at this exact
      // frame. Render only after those triggers so the WAV stays aligned with
      // the built-in drum instead of slipping by one 512-frame audio block.
      samplerTrack->processFrame(samplerSample, *sampleStore);
      sample += samplerSample;
    }
    if (detailedProfile) tSamplerTotal += (micros() - tS0);
    uint32_t tVocal0 = 0;
    if (detailedProfile) tVocal0 = micros();
    float vocalSample = 0.0f;
    if (!voiceTrackMuted_ && vocalSynth_.isActive()) {
      vocalSample = voiceCompressor_.process(vocalSynth_.process());
    }
    sample += vocalSample;
    if (detailedProfile) tVocalTotal += (micros() - tVocal0);

    uint32_t tF0 = 0;
    if (detailedProfile) tF0 = micros();
    if (diagEnabled) {
      diag.trackSource(sample303, drumsMix, samplerSample, 0.0f, vocalSample, tapeLooper->getPeak(), 0.0f);
    }
    if (looperActive) {
      float loopSample = 0.0f;
      tapeLooper->process(sample, &loopSample);
      if (tapeLooper->hasLoop() && tapeState.mode == TapeMode::Play) {
        // Crossfade live↔loop to prevent dissonant doubling.
        // loopSample already has looperVolume baked in.
        float lv = std::min(tapeState.looperVolume, 1.0f);
        sample = sample * (1.0f - lv) + loopSample;
      } else {
        sample += loopSample * fxSafetyMix_;
      }
    }
    if (tapeFxEnabled) {
      float wet = tapeFX->process(sample);
      sample = sample + (wet - sample) * fxSafetyMix_;
    }

    sample *= 0.65f;
    masterOutputLpState_ += masterOutputLpAlpha_ * (sample - masterOutputLpState_);
    sample = masterOutputLpState_;
    float dcIn = sample;
    float dcOut = dcIn - dcBlockX1_ + 0.995f * dcBlockY1_;
    dcBlockX1_ = dcIn; dcBlockY1_ = dcOut;
    float preLimiter = dcOut;
    float limited = softLimit(dcOut);
    float vol = params[static_cast<int>(MiniAcidParamId::MainVolume)].value();
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.8f) vol = 1.8f;
    float finalSample = softLimit(limited * vol);
    ditherState_ = ditherState_ * 1664525u + 1013904223u;
    float r1 = (float)(ditherState_ & 65535) * (1.0f / 65536.0f);
    ditherState_ = ditherState_ * 1664525u + 1013904223u;
    float r2 = (float)(ditherState_ & 65535) * (1.0f / 65536.0f);
    finalSample += (r1 - r2) * (1.0f / 32768.0f); 
    if (finalSample > 1.0f) finalSample = 1.0f;
    if (finalSample < -1.0f) finalSample = -1.0f;
    if (diagEnabled) diag.accumulate(preLimiter, limited);
    buffer[i] = (int16_t)(finalSample * 32767.0f);
    if (detailedProfile) tFxTotal += (micros() - tF0);
  }
  // seq handled by wrapper for accuracy

  perfStats.dspTimeUs = micros() - tLoopStart;
  if (detailedProfile) {
    perfStats.dspVoicesUs = tVoicesTotal;
    perfStats.dspDrumsUs = tDrumsTotal;
    perfStats.dspSamplerUs = tSamplerTotal + tVocalTotal;
    perfStats.dspFxUs = tFxTotal;
  }

  // Tape looper can change mode internally (e.g. REC->PLAY, safety DUB->PLAY).
  // Mirror it back into scene state so UI/state remain consistent.
  if (tapeState.mode != tapeLooper->mode()) {
    sceneManager_.currentScene().tape.mode = tapeLooper->mode();
    lastTapeMode_ = tapeLooper->mode();
  }

  size_t copyCount = std::min(numSamples, (size_t)AUDIO_BUFFER_SAMPLES);
  memcpy(waveformBuffers_[writeBufferIndex_].data, buffer, copyCount * sizeof(int16_t));
  waveformBuffers_[writeBufferIndex_].count = copyCount;
  displayBufferIndex_.store(writeBufferIndex_, std::memory_order_release);
  writeBufferIndex_ = 1 - writeBufferIndex_;
  if (diagEnabled) diag.flushIfReady(millis());
}

void MiniAcid::randomize303Pattern(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  hardBarrierPatternPlayback_(idx);
  // Use the complete compiled genre profile. GrooveRecipe is a compact legacy
  // view and cannot represent pitch, articulation or microtiming parameters.
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
  auto behavior = genreManager_.getBehavior();
  if (genreManager_.generativeMode() == GenerativeMode::Reggae) {
    // Reggae split: bass anchors downbeats, lead handles offbeat movement.
    if (idx == 0) {
      behavior.stepMask = 0x1111;
      behavior.motifLength = 2;
      behavior.avoidClusters = true;
      behavior.forceOctaveJump = false;
    } else {
      behavior.stepMask = 0xAAAA;
      behavior.motifLength = 4;
      behavior.avoidClusters = false;
      behavior.forceOctaveJump = false;
    }
  }
  modeManager_.generatePattern(editSynthPattern(idx), bpmValue, genreParams, behavior, idx);
}

void MiniAcid::setParameter(MiniAcidParamId id, float value) {
  params[static_cast<int>(id)].setValue(value);
  
  // Update real-time DSP parameters for voice
  if (id == MiniAcidParamId::VoicePitch) vocalSynth_.setPitch(value);
  else if (id == MiniAcidParamId::VoiceSpeed) vocalSynth_.setSpeed(value);
  else if (id == MiniAcidParamId::VoiceRobotness) vocalSynth_.setRobotness(value);
  else if (id == MiniAcidParamId::VoiceVolume) vocalSynth_.setVolume(value);
}

void MiniAcid::adjustParameter(MiniAcidParamId id, int steps) {
  params[static_cast<int>(id)].addSteps(steps);
  setParameter(id, params[static_cast<int>(id)].value());
}

void MiniAcid::randomizeDrumPattern() {
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
  const auto behavior = genreManager_.getBehavior();
  modeManager_.generateDrumPattern(
      sceneManager_.editCurrentDrumPattern(), genreParams, behavior);
}

void MiniAcid::randomizeDrumVoice(int voiceIndex) {
  int idx = clampDrumVoice(voiceIndex);
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
  const auto behavior = genreManager_.getBehavior();
  modeManager_.generateDrumVoice(
      sceneManager_.editCurrentDrumPattern().voices[idx], idx,
      genreParams, behavior);
}

// Helper to clear a step for REST
void MiniAcid::clear303Step(int stepIndex, int synthIndex) {
    if (synthIndex < 0 || synthIndex > 1) return;
    SynthPattern& pattern = sceneManager_.editCurrentSynthPattern(synthIndex);
    if (stepIndex >= 0 && stepIndex < SynthPattern::kSteps) {
        pattern.steps[stepIndex].note = -1;  // REST
        pattern.steps[stepIndex].slide = false;
        pattern.steps[stepIndex].accent = false;
        pattern.steps[stepIndex].ghost = false;
        pattern.steps[stepIndex].probability = 100;
        pattern.steps[stepIndex].fx = 0;
        pattern.steps[stepIndex].fxParam = 0;
    }
}

void MiniAcid::randomizeDrumPatternChaos() {
  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();

  // Scramble structural placement while retaining bounded genre parameters.
  DrumPatternSet& patternSet = sceneManager_.editCurrentDrumPattern();
  for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
      // Use random behavior for each voice for "chaos"
      GenreBehavior chaosBehavior;
      chaosBehavior.stepMask = (uint16_t)(rand() % 0xFFFF);
      chaosBehavior.motifLength = (uint16_t)(1 + (rand() % 8));
      chaosBehavior.useMotif = (rand() % 100 < 50);
      chaosBehavior.avoidClusters = (rand() % 100 < 50);
      chaosBehavior.allowChromatic = true;
      chaosBehavior.forceOctaveJump = true;
      
      modeManager_.generateDrumVoice(patternSet.voices[v], v, genreParams, chaosBehavior);
  }
}

void MiniAcid::regeneratePatternsWithGenre() {
  // NOTE: applyTexture is NOT called here - it's applied separately by UI on texture change
  // This prevents double-application which would cause delta-bias drift
  syncGrooveModeToGenre();

  AtlasRuntimeMetadata atlasMetadata{};
  if (AtlasRuntime::applyRecipe(
          genreManager_.recipe(), 0,
          editSynthPattern(0), editSynthPattern(1),
          sceneManager_.editCurrentDrumPattern(), &atlasMetadata)) {
    // GF2-I1/I2: this is a material owner only. Tempo belongs to the
    // generation request that resolved it from the profile corridor, and
    // swing belongs to the musician's FEEL settings. Atlas BPM and swing stay
    // readable provenance for diagnostics and corpus review.
    LOG_DEBUG("  - Atlas recipe applied: %s %s bpm=%u swing=%u\n",
              atlasMetadata.displayName, atlasMetadata.slotId,
              static_cast<unsigned>(atlasMetadata.bpm),
              static_cast<unsigned>(atlasMetadata.swingPercent));
    return;
  }

  const GenerativeParams& genreParams =
      genreManager_.getCompiledGenerativeParams();
  const auto behavior = genreManager_.getBehavior();

  // Regenerate synth patterns using the complete compiled genre profile.
  // Voice 0 = bass (low, repetitive), Voice 1 = lead/arp (high, melodic)
  auto bassBehavior = behavior;
  auto leadBehavior = behavior;
  if (genreManager_.generativeMode() == GenerativeMode::Reggae) {
    // Bass breathes on downbeats, skank/lead stays offbeat.
    bassBehavior.stepMask = 0x1111;
    bassBehavior.motifLength = 2;
    bassBehavior.avoidClusters = true;
    bassBehavior.forceOctaveJump = false;

    leadBehavior.stepMask = 0xAAAA;
    leadBehavior.motifLength = 4;
    leadBehavior.avoidClusters = false;
    leadBehavior.forceOctaveJump = false;
  }
  modeManager_.generatePattern(
      editSynthPattern(0), bpmValue, genreParams, bassBehavior, 0); // Bass
  modeManager_.generatePattern(
      editSynthPattern(1), bpmValue, genreParams, leadBehavior, 1); // Lead

  // Regenerate drum pattern
  modeManager_.generateDrumPattern(
      sceneManager_.editCurrentDrumPattern(), genreParams, behavior);
}

void MiniAcid::syncGrooveModeToGenre() {
  const GrooveboxMode linkedMode =
      GenreManager::grooveboxModeForRecipe(
          genreManager_.recipe(), genreManager_.generativeMode());
  if (sceneManager_.getMode() != linkedMode) {
    LOG_DEBUG("  - MiniAcid::syncGrooveModeToGenre: mode realigned to genre (%d)\n",
              static_cast<int>(linkedMode));
  }
  setGrooveboxMode(linkedMode);
}

void MiniAcid::toggleAudioDiag() {
  bool enabled = !AudioDiagnostics::instance().isEnabled();
  AudioDiagnostics::instance().enable(enabled);
  Serial.printf("[DIAG] Audio diagnostics %s\n", enabled ? "ENABLED" : "DISABLED");
}

void MiniAcid::setGrooveboxMode(GrooveboxMode mode) {
  sceneManager_.setMode(mode);
  modeManager_.setModeLocal(mode);
  syncModeToVoices();
}

void MiniAcid::activateCommittedGrooveboxModeRuntime(GrooveboxMode mode) {
  // SceneManager already contains the committed mode. ACTIVATE only publishes
  // the matching DSP/runtime state and therefore owns no persistence revision.
  modeManager_.setModeLocal(mode);
  syncModeToVoices();
}

void MiniAcid::syncModeToVoices() {
  GrooveboxMode mode = sceneManager_.getMode();
  const ModeConfig& cfg = modeManager_.config();
  
  if (synthVoices_[0]) {
    synthVoices_[0]->setMode(mode);
    if (TB303Voice* v303 = tb303Voice(0)) {
      v303->setSubOscillator(cfg.dsp.subOscillator);
      v303->setNoiseAmount(cfg.dsp.noiseAmount);
    }
  }
  
  if (synthVoices_[1]) {
    synthVoices_[1]->setMode(mode);
    if (TB303Voice* v303 = tb303Voice(1)) {
      v303->setSubOscillator(cfg.dsp.subOscillator);
      v303->setNoiseAmount(cfg.dsp.noiseAmount);
    }
  }
  
  if (drums) {
    drums->setLoFiMode(cfg.dsp.lofiDrums);
    drums->setLoFiAmount(0.4f);
  }
}

GrooveboxMode MiniAcid::grooveboxMode() const {
  return sceneManager_.getMode();
}

void MiniAcid::toggleGrooveboxMode() {
  modeManager_.toggle();
}

void MiniAcid::setGrooveFlavor(int flavor) {
  sceneManager_.setGrooveFlavor(flavor);
  const int flv = sceneManager_.getGrooveFlavor();
  modeManager_.setFlavorLocal(flv);

  // Flavor selection is metadata-only. Sound mutation belongs to explicit
  // Genre MATERIALIZE/APPLY and must not be hidden behind Project settings.
}

int MiniAcid::grooveFlavor() const {
  return sceneManager_.getGrooveFlavor();
}

void MiniAcid::shiftGrooveFlavor(int delta) {
  int flavor = sceneManager_.getGrooveFlavor() + delta;
  while (flavor < 0) flavor += 5;
  while (flavor >= 5) flavor -= 5;
  setGrooveFlavor(flavor);
}

std::string MiniAcid::currentSceneName() const {
  if (!sceneStorage_) return {};
  return sceneStorage_->getCurrentSceneName();
}

std::vector<std::string> MiniAcid::availableSceneNames() const {
  if (!sceneStorage_) return {};
  std::vector<std::string> names = sceneStorage_->getAvailableSceneNames();
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

bool MiniAcid::loadSceneByName(const std::string& name) {
  if (!sceneStorage_) {
    Serial.println("[LoadScene] ERROR: sceneStorage_ is null");
    return false;
  }
  
  Serial.printf("[LoadScene] Starting load for: %s\n", name.c_str());
  
  // Do not auto-save here: filesystem writes can stall UX/audio path on constrained devices.
  // Scene persistence is explicit via Save/Save As.
  std::string previousName = sceneStorage_->getCurrentSceneName();
  
  if (!sceneStorage_->setCurrentSceneName(name)) {
    Serial.printf("[LoadScene] Failed to select scene: %s\n", name.c_str());
    return false;
  }

  bool recoveredAuto = false;
  bool loaded = false;
  if (sceneStorage_->hasSceneAuto()) {
    recoveredAuto = sceneStorage_->readSceneAuto(sceneManager_);
    loaded = recoveredAuto;
  }
  if (!loaded) loaded = sceneStorage_->readScene(sceneManager_);
  lastSceneLoadRecoveredAutosave_ = loaded && recoveredAuto;
  Serial.printf("[LoadScene] loaded=%d recovery=%d\n",
                loaded ? 1 : 0, recoveredAuto ? 1 : 0);
  // String-based fallback REMOVED - causes OOM on DRAM-only devices
  
  if (!loaded) {
    Serial.printf("[LoadScene] FAILED - reverting to: %s\n", previousName.c_str());
    sceneStorage_->setCurrentSceneName(previousName);
    lastSceneLoadRecoveredAutosave_ = false;
    return false;
  }
  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
  Serial.println("[LoadScene] Applying scene state...");
  applySceneStateFromManager();
  const int residentPage = PatternPagingService::activePageIndex();
  if (!rebuildPatternRuntimeEventBank() ||
      patternRuntimeBank_.pageIdentity() != residentPage) {
    patternRuntimeBank_.invalidatePageIdentity();
    Serial.println("[LoadScene] runtime Pattern bank publication failed");
    return false;
  }
  setCurrentPage(static_cast<int8_t>(residentPage));
  Serial.println("[LoadScene] SUCCESS");
  return true;
}

bool MiniAcid::saveSceneAs(const std::string& name) {
  if (!sceneStorage_) return false;
  const std::string previousName = sceneStorage_->getCurrentSceneName();
  if (!sceneStorage_->setCurrentSceneName(name)) return false;
  if (saveSceneToStorage()) return true;
  sceneStorage_->setCurrentSceneName(previousName);
  return false;
}

bool MiniAcid::createNewSceneWithName(const std::string& name) {
  if (!sceneStorage_) return false;
  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
  const std::string previousName = sceneStorage_->getCurrentSceneName();
  if (!sceneStorage_->setCurrentSceneName(name)) return false;

  sceneManager_.wipeToZero();
  applySceneStateFromManager();
  const int residentPage = PatternPagingService::activePageIndex();
  if (!rebuildPatternRuntimeEventBank() ||
      patternRuntimeBank_.pageIdentity() != residentPage) {
    patternRuntimeBank_.invalidatePageIdentity();
    sceneStorage_->setCurrentSceneName(previousName);
    return false;
  }
  setCurrentPage(static_cast<int8_t>(residentPage));
  if (saveSceneToStorage()) return true;

  sceneStorage_->setCurrentSceneName(previousName);
  return false;
}

void MiniAcid::loadSceneFromStorage() {
  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);
  lastSceneLoadRecoveredAutosave_ = false;
  if (sceneStorage_) {
    if (sceneStorage_->hasSceneAuto() &&
        sceneStorage_->readSceneAuto(sceneManager_)) {
      lastSceneLoadRecoveredAutosave_ = true;
      LOG_PRINTLN("  - loadSceneFromStorage: recovered autosave");
      PatternPagingService::loadPage(0, sceneManager_.currentScene());
      hydrateAcceptedMaterialAtBoot_();
      return;
    }
    if (sceneStorage_->readScene(sceneManager_)) {
      PatternPagingService::loadPage(0, sceneManager_.currentScene());
      hydrateAcceptedMaterialAtBoot_();
      return;
    }
    LOG_PRINTLN("  - loadSceneFromStorage: Streaming parse failed, loading default scene");
  }
  sceneManager_.loadDefaultScene();
  PatternPagingService::loadPage(0, sceneManager_.currentScene());
  hydrateAcceptedMaterialAtBoot_();
}

void MiniAcid::hydrateAcceptedMaterialAtBoot_() {
  const std::string& proj = PatternPagingService::currentProjectName();
  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {
    const int bank = current303BankIndex(idx);
    const int pattern = display303LocalPatternIndex(idx);
    if (bank < 0 || bank >= kBankCount || pattern < 0 ||
        pattern >= Bank<SynthPattern>::kPatterns) {
      continue;
    }
    const int slot = bank * Bank<SynthPattern>::kPatterns + pattern;
    const auto kind = sceneManager_.currentScene().materialSlots[idx][slot].kind;
    publishActiveMaterial(idx, static_cast<uint16_t>(slot), kind);
    if (kind == GroovePuterMaterial::MaterialKind::Melody) {
      setSequencedSource(idx, SequencedSource::Phrase);
      PhraseRuntime::RuntimeSynthEventBuffer loaded{};
      const GroovePuterMaterial::MaterialAddress addr{
          static_cast<uint8_t>(idx),
          static_cast<uint8_t>(slot)
      };
      if (MelodyPromotion::loadMaterial(MelodyPromotion::defaultFileSystem(),
                                        proj, addr, loaded)) {
        workingMaterial_[idx].storeMelody(loaded);
      }
    } else {
      setSequencedSource(idx, SequencedSource::Pattern);
      workingMaterial_[idx].clear();
    }
  }
}


bool MiniAcid::saveSceneToStorage() {
  if (!sceneStorage_) return false;
  syncSceneStateToManager();
#ifdef ARDUINO
  Serial.printf("[SamplerScene] save layer=%d\n",
                sceneManager_.currentScene().samplerEnabled ? 1 : 0);
#endif
  if (!sceneStorage_->writeScene(sceneManager_)) return false;
  if (!sceneStorage_->clearSceneAuto()) {
    Serial.println("[SceneSave] main saved but recovery cleanup failed");
    return false;
  }
  lastSceneLoadRecoveredAutosave_ = false;
  return true;
}

bool MiniAcid::autoSaveSceneRecovery() {
  if (!sceneStorage_ || playing) return false;
  syncSceneStateToManager();
  return sceneStorage_->writeSceneAuto(sceneManager_);
}

float MiniAcid::mainVolume() const {
  return params[static_cast<int>(MiniAcidParamId::MainVolume)].value();
}

void MiniAcid::setDeviceMasterVolume(float value) {
  params[static_cast<int>(MiniAcidParamId::MainVolume)].setValue(value);
  deviceMasterVolumeOverride_ = true;
}

void MiniAcid::applySceneStateFromManager() {
  if (playing) publishPatternAllNotesOff_();
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: Start");
  
  modeManager_.setModeLocal(sceneManager_.getMode());
  modeManager_.setFlavorLocal(sceneManager_.getGrooveFlavor());
  syncModeToVoices();
  setBpm(sceneManager_.getBpm());
  
  // Scene volume remains codec-compatible, but a device-session override
  // wins after boot so loading another project cannot change speaker level.
  if (!deviceMasterVolumeOverride_) {
    const float sceneVolume = sceneManager_.currentScene().masterVolume;
    params[static_cast<int>(MiniAcidParamId::MainVolume)].setValue(sceneVolume);
  }
  // Fixed master safety LPF: keep this independent from scene/UI state.
  setMasterOutputHighCutHz(kMasterHighCutHz);
  const std::string& drumEngineName = sceneManager_.getDrumEngineName();
  if (!drumEngineName.empty()) {
    LOG_DEBUG("  - MiniAcid::applySceneStateFromManager: setting drum engine to %s\n", drumEngineName.c_str());
    setDrumEngine(drumEngineName);
  }

  for (int i = 0; i < 2; ++i) {
    const std::string& sname = sceneManager_.getSynthEngineName(i);
    if (!sname.empty()) {
      setSynthEngine(i, sname);
    }
  }

  mute303 = sceneManager_.getSynthMute(0);
  mute303_2 = sceneManager_.getSynthMute(1);

  muteKick = sceneManager_.getDrumMute(kDrumKickVoice);
  muteSnare = sceneManager_.getDrumMute(kDrumSnareVoice);
  muteHat = sceneManager_.getDrumMute(kDrumHatVoice);
  muteOpenHat = sceneManager_.getDrumMute(kDrumOpenHatVoice);
  muteMidTom = sceneManager_.getDrumMute(kDrumMidTomVoice);
  muteHighTom = sceneManager_.getDrumMute(kDrumHighTomVoice);
  muteRim = sceneManager_.getDrumMute(kDrumRimVoice);
  muteClap = sceneManager_.getDrumMute(kDrumClapVoice);
  distortion303Enabled = sceneManager_.getSynthDistortionEnabled(0);
  distortion3032Enabled = sceneManager_.getSynthDistortionEnabled(1);
  delay303Enabled = sceneManager_.getSynthDelayEnabled(0);
  delay3032Enabled = sceneManager_.getSynthDelayEnabled(1);

  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: setting voice params...");
  auto clamp01 = [](float v) -> float {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
  };

  if (sceneManager_.hasVersionedSynthState()) {
    for (int idx = 0; idx < 2; ++idx) {
      const PersistedSynthPatch& patch = sceneManager_.getSynthPatch(idx);
      setSynthEngine(idx, patch.engineName);
      if (!synthVoices_[idx]) continue;
      SynthVoiceState runtimeState = synthVoices_[idx]->getState();
      runtimeState.paramCount = std::min<uint8_t>(
          patch.paramCount, PersistedSynthPatch::kMaxParams);
      for (uint8_t p = 0; p < runtimeState.paramCount; ++p) {
        runtimeState.params[p] = clamp01(patch.params[p]);
      }
      synthVoices_[idx]->setState(runtimeState);
      synthEngineNames_[idx] = synthVoices_[idx]->getEngineName();
      sceneManager_.setSynthEngineName(idx, synthEngineNames_[idx]);
    }
  } else {
    // Legacy compatibility. TB303 raw values keep their historical units.
    // Non-TB engines with no legacy synthParams stay at engine-native defaults.
    for (int idx = 0; idx < 2; ++idx) {
      const SynthParameters& sp = sceneManager_.getSynthParameters(idx);
      if (TB303Voice* v303 = tb303Voice(idx)) {
        if (sceneManager_.legacySynthParametersPresent(idx)) {
          v303->setParameter(TB303ParamId::Cutoff, sp.cutoff);
          v303->setParameter(TB303ParamId::Resonance, sp.resonance);
          v303->setParameter(TB303ParamId::EnvAmount, sp.envAmount);
          v303->setParameter(TB303ParamId::EnvDecay, sp.envDecay);
          v303->setParameter(TB303ParamId::Oscillator, static_cast<float>(sp.oscType));
        }
        continue;
      }
      if (!sceneManager_.legacySynthParametersPresent(idx) || !synthVoices_[idx]) {
        continue;
      }
      // Historical non-TB scenes used these legacy field names as normalized
      // slots 0..3 and oscType/100 as slot 4. Preserve that decode-only path.
      const uint8_t count = synthVoices_[idx]->parameterCount();
      if (count > 0) synthVoices_[idx]->setParameterNormalized(0, clamp01(sp.cutoff));
      if (count > 1) synthVoices_[idx]->setParameterNormalized(1, clamp01(sp.resonance));
      if (count > 2) synthVoices_[idx]->setParameterNormalized(2, clamp01(sp.envAmount));
      if (count > 3) synthVoices_[idx]->setParameterNormalized(3, clamp01(sp.envDecay));
      if (count > 4) synthVoices_[idx]->setParameterNormalized(
          4, clamp01(static_cast<float>(sp.oscType) / 100.0f));
    }
  }
  
  
  distortion303.setEnabled(distortion303Enabled);
  distortion3032.setEnabled(distortion3032Enabled);
  delay303.setEnabled(delay303Enabled);
  delay3032.setEnabled(delay3032Enabled);
  
  const DrumFX& dfx = sceneManager_.currentScene().drumFX;
  updateDrumCompression(dfx.compression);
  updateDrumTransientAttack(dfx.transientAttack);
  updateDrumTransientSustain(dfx.transientSustain);
  updateDrumReverbMix(dfx.reverbMix);
  updateDrumReverbDecay(dfx.reverbDecay);

  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: syncing patterns...");
  patternModeDrumPatternIndex_ = sceneManager_.getCurrentDrumPatternIndex();
  patternModeSynthPatternIndex_[0] = sceneManager_.getCurrentSynthPatternIndex(0);
  patternModeSynthPatternIndex_[1] = sceneManager_.getCurrentSynthPatternIndex(1);
  songMode_ = sceneManager_.songMode();
  songPlaybackSlot_ = sceneManager_.activeSongSlot();
  liveMixMode_ = false;
  songPlayheadPosition_ = clampSongPosition(sceneManager_.getSongPosition());
  songBarIndex_ = -1;
  if (songMode_) {
    applySongPositionSelection();
  }

  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: syncing Sampler...");
  // Sync Sampler
  samplerTrack->setEnabled(sceneManager_.currentScene().samplerEnabled);
#ifdef ARDUINO
  Serial.printf("[SamplerScene] apply layer=%d\n",
                samplerTrack->isEnabled() ? 1 : 0);
#endif
  for (int i = 0; i < 16; ++i) {
    const auto& s = sceneManager_.currentScene().samplerPads[i];
    auto& p = samplerTrack->pad(i);
    p.id.value = s.sampleId;
    p.volume = s.volume;
    p.pitch = s.pitch;
    p.startFrame = s.startFrame;
    p.endFrame = s.endFrame;
    p.chokeGroup = s.chokeGroup;
    p.reverse = s.reverse;
    p.loop = s.loop;
    if (p.id.value != 0 && sampleStore) sampleStore->preload(p.id);
  }

  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: syncing Tape...");
  // Sync Tape - uses dirty flag so this is safe to call
  const auto& t = sceneManager_.currentScene().tape;
  if (tapeFX) tapeFX->applyMacro(t.macro);
  if (tapeLooper) {
    tapeLooper->setMode(t.mode);
    tapeLooper->setSpeed(t.speed);
    tapeLooper->setVolume(t.looperVolume);
  }

  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: syncing Voice...");
  const auto& v = sceneManager_.currentScene().vocal;
  params[static_cast<int>(MiniAcidParamId::VoicePitch)].setValue(v.pitch);
  params[static_cast<int>(MiniAcidParamId::VoiceSpeed)].setValue(v.speed);
  params[static_cast<int>(MiniAcidParamId::VoiceRobotness)].setValue(v.robotness);
  params[static_cast<int>(MiniAcidParamId::VoiceVolume)].setValue(v.volume);

  vocalSynth_.setPitch(v.pitch);
  vocalSynth_.setSpeed(v.speed);
  vocalSynth_.setRobotness(v.robotness);
  vocalSynth_.setVolume(v.volume);

  for (int i = 0; i < Scene::kMaxCustomPhrases; ++i) {
    vocalSynth_.setCustomPhrase(i, sceneManager_.currentScene().customPhrases[i]);
  }
  
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: restore genre metadata...");
  // Restore genre metadata. Normal Scene Load must not project genre sound over
  // the explicitly restored synth patch.
  const auto& gs = sceneManager_.currentScene().genre;
  genreManager_.setGenerativeMode(static_cast<GenerativeMode>(gs.generativeMode));
  genreManager_.setRecipe(gs.recipe);
  genreManager_.setMorphTarget(gs.morphTarget);
  genreManager_.setMorphAmount(gs.morphAmount);
  syncGrooveModeToGenre();

  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: applyFeelTiming...");
  applyFeelTimingFromScene_();

  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: applyFeelTexture...");
  applyTextureFromScene_();
  
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: Done");
}

void MiniAcid::applyTextureFromScene_() {
  Scene& sc = sceneManager_.currentScene();
  const FeelSettings& f = sc.feel;

  // --- LoFi ---
  const float lofiAmt = f.lofiEnabled ? (static_cast<float>(f.lofiAmount) / 100.0f) : 0.0f;
  if (synthVoices_[0]) synthVoices_[0]->setLoFiAmount(lofiAmt);
  if (synthVoices_[1]) synthVoices_[1]->setLoFiAmount(lofiAmt);
  if (drums) {
    drums->setLoFiMode(f.lofiEnabled);
    drums->setLoFiAmount(lofiAmt);
  }

  // --- Drive ---
  const float driveAmtNorm = f.driveEnabled ? (static_cast<float>(f.driveAmount) / 100.0f) : 0.0f;
  const bool driveOn = f.driveEnabled && driveAmtNorm > 0.001f;
  const float macroDriveVal = 0.1f + driveAmtNorm * 9.9f; // TubeDistortion clamps 0.1..10
  const float perVoiceFallbackDrive = 8.0f;               // Default audible DST drive

  // Important: per-voice DST (TB303 page) and FEEL Drive share the same processor.
  // If FEEL Drive is OFF we must keep an audible drive for DST=ON, otherwise
  // drive=0.1 attenuates the signal and sounds like "no sound".
  const bool voiceDistA = distortion303Enabled;
  const bool voiceDistB = distortion3032Enabled;
  const bool distAOn = driveOn || voiceDistA;
  const bool distBOn = driveOn || voiceDistB;

  const float driveA = driveOn ? macroDriveVal : (voiceDistA ? perVoiceFallbackDrive : 0.1f);
  const float driveB = driveOn ? macroDriveVal : (voiceDistB ? perVoiceFallbackDrive : 0.1f);

  distortion303.setDrive(driveA);
  distortion3032.setDrive(driveB);
  distortion303.setEnabled(distAOn);
  distortion3032.setEnabled(distBOn);

  // --- Tape ---
  // FEEL/TEXTURE controls FX enable. Keep looper mode intact while enabled
  // so Tape page REC/PLAY workflow is not interrupted.
  sc.tape.fxEnabled = f.tapeEnabled;
  if (!f.tapeEnabled) {
    sc.tape.mode = TapeMode::Stop;
  }
}

void MiniAcid::applyFeelTimingFromScene_() {
  updateTickIncrement();
  tickPhaseAccum_ = 0;
}

void MiniAcid::syncSceneStateToManager() {
  if (!GroovePuterRhythm::QuantizedGenerationDetail::hasPendingFullGenerationActivation(*this)) {
    sceneManager_.setBpm(projectBpmValue);
  }
  sceneManager_.setDrumEngineName(drumEngineName_);
  sceneManager_.setSynthEngineName(0, currentSynthEngineName(0));
  sceneManager_.setSynthEngineName(1, currentSynthEngineName(1));
  
  // Save master volume to scene
  sceneManager_.currentScene().masterVolume = params[static_cast<int>(MiniAcidParamId::MainVolume)].value();
  sceneManager_.currentScene().genre.generativeMode = static_cast<uint8_t>(genreManager_.generativeMode());
  sceneManager_.currentScene().genre.recipe = static_cast<uint8_t>(genreManager_.recipe());
  sceneManager_.currentScene().genre.morphTarget = static_cast<uint8_t>(genreManager_.morphTarget());
  sceneManager_.currentScene().genre.morphAmount = genreManager_.morphAmount();
  
  sceneManager_.setSynthMute(0, mute303);
  sceneManager_.setSynthMute(1, mute303_2);

  sceneManager_.setDrumMute(kDrumKickVoice, muteKick);
  sceneManager_.setDrumMute(kDrumSnareVoice, muteSnare);
  sceneManager_.setDrumMute(kDrumHatVoice, muteHat);
  sceneManager_.setDrumMute(kDrumOpenHatVoice, muteOpenHat);
  sceneManager_.setDrumMute(kDrumMidTomVoice, muteMidTom);
  sceneManager_.setDrumMute(kDrumHighTomVoice, muteHighTom);
  sceneManager_.setDrumMute(kDrumRimVoice, muteRim);
  sceneManager_.setDrumMute(kDrumClapVoice, muteClap);
  sceneManager_.setSynthDistortionEnabled(0, distortion303Enabled);
  sceneManager_.setSynthDistortionEnabled(1, distortion3032Enabled);
  sceneManager_.setSynthDelayEnabled(0, delay303Enabled);
  sceneManager_.setSynthDelayEnabled(1, delay3032Enabled);
  sceneManager_.setSongMode(songMode_);
  int songPosToStore = songMode_ ? songPlayheadPosition_ : sceneManager_.getSongPosition();
  sceneManager_.setSongPosition(clampSongPosition(songPosToStore));

  for (int idx = 0; idx < 2; ++idx) {
    PersistedSynthPatch patch;
    patch.engineName = currentSynthEngineName(idx);
    if (synthVoices_[idx]) {
      const SynthVoiceState runtimeState = synthVoices_[idx]->getState();
      patch.paramCount = std::min<uint8_t>(
          runtimeState.paramCount, PersistedSynthPatch::kMaxParams);
      for (uint8_t p = 0; p < patch.paramCount; ++p) {
        float value = runtimeState.params[p];
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        patch.params[p] = value;
      }
    }
    sceneManager_.setSynthPatch(idx, patch);
    sceneManager_.setLegacySynthParametersPresent(idx, false);
  }

  // SamplerPage edits the realtime pad objects. Explicit Save must mirror
  // those values into the resident Scene before the Scene writer converts the
  // compact runtime SampleId to its authoritative stable SampleRef.
  if (samplerTrack) {
    sceneManager_.currentScene().samplerEnabled = samplerTrack->isEnabled();
    for (int i = 0; i < 16; ++i) {
      const auto& runtimePad = samplerTrack->pad(i);
      auto& scenePad = sceneManager_.currentScene().samplerPads[i];
      scenePad.sampleId = runtimePad.id.value;
      scenePad.volume = runtimePad.volume;
      scenePad.pitch = runtimePad.pitch;
      scenePad.startFrame = runtimePad.startFrame;
      scenePad.endFrame = runtimePad.endFrame;
      scenePad.chokeGroup = runtimePad.chokeGroup;
      scenePad.reverse = runtimePad.reverse;
      scenePad.loop = runtimePad.loop;
    }
  }

  // Save voice parameters to scene
  auto& v = sceneManager_.currentScene().vocal;
  v.pitch = params[static_cast<int>(MiniAcidParamId::VoicePitch)].value();
  v.speed = params[static_cast<int>(MiniAcidParamId::VoiceSpeed)].value();
  v.robotness = params[static_cast<int>(MiniAcidParamId::VoiceRobotness)].value();
  v.volume = params[static_cast<int>(MiniAcidParamId::VoiceVolume)].value();

  // Sync Voice Custom Phrases back to scene
  for (int i = 0; i < Scene::kMaxCustomPhrases; ++i) {
    std::strncpy(sceneManager_.currentScene().customPhrases[i], vocalSynth_.getCustomPhrase(i), Scene::kMaxPhraseLength - 1);
    sceneManager_.currentScene().customPhrases[i][Scene::kMaxPhraseLength - 1] = '\0';
  }
}



int dorian_intervals[7] = {0, 2, 3, 5, 7, 9, 10};
int phrygian_intervals[7] = {0, 1, 3, 5, 7, 8, 10};

void PatternGenerator::generateRandom303Pattern(SynthPattern& pattern) {
  int rootNote = 26;

  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    int r = rand() % 10;
    if (r < 7) {
      pattern.steps[i].note = rootNote + dorian_intervals[rand() % 7] + 12 * (rand() % 3);
    } else {
      pattern.steps[i].note = -1; // 30% chance of rest
    }

    // Random accent (30% chance)
    pattern.steps[i].accent = (rand() % 100) < 30;

    // Random slide (20% chance)
    pattern.steps[i].slide = (rand() % 100) < 20;
  }
}

void PatternGenerator::generateRandomDrumPattern(DrumPatternSet& patternSet) {
  const int stepCount = DrumPattern::kSteps;
  const int drumVoiceCount = DrumPatternSet::kVoices;

  for (int v = 0; v < drumVoiceCount; ++v) {
    for (int i = 0; i < stepCount; ++i) {
      patternSet.voices[v].steps[i].hit = false;
      patternSet.voices[v].steps[i].accent = false;
    }
  }

  for (int i = 0; i < stepCount; ++i) {
    if (drumVoiceCount > kDrumKickVoice) {
      if (i % 4 == 0 || (rand() % 100) < 20) {
        patternSet.voices[kDrumKickVoice].steps[i].hit = true;
      } else {
        patternSet.voices[kDrumKickVoice].steps[i].hit = false;
      }
      patternSet.voices[kDrumKickVoice].steps[i].accent =
        patternSet.voices[kDrumKickVoice].steps[i].hit && (rand() % 100) < 35;
    }

    if (drumVoiceCount > kDrumSnareVoice) {
      if (i % 4 == 2 || (rand() % 100) < 15) {
        patternSet.voices[kDrumSnareVoice].steps[i].hit = (rand() % 100) < 80;
      } else {
        patternSet.voices[kDrumSnareVoice].steps[i].hit = false;
      }
      patternSet.voices[kDrumSnareVoice].steps[i].accent =
        patternSet.voices[kDrumSnareVoice].steps[i].hit && (rand() % 100) < 30;
    }

    bool hatVal = false;
    if (drumVoiceCount > kDrumHatVoice) {
      if ((rand() % 100) < 90) {
        hatVal = (rand() % 100) < 80;
      } else {
        hatVal = false;
      }
      patternSet.voices[kDrumHatVoice].steps[i].hit = hatVal;
      patternSet.voices[kDrumHatVoice].steps[i].accent = hatVal && (rand() % 100) < 20;
    }

    bool openVal = false;
    if (drumVoiceCount > kDrumOpenHatVoice) {
      openVal = (i % 4 == 3 && (rand() % 100) < 65) || ((rand() % 100) < 20 && hatVal);
      patternSet.voices[kDrumOpenHatVoice].steps[i].hit = openVal;
      patternSet.voices[kDrumOpenHatVoice].steps[i].accent = openVal && (rand() % 100) < 25;
      if (openVal && drumVoiceCount > kDrumHatVoice) {
        patternSet.voices[kDrumHatVoice].steps[i].hit = false;
        patternSet.voices[kDrumHatVoice].steps[i].accent = false;
      }
    }

    if (drumVoiceCount > kDrumMidTomVoice) {
      bool midTom = (i % 8 == 4 && (rand() % 100) < 75) || ((rand() % 100) < 8);
      patternSet.voices[kDrumMidTomVoice].steps[i].hit = midTom;
      patternSet.voices[kDrumMidTomVoice].steps[i].accent = midTom && (rand() % 100) < 35;
    }

    if (drumVoiceCount > kDrumHighTomVoice) {
      bool highTom = (i % 8 == 6 && (rand() % 100) < 70) || ((rand() % 100) < 6);
      patternSet.voices[kDrumHighTomVoice].steps[i].hit = highTom;
      patternSet.voices[kDrumHighTomVoice].steps[i].accent = highTom && (rand() % 100) < 35;
    }

    if (drumVoiceCount > kDrumRimVoice) {
      bool rim = (i % 4 == 1 && (rand() % 100) < 25);
      patternSet.voices[kDrumRimVoice].steps[i].hit = rim;
      patternSet.voices[kDrumRimVoice].steps[i].accent = rim && (rand() % 100) < 30;
    }

    if (drumVoiceCount > kDrumClapVoice) {
      bool clap = false;
      if (i % 4 == 2) {
        clap = (rand() % 100) < 80;
      } else {
        clap = (rand() % 100) < 5;
      }
      patternSet.voices[kDrumClapVoice].steps[i].hit = clap;
    }
  }
}

void MiniAcid::setTestTone(bool enabled) {
  testToneEnabled_ = enabled;
  if (!enabled) {
    testTonePhase_ = 0.0f;
  }
}

// ════════════════════════════════════════════════════════════════════════════
// Vocal Synth Implementation
// ════════════════════════════════════════════════════════════════════════════

// Built-in phrases for quick announcements
static const char* const BUILTIN_PHRASES[] = {
    "Acid",           // 0
    "Techno",         // 1
    "Minimal",        // 2
    "Pattern One",    // 3
    "Pattern Two",    // 4
    "Ready",          // 5
    "Go",             // 6
    "Stop",           // 7
    "Recording",      // 8
    "Saved",          // 9
    "Error",          // 10
    "BPM",            // 11
    "Play",           // 12
    "Mute",           // 13
    "Welcome",        // 14
    "Goodbye",        // 15
};
static const int NUM_BUILTIN_PHRASES = sizeof(BUILTIN_PHRASES) / sizeof(BUILTIN_PHRASES[0]);

void MiniAcid::speak(const char* text) {
    vocalSynth_.speak(text);
}

void MiniAcid::speakPhrase(int phraseIndex) {
    if (phraseIndex >= 0 && phraseIndex < NUM_BUILTIN_PHRASES) {
        vocalSynth_.speak(BUILTIN_PHRASES[phraseIndex]);
    }
}

void MiniAcid::speakCustomPhrase(int index) {
    vocalSynth_.speakCustomPhrase(index);
}

void MiniAcid::stopSpeaking() {
    vocalSynth_.stop();
    voiceCache_.stopPlayback();
}

bool MiniAcid::speakCached(const char* text) {
    // Try to play from cache first
    if (voiceCache_.isInitialized() && voiceCache_.startPlayback(text)) {
        Serial.printf("[VoiceCache] Playing from cache: %s\n", text);
        return true;
    }
    
    // Fallback to live synthesis
    vocalSynth_.speak(text);
    return false;
}


void MiniAcid::toggleVoiceTrackMute() {
    voiceTrackMuted_ = !voiceTrackMuted_;
}

void MiniAcid::setVoiceTrackMute(bool muted) {
    voiceTrackMuted_ = muted;
}

/*
void MiniAcid::toggleAudioDiag() {
  bool enabled = AudioDiagnostics::instance().isEnabled();
  AudioDiagnostics::instance().enable(!enabled);
}
  */

bool MiniAcid::renderProjectToWav(const std::string& filename, std::function<void(float)> progressCallback) {
  if (filename.empty()) return false;
  
  // 1. Stop Audio
  bool wasPlaying = isPlaying();
  stop();

  // 2. Open File
  if (SD.exists(filename.c_str())) {
      SD.remove(filename.c_str());
  }
  File file = SD.open(filename.c_str(), FILE_WRITE);
  if (!file) {
      if (wasPlaying) start();
      return false;
  }

  // 3. Setup Render Parameters
  // Determine duration
  int totalSteps = 0;
  if (songModeEnabled()) {
      totalSteps = songLength() * SEQ_STEPS;
      // Reset to start of song
      setSongPosition(0);
      songPlayheadPosition_ = 0;
  } else {
      // Pattern Mode: Render 4 bars (4 patterns) = 64 steps
      totalSteps = 64; 
      // Reset to step 0
      set303PatternIndex(0, current303PatternIndex(0));
      set303PatternIndex(1, current303PatternIndex(1));
      setDrumPatternIndex(currentDrumPatternIndex());
  }
  
  if (totalSteps <= 0) totalSteps = 64; // Fallback

  // Reset Engine State for rendering
  currentStepIndex = 0;
  tickPhaseAccum_ = 0;
  currentTick_ = 0;
  
  // 4. Write WAV Header placeholder
  WavHeader header;
  header.sample_rate = (uint32_t)sampleRateValue;
  header.channels = 1; // Mono output from generateAudioBuffer? CHECK
  // generateAudioBuffer fills int16 buffer. Assuming Mono.
  // Actually MiniAcid output is generally Mono unless platform specific.
  // We write Mono.
  header.byterate = header.sample_rate * 1 * 2; // Mono, 16-bit
  header.block_align = 2; // 1 * 16/8
  header.overall_size = 0; // Update later
  header.data_size = 0;    // Update later
  
  file.write((const uint8_t*)&header, sizeof(WavHeader));
  
  // 5. Render Loop
  const int kRenderBlockSize = 512;
  int16_t buffer[kRenderBlockSize];
  
  // Approximate total samples
  float sps = (sampleRateValue * 60.0f) / (bpmValue * 4.0f); 
  // samples per beat = (SR * 60) / BPM.
  // samples per step (1/16th) = samples per beat / 4.
  // So samplesPerStep = (SR * 60) / (BPM * 4).
  float samplesPerStepAvg = (sampleRateValue * 60.0f) / (bpmValue * 4.0f);
  int expectedTotalSamples = (int)(totalSteps * samplesPerStepAvg);
  
  int stepsRendered = 0;
  int currentSampleCount = 0;
  
  // We need to simulate playback.
  playing = true; // Force playing flag for generateAudioBuffer
  
  // Hack: Reset internal counters
  tickPhaseAccum_ = 0;
  currentTick_ = 0;
  
  uint32_t bytesWritten = 0;
  int lastStepIndex = 0;
  int stepsAdvanced = 0;
  
  // We loop until we advanced enough steps
  // We monitor 'currentStepIndex' wrapping or handled by stepsAdvanced?
  // generateAudioBuffer updates currentStepIndex.
  // We need to track how many times we advanced.
  // But generateAudioBuffer doesn't expose "steps advanced".
  // We can track step index changes?
  // Or simpler: just render the expected number of samples.
  
  while (currentSampleCount < expectedTotalSamples) {
      // Render a block
      generateAudioBuffer(buffer, kRenderBlockSize);
      
      // Mono write
      // Note: FILE system on ESP32 is sometimes slow.
      file.write((const uint8_t*)buffer, kRenderBlockSize * sizeof(int16_t));
      bytesWritten += kRenderBlockSize * sizeof(int16_t);
      currentSampleCount += kRenderBlockSize;
      
      if (progressCallback && (currentSampleCount % (44100/2) == 0)) {
          progressCallback((float)currentSampleCount / (float)expectedTotalSamples);
          #if defined(ARDUINO)
          vTaskDelay(1); // Yield to keep callback responsive / WDT happy
          #else
          delay(1);
          #endif
      }
  }
  
  stop(); // Stop internal playing flag
  if (wasPlaying) playing = true; // Restore playing state logical? No, wait for start()
  
  // 6. Update Header
  header.data_size = bytesWritten;
  header.overall_size = bytesWritten + sizeof(WavHeader) - 8;
  file.seek(0);
  file.write((const uint8_t*)&header, sizeof(WavHeader));
  file.close();

  // 7. Cleanup
  if (wasPlaying) start();
  
  return true;
}

void MiniAcid::rotatePattern(int voiceIndex, int steps) {
    if (steps == 0) return;
    
    int idx = clamp303Voice(voiceIndex);
    SynthPattern& pattern = editSynthPattern(idx);
    
    // Normalize steps to range [0, kSteps)
    int shift = steps % SynthPattern::kSteps;
    if (shift < 0) shift += SynthPattern::kSteps;
    
    // Standard rotation: std::rotate(begin, middle, end)
    // To rotate RIGHT by K:
    // middle = end - K
    
    auto& s = pattern.steps;
    std::rotate(std::begin(s), std::end(s) - shift, std::end(s));
    
    // Also rotate slides/accents in the cache? 
    // Wait, the cache is rebuilt from pattern.steps in refreshSynthCaches.
    // So modifying pattern.steps is sufficient.
}

void MiniAcid::updateDrumCompression(float value) {
  drumCompressor.setAmount(value);
  bool on = (value > 0.01f);
  drumCompressor.setEnabled(on);
}

void MiniAcid::updateDrumTransientAttack(float value) {
  drumTransientShaper.setAttackAmount(value);
}

void MiniAcid::updateDrumTransientSustain(float value) {
  drumTransientShaper.setSustainAmount(value);
}

void MiniAcid::updateDrumReverbMix(float value) {
  drumReverb.setMix(value);
}

bool MiniAcid::rebuildPatternRuntimeEventBank() {
  const int residentPage = PatternPagingService::activePageIndex();
  if (residentPage < 0 || residentPage >= kMaxPages) return false;

  const Scene& scene = sceneManager_.currentScene();
  const auto recipe = genreManager_.getGrooveRecipe();
  const int swingPct = std::clamp(
      static_cast<int>(scene.feel.swingPct), 50, 75);

  PhraseRuntime::RuntimePatternEventBank candidate{};
  for (uint8_t synth = 0; synth < NUM_303_VOICES; ++synth) {
    PhraseRuntime::PatternProjectionSettings settings{};
    settings.synthIndex = synth;
    settings.gateLengthRatio = recipe.gateLengthRatio;
    settings.swingPercent = static_cast<uint8_t>(swingPct);
    const VoiceId voice = synth == 0 ? VoiceId::SynthA : VoiceId::SynthB;
    settings.swingEnabled =
        (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;

    for (uint8_t bank = 0; bank < kBankCount; ++bank) {
      for (uint8_t pattern = 0;
           pattern < Bank<SynthPattern>::kPatterns;
           ++pattern) {
        const SynthPattern& source = synth == 0
            ? scene.synthABanks[bank].patterns[pattern]
            : scene.synthBBanks[bank].patterns[pattern];
        if (candidate.refresh(synth, bank, pattern, source, settings) !=
            PhraseRuntime::PatternBankRefreshStatus::Ready) {
          return false;
        }
      }
    }
  }

  if (!candidate.publishPageIdentity(residentPage)) return false;
  patternRuntimeBank_ = candidate;
  return true;
}

bool MiniAcid::refreshPatternRuntimeEvents(int synthIndex,
                                           int bankIndex,
                                           int patternIndex) {
  if (synthIndex < 0 || synthIndex >= NUM_303_VOICES ||
      bankIndex < 0 || bankIndex >= kBankCount ||
      patternIndex < 0 || patternIndex >= Bank<SynthPattern>::kPatterns) {
    return false;
  }

  const int residentPage = PatternPagingService::activePageIndex();
  if (residentPage != currentPageIndex() ||
      patternRuntimeBank_.pageIdentity() != residentPage) {
    return false;
  }

  const Scene& scene = sceneManager_.currentScene();
  const auto recipe = genreManager_.getGrooveRecipe();
  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = static_cast<uint8_t>(synthIndex);
  settings.gateLengthRatio = recipe.gateLengthRatio;
  settings.swingPercent = static_cast<uint8_t>(std::clamp(
      static_cast<int>(scene.feel.swingPct), 50, 75));
  const VoiceId voice = synthIndex == 0 ? VoiceId::SynthA : VoiceId::SynthB;
  settings.swingEnabled =
      (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;

  const SynthPattern& source = synthIndex == 0
      ? scene.synthABanks[bankIndex].patterns[patternIndex]
      : scene.synthBBanks[bankIndex].patterns[patternIndex];
  return patternRuntimeBank_.refresh(
             static_cast<uint8_t>(synthIndex),
             static_cast<uint8_t>(bankIndex),
             static_cast<uint8_t>(patternIndex),
             source,
             settings) == PhraseRuntime::PatternBankRefreshStatus::Ready;
}

const PhraseRuntime::RuntimePatternEventBuffer&
MiniAcid::activePatternRuntimeEvents(int synthIndex) const {
  if (synthIndex < 0 || synthIndex >= NUM_303_VOICES) {
    return patternRuntimeBank_.empty();
  }
  if (const PhraseRuntime::RuntimePatternEventBuffer* pending =
          GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleSynthRuntime(
              *this, synthIndex)) {
    return *pending;
  }
  // Keep the accepted SONG/PATTERN source gate. In SONG mode an empty synth
  // track is authoritative silence; it must not fall back to the Scene's
  // current PATTERN-mode index merely because that resident slot exists.
  const SongTrack track = synthIndex == 0 ? SongTrack::SynthA : SongTrack::SynthB;
  const int patternIndex = songPatternIndexForTrack(track);
  if (patternIndex < 0) return patternRuntimeBank_.empty();
  const int bankIndex = current303BankIndex(synthIndex);
  if (bankIndex < 0 || bankIndex >= kBankCount ||
      patternIndex >= Bank<SynthPattern>::kPatterns) {
    return patternRuntimeBank_.empty();
  }
  return patternRuntimeBank_.selectForPage(
      currentPageIndex(),
      static_cast<uint8_t>(synthIndex),
      static_cast<uint8_t>(bankIndex),
      static_cast<uint8_t>(patternIndex));
}

void MiniAcid::updateDrumReverbDecay(float value) {
  drumReverb.setDecay(value);
}

void MiniAcid::hardBarrierPatternPlayback_(int synthIdx) {
  const auto actions = patternPlaybackState_[synthIdx].hardBarrier();
  consumePatternPlaybackActions_(synthIdx, actions);
}

void MiniAcid::hardBarrierPatternPlayback_() {
  for (int synth = 0; synth < NUM_303_VOICES; ++synth) {
    hardBarrierPatternPlayback_(synth);
  }
}

void MiniAcid::barrierPatternRuntimeSourceTransition() {
  hardBarrierPatternPlayback_();
}

void MiniAcid::cleanupLiveNotesForTransportBarrier_(
    uint8_t patternAuthorityAtEntry) {
  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {
    const uint8_t targetMask = static_cast<uint8_t>(1u << idx);
    const bool patternOwnedBackendAtEntry =
        (patternAuthorityAtEntry & targetMask) != 0u;
    if (!patternOwnedBackendAtEntry && liveNotes_[idx] >= 0 &&
        synthVoices_[idx]) {
      synthVoices_[idx]->release();
    }
    liveNotes_[idx] = -1;
  }
}

void MiniAcid::consumePatternPlaybackActions_(
    int synthIdx,
    const PhraseRuntime::RuntimeSynthPlaybackActions& actions) {
  const int idx = clamp303Voice(synthIdx);
  for (uint8_t i = 0; i < actions.count; ++i) {
    const PhraseRuntime::RuntimeSynthPlaybackAction& action = actions.values[i];
    const PhraseRuntime::RuntimeSynthEvent& event = action.event;
    const bool accent = (event.flags & PhraseRuntime::kEventAccent) != 0;
    const bool slide = (event.flags & PhraseRuntime::kEventSlide) != 0;

    // RuntimeSynthPlaybackState owns the logical replacement as Release ->
    // Start. TB303's accepted legacy slide, however, is legato only while the
    // internal gate remains high. Translate a slide replacement without an
    // internal gate-off while still closing/reopening external MIDI ownership.
    const bool slideReplacement =
        action.type == PhraseRuntime::RuntimeSynthPlaybackActionType::Release &&
        i + 1u < actions.count &&
        actions.values[i + 1u].type ==
            PhraseRuntime::RuntimeSynthPlaybackActionType::Start &&
        (actions.values[i + 1u].event.flags & PhraseRuntime::kEventSlide) != 0;
    const bool skipInternalRelease = slideReplacement;

    switch (action.type) {
      case PhraseRuntime::RuntimeSynthPlaybackActionType::Release:
        if (!skipInternalRelease && synthVoices_[idx]) {
          synthVoices_[idx]->release();
        }
        publishPatternNoteOff_(idx);
        break;

      case PhraseRuntime::RuntimeSynthPlaybackActionType::Start:
        if (synthVoices_[idx]) {
          synthVoices_[idx]->startNote(
              noteToFreq(event.note), accent, slide, event.velocity);
        }
        publishPatternNoteOn_(idx, event.note, event.velocity);
        LedManager::instance().onVoiceTriggered(
            idx == 0 ? VoiceId::SynthA : VoiceId::SynthB,
            sceneManager_.currentScene().led);
        break;

      case PhraseRuntime::RuntimeSynthPlaybackActionType::Retrigger:
        if (synthVoices_[idx]) synthVoices_[idx]->release();
        publishPatternNoteOff_(idx);
        if (synthVoices_[idx]) {
          synthVoices_[idx]->startNote(
              noteToFreq(event.note), accent, slide, event.velocity);
        }
        publishPatternNoteOn_(idx, event.note, event.velocity);
        LedManager::instance().onVoiceTriggered(
            idx == 0 ? VoiceId::SynthA : VoiceId::SynthB,
            sceneManager_.currentScene().led);
        break;

      default:
        break;
    }
  }
}

uint32_t MiniAcid::currentAbsoluteSubtick_() const {
  const uint32_t fractionalSubtick = static_cast<uint32_t>(
      (tickPhaseAccum_ & 0xFFFFFFFFULL) >> 28);
  return currentTick_ * static_cast<uint32_t>(PhraseRuntime::kSubticksPerTick) +
         fractionalSubtick;
}

const PhraseRuntime::RuntimeSynthEventBuffer* MiniAcid::retainedWorkingMelody_(
    int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return nullptr;
  const int idx = clamp303Voice(voiceIndex);
  const auto* melody = workingMaterial_[idx].melodyIfHeld();
  if (melody == nullptr) return nullptr;
  if (!RuntimePhraseEdit::validate(*melody)) return nullptr;
  return melody;
}

const PhraseRuntime::RuntimeSynthEventBuffer* MiniAcid::readableWorkingMelody_(
    int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return nullptr;
  const int idx = clamp303Voice(voiceIndex);
  if (activeMaterial_[idx].kind != GroovePuterMaterial::MaterialKind::Melody) {
    return nullptr;
  }
  return retainedWorkingMelody_(idx);
}

uint16_t MiniAcid::phraseRelativeTick_(int voiceIndex,
                                      uint32_t absoluteTick) const {
  const auto* phrase = readableWorkingMelody_(voiceIndex);
  if (phrase == nullptr || phrase->lengthTicks == 0) return 0;
  return static_cast<uint16_t>(absoluteTick % phrase->lengthTicks);
}

const PhraseRuntime::RuntimeSynthEvent* MiniAcid::phraseEventAt_(
    int voiceIndex,
    uint32_t absoluteTick) const {
  const auto* phrase = readableWorkingMelody_(voiceIndex);
  if (phrase == nullptr || phrase->lengthTicks == 0) return nullptr;
  const uint16_t phraseTick = phraseRelativeTick_(voiceIndex, absoluteTick);
  // First match wins. RuntimeSynthPlaybackState is monophonic, so several events
  // sharing a startTick would collapse to the last one while still spending a
  // set of RNG draws each; taking one keeps both the note and the draw count
  // determinate.
  for (uint16_t i = 0; i < phrase->count; ++i) {
    if (phrase->events[i].startTick == phraseTick) return &phrase->events[i];
  }
  return nullptr;
}

void MiniAcid::triggerSynthStep_(
    int synthIdx,
    const PhraseRuntime::RuntimeSynthEvent& event,
    uint32_t absoluteStartSubtick) {
  const int songPattern = songPatternIndexForTrack(
      synthIdx == 0 ? SongTrack::SynthA : SongTrack::SynthB);
  if (songPattern < 0) return;
  if (synthIdx == 0 && mute303) return;
  if (synthIdx == 1 && mute303_2) return;

  // Preserve legacy RNG consumption exactly: ghost first, probability only
  // after ghost accepts. Projection is deterministic and never consumes RNG.
  const bool ghost = (event.flags & PhraseRuntime::kEventGhost) != 0;
  if (ghost && (rand() % 100 >= 80)) return;
  if (event.probability < 100 && (rand() % 100 >= event.probability)) return;

  consumePatternPlaybackActions_(
      synthIdx,
      patternPlaybackState_[synthIdx].acceptOnset(event, absoluteStartSubtick));

  RetrigState& retrig = synthIdx == 0 ? retrigA_ : retrigB_;
  retrig.active = false;
  patternRetrigEvent_[synthIdx] = event;
  if (event.fx == static_cast<uint8_t>(StepFx::Retrig) && event.fxParam > 0) {
    // Legacy projects could persist the old 0..255 parameter range. Execution
    // follows the new bounded musician-facing contract even before the user
    // edits that step.
    const int retrigCount =
        std::clamp(static_cast<int>(event.fxParam), 1, 8);
    retrig.countRemaining = retrigCount;

    // Rn means N audible retriggers of this onset. RuntimeSynthPlaybackState
    // correctly refuses to retrigger a note after its gate has released, so
    // schedule the requested repeats inside the event's actual gate instead of
    // across the whole 16th-step. Cap long/tied events to one physical step:
    // step FX belongs to this step, not to the following one.
    constexpr uint32_t kSubticksPerStep =
        24u * static_cast<uint32_t>(PhraseRuntime::kSubticksPerTick);
    const uint32_t gateSubticks = std::max<uint32_t>(
        1u, std::min<uint32_t>(event.durationSubticks, kSubticksPerStep));
    const float gateFraction =
        static_cast<float>(gateSubticks) / static_cast<float>(kSubticksPerStep);
    const int retrigSpanSamples =
        std::max(1, static_cast<int>(samplesPerStep_ * gateFraction));
    retrig.interval =
        std::max(1, retrigSpanSamples / (retrigCount + 1));
    retrig.counter = retrig.interval;
    retrig.active = true;
  }
}

void MiniAcid::triggerDrumVoice_(int voiceIdx, int stepIdx) {
  int songPatternDrums = songPatternIndexForTrack(SongTrack::Drums);
  if (songPatternDrums < 0) return;

  const DrumPatternSet* pendingDrums =
      GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(*this);
  const DrumPatternSet& currentDrumPatternSet = pendingDrums
      ? *pendingDrums
      : sceneManager_.getCurrentDrumPattern();
  if (stepIdx == currentStepIndex) {
      applyDrumAutomationLanesForStep_(currentDrumPatternSet, stepIdx);
  }

  const DrumPattern& pattern = currentDrumPatternSet.voices[voiceIdx];
  const DrumStep& step = pattern.steps[stepIdx];

  bool muted = false;
  switch(voiceIdx) {
      case kDrumKickVoice: muted = muteKick; break;
      case kDrumSnareVoice: muted = muteSnare; break;
      case kDrumHatVoice: muted = muteHat; break;
      case kDrumOpenHatVoice: muted = muteOpenHat; break;
      case kDrumMidTomVoice: muted = muteMidTom; break;
      case kDrumHighTomVoice: muted = muteHighTom; break;
      case kDrumRimVoice: muted = muteRim; break;
      case kDrumClapVoice: muted = muteClap; break;
  }

  if (muted || !step.hit) return;
  if (step.probability < 100 && (rand() % 100 >= step.probability)) return;

  bool accent = step.accent;
  bool rev = (step.fx == (uint8_t)StepFx::Reverse);
  
  switch(voiceIdx) {
    case kDrumKickVoice: 
        drums->triggerKick(accent, (uint8_t)step.velocity);
        if (sampleStore) samplerTrack->triggerPad(0, accent ? 1.0f : 0.6f, *sampleStore, rev);
        LedManager::instance().onVoiceTriggered(VoiceId::DrumKick, sceneManager_.currentScene().led);
        break;
    case kDrumSnareVoice:
        drums->triggerSnare(accent, (uint8_t)step.velocity);
        if (sampleStore) samplerTrack->triggerPad(1, accent ? 1.0f : 0.6f, *sampleStore, rev);
        LedManager::instance().onVoiceTriggered(VoiceId::DrumSnare, sceneManager_.currentScene().led);
        break;
    case kDrumHatVoice:
        drums->triggerHat(accent, (uint8_t)step.velocity);
        if (sampleStore) samplerTrack->triggerPad(2, accent ? 1.0f : 0.6f, *sampleStore, rev);
        LedManager::instance().onVoiceTriggered(VoiceId::DrumHatC, sceneManager_.currentScene().led);
        break;
    case kDrumOpenHatVoice:
        drums->triggerOpenHat(accent, (uint8_t)step.velocity);
        if (sampleStore) samplerTrack->triggerPad(3, accent ? 1.0f : 0.6f, *sampleStore, rev);
        LedManager::instance().onVoiceTriggered(VoiceId::DrumHatO, sceneManager_.currentScene().led);
        break;
    case kDrumMidTomVoice:
        drums->triggerMidTom(accent, (uint8_t)step.velocity);
        if (sampleStore) samplerTrack->triggerPad(4, accent ? 1.0f : 0.6f, *sampleStore, rev);
        LedManager::instance().onVoiceTriggered(VoiceId::DrumTomM, sceneManager_.currentScene().led);
        break;
    case kDrumHighTomVoice:
        drums->triggerHighTom(accent, (uint8_t)step.velocity);
        if (sampleStore) samplerTrack->triggerPad(5, accent ? 1.0f : 0.6f, *sampleStore, rev);
        LedManager::instance().onVoiceTriggered(VoiceId::DrumTomH, sceneManager_.currentScene().led);
        break;
    case kDrumRimVoice:
        drums->triggerRim(accent, (uint8_t)step.velocity);
        if (sampleStore) samplerTrack->triggerPad(6, accent ? 1.0f : 0.6f, *sampleStore, rev);
        LedManager::instance().onVoiceTriggered(VoiceId::DrumRim, sceneManager_.currentScene().led);
        break;
    case kDrumClapVoice:
        drums->triggerClap(accent, (uint8_t)step.velocity);
        if (sampleStore) samplerTrack->triggerPad(7, accent ? 1.0f : 0.6f, *sampleStore, rev);
        LedManager::instance().onVoiceTriggered(VoiceId::DrumClap, sceneManager_.currentScene().led);
        break;
  }
  
  if (step.fx != (uint8_t)StepFx::Reverse) {
      setupDrumStepFx_(voiceIdx, step.fx, step.fxParam, (uint8_t)step.velocity);
  } else {
      retrigDrums_[voiceIdx].active = false;
  }
}

void MiniAcid::advanceSongBar_() {
  const SongCycleBoundary boundary = nextSongCycleBoundary(
      songBarIndex_, sceneManager_.currentScene().feel.patternBars);
  songBarIndex_ = boundary.barIndex;

  if (boundary.advanceRow) {
    cyclePulseCounter_++;
    if (songMode_) {
      advanceSongPlayhead();
    }
  }
}

// P3: Bounded Phrase Source

void MiniAcid::setSequencedSource(int voiceIndex, SequencedSource source) {
  const int voice = clamp303Voice(voiceIndex);
  if (source == SequencedSource::Phrase) {
    if (retainedWorkingMelody_(voice) == nullptr) return;
  }
  const GroovePuterMaterial::MaterialKind nextKind =
      source == SequencedSource::Phrase
          ? GroovePuterMaterial::MaterialKind::Melody
          : GroovePuterMaterial::MaterialKind::Pattern;
  if (activeMaterial_[voice].kind == nextKind) return;
  hardBarrierPatternPlayback_(voice);
  activeMaterial_[voice].kind = nextKind;
}

MiniAcid::SequencedSource MiniAcid::currentSequencedSource(int voiceIndex) const {
  return activeMaterial_[clamp303Voice(voiceIndex)].kind ==
                 GroovePuterMaterial::MaterialKind::Melody
             ? SequencedSource::Phrase
             : SequencedSource::Pattern;
}

bool MiniAcid::initPendingMaterial() {
  for (int voice = 0; voice < NUM_303_VOICES; ++voice) {
    if (pendingMaterial_[voice].melody != nullptr) continue;
    pendingMaterial_[voice].melody =
        new (std::nothrow) PhraseRuntime::RuntimeSynthEventBuffer();
    if (pendingMaterial_[voice].melody == nullptr) return false;
  }
  if (!sourceAnchorSnapshot_) {
    sourceAnchorSnapshot_.reset(
        new (std::nothrow) PhraseRuntime::RuntimeSynthEventBuffer[NUM_303_VOICES]);
  }
  if (!sourceAnchorUndoSnapshot_) {
    sourceAnchorUndoSnapshot_.reset(
        new (std::nothrow) PhraseRuntime::RuntimeSynthEventBuffer[NUM_303_VOICES]);
  }
  return sourceAnchorMemoryReady_();
}

bool MiniAcid::pendingMaterialReady() const {
  for (int voice = 0; voice < NUM_303_VOICES; ++voice) {
    if (pendingMaterial_[voice].melody == nullptr) return false;
  }
  return true;
}

bool MiniAcid::sourceAnchorMemoryReady_() const {
  return sourceAnchorSnapshot_ != nullptr && sourceAnchorUndoSnapshot_ != nullptr;
}

const void* MiniAcid::pendingMaterialAddress(int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return nullptr;
  return pendingMaterial_[voiceIndex].melody;
}

bool MiniAcid::hasPendingMaterial(int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  return pendingMaterial_[voiceIndex].queued;
}

MiniAcid::CurrentNextState MiniAcid::classifyCurrentForNext_(
    int voiceIndex, GroovePuterMaterial::MaterialReference& reference,
    GroovePuterMaterial::MaterialVersionToken& acceptedVersion) const {
  using GroovePuterMaterial::MaterialKind;

  reference = {};
  acceptedVersion = {};
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return CurrentNextState::UnsupportedCurrentState;
  }

  const int idx = clamp303Voice(voiceIndex);
  if (!current303MaterialReference_(idx, reference)) {
    return CurrentNextState::UnsupportedCurrentState;
  }

  const Scene& scene = sceneManager_.currentScene();
  const int residentSlot = GroovePuterMaterial::residentSlotFor(reference.address);
  if (GroovePuterMaterial::residentKind(scene, idx, residentSlot) !=
      MaterialKind::Pattern) {
    // Accepted Melody requires filesystem resolution to prove canonical bytes.
    // FS2A deliberately performs no SD I/O in the NEXT lifecycle gate.
    return CurrentNextState::UnsupportedCurrentState;
  }

  const int bank = current303BankIndex(idx);
  const int pattern = display303LocalPatternIndex(idx);
  if (bank < 0 || bank >= kBankCount || pattern < 0 ||
      pattern >= Bank<SynthPattern>::kPatterns) {
    return CurrentNextState::UnsupportedCurrentState;
  }

  const SynthPattern& accepted = idx == 0
      ? scene.synthABanks[bank].patterns[pattern]
      : scene.synthBBanks[bank].patterns[pattern];
  acceptedVersion = GroovePuterMaterial::versionForPattern(accepted);

  const auto& working = workingMaterial_[idx];
  if (working.empty()) return CurrentNextState::CleanAcceptedPattern;
  if (working.holdsMelody()) return CurrentNextState::DirtyCurrent;
  if (!working.patternMatches(reference)) {
    // Retained Working that cannot be proven to belong to CURRENT is not safe
    // to overwrite. Fail closed instead of guessing from address alone.
    return CurrentNextState::UnsupportedCurrentState;
  }
  if (GroovePuterMaterial::versionForPattern(working.pattern()) !=
      acceptedVersion) {
    return CurrentNextState::DirtyCurrent;
  }
  return CurrentNextState::CleanAcceptedPattern;
}

MiniAcid::PreparationBasis MiniAcid::captureCurrentPreparationBasis(
    int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return PreparationBasis{};
  }
  const int idx = clamp303Voice(voiceIndex);
  GroovePuterMaterial::MaterialReference reference{};
  if (!current303MaterialReference_(idx, reference)) {
    return PreparationBasis{};
  }

  const auto& working = workingMaterial_[idx];
  if (working.holdsMelody()) {
    const auto* melody = working.melodyIfHeld();
    if (melody == nullptr || !RuntimePhraseEdit::validate(*melody)) {
      return PreparationBasis{};
    }
    return {reference, GroovePuterMaterial::MaterialKind::Melody,
            GroovePuterMaterial::versionForMelody(*melody)};
  }

  if (working.holdsPattern()) {
    if (!working.patternMatches(reference)) {
      return PreparationBasis{};
    }
    return {reference, GroovePuterMaterial::MaterialKind::Pattern,
            GroovePuterMaterial::versionForPattern(working.pattern())};
  }

  // working is empty: CURRENT is the accepted Pattern
  const Scene& scene = sceneManager_.currentScene();
  const int residentSlot =
      GroovePuterMaterial::residentSlotFor(reference.address);
  if (GroovePuterMaterial::residentKind(scene, idx, residentSlot) !=
      GroovePuterMaterial::MaterialKind::Pattern) {
    return PreparationBasis{};
  }

  const int bank = current303BankIndex(idx);
  const int pattern = display303LocalPatternIndex(idx);
  if (bank < 0 || bank >= kBankCount || pattern < 0 ||
      pattern >= Bank<SynthPattern>::kPatterns) {
    return PreparationBasis{};
  }

  const SynthPattern& accepted = idx == 0
      ? scene.synthABanks[bank].patterns[pattern]
      : scene.synthBBanks[bank].patterns[pattern];
  return {reference, GroovePuterMaterial::MaterialKind::Pattern,
          GroovePuterMaterial::versionForPattern(accepted)};
}

MiniAcid::NextPrepareResult MiniAcid::prepareNextMelody(
    int voiceIndex,
    const PhraseRuntime::RuntimeSynthEventBuffer& melody,
    const PreparationBasis& basis,
    GroovePuterMaterial::IdeaClassification classification) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return NextPrepareResult::InvalidVoice;
  }
  if (!RuntimePhraseEdit::validate(melody)) {
    return NextPrepareResult::InvalidCandidate;
  }

  PendingMaterial& pending = pendingMaterial_[voiceIndex];
  if (pending.melody == nullptr) {
    return NextPrepareResult::PendingUnavailable;
  }

  const PreparationBasis actualBasis =
      captureCurrentPreparationBasis(voiceIndex);
  if (!actualBasis.valid()) {
    return NextPrepareResult::UnsupportedCurrentState;
  }
  if (!basis.valid() || actualBasis != basis) {
    return NextPrepareResult::StalePreparationBasis;
  }

  const bool replacingLifecycleCandidate =
      pending.queued && pending.lifecycleBound;

  // stagePendingMaterial validates/copies before publishing queued metadata, so
  // causal metadata is replaced only after the new payload is fully staged.
  if (!stagePendingMaterial(
          voiceIndex,
          static_cast<uint16_t>(actualBasis.reference.address.globalSlot),
          GroovePuterMaterial::MaterialKind::Melody, &melody)) {
    return NextPrepareResult::PendingUnavailable;
  }

  pendingGeneration_[voiceIndex]++;
  if (goQueued_[voiceIndex] && goQueuedGeneration_[voiceIndex] != pendingGeneration_[voiceIndex]) {
    goQueued_[voiceIndex] = false;
  }

  pending.preparedFor = actualBasis.reference;
  pending.basisKind = actualBasis.kind;
  pending.acceptedVersion = actualBasis.version;
  pending.ideaClassification = classification;
  pending.lifecycleBound = true;
  return replacingLifecycleCandidate ? NextPrepareResult::Replaced
                                     : NextPrepareResult::Prepared;
}

bool MiniAcid::cancelNextMaterial(int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  const int idx = clamp303Voice(voiceIndex);
  cancelGoQueue(idx);
  pendingGeneration_[idx]++;
  PendingMaterial& pending = pendingMaterial_[idx];
  if (!pending.queued || !pending.lifecycleBound) return false;

  pending.queued = false;
  pending.lifecycleBound = false;
  pending.preparedFor = {};
  pending.basisKind = GroovePuterMaterial::MaterialKind::Pattern;
  pending.acceptedVersion = {};
  pending.ideaClassification = GroovePuterMaterial::IdeaClassification::Unknown;
  return true;
}

bool MiniAcid::acquireWorkingMelodySource(
    int voiceIndex,
    PhraseRuntime::RuntimeSynthEventBuffer& outBuffer) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  const int idx = clamp303Voice(voiceIndex);

  // If working holds Melody, use it directly without reinterpreting union
  if (workingMaterial_[idx].holdsMelody()) {
    const auto* melody = workingMaterial_[idx].melodyIfHeld();
    if (melody != nullptr && RuntimePhraseEdit::validate(*melody) && melody->count > 0) {
      outBuffer = *melody;
      return true;
    }
    return false;
  }

  // Working is Pattern or Empty (i.e. accepted Pattern is active).
  // Project activeSynthPattern into outBuffer privately without mutating CURRENT!
  const Scene& scene = sceneManager_.currentScene();
  const auto recipe = genreManager_.getGrooveRecipe();
  int swingPct = static_cast<int>(scene.feel.swingPct);
  if (swingPct < 50) swingPct = 50;
  if (swingPct > 75) swingPct = 75;

  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = static_cast<uint8_t>(idx);
  settings.gateLengthRatio = recipe.gateLengthRatio;
  settings.swingPercent = static_cast<uint8_t>(swingPct);
  const VoiceId voice = idx == 0 ? VoiceId::SynthA : VoiceId::SynthB;
  settings.swingEnabled =
      (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;

  if (PhraseRuntime::projectPatternToRuntimeEvents(
          activeSynthPattern(idx), settings, outBuffer) !=
      PhraseRuntime::PatternProjectionStatus::Ready) {
    return false;
  }

  const uint32_t phraseEndSubtick =
      static_cast<uint32_t>(outBuffer.lengthTicks) *
      PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < outBuffer.count; ++i) {
    auto& event = outBuffer.events[i];
    const uint32_t startSubtick =
        static_cast<uint32_t>(event.startTick) *
        PhraseRuntime::kSubticksPerTick;
    if (startSubtick >= phraseEndSubtick) {
      return false;
    }
    const uint32_t maxDuration = phraseEndSubtick - startSubtick;
    if (event.durationSubticks > maxDuration) {
      event.durationSubticks = static_cast<uint16_t>(maxDuration);
    }
  }

  return RuntimePhraseEdit::validate(outBuffer) && outBuffer.count > 0;
}

MiniAcid::GoRequestResult MiniAcid::requestGoNextMaterial(int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return GoRequestResult::InvalidVoice;
  }
  const int idx = clamp303Voice(voiceIndex);
  if (!pendingMaterial_[idx].queued || !pendingMaterial_[idx].lifecycleBound) {
    return GoRequestResult::NoPendingMaterial;
  }
  if (!playing) {
    const auto res = activateNextMaterialAtBoundary(idx);
    return (res == NextActivationResult::Activated)
        ? GoRequestResult::ActivatedImmediately
        : GoRequestResult::Failed;
  }
  goQueued_[idx] = true;
  goQueuedGeneration_[idx] = pendingGeneration_[idx];
  return GoRequestResult::Queued;
}

bool MiniAcid::isGoQueued(int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  return goQueued_[clamp303Voice(voiceIndex)];
}

void MiniAcid::cancelGoQueue(int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return;
  goQueued_[clamp303Voice(voiceIndex)] = false;
}

MiniAcid::NextPrepareResult MiniAcid::developWorkingMaterial(
    int voiceIndex,
    const GroovePuterDevelopment::DevelopmentRequest& request,
    GroovePuterDevelopment::DevelopmentResult* outResult) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return NextPrepareResult::InvalidVoice;
  }
  const int idx = clamp303Voice(voiceIndex);

  if (!sourceAnchorMemoryReady_()) {
    if (outResult != nullptr) {
      *outResult = {};
      outResult->classification.failureReason = "MATERIAL MEMORY UNAVAILABLE";
    }
    return NextPrepareResult::PendingUnavailable;
  }

  PhraseRuntime::RuntimeSynthEventBuffer sourceBuffer{};
  if (!acquireWorkingMelodySource(idx, sourceBuffer)) {
    return NextPrepareResult::UnsupportedCurrentState;
  }

  const PreparationBasis basis = captureCurrentPreparationBasis(idx);
  if (!basis.valid()) {
    return NextPrepareResult::UnsupportedCurrentState;
  }

  if (!hasSourceAnchorSnapshot_[idx]) {
    sourceAnchorSnapshot_[idx] = sourceBuffer;
    hasSourceAnchorSnapshot_[idx] = true;
  }

  const auto* anchorPtr = hasSourceAnchorSnapshot_[idx] ? &sourceAnchorSnapshot_[idx] : nullptr;
  const auto dev = GroovePuterDevelopment::developCandidate(sourceBuffer, request, anchorPtr);
  if (outResult != nullptr) {
    *outResult = dev;
  }
  if (!dev.success) {
    return NextPrepareResult::InvalidCandidate;
  }

  return prepareNextMelody(
      idx, dev.candidate, basis, dev.classification.idea);
}

MiniAcid::NextPrepareResult MiniAcid::growWorkingMaterial(
    int voiceIndex,
    uint8_t targetBars,
    GroovePuterDevelopment::GrowthMode mode,
    const GroovePuterDevelopment::DevelopmentRequest& request,
    GroovePuterDevelopment::DevelopmentResult* outResult) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return NextPrepareResult::InvalidVoice;
  }
  const int idx = clamp303Voice(voiceIndex);

  PhraseRuntime::RuntimeSynthEventBuffer sourceBuffer{};
  if (!acquireWorkingMelodySource(idx, sourceBuffer)) {
    return NextPrepareResult::UnsupportedCurrentState;
  }

  const PreparationBasis basis = captureCurrentPreparationBasis(idx);
  if (!basis.valid()) {
    return NextPrepareResult::UnsupportedCurrentState;
  }

  const auto dev = GroovePuterDevelopment::growMaterial(
      sourceBuffer, targetBars, mode, request);
  if (outResult != nullptr) {
    *outResult = dev;
  }
  if (!dev.success) {
    return NextPrepareResult::InvalidCandidate;
  }

  return prepareNextMelody(
      idx, dev.candidate, basis, dev.classification.idea);
}

MiniAcid::PreparationBasis MiniAcid::sourceAnchor(int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return {};
  const int idx = clamp303Voice(voiceIndex);
  if (developmentLineage_[idx].sourceAnchorBasis.valid()) {
    return developmentLineage_[idx].sourceAnchorBasis;
  }
  return captureCurrentPreparationBasis(idx);
}

MiniAcid::PreparationBasis MiniAcid::predecessor(int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return {};
  const int idx = clamp303Voice(voiceIndex);
  if (developmentLineage_[idx].predecessorBasis.valid()) {
    return developmentLineage_[idx].predecessorBasis;
  }
  return captureCurrentPreparationBasis(idx);
}

const PhraseRuntime::RuntimeSynthEventBuffer* MiniAcid::sourceAnchorSnapshot(
    int voiceIndex) const {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return nullptr;
  if (!sourceAnchorMemoryReady_()) return nullptr;
  const int idx = clamp303Voice(voiceIndex);
  return hasSourceAnchorSnapshot_[idx] ? &sourceAnchorSnapshot_[idx] : nullptr;
}

MiniAcid::NextActivationResult MiniAcid::activateNextMaterialAtBoundary(
    int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return NextActivationResult::InvalidVoice;
  }

  const int idx = clamp303Voice(voiceIndex);
  PendingMaterial& pending = pendingMaterial_[idx];
  if (!pending.queued) return NextActivationResult::NoPending;
  if (!pending.lifecycleBound) return NextActivationResult::UnboundPending;

  const PreparationBasis currentBasis =
      captureCurrentPreparationBasis(idx);
  if (!currentBasis.valid()) {
    return NextActivationResult::UnsupportedCurrentState;
  }

  if (currentBasis.reference.address != pending.preparedFor.address ||
      currentBasis.reference.id != pending.preparedFor.id) {
    return NextActivationResult::RejectedReferenceMismatch;
  }

  if (currentBasis.kind != pending.basisKind ||
      currentBasis.version != pending.acceptedVersion) {
    return NextActivationResult::RejectedCanonicalChanged;
  }

  if (pending.kind != GroovePuterMaterial::MaterialKind::Melody ||
      pending.melody == nullptr) {
    return NextActivationResult::UnboundPending;
  }
  if (!sourceAnchorMemoryReady_()) {
    return NextActivationResult::UnboundPending;
  }

  // Session lifecycle single-slot Undo receipt.
  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = static_cast<uint8_t>(idx);
  receipt.source = static_cast<uint8_t>(currentSequencedSource(idx));
  receipt.reference = currentBasis.reference;

  if (workingMaterial_[idx].holdsMelody()) {
    receipt.representation = 1;
    receipt.before = workingMaterial_[idx].melody();
    receipt.wasDirty = true;
  } else {
    receipt.representation = 0;
    receipt.wasDirty = workingMaterial_[idx].holdsPattern();
    if (receipt.wasDirty) {
      receipt.patternBefore = workingMaterial_[idx].pattern();
    } else {
      const int bank = current303BankIndex(idx);
      const int pattern = display303LocalPatternIndex(idx);
      const Scene& scene = sceneManager_.currentScene();
      receipt.patternBefore = idx == 0 ? scene.synthABanks[bank].patterns[pattern]
                                       : scene.synthBBanks[bank].patterns[pattern];
    }
    receipt.before.lengthTicks = PhraseRuntime::kTicksPerBar;
    receipt.before.count = 0;
  }

  receipt.lineageBefore = developmentLineage_[idx];
  receipt.hadSourceAnchorSnapshot = hasSourceAnchorSnapshot_[idx];

  const PreparationBasis preparedBasis{
      pending.preparedFor, pending.basisKind, pending.acceptedVersion};
  const auto classification = pending.ideaClassification;

  bool committed = false;
  auto& undo = GroovePuterUndo::undoOwner();
  const bool published = undo.commitRuntimePrepared(
      GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
        if (!activatePendingMaterialForVoice_(idx)) {
          return;
        }

        // Lineage tracking:
        developmentLineage_[idx].predecessorBasis = preparedBasis;
        if (classification == GroovePuterMaterial::IdeaClassification::NewIdea) {
          developmentLineage_[idx].sourceAnchorBasis =
              captureCurrentPreparationBasis(idx);
          sourceAnchorUndoSnapshot_[idx] = sourceAnchorSnapshot_[idx];
          hasSourceAnchorUndoSnapshot_[idx] = hasSourceAnchorSnapshot_[idx];
          sourceAnchorSnapshot_[idx] = *pending.melody;
          hasSourceAnchorSnapshot_[idx] = true;
        } else {
          if (!developmentLineage_[idx].sourceAnchorBasis.valid()) {
            developmentLineage_[idx].sourceAnchorBasis = preparedBasis;
          }
          if (!hasSourceAnchorSnapshot_[idx] && pending.melody != nullptr) {
            sourceAnchorSnapshot_[idx] = *pending.melody;
            hasSourceAnchorSnapshot_[idx] = true;
          }
        }
        committed = true;
      });

  if (!published || !committed) {
    return NextActivationResult::UnboundPending;
  }
  return NextActivationResult::Activated;
}

MiniAcid::DiscardResult MiniAcid::discardCurrentMaterial(int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return DiscardResult::InvalidVoice;
  }
  const int idx = clamp303Voice(voiceIndex);

  GroovePuterMaterial::MaterialReference reference{};
  GroovePuterMaterial::MaterialVersionToken acceptedVersion{};
  const CurrentNextState state =
      classifyCurrentForNext_(idx, reference, acceptedVersion);
  if (state == CurrentNextState::UnsupportedCurrentState) {
    return DiscardResult::UnsupportedCurrentState;
  }

  // A genuinely clean Pattern already projects ACCEPTED directly. Keep this
  // path a strict no-op, including NEXT and runtime state.
  if (state == CurrentNextState::CleanAcceptedPattern &&
      workingMaterial_[idx].empty() &&
      activeMaterial_[idx].kind == GroovePuterMaterial::MaterialKind::Pattern) {
    return DiscardResult::AlreadyClean;
  }

  const int page = currentPageIndex();
  const int bank = current303BankIndex(idx);
  const int pattern = display303LocalPatternIndex(idx);
  if (patternRuntimeBank_.pageIdentity() != page || bank < 0 ||
      bank >= kBankCount || pattern < 0 ||
      pattern >= Bank<SynthPattern>::kPatterns) {
    return DiscardResult::UnsupportedCurrentState;
  }

  const Scene& scene = sceneManager_.currentScene();
  const SynthPattern& accepted =
      idx == 0 ? scene.synthABanks[bank].patterns[pattern]
               : scene.synthBBanks[bank].patterns[pattern];
  if (GroovePuterMaterial::versionForPattern(accepted) != acceptedVersion) {
    // The canonical basis must remain the exact one proven by the classifier.
    return DiscardResult::UnsupportedCurrentState;
  }

  const auto recipe = genreManager_.getGrooveRecipe();
  int swingPct = static_cast<int>(scene.feel.swingPct);
  if (swingPct < 50) swingPct = 50;
  if (swingPct > 75) swingPct = 75;

  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = static_cast<uint8_t>(idx);
  settings.gateLengthRatio = recipe.gateLengthRatio;
  settings.swingPercent = static_cast<uint8_t>(swingPct);
  const VoiceId voice = idx == 0 ? VoiceId::SynthA : VoiceId::SynthB;
  settings.swingEnabled =
      (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;

  // Refresh is prepare-then-publish internally. Do the only fallible audible
  // operation before clearing Working or changing source ownership.
  if (patternRuntimeBank_.refresh(
          static_cast<uint8_t>(idx), static_cast<uint8_t>(bank),
          static_cast<uint8_t>(pattern), accepted, settings) !=
      PhraseRuntime::PatternBankRefreshStatus::Ready) {
    return DiscardResult::UnsupportedCurrentState;
  }

  setSequencedSource(idx, SequencedSource::Pattern);
  publishActiveMaterial(
      idx, static_cast<uint16_t>(reference.address.globalSlot),
      GroovePuterMaterial::MaterialKind::Pattern);
  workingMaterial_[idx].clear();
  developmentLineage_[idx] = {};
  hasSourceAnchorSnapshot_[idx] = false;
  hasSourceAnchorUndoSnapshot_[idx] = false;
  cancelGoQueue(idx);
  return DiscardResult::Discarded;
}

MiniAcid::MaterialLengthResult MiniAcid::setMaterialLength(
    int voiceIndex, uint8_t targetBars) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return MaterialLengthResult::InvalidVoice;
  }
  const int idx = clamp303Voice(voiceIndex);

  if (targetBars != 1 && targetBars != 2 && targetBars != 4 && targetBars != 8) {
    return MaterialLengthResult::InvalidLength;
  }

  GroovePuterMaterial::MaterialReference reference{};
  GroovePuterMaterial::MaterialVersionToken acceptedVersion{};
  const CurrentNextState state =
      classifyCurrentForNext_(idx, reference, acceptedVersion);
  if (state == CurrentNextState::UnsupportedCurrentState) {
    return MaterialLengthResult::UnsupportedCurrentState;
  }

  const int page = currentPageIndex();
  const int bank = current303BankIndex(idx);
  const int pattern = display303LocalPatternIndex(idx);
  if (patternRuntimeBank_.pageIdentity() != page || bank < 0 ||
      bank >= kBankCount || pattern < 0 ||
      pattern >= Bank<SynthPattern>::kPatterns) {
    return MaterialLengthResult::UnsupportedCurrentState;
  }

  const Scene& scene = sceneManager_.currentScene();
  const SynthPattern& accepted =
      idx == 0 ? scene.synthABanks[bank].patterns[pattern]
               : scene.synthBBanks[bank].patterns[pattern];
  if (GroovePuterMaterial::versionForPattern(accepted) != acceptedVersion) {
    return MaterialLengthResult::UnsupportedCurrentState;
  }

  const bool isMelody =
      workingMaterial_[idx].holdsMelody() ||
      activeMaterial_[idx].kind == GroovePuterMaterial::MaterialKind::Melody;
  const uint8_t currentBars = isMelody
      ? static_cast<uint8_t>(
            workingMaterial_[idx].melody().lengthTicks / PhraseRuntime::kTicksPerBar)
      : 1;

  if (targetBars == currentBars) {
    return MaterialLengthResult::Unchanged;
  }

  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  receipt.voiceIndex = static_cast<uint8_t>(idx);
  receipt.source = static_cast<uint8_t>(currentSequencedSource(idx));
  receipt.reference = reference;
  receipt.wasDirty = (state == CurrentNextState::DirtyCurrent);

  PhraseRuntime::RuntimeSynthEventBuffer candidate{};

  if (isMelody) {
    receipt.representation = 1;
    receipt.before = workingMaterial_[idx].melody();
    candidate = workingMaterial_[idx].melody();

    const auto editResult = RuntimePhraseEdit::setLengthBars(candidate, targetBars);
    if (editResult == RuntimePhraseEdit::LengthEditResult::NoChange) {
      return MaterialLengthResult::Unchanged;
    }
    if (editResult == RuntimePhraseEdit::LengthEditResult::Rejected) {
      if (targetBars < currentBars) {
        return MaterialLengthResult::WouldTruncate;
      }
      return MaterialLengthResult::UnsupportedCurrentState;
    }
  } else {
    // Current is Pattern (1 bar). Target is 2, 4, or 8 bars.
    receipt.representation = 0;
    receipt.patternBefore = workingMaterial_[idx].holdsPattern()
        ? workingMaterial_[idx].pattern()
        : accepted;
    receipt.before.lengthTicks = PhraseRuntime::kTicksPerBar;
    receipt.before.count = 0;

    const SynthPattern& sourcePattern = receipt.patternBefore;
    const auto recipe = genreManager_.getGrooveRecipe();
    int swingPct = static_cast<int>(scene.feel.swingPct);
    if (swingPct < 50) swingPct = 50;
    if (swingPct > 75) swingPct = 75;

    PhraseRuntime::PatternProjectionSettings settings{};
    settings.synthIndex = static_cast<uint8_t>(idx);
    settings.gateLengthRatio = recipe.gateLengthRatio;
    settings.swingPercent = static_cast<uint8_t>(swingPct);
    const VoiceId voice = idx == 0 ? VoiceId::SynthA : VoiceId::SynthB;
    settings.swingEnabled =
        (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;

    if (PhraseRuntime::projectPatternToRuntimeEvents(
            sourcePattern, settings, candidate) !=
        PhraseRuntime::PatternProjectionStatus::Ready) {
      return MaterialLengthResult::UnsupportedCurrentState;
    }

    const uint32_t phraseEndSubtick =
        static_cast<uint32_t>(candidate.lengthTicks) *
        PhraseRuntime::kSubticksPerTick;
    for (uint16_t i = 0; i < candidate.count; ++i) {
      auto& event = candidate.events[i];
      const uint32_t startSubtick =
          static_cast<uint32_t>(event.startTick) *
          PhraseRuntime::kSubticksPerTick;
      if (startSubtick >= phraseEndSubtick) {
        return MaterialLengthResult::UnsupportedCurrentState;
      }
      const uint32_t maxDuration = phraseEndSubtick - startSubtick;
      if (event.durationSubticks > maxDuration) {
        event.durationSubticks = static_cast<uint16_t>(maxDuration);
      }
    }

    if (RuntimePhraseEdit::setLengthBars(candidate, targetBars) !=
        RuntimePhraseEdit::LengthEditResult::Changed) {
      return MaterialLengthResult::UnsupportedCurrentState;
    }
  }

  if (!RuntimePhraseEdit::validate(candidate)) {
    return MaterialLengthResult::UnsupportedCurrentState;
  }

  bool committed = false;
  auto& undo = GroovePuterUndo::undoOwner();
  const bool published = undo.commitRuntimePrepared(
      GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
        workingMaterial_[idx].storeMelody(candidate);
        setSequencedSource(idx, SequencedSource::Phrase);
        publishActiveMaterial(
            idx, static_cast<uint16_t>(reference.address.globalSlot),
            GroovePuterMaterial::MaterialKind::Melody);
        committed = true;
      });

  if (!published || !committed) {
    return MaterialLengthResult::UnsupportedCurrentState;
  }
  return MaterialLengthResult::Changed;
}

bool MiniAcid::undoMaterialWorking(int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  const int idx = clamp303Voice(voiceIndex);

  auto& owner = GroovePuterUndo::undoOwner();
  if (!owner.hasUndo() ||
      owner.kind() != GroovePuterUndo::UndoKind::RuntimePhrase) {
    return false;
  }

  GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
  if (!owner.read(GroovePuterUndo::UndoKind::RuntimePhrase, receipt)) {
    return false;
  }
  if (receipt.voiceIndex != idx) {
    return false;
  }

  if (receipt.representation == 0) {
    if (receipt.wasDirty) {
      workingMaterial_[idx].storePattern(receipt.patternBefore, receipt.reference);
    } else {
      workingMaterial_[idx].clear();
    }
    setSequencedSource(idx, static_cast<SequencedSource>(receipt.source));
    publishActiveMaterial(
        idx, static_cast<uint16_t>(receipt.reference.address.globalSlot),
        GroovePuterMaterial::MaterialKind::Pattern);

    const int page = currentPageIndex();
    const int bank = current303BankIndex(idx);
    const int pattern = display303LocalPatternIndex(idx);
    if (patternRuntimeBank_.pageIdentity() == page && bank >= 0 &&
        bank < kBankCount && pattern >= 0 &&
        pattern < Bank<SynthPattern>::kPatterns) {
      const Scene& scene = sceneManager_.currentScene();
      const SynthPattern& patternToRefresh = receipt.wasDirty
          ? receipt.patternBefore
          : (idx == 0 ? scene.synthABanks[bank].patterns[pattern]
                      : scene.synthBBanks[bank].patterns[pattern]);
      const auto recipe = genreManager_.getGrooveRecipe();
      int swingPct = static_cast<int>(scene.feel.swingPct);
      if (swingPct < 50) swingPct = 50;
      if (swingPct > 75) swingPct = 75;
      PhraseRuntime::PatternProjectionSettings settings{};
      settings.synthIndex = static_cast<uint8_t>(idx);
      settings.gateLengthRatio = recipe.gateLengthRatio;
      settings.swingPercent = static_cast<uint8_t>(swingPct);
      const VoiceId voice = idx == 0 ? VoiceId::SynthA : VoiceId::SynthB;
      settings.swingEnabled =
          (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;
      (void)patternRuntimeBank_.refresh(
          static_cast<uint8_t>(idx), static_cast<uint8_t>(bank),
          static_cast<uint8_t>(pattern), patternToRefresh, settings);
    }
  } else {
    workingMaterial_[idx].storeMelody(receipt.before);
    setSequencedSource(idx, static_cast<SequencedSource>(receipt.source));
    publishActiveMaterial(
        idx, static_cast<uint16_t>(receipt.reference.address.globalSlot),
        GroovePuterMaterial::MaterialKind::Melody);
  }

  developmentLineage_[idx] = receipt.lineageBefore;
  hasSourceAnchorSnapshot_[idx] = receipt.hadSourceAnchorSnapshot;
  if (receipt.hadSourceAnchorSnapshot) {
    sourceAnchorSnapshot_[idx] = sourceAnchorUndoSnapshot_[idx];
  }

  owner.clear();
  return true;
}

MiniAcid::AcceptResult MiniAcid::acceptMaterialWorking(int voiceIndex) {
  LOG_DEBUG("[ACCEPT] voice=%d\n", voiceIndex);
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    LOG_DEBUG("[ACCEPT] stage=preflight result=InvalidVoice\n");
    return AcceptResult::InvalidVoice;
  }
  const int idx = clamp303Voice(voiceIndex);

  auto& working = workingMaterial_[idx];
  const char* workingType = working.holdsPattern() ? "pattern" : (working.holdsMelody() ? "melody" : "none");
  LOG_DEBUG("[ACCEPT] working=%s\n", workingType);
  const bool dirty = hasModifiedWorking303Pattern(idx) || working.holdsMelody();
  LOG_DEBUG("[ACCEPT] dirty=%d\n", dirty ? 1 : 0);

  if (working.empty()) {
    LOG_DEBUG("[ACCEPT] stage=preflight result=AlreadyClean\n");
    return AcceptResult::AlreadyClean;
  }
  LOG_DEBUG("[ACCEPT] stage=preflight result=OK\n");

  const int page = currentPageIndex();
  const int bank = current303BankIndex(idx);
  const int pattern = display303LocalPatternIndex(idx);
  if (patternRuntimeBank_.pageIdentity() != page || bank < 0 ||
      bank >= kBankCount || pattern < 0 ||
      pattern >= Bank<SynthPattern>::kPatterns) {
    LOG_DEBUG("[ACCEPT] stage=resolve result=PageOrBankMismatch page=%d pageIdent=%d bank=%d pat=%d\n",
              page, patternRuntimeBank_.pageIdentity(), bank, pattern);
    return AcceptResult::UnsupportedCurrentState;
  }

  const int globalSlot = songPatternFromPageBankIndex(page, bank, pattern);
  if (globalSlot < 0 || globalSlot >= kMaxGlobalPatterns || globalSlot > 0xff) {
    LOG_DEBUG("[ACCEPT] stage=resolve result=InvalidGlobalSlot\n");
    return AcceptResult::UnsupportedCurrentState;
  }

  const GroovePuterMaterial::MaterialAddress address{
      static_cast<uint8_t>(idx), static_cast<uint8_t>(globalSlot)};
  if (!GroovePuterMaterial::materialAddressIsResident(address, page)) {
    LOG_DEBUG("[ACCEPT] stage=resolve result=NotResident\n");
    return AcceptResult::UnsupportedCurrentState;
  }

  Scene& scene = sceneManager_.currentScene();
  const int residentSlot = GroovePuterMaterial::residentSlotFor(address);
  const auto existingId = GroovePuterMaterial::residentId(scene, idx, residentSlot);
  const GroovePuterMaterial::MaterialReference reference{address, existingId};
  LOG_DEBUG("[ACCEPT] ref.address={voice:%u, globalSlot:%u}\n",
            reference.address.voice, reference.address.globalSlot);
  LOG_DEBUG("[ACCEPT] ref.id=%u\n", reference.id.value);
  LOG_DEBUG("[ACCEPT] stage=resolve result=OK\n");

  const std::string& proj = PatternPagingService::currentProjectName();

  // 1. If Working holds a Pattern
  if (working.holdsPattern()) {
    if (existingId.valid()) {
      if (!working.patternMatches(reference)) {
        LOG_DEBUG("[ACCEPT] stage=validate result=RefMismatch\n");
        return AcceptResult::UnsupportedCurrentState;
      }
    } else {
      const auto stored = working.patternReference();
      if (stored.id.valid() ||
          (stored.address != address && stored.address.voice != 0xFF)) {
        LOG_DEBUG("[ACCEPT] stage=validate result=UnassignedRefMismatch\n");
        return AcceptResult::UnsupportedCurrentState;
      }
    }

    const SynthPattern candidate = working.pattern();
    const SynthPattern& accepted = (idx == 0)
        ? scene.synthABanks[bank].patterns[pattern]
        : scene.synthBBanks[bank].patterns[pattern];

    if (GroovePuterMaterial::versionForPattern(candidate) ==
        GroovePuterMaterial::versionForPattern(accepted)) {
      working.clear();
      LOG_DEBUG("[ACCEPT] stage=validate result=AlreadyClean\n");
      return AcceptResult::AlreadyClean;
    }
    LOG_DEBUG("[ACCEPT] stage=validate result=OK\n");

    if (!PatternPagingService::commitPatternCandidate(
            page, scene, idx, bank, pattern, candidate)) {
      LOG_DEBUG("[ACCEPT] stage=page_candidate result=CommitFailed\n");
      return AcceptResult::CommitFailed;
    }
    LOG_DEBUG("[ACCEPT] stage=page_candidate result=OK\n");
    LOG_DEBUG("[ACCEPT] stage=nvs_publish result=OK\n");

    const auto sourceBefore = currentSequencedSource(idx);
    working.clear();
    developmentLineage_[idx] = {};
    setSequencedSource(idx, sourceBefore);
    publishActiveMaterial(idx, static_cast<uint16_t>(reference.address.globalSlot),
                          GroovePuterMaterial::MaterialKind::Pattern);

    const auto recipe = genreManager_.getGrooveRecipe();
    int swingPct = static_cast<int>(scene.feel.swingPct);
    if (swingPct < 50) swingPct = 50;
    if (swingPct > 75) swingPct = 75;
    PhraseRuntime::PatternProjectionSettings settings{};
    settings.synthIndex = static_cast<uint8_t>(idx);
    settings.gateLengthRatio = recipe.gateLengthRatio;
    settings.swingPercent = static_cast<uint8_t>(swingPct);
    const VoiceId voice = idx == 0 ? VoiceId::SynthA : VoiceId::SynthB;
    settings.swingEnabled = (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;
    (void)patternRuntimeBank_.refresh(static_cast<uint8_t>(idx),
                                     static_cast<uint8_t>(bank),
                                     static_cast<uint8_t>(pattern),
                                     candidate, settings);

    auto& owner = GroovePuterUndo::undoOwner();
    if (owner.hasUndo() && owner.kind() == GroovePuterUndo::UndoKind::RuntimePhrase) {
      GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
      if (owner.read(GroovePuterUndo::UndoKind::RuntimePhrase, receipt) && receipt.voiceIndex == idx) {
        owner.clear();
      }
    }

    LOG_DEBUG("[ACCEPT] stage=ram_publish result=OK\n");
    return AcceptResult::Accepted;
  }

  // 2. If Working holds a Melody
  if (working.holdsMelody()) {
    const auto candidate = working.melody();
    if (!RuntimePhraseEdit::validate(candidate)) {
      LOG_DEBUG("[ACCEPT] stage=validate result=InvalidCandidate\n");
      return AcceptResult::InvalidCandidate;
    }
    LOG_DEBUG("[ACCEPT] stage=validate result=OK\n");

    const size_t slotOffset = static_cast<size_t>(bank * Bank<SynthPattern>::kPatterns + pattern);
    GroovePuterMaterial::MaterialId candidateId = scene.materialSlots[idx][slotOffset].id;
    if (!candidateId.valid()) {
      candidateId = PatternPagingService::allocateMaterialId();
      if (!candidateId.valid()) {
        LOG_DEBUG("[ACCEPT] stage=melody_payload result=AllocIdFailed\n");
        return AcceptResult::CommitFailed;
      }
    }

    uint32_t melodyGen = 0;
    auto& fs = MelodyPromotion::defaultFileSystem();
    const auto melodyErr = MelodyPromotion::commitMelodyCandidate(
        fs, proj, address, candidate, melodyGen);
    if (melodyErr != MelodyPromotion::Error::None) {
      LOG_DEBUG("[ACCEPT] stage=melody_payload result=CommitMelodyFailed err=%d gen=%u proj=%s\n",
                static_cast<int>(melodyErr), melodyGen, proj.c_str());
      return AcceptResult::CommitFailed;
    }
    LOG_DEBUG("[ACCEPT] stage=melody_payload result=OK gen=%u\n", melodyGen);

    if (!PatternPagingService::commitMelodyDescriptor(
            page, scene, idx, bank, pattern, candidateId)) {
      const auto activeSlot = PatternPagingService::activePublicationSlot(page);
      const auto targetSlot = (activeSlot == GroovePuterMaterial::PublicationSlot::SlotA)
          ? GroovePuterMaterial::PublicationSlot::SlotB
          : GroovePuterMaterial::PublicationSlot::SlotA;
      fs.remove(MelodyPromotion::slotPath(proj, address, targetSlot).c_str());
      LOG_DEBUG("[ACCEPT] stage=page_candidate result=CommitMelodyDescriptorFailed page=%d activeSlot=%d\n",
                page, static_cast<int>(activeSlot));
      return AcceptResult::CommitFailed;
    }
    LOG_DEBUG("[ACCEPT] stage=page_candidate result=OK\n");
    LOG_DEBUG("[ACCEPT] stage=nvs_publish result=OK\n");

    const auto sourceBefore = currentSequencedSource(idx);
    setSequencedSource(idx, sourceBefore);
    publishActiveMaterial(idx, static_cast<uint16_t>(reference.address.globalSlot),
                          GroovePuterMaterial::MaterialKind::Melody);
    developmentLineage_[idx] = {};

    auto& owner = GroovePuterUndo::undoOwner();
    if (owner.hasUndo() && owner.kind() == GroovePuterUndo::UndoKind::RuntimePhrase) {
      GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
      if (owner.read(GroovePuterUndo::UndoKind::RuntimePhrase, receipt) && receipt.voiceIndex == idx) {
        owner.clear();
      }
    }

    LOG_DEBUG("[ACCEPT] stage=ram_publish result=OK\n");
    return AcceptResult::Accepted;
  }

  LOG_DEBUG("[ACCEPT] stage=validate result=UnsupportedCurrentState\n");
  return AcceptResult::UnsupportedCurrentState;
}


bool MiniAcid::stagePendingMaterial(
    int voiceIndex, uint16_t slot, GroovePuterMaterial::MaterialKind kind,
    const PhraseRuntime::RuntimeSynthEventBuffer* melody) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  PendingMaterial& pending = pendingMaterial_[voiceIndex];
  if (pending.melody == nullptr) return false;

  // A Melody request that arrives without prepared material is the failed-load
  // case. Refusing it here is what keeps a half-prepared request from ever
  // reaching a boundary; the voice simply keeps playing what it was.
  if (kind == GroovePuterMaterial::MaterialKind::Melody) {
    if (melody == nullptr) return false;
    if (!RuntimePhraseEdit::validate(*melody)) return false;
    *pending.melody = *melody;
  }
  // The lower M4 primitive has no proof that its payload was prepared
  // against the upper FS2A CURRENT identity/version. A successful raw stage
  // therefore becomes explicitly unbound; prepareNextMelody() re-binds only
  // after staging succeeds and it has captured the exact canonical basis.
  pending.lifecycleBound = false;
  pending.preparedFor = {};
  pending.acceptedVersion = {};
  pending.slot = slot;
  pending.kind = kind;
  pending.queued = true;
  return true;
}

bool MiniAcid::activatePendingMaterialForVoice_(int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  PendingMaterial& pending = pendingMaterial_[voiceIndex];
  if (!pending.queued) return false;
  if (pending.kind == GroovePuterMaterial::MaterialKind::Melody &&
      pending.melody != nullptr) {
    workingMaterial_[voiceIndex].storeMelody(*pending.melody);
    setSequencedSource(voiceIndex, SequencedSource::Phrase);
  }
  publishActiveMaterial(voiceIndex, pending.slot, pending.kind);
  pending.queued = false;
  pending.lifecycleBound = false;
  return true;
}

void MiniAcid::activatePendingMaterial() {
  for (int voice = 0; voice < NUM_303_VOICES; ++voice) {
    (void)activatePendingMaterialForVoice_(voice);
  }
}

void MiniAcid::publishActiveMaterial(int voiceIndex, uint16_t slot,
                                     GroovePuterMaterial::MaterialKind kind) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return;
  activeMaterial_[voiceIndex].slot = slot;
  activeMaterial_[voiceIndex].kind = kind;
}

const MiniAcid::ActiveMaterial& MiniAcid::activeMaterial(int voiceIndex) const {
  static const ActiveMaterial kFallback{};
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return kFallback;
  return activeMaterial_[voiceIndex];
}

uint16_t MiniAcid::currentPhrasePlayTick(int voiceIndex) const {
  const auto* phrase = readableWorkingMelody_(voiceIndex);
  if (phrase == nullptr || phrase->lengthTicks == 0) return 0;
  return static_cast<uint16_t>(currentTick_ % phrase->lengthTicks);
}

bool MiniAcid::makePhrase(int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  // One-way. A voice already on Phrase keeps what it has; re-projecting would
  // silently discard every edit made since the conversion.
  if (activeMaterial_[voiceIndex].kind ==
      GroovePuterMaterial::MaterialKind::Melody) {
    return false;
  }

  // Same projection inputs the runtime bank already uses, so converted material
  // sounds like the Pattern it came from rather than a second interpretation.
  const Scene& scene = sceneManager_.currentScene();
  const auto recipe = genreManager_.getGrooveRecipe();
  const int swingPct = std::clamp(
      static_cast<int>(scene.feel.swingPct), 50, 75);

  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = static_cast<uint8_t>(voiceIndex);
  settings.gateLengthRatio = recipe.gateLengthRatio;
  settings.swingPercent = static_cast<uint8_t>(swingPct);
  const VoiceId voice = voiceIndex == 0 ? VoiceId::SynthA : VoiceId::SynthB;
  settings.swingEnabled =
      (scene.feel.swingMask & (1u << static_cast<int>(voice))) != 0;

  // Project into a candidate first. A failed projection must leave the voice
  // exactly as it was, with neither half of the conversion committed.
  PhraseRuntime::RuntimeSynthEventBuffer candidate{};
  if (PhraseRuntime::projectPatternToRuntimeEvents(
          activeSynthPattern(voiceIndex), settings, candidate) !=
      PhraseRuntime::PatternProjectionStatus::Ready) {
    return false;
  }

  // Pattern lifetime is cyclic: a late onset may legitimately sustain through
  // the bar boundary into step 0. A Phrase/Melody is linear, so MAKE PHRASE is
  // the ownership boundary where that cyclic tail must be bounded to the new
  // object's terminal extent. Do not change Pattern projection semantics.
  const uint32_t phraseEndSubtick =
      static_cast<uint32_t>(candidate.lengthTicks) *
      PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& event = candidate.events[i];
    const uint32_t startSubtick =
        static_cast<uint32_t>(event.startTick) *
        PhraseRuntime::kSubticksPerTick;
    if (startSubtick >= phraseEndSubtick) return false;
    const uint32_t maxDuration = phraseEndSubtick - startSubtick;
    if (event.durationSubticks > maxDuration) {
      event.durationSubticks = static_cast<uint16_t>(maxDuration);
    }
  }
  if (!RuntimePhraseEdit::validate(candidate)) return false;

  workingMaterial_[voiceIndex].storeMelody(candidate);
  setSequencedSource(voiceIndex, SequencedSource::Phrase);
  return true;
}

bool MiniAcid::setPhraseLength(int voiceIndex, uint8_t barCount) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;

  auto* held = workingMaterial_[voiceIndex].melodyIfHeld();
  if (held == nullptr) return false;
  auto candidate = *held;
  const uint16_t targetTicks =
      RuntimePhraseEdit::lengthTicksForBars(barCount);
  if (targetTicks == 0) return false;
  if (targetTicks == candidate.lengthTicks) {
    return RuntimePhraseEdit::validate(candidate);
  }

  if (targetTicks > candidate.lengthTicks) {
    // Expansion may repair a note which already crosses the old end, so grant
    // the requested extent before validating the complete candidate.
    candidate.lengthTicks = targetTicks;
    if (!RuntimePhraseEdit::validate(candidate)) return false;
  } else if (RuntimePhraseEdit::setLengthBars(candidate, barCount) !=
             RuntimePhraseEdit::LengthEditResult::Changed) {
    return false;
  }

  return RuntimePhraseEdit::commit(*held, candidate);
}

PhraseRuntime::RuntimeSynthEventBuffer& MiniAcid::currentPhraseBuffer(
    int voiceIndex) {
  return workingMaterial_[clamp303Voice(voiceIndex)].melody();
}

const PhraseRuntime::RuntimeSynthEventBuffer& MiniAcid::currentPhraseBuffer(
    int voiceIndex) const {
  return workingMaterial_[clamp303Voice(voiceIndex)].melody();
}
