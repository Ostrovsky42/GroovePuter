#include "miniacid_engine.h"

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
#include <string>

#include "../audio/audio_diagnostics.h"

#include "../sampler/sample_index.h"
#include "../ui/led_manager.h"

#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#include <esp_psram.h>
#endif

#include "../platform/log.h"
#include "advanced_pattern_generator.h"

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
  
  maxDelaySamples = newMaxSamples;
  size_t required = static_cast<size_t>(maxDelaySamples);
  
  // Log allocation attempt
  LOG_DEBUG("TempoDelay::init: Allocating %d samples (%.1f KB)...\n", 
                maxDelaySamples, (maxDelaySamples * sizeof(float)) / 1024.0f);

  buffer.assign(required, 0.0f);
  
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
  : voice303(sampleRate),
    voice3032(sampleRate),
    drums(std::make_unique<TR808DrumSynthVoice>(sampleRate)),
    sampleRateValue(sampleRate),
    drumEngineName_("808"),
    sceneStorage_(sceneStorage),
    samplerOutBuffer(std::make_unique<float[]>(AUDIO_BUFFER_SAMPLES)),
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
    currentStepIndex(-1),
    samplesIntoStep(0),
    samplesPerStep(0.0f),
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
  
  // Initialize Drum FX
  drumReverb.setSampleRate(sampleRateValue);
  drumTransientShaper.setSampleRate(sampleRateValue);
  
  // NEW: Configure voice processing chain
  // HPF @ 150Hz is built-in to compressor
  voiceCompressor_.setThreshold(0.3f);      // -10dB
  voiceCompressor_.setRatio(4.0f);          // 4:1 compression
  voiceCompressor_.setMakeupGain(2.8f);     // +9dB boost
  voiceCompressor_.setPresenceBoost(0.5f);  // +3dB @ 2kHz
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
    if (tapeLooper) tapeLooper->init(1.0f);
    if (sampleStore) sampleStore->setPoolSize(32 * 1024); // 32KB sampler pool
    delay303.init(0.15f);
    delay3032.init(0.15f);
    
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

  LOG_PRINTLN("  - MiniAcid::init: reset()...");
  reset();
  LOG_PRINTLN("  - MiniAcid::init: applySceneStateFromManager()...");
  applySceneStateFromManager();
  LOG_PRINTLN("  - MiniAcid::init: Done");
}

void MiniAcid::reset() {
  LOG_PRINTLN("    - MiniAcid::reset: Start");
  voice303.reset();
  voice3032.reset();
  LOG_PRINTLN("    - MiniAcid::reset: voices reset");
  
  // Make the second voice have different params (intentional base offset)
  voice3032.adjustParameter(TB303ParamId::Cutoff, -3);
  voice3032.adjustParameter(TB303ParamId::Resonance, -3);
  voice3032.adjustParameter(TB303ParamId::EnvAmount, -1);
  
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
  currentStepIndex = -1;
  samplesIntoStep = 0;
  currentTimingOffset_ = 0;
  currentStepDurationSamples_ = 1;
  updateSamplesPerStep();
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
  
  // NOW reset bias tracking (after all base params are set)
  genreManager_.resetTextureBiasTracking();
  // Apply texture to bring engine into consistent state with current genre
  genreManager_.applyTexture(*this);
  
  // Reset Retrig States
  retrigA_ = {};
  retrigB_ = {};
  for(int i=0; i<NUM_DRUM_VOICES; ++i) retrigDrums_[i] = {};

  LOG_PRINTLN("    - MiniAcid::reset: Done");
}

void MiniAcid::start() {
  playing = true;
  currentStepIndex = -1;
  // Force immediate first step trigger.
  samplesIntoStep = 0;
  currentTimingOffset_ = 0;
  currentStepDurationSamples_ = 1;
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
  playing = false;
  currentStepIndex = -1;
  samplesIntoStep = 0;
  currentStepDurationSamples_ = 1;
  voice303.release();
  voice3032.release();
  drums->reset();
  if (songMode_) {
    sceneManager_.setSongPosition(clampSongPosition(songPlayheadPosition_));
  }
}

void MiniAcid::setBpm(float bpm) {
  bpmValue = bpm;
  if (bpmValue < 10.0f)
    bpmValue = 10.0f;
  if (bpmValue > 250.0f)
    bpmValue = 250.0f;
  updateSamplesPerStep();
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

int MiniAcid::currentStep() const { return currentStepIndex; }

int MiniAcid::cycleBarCount() const {
  int bars = sceneManager_.currentScene().feel.patternBars;
  if (bars != 1 && bars != 2 && bars != 4 && bars != 8) bars = 1;
  return bars;
}

int MiniAcid::cycleBarIndex() const {
  int bars = cycleBarCount();
  int cycleSteps = SEQ_STEPS * bars;
  if (cycleSteps <= 0) return 0;
  int step = songStepCounter_ - 1;
  if (step < 0) step = cycleSteps - 1;
  if (step < 0) step = 0;
  step %= cycleSteps;
  int bar = step / SEQ_STEPS;
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
  int idx = clamp303Voice(voiceIndex);
  return idx == 0 ? voice303.parameter(id) : voice3032.parameter(id);
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
  if (!liveMixMode_) {
    songPlaybackSlot_ = sceneManager_.activeSongSlot();
  }
  if (songMode_ && songPlaybackSlot_ == sceneManager_.activeSongSlot()) {
    applySongPositionSelection();
  }
}
int MiniAcid::songPlaybackSlot() const { return songPlaybackSlot_; }
void MiniAcid::setSongPlaybackSlot(int slot) {
  if (slot < 0) slot = 0;
  if (slot > 1) slot = 1;
  if (songPlaybackSlot_ == slot) return;
  songPlaybackSlot_ = slot;
  if (songMode_) applySongPositionSelection();
}
bool MiniAcid::liveMixModeEnabled() const { return liveMixMode_; }
void MiniAcid::setLiveMixMode(bool enabled) {
  if (liveMixMode_ == enabled) return;
  liveMixMode_ = enabled;
  if (!liveMixMode_) {
    songPlaybackSlot_ = sceneManager_.activeSongSlot();
    if (songMode_) applySongPositionSelection();
  }
}
void MiniAcid::toggleLiveMixMode() { setLiveMixMode(!liveMixMode_); }
void MiniAcid::mergeSongs() { sceneManager_.mergeSongs(); }
void MiniAcid::alternateSongs() { sceneManager_.alternateSongs(); }
void MiniAcid::setSongReverse(bool reverse) { sceneManager_.setSongReverse(reverse); }
bool MiniAcid::isSongReverse() const { return sceneManager_.isSongReverse(); }
void MiniAcid::queueSongReverseToggle() {
  if (playing && songMode_) {
    songReverseTogglePending_ = true;
    return;
  }
  sceneManager_.setSongReverse(!sceneManager_.isSongReverse());
}
bool MiniAcid::hasPendingSongReverseToggle() const { return songReverseTogglePending_; }

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

std::vector<std::string> MiniAcid::getAvailableDrumEngines() const {
  return {"808", "909", "606"};
}


void MiniAcid::setDrumEngine(const std::string& engineName) {
  std::string name = toLowerCopy(engineName);
  LOG_DEBUG("    - MiniAcid::setDrumEngine: setting to %s\n", name.c_str());
  if (name.find("909") != std::string::npos) {
    drums = std::make_unique<TR909DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "909";
  } else if (name.find("606") != std::string::npos) {
    drums = std::make_unique<TR606DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "606";
  } else if (name.find("808") != std::string::npos) {
    drums = std::make_unique<TR808DrumSynthVoice>(sampleRateValue);
    drumEngineName_ = "808";
  } else {
    LOG_PRINTLN("    - MiniAcid::setDrumEngine: Unknown engine!");
    return;
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
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0)
    voice303.adjustParameter(id, steps);
  else
    voice3032.adjustParameter(id, steps);
}
void MiniAcid::set303Parameter(TB303ParamId id, float value, int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0) voice303.setParameter(id, value);
  else voice3032.setParameter(id, value);
}
void MiniAcid::set303ParameterNormalized(TB303ParamId id, float norm, int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0) voice303.setParameterNormalized(id, norm);
  else voice3032.setParameterNormalized(id, norm);
}
void MiniAcid::set303PatternIndex(int voiceIndex, int16_t patternIndex) {
  int idx = clamp303Voice(voiceIndex);
  sceneManager_.setCurrentSynthPatternIndex(idx, patternIndex);
}
void MiniAcid::shift303PatternIndex(int voiceIndex, int delta) {
  int idx = clamp303Voice(voiceIndex);
  int current = sceneManager_.getCurrentSynthPatternIndex(idx);
  int next = current + delta;
  if (next < 0) next = Bank<SynthPattern>::kPatterns - 1;
  if (next >= Bank<SynthPattern>::kPatterns) next = 0;
  sceneManager_.setCurrentSynthPatternIndex(idx, next);
}

void MiniAcid::set303BankIndex(int voiceIndex, int bankIndex) {
  int idx = clamp303Voice(voiceIndex);
  sceneManager_.setCurrentBankIndex(idx + 1, bankIndex);
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
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  uint8_t current = pattern.steps[step].fx;
  // Cycle: None -> Retrig -> Reverse -> None
  if (current == (uint8_t)StepFx::None) current = (uint8_t)StepFx::Retrig;
  else if (current == (uint8_t)StepFx::Retrig) current = (uint8_t)StepFx::Reverse;
  else current = (uint8_t)StepFx::None;
  pattern.steps[step].fx = current;
}

void MiniAcid::adjust303StepFxParam(int voiceIndex, int stepIndex, int delta) {
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  int val = pattern.steps[step].fxParam;
  val += delta;
  if (val < 0) val = 0;
  if (val > 255) val = 255;
  pattern.steps[step].fxParam = (uint8_t)val;
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
  SongTrack track = idx == 0 ? SongTrack::SynthA : SongTrack::SynthB;
  int pat = songPatternIndexForTrack(track);
  if (pat < 0) return kEmptySynthPattern;
  return sceneManager_.getSynthPattern(idx, pat);
}

const DrumPattern& MiniAcid::activeDrumPattern(int drumVoiceIndex) const {
  int idx = clampDrumVoice(drumVoiceIndex);
  int pat = songPatternIndexForTrack(SongTrack::Drums);
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

void MiniAcid::updateSamplesPerStep() {
  int steps = sceneManager_.currentScene().feel.gridSteps;
  if (steps != 8 && steps != 16 && steps != 32) steps = 16;
  int tb = sceneManager_.currentScene().feel.timebase;
  if (tb < 0) tb = 0;
  if (tb > 2) tb = 2;
  float timebaseMul = 1.0f;
  if (tb == 0) timebaseMul = 0.5f;
  else if (tb == 2) timebaseMul = 2.0f;
  float effectiveSteps = steps * timebaseMul;
  if (effectiveSteps < 1.0f) effectiveSteps = 1.0f;
  samplesPerStep = sampleRateValue * 240.0f / (bpmValue * effectiveSteps);
}

float MiniAcid::noteToFreq(int note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

unsigned long MiniAcid::computeStepDurationSamples_() const {
  // Compute once per step:
  // target = nominalStep + nextOffset - currentOffset
  int nextStepIndex = (currentStepIndex + 1) % SEQ_STEPS;
  int8_t nextTiming = 0;
  const SynthPattern& groovePattern = sceneManager_.getCurrentSynthPattern(0);
  nextTiming = groovePattern.steps[nextStepIndex].timing;

  long samplesPerTick = static_cast<long>(samplesPerStep / 24.0f);
  if (samplesPerTick < 1) samplesPerTick = 1;
  long nextOffset = static_cast<long>(nextTiming) * samplesPerTick;

  long targetDuration = static_cast<long>(samplesPerStep) + nextOffset - currentTimingOffset_;
  if (targetDuration < 1) targetDuration = 1;
  return static_cast<unsigned long>(targetDuration);
}

void MiniAcid::advanceStep() {
  int patternBars = sceneManager_.currentScene().feel.patternBars;
  if (patternBars != 1 && patternBars != 2 && patternBars != 4 && patternBars != 8) patternBars = 1;
  int cycleSteps = SEQ_STEPS * patternBars;
  if (cycleSteps <= 0) cycleSteps = SEQ_STEPS;
  if (songStepCounter_ >= cycleSteps) songStepCounter_ = 0;

  int prevStep = currentStepIndex;
  if (songMode_ && prevStep < 0) {
    songPlayheadPosition_ = clampSongPosition(sceneManager_.getSongPosition());
    sceneManager_.setSongPosition(songPlayheadPosition_);
    applySongPositionSelection();
    songStepCounter_ = 0;
  }

  // Song reverse affects only SONG position order, not step order inside patterns.
  currentStepIndex = songStepCounter_ % SEQ_STEPS;

  LedManager::instance().onBeat(currentStepIndex, sceneManager_.currentScene().led);

  if (songMode_) {
    // song position advance happens after step increment (see below)
  }

  // DEBUG: toggle drum kit every measure for testing
  /*
  if (prevStep >= 0 && currentStepIndex == 0) {
    drumCycleIndex_ = (drumCycleIndex_ + 1) % 3;
    if (drumCycleIndex_ == 0) {
      drums = std::make_unique<TR808DrumSynthVoice>(sampleRateValue);
      // printf("Switched to TR-808 drum kit\n");
    } else if (drumCycleIndex_ == 1) {
      drums = std::make_unique<TR909DrumSynthVoice>(sampleRateValue);
      // printf("Switched to TR-909 drum kit\n");
    } else {
      drums = std::make_unique<TR606DrumSynthVoice>(sampleRateValue);
      // printf("Switched to TR-606 drum kit\n");
    }
  }
  */
  int songPatternA = songPatternIndexForTrack(SongTrack::SynthA);
  int songPatternB = songPatternIndexForTrack(SongTrack::SynthB);
  int songPatternDrums = songPatternIndexForTrack(SongTrack::Drums);

  // 303 voices
  const SynthPattern& synthA = activeSynthPattern(0);
  const SynthPattern& synthB = activeSynthPattern(1);
  const SynthStep& stepA = synthA.steps[currentStepIndex];
  const SynthStep& stepB = synthB.steps[currentStepIndex];
  
  // Get gate length multiplier from genre params
  float gateMult = genreManager_.getGenerativeParams().gateLengthMultiplier;
  if (gateMult < 0.1f) gateMult = 0.5f; // Default fallback
  if (gateMult > 1.0f) gateMult = 1.0f;
  
  // Voice A (Bass): slightly shorter gate for tighter feel
  float gateMultA = gateMult * 0.85f;
  if (gateMultA < 0.15f) gateMultA = 0.15f;
  
  // Voice B (Lead): slightly longer gate for more legato
  float gateMultB = gateMult * 1.05f;
  if (gateMultB > 0.98f) gateMultB = 0.98f;
  
  // Note: ghost notes handled by velocity < 50 usually
  retrigA_.active = false;
  if (!mute303 && songPatternA >= 0) {
      if (stepA.note == -2) { // TIE (Note Continue)
          // Extend gate if active
          if (gateCountdownA_ > 0) gateCountdownA_ += (long)(samplesPerStep * gateMultA);
          else if (stepA.slide) {
               // If sliding into a TIE from silence, maybe start? 
               // For now, TIE only extends.
          }
      } else if (stepA.note >= 0 && (!stepA.ghost || (rand() % 100 < 80))) {
        // Probability check
        if (stepA.probability >= 100 || (rand() % 100 < stepA.probability)) {
            voice303.startNote(noteToFreq(stepA.note), stepA.accent, stepA.slide, stepA.velocity);
            // Set gate countdown: gate_samples = step_duration * multiplier
            gateCountdownA_ = (long)(samplesPerStep * gateMultA);
            LedManager::instance().onVoiceTriggered(VoiceId::SynthA, sceneManager_.currentScene().led);
            
            // Setup Retrig
            if (stepA.fx == (uint8_t)StepFx::Retrig && stepA.fxParam > 0) {
                retrigA_.countRemaining = stepA.fxParam;
                // Retrigger interval should be relative to current step duration
                retrigA_.interval = (int)(samplesPerStep / (stepA.fxParam + 1));
                retrigA_.counter = retrigA_.interval;
                retrigA_.active = true;
            }
        }
      }
  }
  // Note: release is now handled by gate countdown, not here

  retrigB_.active = false;
  if (!mute303_2 && songPatternB >= 0) {
      if (stepB.note == -2) { // TIE
          if (gateCountdownB_ > 0) gateCountdownB_ += (long)(samplesPerStep * gateMultB);
      } else if (stepB.note >= 0 && (!stepB.ghost || (rand() % 100 < 80))) {
        if (stepB.probability >= 100 || (rand() % 100 < stepB.probability)) {
            voice3032.startNote(noteToFreq(stepB.note), stepB.accent, stepB.slide, stepB.velocity);
            gateCountdownB_ = (long)(samplesPerStep * gateMultB);
            LedManager::instance().onVoiceTriggered(VoiceId::SynthB, sceneManager_.currentScene().led);
            
            if (stepB.fx == (uint8_t)StepFx::Retrig && stepB.fxParam > 0) {
                retrigB_.countRemaining = stepB.fxParam;
                retrigB_.interval = (int)(samplesPerStep / (stepB.fxParam + 1));
                retrigB_.counter = retrigB_.interval;
                retrigB_.active = true;
            }
        }
      }
  }

  // Drums
  const DrumPattern& kick = activeDrumPattern(kDrumKickVoice);
  const DrumPattern& snare = activeDrumPattern(kDrumSnareVoice);
  const DrumPattern& hat = activeDrumPattern(kDrumHatVoice);
  const DrumPattern& openHat = activeDrumPattern(kDrumOpenHatVoice);
  const DrumPattern& midTom = activeDrumPattern(kDrumMidTomVoice);
  const DrumPattern& highTom = activeDrumPattern(kDrumHighTomVoice);
  const DrumPattern& rim = activeDrumPattern(kDrumRimVoice);
  const DrumPattern& clap = activeDrumPattern(kDrumClapVoice);

  bool drumsActive = songPatternDrums >= 0;

  auto shouldPlay = [&](const DrumStep& step, bool muted) -> bool {
    if (!drumsActive || muted || !step.hit) return false;
    if (step.probability >= 100) return true;
    return (rand() % 100) < step.probability;
  };

  const bool kickPlay = shouldPlay(kick.steps[currentStepIndex], muteKick);
  const bool snarePlay = shouldPlay(snare.steps[currentStepIndex], muteSnare);
  const bool hatPlay = shouldPlay(hat.steps[currentStepIndex], muteHat);
  const bool openHatPlay = shouldPlay(openHat.steps[currentStepIndex], muteOpenHat);
  const bool midTomPlay = shouldPlay(midTom.steps[currentStepIndex], muteMidTom);
  const bool highTomPlay = shouldPlay(highTom.steps[currentStepIndex], muteHighTom);
  const bool rimPlay = shouldPlay(rim.steps[currentStepIndex], muteRim);
  const bool clapPlay = shouldPlay(clap.steps[currentStepIndex], muteClap);

  const bool stepAccent =
    (kickPlay && kick.steps[currentStepIndex].accent) ||
    (snarePlay && snare.steps[currentStepIndex].accent) ||
    (hatPlay && hat.steps[currentStepIndex].accent) ||
    (openHatPlay && openHat.steps[currentStepIndex].accent) ||
    (midTomPlay && midTom.steps[currentStepIndex].accent) ||
    (highTomPlay && highTom.steps[currentStepIndex].accent) ||
    (rimPlay && rim.steps[currentStepIndex].accent) ||
    (clapPlay && clap.steps[currentStepIndex].accent);

  if (kickPlay) {
    bool rev = (kick.steps[currentStepIndex].fx == (uint8_t)StepFx::Reverse);
    drums->triggerKick(stepAccent, kick.steps[currentStepIndex].velocity);
    if (sampleStore) samplerTrack->triggerPad(0, stepAccent ? 1.0f : 0.6f, *sampleStore, rev);
    LedManager::instance().onVoiceTriggered(VoiceId::DrumKick, sceneManager_.currentScene().led);
    if (kick.steps[currentStepIndex].fx == (uint8_t)StepFx::Retrig && kick.steps[currentStepIndex].fxParam > 0) {
      retrigDrums_[kDrumKickVoice].countRemaining = kick.steps[currentStepIndex].fxParam;
      retrigDrums_[kDrumKickVoice].interval = (int)(samplesPerStep / (kick.steps[currentStepIndex].fxParam + 1));
      retrigDrums_[kDrumKickVoice].counter = retrigDrums_[kDrumKickVoice].interval;
      retrigDrums_[kDrumKickVoice].active = true;
    } else {
      retrigDrums_[kDrumKickVoice].active = false;
    }
  } else {
    retrigDrums_[kDrumKickVoice].active = false;
  }

  if (snarePlay) {
    bool rev = (snare.steps[currentStepIndex].fx == (uint8_t)StepFx::Reverse);
    drums->triggerSnare(stepAccent, snare.steps[currentStepIndex].velocity);
    if (sampleStore) samplerTrack->triggerPad(1, stepAccent ? 1.0f : 0.6f, *sampleStore, rev);
    LedManager::instance().onVoiceTriggered(VoiceId::DrumSnare, sceneManager_.currentScene().led);
    if (snare.steps[currentStepIndex].fx == (uint8_t)StepFx::Retrig && snare.steps[currentStepIndex].fxParam > 0) {
      retrigDrums_[kDrumSnareVoice].countRemaining = snare.steps[currentStepIndex].fxParam;
      retrigDrums_[kDrumSnareVoice].interval = (int)(samplesPerStep / (snare.steps[currentStepIndex].fxParam + 1));
      retrigDrums_[kDrumSnareVoice].counter = retrigDrums_[kDrumSnareVoice].interval;
      retrigDrums_[kDrumSnareVoice].active = true;
    } else {
      retrigDrums_[kDrumSnareVoice].active = false;
    }
  } else {
    retrigDrums_[kDrumSnareVoice].active = false;
  }

  if (hatPlay) {
    bool rev = (hat.steps[currentStepIndex].fx == (uint8_t)StepFx::Reverse);
    drums->triggerHat(stepAccent, hat.steps[currentStepIndex].velocity);
    if (sampleStore) samplerTrack->triggerPad(2, stepAccent ? 1.0f : 0.6f, *sampleStore, rev);
    LedManager::instance().onVoiceTriggered(VoiceId::DrumHatC, sceneManager_.currentScene().led);
    if (hat.steps[currentStepIndex].fx == (uint8_t)StepFx::Retrig && hat.steps[currentStepIndex].fxParam > 0) {
      retrigDrums_[kDrumHatVoice].countRemaining = hat.steps[currentStepIndex].fxParam;
      retrigDrums_[kDrumHatVoice].interval = (int)(samplesPerStep / (hat.steps[currentStepIndex].fxParam + 1));
      retrigDrums_[kDrumHatVoice].counter = retrigDrums_[kDrumHatVoice].interval;
      retrigDrums_[kDrumHatVoice].active = true;
    } else {
      retrigDrums_[kDrumHatVoice].active = false;
    }
  } else {
    retrigDrums_[kDrumHatVoice].active = false;
  }

  if (openHatPlay) {
    bool rev = (openHat.steps[currentStepIndex].fx == (uint8_t)StepFx::Reverse);
    drums->triggerOpenHat(stepAccent, openHat.steps[currentStepIndex].velocity);
    if (sampleStore) samplerTrack->triggerPad(3, stepAccent ? 1.0f : 0.6f, *sampleStore, rev);
    LedManager::instance().onVoiceTriggered(VoiceId::DrumHatO, sceneManager_.currentScene().led);
    if (openHat.steps[currentStepIndex].fx == (uint8_t)StepFx::Retrig && openHat.steps[currentStepIndex].fxParam > 0) {
      retrigDrums_[kDrumOpenHatVoice].countRemaining = openHat.steps[currentStepIndex].fxParam;
      retrigDrums_[kDrumOpenHatVoice].interval = (int)(samplesPerStep / (openHat.steps[currentStepIndex].fxParam + 1));
      retrigDrums_[kDrumOpenHatVoice].counter = retrigDrums_[kDrumOpenHatVoice].interval;
      retrigDrums_[kDrumOpenHatVoice].active = true;
    } else {
      retrigDrums_[kDrumOpenHatVoice].active = false;
    }
  } else {
    retrigDrums_[kDrumOpenHatVoice].active = false;
  }

  if (midTomPlay) {
    bool rev = (midTom.steps[currentStepIndex].fx == (uint8_t)StepFx::Reverse);
    drums->triggerMidTom(stepAccent, midTom.steps[currentStepIndex].velocity);
    if (sampleStore) samplerTrack->triggerPad(4, stepAccent ? 1.0f : 0.6f, *sampleStore, rev);
    LedManager::instance().onVoiceTriggered(VoiceId::DrumTomM, sceneManager_.currentScene().led);
    if (midTom.steps[currentStepIndex].fx == (uint8_t)StepFx::Retrig && midTom.steps[currentStepIndex].fxParam > 0) {
      retrigDrums_[kDrumMidTomVoice].countRemaining = midTom.steps[currentStepIndex].fxParam;
      retrigDrums_[kDrumMidTomVoice].interval = (int)(samplesPerStep / (midTom.steps[currentStepIndex].fxParam + 1));
      retrigDrums_[kDrumMidTomVoice].counter = retrigDrums_[kDrumMidTomVoice].interval;
      retrigDrums_[kDrumMidTomVoice].active = true;
    } else {
      retrigDrums_[kDrumMidTomVoice].active = false;
    }
  } else {
    retrigDrums_[kDrumMidTomVoice].active = false;
  }

  if (highTomPlay) {
    bool rev = (highTom.steps[currentStepIndex].fx == (uint8_t)StepFx::Reverse);
    drums->triggerHighTom(stepAccent, highTom.steps[currentStepIndex].velocity);
    if (sampleStore) samplerTrack->triggerPad(5, stepAccent ? 1.0f : 0.6f, *sampleStore, rev);
    LedManager::instance().onVoiceTriggered(VoiceId::DrumTomH, sceneManager_.currentScene().led);
    if (highTom.steps[currentStepIndex].fx == (uint8_t)StepFx::Retrig && highTom.steps[currentStepIndex].fxParam > 0) {
      retrigDrums_[kDrumHighTomVoice].countRemaining = highTom.steps[currentStepIndex].fxParam;
      retrigDrums_[kDrumHighTomVoice].interval = (int)(samplesPerStep / (highTom.steps[currentStepIndex].fxParam + 1));
      retrigDrums_[kDrumHighTomVoice].counter = retrigDrums_[kDrumHighTomVoice].interval;
      retrigDrums_[kDrumHighTomVoice].active = true;
    } else {
      retrigDrums_[kDrumHighTomVoice].active = false;
    }
  } else {
    retrigDrums_[kDrumHighTomVoice].active = false;
  }

  if (rimPlay) {
    bool rev = (rim.steps[currentStepIndex].fx == (uint8_t)StepFx::Reverse);
    drums->triggerRim(stepAccent, rim.steps[currentStepIndex].velocity);
    if (sampleStore) samplerTrack->triggerPad(6, stepAccent ? 1.0f : 0.6f, *sampleStore, rev);
    LedManager::instance().onVoiceTriggered(VoiceId::DrumRim, sceneManager_.currentScene().led);
    if (rim.steps[currentStepIndex].fx == (uint8_t)StepFx::Retrig && rim.steps[currentStepIndex].fxParam > 0) {
      retrigDrums_[kDrumRimVoice].countRemaining = rim.steps[currentStepIndex].fxParam;
      retrigDrums_[kDrumRimVoice].interval = (int)(samplesPerStep / (rim.steps[currentStepIndex].fxParam + 1));
      retrigDrums_[kDrumRimVoice].counter = retrigDrums_[kDrumRimVoice].interval;
      retrigDrums_[kDrumRimVoice].active = true;
    } else {
      retrigDrums_[kDrumRimVoice].active = false;
    }
  } else {
    retrigDrums_[kDrumRimVoice].active = false;
  }

  if (clapPlay) {
    bool rev = (clap.steps[currentStepIndex].fx == (uint8_t)StepFx::Reverse);
    drums->triggerClap(stepAccent, clap.steps[currentStepIndex].velocity);
    if (sampleStore) samplerTrack->triggerPad(7, stepAccent ? 1.0f : 0.6f, *sampleStore, rev);
    LedManager::instance().onVoiceTriggered(VoiceId::DrumClap, sceneManager_.currentScene().led);
    if (clap.steps[currentStepIndex].fx == (uint8_t)StepFx::Retrig && clap.steps[currentStepIndex].fxParam > 0) {
      retrigDrums_[kDrumClapVoice].countRemaining = clap.steps[currentStepIndex].fxParam;
      retrigDrums_[kDrumClapVoice].interval = (int)(samplesPerStep / (clap.steps[currentStepIndex].fxParam + 1));
      retrigDrums_[kDrumClapVoice].counter = retrigDrums_[kDrumClapVoice].interval;
      retrigDrums_[kDrumClapVoice].active = true;
    } else {
      retrigDrums_[kDrumClapVoice].active = false;
    }
  } else {
    retrigDrums_[kDrumClapVoice].active = false;
  }

  bool wrapped = false;
  songStepCounter_++;
  if (songStepCounter_ >= cycleSteps) {
    songStepCounter_ = 0;
    wrapped = true;
  }

  if (wrapped) {
    cyclePulseCounter_++;
  }

  if (songMode_ && prevStep >= 0 && wrapped) {
    if (songReverseTogglePending_) {
      sceneManager_.setSongReverse(!sceneManager_.isSongReverse());
      songReverseTogglePending_ = false;
    }
    advanceSongPlayhead();
  }

  // Cache duration for the just-started step.
  // Update timing offset only after target is computed.
  currentStepDurationSamples_ = computeStepDurationSamples_();
  int nextStepIndex = (currentStepIndex + 1) % SEQ_STEPS;
  const SynthPattern& groovePattern = sceneManager_.getCurrentSynthPattern(0);
  long samplesPerTick = static_cast<long>(samplesPerStep / 24.0f);
  if (samplesPerTick < 1) samplesPerTick = 1;
  currentTimingOffset_ = static_cast<long>(groovePattern.steps[nextStepIndex].timing) * samplesPerTick;
}

void MiniAcid::generateAudioBuffer(int16_t *buffer, size_t numSamples) {
  if (!buffer || numSamples == 0) return;

  // Test Tone Mode (Hardware diagnostic)
  if (testToneEnabled_) {
    for (size_t i = 0; i < numSamples; ++i) {
      testTonePhase_ += 440.0f / sampleRateValue;
      if (testTonePhase_ >= 1.0f) testTonePhase_ -= 1.0f;
      float val = sinf(2.0f * 3.14159265f * testTonePhase_) * 0.707f; 
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

  updateSamplesPerStep();
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

  // Optimization: render sampler track in a block once per buffer
  uint32_t tSamplerStart = micros();
  bool hasSampleStore = (sampleStore != nullptr);
  if (hasSampleStore) {
    std::fill(samplerOutBuffer.get(), samplerOutBuffer.get() + numSamples, 0.0f);
    samplerTrack->process(samplerOutBuffer.get(), numSamples, *sampleStore);
  }
  uint32_t tSamplerTime = micros() - tSamplerStart;

  // Cache immutable-per-buffer flags
  const float* trackVolumes = sceneManager_.currentScene().trackVolumes;
  const bool looperActive = (tapeState.mode != TapeMode::Stop);
  const bool tapeFxEnabled = tapeState.fxEnabled;
  AudioDiagnostics& diag = AudioDiagnostics::instance();
  const bool diagEnabled = diag.isEnabled();
  const bool detailedProfile = diagEnabled && ((perfDetailCounter_++ & 0x1Fu) == 0);

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
  uint32_t tFxTotal = 0;
  uint32_t tVocalTotal = 0;
  uint32_t tLoopStart = micros();

  for (size_t i = 0; i < numSamples; ++i) {
    if (playing) {
      if (samplesIntoStep >= currentStepDurationSamples_) {
        samplesIntoStep = 0;
        advanceStep();
      }
      samplesIntoStep++;
      if (gateCountdownA_ > 0 && --gateCountdownA_ <= 0) voice303.release();
      if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0) voice3032.release();
    }

    float sample = 0.0f;
    float sample303 = 0.0f;
    float drumsMix = 0.0f;
    float samplerSample = 0.0f;

    // Retrig Logic (omitted for brevity in this view? No, I must keep it!)
    // [Keeping retrig logic as it was in the file]
    if (retrigA_.active) {
        if (--retrigA_.counter <= 0 && retrigA_.countRemaining > 0) {
            const SynthStep& step = activeSynthPattern(0).steps[currentStepIndex];
            voice303.startNote(noteToFreq(step.note), step.accent, step.slide, step.velocity);
            LedManager::instance().onVoiceTriggered(VoiceId::SynthA, sceneManager_.currentScene().led);
            retrigA_.counter = retrigA_.interval;
            retrigA_.countRemaining--;
            if (retrigA_.countRemaining <= 0) retrigA_.active = false;
        }
    }
    if (retrigB_.active) {
        if (--retrigB_.counter <= 0 && retrigB_.countRemaining > 0) {
            const SynthStep& step = activeSynthPattern(1).steps[currentStepIndex];
            voice3032.startNote(noteToFreq(step.note), step.accent, step.slide, step.velocity);
            LedManager::instance().onVoiceTriggered(VoiceId::SynthB, sceneManager_.currentScene().led);
            retrigB_.counter = retrigB_.interval;
            retrigB_.countRemaining--;
            if (retrigB_.countRemaining <= 0) retrigB_.active = false;
        }
    }
    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
        if (retrigDrums_[v].active) {
             if (--retrigDrums_[v].counter <= 0 && retrigDrums_[v].countRemaining > 0) {
                 const DrumPattern& pattern = activeDrumPattern(v);
                 const DrumStep& step = pattern.steps[currentStepIndex];
                 bool accent = step.accent;
                 switch(v) {
                     case kDrumKickVoice: if (!muteKick) { drums->triggerKick(accent, step.velocity); if(sampleStore) samplerTrack->triggerPad(0, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumSnareVoice: if (!muteSnare) { drums->triggerSnare(accent, step.velocity); if(sampleStore) samplerTrack->triggerPad(1, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumHatVoice: if (!muteHat) { drums->triggerHat(accent, step.velocity); if(sampleStore) samplerTrack->triggerPad(2, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumOpenHatVoice: if (!muteOpenHat) { drums->triggerOpenHat(accent, step.velocity); if(sampleStore) samplerTrack->triggerPad(3, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumMidTomVoice: if (!muteMidTom) { drums->triggerMidTom(accent, step.velocity); if(sampleStore) samplerTrack->triggerPad(4, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumHighTomVoice: if (!muteHighTom) { drums->triggerHighTom(accent, step.velocity); if(sampleStore) samplerTrack->triggerPad(5, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumRimVoice: if (!muteRim) { drums->triggerRim(accent, step.velocity); if(sampleStore) samplerTrack->triggerPad(6, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                     case kDrumClapVoice: if (!muteClap) { drums->triggerClap(accent, step.velocity); if(sampleStore) samplerTrack->triggerPad(7, accent?1.0f:0.6f, *sampleStore, step.fx == (uint8_t)StepFx::Reverse); } break;
                 }
                 retrigDrums_[v].counter = retrigDrums_[v].interval;
                 retrigDrums_[v].countRemaining--;
                 if (retrigDrums_[v].countRemaining <= 0) retrigDrums_[v].active = false;
             }
        }
    }

    uint32_t tV0 = 0;
    if (detailedProfile) tV0 = micros();
    if (playing) {
      if (!mute303) {
        float v = voice303.process() * 0.5f;
        v = distortion303.process(v);
        v *= trackVolumes[(int)VoiceId::SynthA];
        sample303 += delay303.process(v);
      } else delay303.process(0.0f);
      if (!mute303_2) {
        float v = voice3032.process() * 0.5f;
        v = distortion3032.process(v);
        v *= trackVolumes[(int)VoiceId::SynthB];
        sample303 += delay3032.process(v);
      } else delay3032.process(0.0f);
    }
    if (detailedProfile) tVoicesTotal += (micros() - tV0);

    uint32_t tD0 = 0;
    if (detailedProfile) tD0 = micros();
    if (playing) {
      if (!muteKick)    drumsMix += drums->processKick() * trackVolumes[(int)VoiceId::DrumKick];
      if (!muteSnare)   drumsMix += drums->processSnare() * trackVolumes[(int)VoiceId::DrumSnare];
      if (!muteHat)     drumsMix += drums->processHat() * trackVolumes[(int)VoiceId::DrumHatC];
      if (!muteOpenHat) drumsMix += drums->processOpenHat() * trackVolumes[(int)VoiceId::DrumHatO];
      if (!muteMidTom)  drumsMix += drums->processMidTom() * trackVolumes[(int)VoiceId::DrumTomM];
      if (!muteHighTom) drumsMix += drums->processHighTom() * trackVolumes[(int)VoiceId::DrumTomH];
      if (!muteRim)     drumsMix += drums->processRim() * trackVolumes[(int)VoiceId::DrumRim];
      if (!muteClap)    drumsMix += drums->processClap() * trackVolumes[(int)VoiceId::DrumClap];
      drumsMix *= 0.60f;
      
      // Drum Bus Processing
      drumsMix = drumTransientShaper.process(drumsMix);
      drumsMix = drumCompressor.process(drumsMix);
      drumsMix = drumReverb.process(drumsMix);
      
      drumsMix = softLimit(drumsMix);
      sample += sample303 + drumsMix;
    }
    if (detailedProfile) tDrumsTotal += (micros() - tD0);

    uint32_t tS0 = 0;
    if (detailedProfile) tS0 = micros();
    if (hasSampleStore) {
      samplerSample = samplerOutBuffer[i];
      sample += samplerSample;
    }
    float vocalSample = 0.0f;
    if (!voiceTrackMuted_ && vocalSynth_.isActive()) {
      vocalSample = voiceCompressor_.process(vocalSynth_.process());
    }
    sample += vocalSample;
    if (detailedProfile) tVocalTotal += (micros() - tS0);

    uint32_t tF0 = 0;
    if (detailedProfile) tF0 = micros();
    if (detailedProfile) {
      diag.trackSource(sample303, drumsMix, samplerSample, 0.0f, vocalSample, 0.0f, 0.0f);
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
    if (detailedProfile) diag.accumulate(preLimiter, limited);
    buffer[i] = (int16_t)(finalSample * 32767.0f);
    if (detailedProfile) tFxTotal += (micros() - tF0);
  }

  perfStats.dspTimeUs = (micros() - tLoopStart) + tSamplerTime;
  if (detailedProfile) {
    perfStats.dspVoicesUs = tVoicesTotal;
    perfStats.dspDrumsUs = tDrumsTotal;
    perfStats.dspSamplerUs = tSamplerTime + tVocalTotal;
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
  if (detailedProfile) diag.flushIfReady(millis());
}

void MiniAcid::randomize303Pattern(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  // Use genre-aware generator with voice role (0=bass, 1=lead)
  const auto& params = genreManager_.getGenerativeParams();
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
  modeManager_.generatePattern(editSynthPattern(idx), bpmValue, params, behavior, idx);
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
  // Use genre-aware drum generator
  const auto& params = genreManager_.getGenerativeParams();
  const auto behavior = genreManager_.getBehavior();
  modeManager_.generateDrumPattern(sceneManager_.editCurrentDrumPattern(), params, behavior);
}

void MiniAcid::randomizeDrumVoice(int voiceIndex) {
  int idx = clampDrumVoice(voiceIndex);
  const auto& params = genreManager_.getGenerativeParams();
  const auto behavior = genreManager_.getBehavior();
  modeManager_.generateDrumVoice(sceneManager_.editCurrentDrumPattern().voices[idx], idx, params, behavior);
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
  const auto& params = genreManager_.getGenerativeParams();
  
  // Scramble EVERYTHING
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
      
      modeManager_.generateDrumVoice(patternSet.voices[v], v, params, chaosBehavior);
  }
}

void MiniAcid::regeneratePatternsWithGenre() {
  // NOTE: applyTexture is NOT called here - it's applied separately by UI on texture change
  // This prevents double-application which would cause delta-bias drift
  
  const auto& params = genreManager_.getGenerativeParams();
  const auto behavior = genreManager_.getBehavior();

  // Regenerate 303 patterns using generative mode + structural behavior
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
  modeManager_.generatePattern(editSynthPattern(0), bpmValue, params, bassBehavior, 0); // Bass
  modeManager_.generatePattern(editSynthPattern(1), bpmValue, params, leadBehavior, 1); // Lead

  // Regenerate drum pattern
  modeManager_.generateDrumPattern(sceneManager_.editCurrentDrumPattern(), params, behavior);
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

void MiniAcid::syncModeToVoices() {
  GrooveboxMode mode = sceneManager_.getMode();
  const ModeConfig& cfg = modeManager_.config();
  
  voice303.setMode(mode);
  voice303.setSubOscillator(cfg.dsp.subOscillator);
  voice303.setNoiseAmount(cfg.dsp.noiseAmount);
  
  voice3032.setMode(mode);
  voice3032.setSubOscillator(cfg.dsp.subOscillator);
  voice3032.setNoiseAmount(cfg.dsp.noiseAmount);
  
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

  // ONLY apply sound macros if explicitly enabled
  if (sceneManager_.currentScene().genre.applySoundMacros) {
    modeManager_.apply303Preset(0, flv);
    modeManager_.apply303Preset(1, flv);
    int tapeCount = 0;
    const TapeModePreset* tapePresets = modeManager_.getTapePresets(tapeCount);
    if (tapePresets && tapeCount > 0) {
      int idx = flv;
      if (idx < 0) idx = 0;
      if (idx >= tapeCount) idx = tapeCount - 1;
      sceneManager_.currentScene().tape.macro = tapePresets[idx].macro;
    }
  }
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
  if (names.empty() && sceneStorage_) {
    std::string current = sceneStorage_->getCurrentSceneName();
    if (!current.empty()) names.push_back(current);
  }
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
  
  sceneStorage_->setCurrentSceneName(name);

  bool loaded = sceneStorage_->readScene(sceneManager_);
  Serial.printf("[LoadScene] readScene returned: %s\n", loaded ? "TRUE" : "FALSE");
  // String-based fallback REMOVED - causes OOM on DRAM-only devices
  
  if (!loaded) {
    Serial.printf("[LoadScene] FAILED - reverting to: %s\n", previousName.c_str());
    sceneStorage_->setCurrentSceneName(previousName);
    return false;
  }
  Serial.println("[LoadScene] Applying scene state...");
  applySceneStateFromManager();
  Serial.println("[LoadScene] SUCCESS");
  return true;
}

bool MiniAcid::saveSceneAs(const std::string& name) {
  if (!sceneStorage_) return false;
  sceneStorage_->setCurrentSceneName(name);
  saveSceneToStorage();
  return true;
}

bool MiniAcid::createNewSceneWithName(const std::string& name) {
  if (!sceneStorage_) return false;
  sceneStorage_->setCurrentSceneName(name);
  sceneManager_.loadDefaultScene();
  applySceneStateFromManager();
  saveSceneToStorage();
  return true;
}

void MiniAcid::loadSceneFromStorage() {
  if (sceneStorage_) {
    if (sceneStorage_->readScene(sceneManager_)) return;
    // String-based fallback REMOVED - it causes OOM on DRAM-only devices
    // If streaming parse fails, load default scene
    LOG_PRINTLN("  - loadSceneFromStorage: Streaming parse failed, loading default scene");
  }
  sceneManager_.loadDefaultScene();
}

void MiniAcid::saveSceneToStorage() {
  if (!sceneStorage_) return;
  syncSceneStateToManager();
  sceneStorage_->writeScene(sceneManager_);
}

void MiniAcid::applySceneStateFromManager() {
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: Start");
  
  // Reset bias tracking since scene overwrites all params
  genreManager_.resetTextureBiasTracking();
  
  modeManager_.setModeLocal(sceneManager_.getMode());
  modeManager_.setFlavorLocal(sceneManager_.getGrooveFlavor());
  syncModeToVoices();
  setBpm(sceneManager_.getBpm());
  
  // Load master volume from scene
  float sceneVolume = sceneManager_.currentScene().masterVolume;
  params[static_cast<int>(MiniAcidParamId::MainVolume)].setValue(sceneVolume);
  // Fixed master safety LPF: keep this independent from scene/UI state.
  setMasterOutputHighCutHz(kMasterHighCutHz);
  LOG_DEBUG("  - MiniAcid::applySceneStateFromManager: loaded volume=%.2f\n", sceneVolume);
  
  const std::string& drumEngineName = sceneManager_.getDrumEngineName();
  if (!drumEngineName.empty()) {
    LOG_DEBUG("  - MiniAcid::applySceneStateFromManager: setting drum engine to %s\n", drumEngineName.c_str());
    setDrumEngine(drumEngineName);
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
  const SynthParameters& paramsA = sceneManager_.getSynthParameters(0);
  const SynthParameters& paramsB = sceneManager_.getSynthParameters(1);

  voice303.setParameter(TB303ParamId::Cutoff, paramsA.cutoff);
  voice303.setParameter(TB303ParamId::Resonance, paramsA.resonance);
  voice303.setParameter(TB303ParamId::EnvAmount, paramsA.envAmount);
  voice303.setParameter(TB303ParamId::EnvDecay, paramsA.envDecay);
  voice303.setParameter(TB303ParamId::Oscillator, static_cast<float>(paramsA.oscType));

  voice3032.setParameter(TB303ParamId::Cutoff, paramsB.cutoff);
  voice3032.setParameter(TB303ParamId::Resonance, paramsB.resonance);
  voice3032.setParameter(TB303ParamId::EnvAmount, paramsB.envAmount);
  voice3032.setParameter(TB303ParamId::EnvDecay, paramsB.envDecay);
  voice3032.setParameter(TB303ParamId::Oscillator, static_cast<float>(paramsB.oscType));
  
  
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
  if (songMode_) {
    applySongPositionSelection();
  }

  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: syncing Sampler...");
  // Sync Sampler
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
  
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: applyGenreTimbre...");
  // Restore genre state from scene before applying timbre/texture
  const auto& gs = sceneManager_.currentScene().genre;
  genreManager_.setGenerativeMode(static_cast<GenerativeMode>(gs.generativeMode));
  genreManager_.setTextureMode(static_cast<TextureMode>(gs.textureMode));

  // 1. Enforce Genre Timbre BASE (overwrites scene params to ensure genre identity)
  genreManager_.applyGenreTimbre(*this);
  
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: resetTextureBiasTracking...");
  // 2. Reset bias tracking so subsequent texture application is fresh delta from new base
  genreManager_.resetTextureBiasTracking();
  
  LOG_PRINTLN("  - MiniAcid::applySceneStateFromManager: applyTexture...");
  // 3. Apply texture (delta bias + FX)
  genreManager_.applyTexture(*this);

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
  voice303.setLoFiAmount(lofiAmt);
  voice3032.setLoFiAmount(lofiAmt);
  if (drums) {
    drums->setLoFiMode(f.lofiEnabled);
    drums->setLoFiAmount(lofiAmt);
  }

  // --- Drive ---
  const float driveAmtNorm = f.driveEnabled ? (static_cast<float>(f.driveAmount) / 100.0f) : 0.0f;
  const bool driveOn = f.driveEnabled && driveAmtNorm > 0.001f;
  const float driveVal = 0.1f + driveAmtNorm * 9.9f; // TubeDistortion clamps 0.1..10
  distortion303.setDrive(driveVal);
  distortion3032.setDrive(driveVal);
  distortion303.setEnabled(driveOn);
  distortion3032.setEnabled(driveOn);

  // --- Tape ---
  // FEEL/TEXTURE controls FX enable. Keep looper mode intact while enabled
  // so Tape page REC/PLAY workflow is not interrupted.
  sc.tape.fxEnabled = f.tapeEnabled;
  if (!f.tapeEnabled) {
    sc.tape.mode = TapeMode::Stop;
  }
}

void MiniAcid::applyFeelTimingFromScene_() {
  updateSamplesPerStep();
  samplesIntoStep = 0;
  currentTimingOffset_ = 0;
  currentStepDurationSamples_ = 1;
}

void MiniAcid::syncSceneStateToManager() {
  sceneManager_.setBpm(bpmValue);
  sceneManager_.setDrumEngineName(drumEngineName_);
  
  // Save master volume to scene
  sceneManager_.currentScene().masterVolume = params[static_cast<int>(MiniAcidParamId::MainVolume)].value();
  sceneManager_.currentScene().genre.generativeMode = static_cast<uint8_t>(genreManager_.generativeMode());
  sceneManager_.currentScene().genre.textureMode = static_cast<uint8_t>(genreManager_.textureMode());
  
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

  SynthParameters paramsA;
  paramsA.cutoff = voice303.parameterValue(TB303ParamId::Cutoff);
  paramsA.resonance = voice303.parameterValue(TB303ParamId::Resonance);
  paramsA.envAmount = voice303.parameterValue(TB303ParamId::EnvAmount);
  paramsA.envDecay = voice303.parameterValue(TB303ParamId::EnvDecay);
  paramsA.oscType = voice303.oscillatorIndex();
  sceneManager_.setSynthParameters(0, paramsA);

  SynthParameters paramsB;
  paramsB.cutoff = voice3032.parameterValue(TB303ParamId::Cutoff);
  paramsB.resonance = voice3032.parameterValue(TB303ParamId::Resonance);
  paramsB.envAmount = voice3032.parameterValue(TB303ParamId::EnvAmount);
  paramsB.envDecay = voice3032.parameterValue(TB303ParamId::EnvDecay);
  paramsB.oscType = voice3032.oscillatorIndex();
  sceneManager_.setSynthParameters(1, paramsB);

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
  samplesIntoStep = 0;
  currentTimingOffset_ = 0;
  currentStepDurationSamples_ = 1;
  
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
  samplesIntoStep = 0;
  currentStepDurationSamples_ = 1;
  
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

void MiniAcid::updateDrumReverbDecay(float value) {
  drumReverb.setDecay(value);
}
