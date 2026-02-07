#pragma once

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <vector>
#include <string>
#include <functional>

#include "mode_manager.h"
#include "src/dsp/genre_manager.h"
#include "../../scene_storage.h"
#include "../../scenes.h"
#include "mini_tb303.h"
#include "mini_drumvoices.h"
#include "tube_distortion.h"
#include "perf_stats.h"
#include "tape_fx.h"
#include "tape_looper.h"
#include "../audio/audio_config.h"
#include "../sampler/sample_store.h"
#include "../sampler/sample_index.h"
#include "../sampler/drum_sampler_track.h"
#include "../sampler/sample_index.h"
#include "formant_synth.h"
#include "../audio/vocal_mixer.h"
#include "voice_compressor.h"
#include "../audio/voice_cache.h"

// ===================== Audio config =====================

static const int SAMPLE_RATE = kSampleRate;        // Hz
static const int AUDIO_BUFFER_SAMPLES = kBlockFrames; // per buffer, mono
static const int SEQ_STEPS = 16;             // 16-step sequencer
static const int NUM_303_VOICES = 2;
static const int NUM_DRUM_VOICES = DrumPatternSet::kVoices;

// ===================== Parameters =====================

class TempoDelay {
public:
  explicit TempoDelay(float sampleRate);
  
  void init(float maxSeconds); // Explicit init with size control
  void reset();
  void setSampleRate(float sr);
  void setBpm(float bpm);
  void setBeats(float beats);
  void setMix(float mix);
  void setFeedback(float fb);
  void setEnabled(bool on);
  bool isEnabled() const;

  float process(float input);

private:
  // for 2 voices at 22050 Hz, this is the max that the cardputer can handle.
#if defined(ARDUINO_M5STACK_CARDPUTER)
  static constexpr float kMaxDelaySeconds = 0.25f; // reduced for memory constrained device
#else
  static constexpr float kMaxDelaySeconds = 1.0f;
#endif

  std::vector<float> buffer;
  int writeIndex;
  int delaySamples;
  float sampleRate;
  int maxDelaySamples;
  float beats;    // delay length in beats
  float mix;      // wet mix 0..1
  float feedback; // feedback 0..1
  bool enabled;
};

enum class MiniAcidParamId : uint8_t {
  MainVolume = 0,
  VoicePitch,
  VoiceSpeed,
  VoiceRobotness,
  VoiceVolume,
  Count
};





class MiniAcid {
public:
  static constexpr int kMin303Note = 24; // C1
  static constexpr int kMax303Note = 71; // B4

  MiniAcid(float sampleRate, SceneStorage* sceneStorage);

  void init();
  void reset();
  void start();
  void stop();
  void setBpm(float bpm);
  float bpm() const;
  float sampleRate() const;
  bool isPlaying() const;
  int currentStep() const;
  int cycleBarIndex() const;
  int cycleBarCount() const;
  uint32_t cyclePulseCounter() const { return cyclePulseCounter_; }
  int currentDrumPatternIndex() const;
  int current303PatternIndex(int voiceIndex = 0) const;
  int currentDrumBankIndex() const;
  int current303BankIndex(int voiceIndex = 0) const;
  bool is303Muted(int voiceIndex = 0) const;
  bool isKickMuted() const;
  bool isSnareMuted() const;
  bool isHatMuted() const;
  bool isOpenHatMuted() const;
  bool isMidTomMuted() const;
  bool isHighTomMuted() const;
  bool isRimMuted() const;
  bool isClapMuted() const;
  bool is303DelayEnabled(int voiceIndex = 0) const;
  bool is303DistortionEnabled(int voiceIndex = 0) const;
  const Parameter& parameter303(TB303ParamId id, int voiceIndex = 0) const;
  
  // Thread-safe waveform buffer access for UI visualization
  struct WaveformBuffer {
    int16_t data[AUDIO_BUFFER_SAMPLES];
    size_t count;
  };
  const WaveformBuffer& getWaveformBuffer() const;
  
  const int8_t* pattern303Steps(int voiceIndex = 0) const;
  const bool* pattern303AccentSteps(int voiceIndex = 0) const;
  const bool* pattern303SlideSteps(int voiceIndex = 0) const;
  const bool* patternKickSteps() const;
  const bool* patternSnareSteps() const;
  const bool* patternHatSteps() const;
  const bool* patternOpenHatSteps() const;
  const bool* patternMidTomSteps() const;
  const bool* patternHighTomSteps() const;
  const bool* patternRimSteps() const;
  const bool* patternClapSteps() const;
  const bool* patternDrumAccentSteps() const;
  const bool* patternKickAccentSteps() const;
  const bool* patternSnareAccentSteps() const;
  const bool* patternHatAccentSteps() const;
  const bool* patternOpenHatAccentSteps() const;
  const bool* patternMidTomAccentSteps() const;
  const bool* patternHighTomAccentSteps() const;
  const bool* patternRimAccentSteps() const;
  const bool* patternClapAccentSteps() const;
  bool songModeEnabled() const;
  void setSongMode(bool enabled);
  void toggleSongMode();
  bool loopModeEnabled() const;
  void setLoopMode(bool enabled);
  void setLoopRange(int startRow, int endRow);
  int loopStartRow() const;
  int loopEndRow() const;
  int songLength() const;
  void setSongLength(int length);
  int currentSongPosition() const;
  int songPlayheadPosition() const;
  void setSongPosition(int position);
  void setSongPattern(int position, SongTrack track, int patternIndex);
  void clearSongPattern(int position, SongTrack track);
  int songPatternAt(int position, SongTrack track) const;
  int songPatternAtSlot(int slot, int position, SongTrack track) const;
  const Song& song() const;
  int activeSongSlot() const;
  void setActiveSongSlot(int slot);
  int songPlaybackSlot() const;
  void setSongPlaybackSlot(int slot);
  bool liveMixModeEnabled() const;
  void setLiveMixMode(bool enabled);
  void toggleLiveMixMode();
  void mergeSongs();
  void alternateSongs();
  void setSongReverse(bool reverse);
  bool isSongReverse() const;
  void queueSongReverseToggle();
  bool hasPendingSongReverseToggle() const;
  int display303PatternIndex(int voiceIndex) const;
  int displayDrumPatternIndex() const;
  std::vector<std::string> getAvailableDrumEngines() const;
  void setDrumEngine(const std::string& engineName);
  std::string currentDrumEngineName() const;
  std::string currentSceneName() const;
  std::vector<std::string> availableSceneNames() const;
  bool loadSceneByName(const std::string& name);
  bool saveSceneAs(const std::string& name);
  bool createNewSceneWithName(const std::string& name);
  bool renderProjectToWav(const std::string& filename, std::function<void(float)> progressCallback);

  void toggleMute303(int voiceIndex = 0);
  void setMute303(int voiceIndex, bool muted);
  void toggleMuteKick();
  void toggleMuteSnare();
  void toggleMuteHat();
  void toggleMuteOpenHat();
  void toggleMuteMidTom();
  void toggleMuteHighTom();
  void toggleMuteRim();
  void toggleMuteClap();
  
  bool isTrackActive(int index) const; // 0=303A, 1=303B, 2=KICK, 3=SNARE, 4=HAT, etc.
  void setTrackVolume(VoiceId id, float volume);
  float getTrackVolume(VoiceId id) const;
  void toggleDelay303(int voiceIndex = 0);
  void toggleDistortion303(int voiceIndex = 0);
  void set303DelayEnabled(int voiceIndex, bool enabled);
  void set303DistortionEnabled(int voiceIndex, bool enabled);
  void setDrumPatternIndex(int patternIndex);
  void shiftDrumPatternIndex(int delta);
  void setDrumBankIndex(int bankIndex);
  void adjust303Parameter(TB303ParamId id, int steps, int voiceIndex = 0);
  void set303Parameter(TB303ParamId id, float value, int voice_index = 0);
  void set303ParameterNormalized(TB303ParamId id, float norm, int voice_index = 0);
  void set303PatternIndex(int voice_index, int patternIndex);
  void shift303PatternIndex(int voiceIndex, int delta);
  void set303BankIndex(int voiceIndex, int bankIndex);
  void adjust303StepNote(int voiceIndex, int stepIndex, int semitoneDelta);
  void adjust303StepOctave(int voiceIndex, int stepIndex, int octaveDelta);

  void clear303StepNote(int voiceIndex, int stepIndex);
  void clear303Step(int stepIndex, int voiceIndex); // Clears entire step
  void toggle303AccentStep(int voiceIndex, int stepIndex);
  void toggle303SlideStep(int voiceIndex, int stepIndex);
  void toggleDrumStep(int voiceIndex, int stepIndex);
  void toggleDrumAccentStep(int stepIndex);
  void setDrumAccentStep(int voiceIndex, int stepIndex, bool accent);
  
  void cycle303StepFx(int voiceIndex, int stepIndex);
  void adjust303StepFxParam(int voiceIndex, int stepIndex, int delta);

  void randomize303Pattern(int voiceIndex = 0);
  void randomizeDrumPattern();
  void randomizeDrumVoice(int voiceIndex);
  void randomizeDrumPatternChaos();
  void setGrooveboxMode(GrooveboxMode mode);
  GrooveboxMode grooveboxMode() const;
  void toggleGrooveboxMode();

  // FEEL/TEXTURE: apply scene feel settings to DSP chain
  void applyTextureFromScene_();
  void applyFeelTimingFromScene_();

  // UI Convenience
  int currentScene() const { return current303BankIndex(0); }
  bool isRecording() const { return sceneManager().currentScene().tape.mode == TapeMode::Rec; }
  float swing() const { return genreManager().getGenerativeParams().swingAmount; }

  GrooveboxModeManager& modeManager() { return modeManager_; }
  const GrooveboxModeManager& modeManager() const { return modeManager_; }
  
  GenreManager& genreManager() { return genreManager_; }
  const GenreManager& genreManager() const { return genreManager_; }
  
  TempoDelay& tempoDelay() { return delay303; }  // Main delay for texture (Legacy/Voice 0)
  const TempoDelay& tempoDelay() const { return delay303; }
  
  TempoDelay& tempoDelay(int voiceIndex) { return (voiceIndex == 1) ? delay3032 : delay303; }
  const TempoDelay& tempoDelay(int voiceIndex) const { return (voiceIndex == 1) ? delay3032 : delay303; }
  
  void regeneratePatternsWithGenre();  // Regenerate patterns using current genre

  Parameter& miniParameter(MiniAcidParamId id);
  void setParameter(MiniAcidParamId id, float value);
  void adjustParameter(MiniAcidParamId id, int steps);
  
  // Audio diagnostics toggle (hotkey: Ctrl+D or similar)
  void toggleAudioDiag();
  
  // Test Tone Mode (Diagnose hardware vs DSP)
  void setTestTone(bool enabled);
  bool isTestToneEnabled() const { return testToneEnabled_; }

  // ════════════════════════════════════════════════════════════
  // Vocal Synth (Formant-based robotic speech)
  // ════════════════════════════════════════════════════════════
  FormantSynth& vocalSynth() { return vocalSynth_; }
  const FormantSynth& vocalSynth() const { return vocalSynth_; }
  
  // Quick speech methods
  void speak(const char* text);
  void speakPhrase(int phraseIndex);  // Built-in phrases
  void speakCustomPhrase(int index);  // User-defined phrases
  void stopSpeaking();
  
  // Voice track in song mode
  bool isVoiceTrackMuted() const { return voiceTrackMuted_; }
  void toggleVoiceTrackMute();
  void setVoiceTrackMute(bool muted);
  float getVoiceDuckingLevel() const { return vocalMixer_.getDuckAmount(); }
  
  // Voice Cache (SD card)
  VoiceCache& voiceCache() { return voiceCache_; }
  const VoiceCache& voiceCache() const { return voiceCache_; }
  bool speakCached(const char* text); // Play from cache or fallback to synth

  void generateAudioBuffer(int16_t *buffer, size_t numSamples);

private:
  void updateSamplesPerStep();
  void advanceStep();
  unsigned long computeStepDurationSamples_() const;
  float noteToFreq(int note);
  int clamp303Voice(int voiceIndex) const;
  int clamp303Step(int stepIndex) const;
  int clamp303Note(int note) const;
  const SynthPattern& synthPattern(int synthIndex) const;
  SynthPattern& editSynthPattern(int synthIndex);
  const DrumPattern& drumPattern(int drumVoiceIndex) const;
  DrumPattern& editDrumPattern(int drumVoiceIndex);
  int clampDrumVoice(int voiceIndex) const;
  void refreshSynthCaches(int synthIndex) const;
  void refreshDrumCache(int drumVoiceIndex) const;
  const SynthPattern& activeSynthPattern(int synthIndex) const;
  const DrumPattern& activeDrumPattern(int drumVoiceIndex) const;
  int songPatternIndexForTrack(SongTrack track) const;
  void applySongPositionSelection();
  void syncModeToVoices();
  void advanceSongPlayhead();
  int clampSongPosition(int position) const;

  TB303Voice voice303;
  TB303Voice voice3032;
  std::unique_ptr<DrumSynthVoice> drums;
  float sampleRateValue;
  std::string drumEngineName_;

  SceneManager sceneManager_;
  SceneStorage* sceneStorage_;
  mutable int8_t synthNotesCache_[NUM_303_VOICES][SEQ_STEPS];
  mutable bool synthAccentCache_[NUM_303_VOICES][SEQ_STEPS];
  mutable bool synthSlideCache_[NUM_303_VOICES][SEQ_STEPS];
  mutable bool drumHitCache_[NUM_DRUM_VOICES][SEQ_STEPS];
  mutable bool drumAccentCache_[NUM_DRUM_VOICES][SEQ_STEPS];
  mutable bool drumStepAccentCache_[SEQ_STEPS];

  // Vocal mixing
  VocalMixer vocalMixer_;
  VoiceCompressor voiceCompressor_;
  
  // Internal state
  volatile bool playing;
  volatile bool mute303;
  volatile bool mute303_2;
  volatile bool muteKick;
  volatile bool muteSnare;
  volatile bool muteHat;
  volatile bool muteOpenHat;
  volatile bool muteMidTom;
  volatile bool muteHighTom;
  volatile bool muteRim;
  volatile bool muteClap;
  volatile bool delay303Enabled;
  volatile bool delay3032Enabled;
  volatile bool distortion303Enabled = false;
  bool distortion3032Enabled = false;
  
  // Timing state
  int currentTimingOffset_ = 0;
  
  volatile float bpmValue;
  volatile int currentStepIndex;
  unsigned long samplesIntoStep;
  unsigned long currentStepDurationSamples_ = 1;
  float samplesPerStep;
  
  // Gate length countdown (samples until release, 0 = released)
  long gateCountdownA_ = 0;
  long gateCountdownB_ = 0;
  bool songMode_;
  int drumCycleIndex_;
  int songPlayheadPosition_;
  int songPlaybackSlot_ = 0;
  bool liveMixMode_ = false;
  int songStepCounter_ = 0;
  bool songReverseTogglePending_ = false;
  volatile uint32_t cyclePulseCounter_ = 0;
  int patternModeDrumPatternIndex_;
  int patternModeDrumBankIndex_;
  int patternModeSynthPatternIndex_[NUM_303_VOICES];
  int patternModeSynthBankIndex_[NUM_303_VOICES];

  struct RetrigState {
    int interval = 0;       // Buffer samples between triggers
    int counter = 0;        // Countdown to next trigger
    int countRemaining = 0; // Number of retrigs left
    bool active = false;    // Is retrig active
  };
  
  RetrigState retrigA_;
  RetrigState retrigB_;
  RetrigState retrigDrums_[NUM_DRUM_VOICES];

  struct SoftLimiter {
    float threshold = 0.95f;
    float process(float in) {
      float absIn = fabsf(in);
      if (absIn <= threshold) return in;
      float over = absIn - threshold;
      float comp = threshold + tanhf(over * 3.0f) * 0.15f; 
      return (in > 0) ? comp : -comp;
    }
  } masterLimiter;

  struct MasterBassBoost {
    float f_coeff = 80.0f / 22050.0f; 
    float boost = 1.25f; 
    float lpf = 0.0f;
    float process(float in) {
      lpf += f_coeff * (in - lpf);
      return in + lpf * (boost - 1.0f);
    }
  } masterBass;

  // High-frequency dampening to soften harsh highs
  struct HighShelfCut {
    float lpf = 0.0f;
    float coeff = 0.15f; // ~3kHz rolloff at 22050Hz 
    float process(float in) {
      lpf += coeff * (in - lpf);
      return lpf; // Lowpass output
    }
  } masterHighCut;

  TempoDelay delay303;
  TempoDelay delay3032;
  TubeDistortion distortion303;
  TubeDistortion distortion3032;
  
  // Thread-safe waveform buffer for UI visualization
  // Uses double-buffering with atomic swap to avoid race conditions
  WaveformBuffer waveformBuffers_[2];
  std::atomic<int> displayBufferIndex_{0};
  int writeBufferIndex_ = 1;

  // Vocal synthesizer (formant-based robotic speech)
  FormantSynth vocalSynth_;
  VoiceCache voiceCache_;
  bool voiceTrackMuted_ = false;

  void loadSceneFromStorage();
  void saveSceneToStorage();
  void applySceneStateFromManager();
  void syncSceneStateToManager();

  Parameter params[static_cast<int>(MiniAcidParamId::Count)];
  
public:
  // Public access to stats and sample bank for now
  PerfStats perfStats;
  ISampleStore* sampleStore = nullptr;
  std::unique_ptr<float[]> samplerOutBuffer;
  SampleIndex sampleIndex;
  std::unique_ptr<DrumSamplerTrack> samplerTrack;
  std::unique_ptr<TapeFX> tapeFX;
  std::unique_ptr<TapeLooper> tapeLooper;
  
  // Scene manager accessor for UI tape state
  SceneManager& sceneManager() { return sceneManager_; }
  const SceneManager& sceneManager() const { return sceneManager_; }

private:
  GrooveboxModeManager modeManager_{*this};
  GenreManager genreManager_;
  
  // DSP State for Audio Quality
  uint32_t ditherState_ = 12345;
  bool tapeControlCached_ = false;
  TapeMacro lastTapeMacro_{};
  uint8_t lastTapeSpace_ = 0xFF;
  uint8_t lastTapeMovement_ = 0xFF;
  uint8_t lastTapeGroove_ = 0xFF;
  TapeMode lastTapeMode_ = TapeMode::Stop;
  uint8_t lastTapeSpeed_ = 0xFF;
  float lastTapeLooperVolume_ = -1.0f;
  float fxSafetyMix_ = 1.0f;
  uint16_t fxSafetyHold_ = 0;
  uint32_t lastUnderrunCount_ = 0;
  uint32_t perfDetailCounter_ = 0;

  float dcBlockX1_ = 0.0f;
  float dcBlockY1_ = 0.0f;
  
  // Test Tone State
  bool testToneEnabled_ = false;
  float testTonePhase_ = 0.0f;
  
  static float softLimit(float x) {
      float absX = (x > 0) ? x : -x;
      return x / (1.0f + absX); 
  }
};

class PatternGenerator {
public:
  static void generateRandom303Pattern(SynthPattern& pattern);
  static void generateRandomDrumPattern(DrumPatternSet& patternSet);
};

inline Parameter& MiniAcid::miniParameter(MiniAcidParamId id) {
  return params[static_cast<int>(id)];
}
