#pragma once
#ifndef MINIACID_ENGINE_H
#define MINIACID_ENGINE_H

#include <stddef.h>
#include <stdint.h>
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <functional>

#include "mode_manager.h"
#include "src/dsp/genre_manager.h"
#include "../../scene_storage.h"
#include "../../scenes.h"
#include "mono_synth_voice.h"
#include "clamped_live_note_identity.h"
#include "mini_tb303.h"
#include "swappable_synth_voice.h"
#include "../output/output_owned_synth_voice.h"
#include "mini_drumvoices.h"
#include "pattern_drum_event_tap.h"
#include "../phrase/runtime_pattern_event_bank.h"
#include "../phrase/runtime_synth_playback.h"
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
#include "drum_reverb.h"
#include "one_knob_compressor.h"
#include "transient_shaper.h"

// ===================== Audio config =====================

static const int SAMPLE_RATE = kSampleRate;        // Hz
static const int AUDIO_BUFFER_SAMPLES = kBlockFrames; // per buffer, mono
static const int SEQ_STEPS = 16;             // 16-step sequencer
static const int kPPQN = 96;                 // Pulses Per Quarter Note
static const int NUM_303_VOICES = 2;
static const int NUM_DRUM_VOICES = DrumPatternSet::kVoices;

class MusicalEventQueue;

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
  float mixValue() const { return mix; }
  void setFeedback(float fb);
  void setEnabled(bool on);
  bool isEnabled() const;

  float process(float input);

private:
#if defined(ARDUINO_M5STACK_CARDPUTER)
  static constexpr float kMaxDelaySeconds = 0.25f;
#else
  static constexpr float kMaxDelaySeconds = 1.0f;
#endif

  std::vector<float> buffer;
  int writeIndex;
  int delaySamples;
  float sampleRate;
  int maxDelaySamples;
  float beats;
  float mix;
  float feedback;
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
  void preallocateConstrainedDelayBuffers();
  void reset();
  void start();
  void stop();
  void pauseTransport();
  void continueTransport();

  void liveNoteOn(int synthIndex, uint8_t midiNote, uint8_t velocity);
  void liveNoteOff(int synthIndex, uint8_t midiNote);
  void allLiveNotesOff();
  void suspendLiveNoteProjection(int synthIndex);
  bool patternOwnsInternalSynth(int synthIndex) const;
  void setPatternEventQueue(MusicalEventQueue* queue);
  int liveNote(int synthIndex) const;
  uint32_t liveInputEpoch() const { return liveInputEpoch_; }

  void setBpm(float bpm);
  void setExternalClockBpm(float bpm);
  float bpm() const;
  float sampleRate() const;
  bool isPlaying() const;
  int currentStep() const;
  float getStepProgress() const;
  float transportPhaseSteps() const;
  int cycleBarIndex() const;
  int cycleBarCount() const;
  uint32_t cyclePulseCounter() const { return cyclePulseCounter_; }
  int16_t currentDrumPatternIndex() const;
  int16_t current303PatternIndex(int voiceIndex = 0) const;
  int16_t currentDrumBankIndex() const;
  int16_t current303BankIndex(int voiceIndex = 0) const;
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
  int currentPageIndex() const { return currentPage_.load(std::memory_order_acquire); }
  int targetPageIndex() const { return targetPage_.load(std::memory_order_acquire); }
  bool isPageLoading() const { return pageLoading_.load(std::memory_order_acquire); }
  void setPageLoading(bool loading) { pageLoading_.store(loading, std::memory_order_release); }
  void setTargetPage(int8_t page) { targetPage_.store(page, std::memory_order_release); }
  void setCurrentPage(int8_t page);
  void barrierPatternRuntimeSourceTransition();
  bool rebuildPatternRuntimeEventBank();
  bool refreshPatternRuntimeEvents(int synthIndex, int bankIndex, int patternIndex);
  const PhraseRuntime::RuntimePatternEventBuffer& activePatternRuntimeEvents(int synthIndex) const;
  void requestPageSwitch(int pageIndex);
  int songLength() const;
  void setSongLength(int length);
  int currentSongPosition() const;
  int songPlayheadPosition() const;
  void setSongPosition(int position);
  void setSongPattern(int position, SongTrack track, int16_t patternIndex);
  void clearSongPattern(int position, SongTrack track);
  int16_t songPatternAt(int position, SongTrack track) const;
  int16_t songPatternAtSlot(int slot, int position, SongTrack track) const;
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
  void insertSongRow(int position);
  void deleteSongRow(int position);

  void acknowledgeRehearsal();
  bool isWaitingForRehearsal() const { return waitingForRehearsal_; }

  void setSongReverse(bool reverse);
  bool isSongReverse() const;
  void queueSongReverseToggle();
  bool hasPendingSongReverseToggle() const;
  int16_t display303PatternIndex(int voiceIndex) const;
  int16_t displayDrumPatternIndex() const;
  int display303LocalPatternIndex(int voiceIndex) const;
  int displayDrumLocalPatternIndex() const;

  const Parameter& synthParameter(int voiceIndex, int knobIndex) const;
  uint8_t synthParameterCount(int voiceIndex) const;
  void adjustSynthParameter(int voiceIndex, int knobIndex, int steps);
  void setSynthEngine(int voiceIndex, const std::string& engineName);
  std::vector<std::string> getAvailableSynthEngines() const;
  std::string currentSynthEngineName(int voiceIndex) const;

  std::vector<std::string> getAvailableDrumEngines() const;
  void setDrumEngine(const std::string& engineName);
  std::string currentDrumEngineName() const;
  std::string currentSceneName() const;
  std::vector<std::string> availableSceneNames() const;
  bool autoSaveSceneRecovery();
  bool lastSceneLoadRecoveredAutosave() const {
    return lastSceneLoadRecoveredAutosave_;
  }
  float mainVolume() const;
  void setDeviceMasterVolume(float value);
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

  bool isTrackActive(int index) const;
  void setTrackVolume(VoiceId id, float volume);
  float getTrackVolume(VoiceId id) const;

  void toggleDelay303(int voiceIndex = 0);
  void toggleDistortion303(int voiceIndex = 0);
  void set303DelayEnabled(int voiceIndex, bool enabled);
  void set303DistortionEnabled(int voiceIndex, bool enabled);
  void setDrumPatternIndex(int16_t patternIndex);
  void shiftDrumPatternIndex(int delta);
  void setDrumBankIndex(int bankIndex);
  void adjust303Parameter(TB303ParamId id, int steps, int voiceIndex = 0);
  void set303Parameter(TB303ParamId id, float value, int voice_index = 0);
  void set303ParameterNormalized(TB303ParamId id, float norm, int voice_index = 0);
  void set303PatternIndex(int voice_index, int16_t patternIndex);
  void shift303PatternIndex(int voiceIndex, int delta);
  void set303BankIndex(int voiceIndex, int bankIndex);
  void adjust303StepNote(int voiceIndex, int stepIndex, int semitoneDelta);
  void adjust303StepOctave(int voiceIndex, int stepIndex, int octaveDelta);

  void clear303StepNote(int voiceIndex, int stepIndex);
  void clear303Step(int stepIndex, int voiceIndex);
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

  void rotatePattern(int voiceIndex, int steps);

  void updateDrumCompression(float value);
  void updateDrumTransientAttack(float value);
  void updateDrumTransientSustain(float value);
  void updateDrumReverbMix(float value);
  void updateDrumReverbDecay(float value);
  void setGrooveboxMode(GrooveboxMode mode);
  void activateCommittedGrooveboxModeRuntime(GrooveboxMode mode);
  GrooveboxMode grooveboxMode() const;
  void toggleGrooveboxMode();
  void setGrooveFlavor(int flavor);
  int grooveFlavor() const;
  void shiftGrooveFlavor(int delta);

  void applyTextureFromScene_();
  void applyFeelTimingFromScene_();

  int currentScene() const { return current303BankIndex(0); }
  bool isRecording() const { return sceneManager().currentScene().tape.mode == TapeMode::Rec; }
  float swing() const { return genreManager().getGenerativeParams().swingAmount; }

  GrooveboxModeManager& modeManager() { return modeManager_; }
  const GrooveboxModeManager& modeManager() const { return modeManager_; }

  GenreManager& genreManager() { return genreManager_; }
  const GenreManager& genreManager() const { return genreManager_; }

  TempoDelay& tempoDelay() { return delay303; }
  const TempoDelay& tempoDelay() const { return delay303; }

  TempoDelay& tempoDelay(int voiceIndex) { return (voiceIndex == 1) ? delay3032 : delay303; }
  const TempoDelay& tempoDelay(int voiceIndex) const { return (voiceIndex == 1) ? delay3032 : delay303; }

  void regeneratePatternsWithGenre();
  void syncGrooveModeToGenre();

  Parameter& miniParameter(MiniAcidParamId id);
  void setParameter(MiniAcidParamId id, float value);
  void adjustParameter(MiniAcidParamId id, int steps);

  void toggleAudioDiag();

  void setTestTone(bool enabled);
  bool isTestToneEnabled() const { return testToneEnabled_; }

  FormantSynth& vocalSynth() { return vocalSynth_; }
  const FormantSynth& vocalSynth() const { return vocalSynth_; }

  void speak(const char* text);
  void speakPhrase(int phraseIndex);
  void speakCustomPhrase(int index);
  void stopSpeaking();

  bool isVoiceTrackMuted() const { return voiceTrackMuted_; }
  void toggleVoiceTrackMute();
  void setVoiceTrackMute(bool muted);
  float getVoiceDuckingLevel() const { return vocalMixer_.getDuckAmount(); }

  VoiceCache& voiceCache() { return voiceCache_; }
  const VoiceCache& voiceCache() const { return voiceCache_; }
  bool speakCached(const char* text);

  // P3: Bounded Phrase Source
  enum class SequencedSource : uint8_t {
    Pattern = 0,
    Phrase = 1,
  };

  // Routing lives in the container: phrase[voice] selects the synth, so
  // RuntimeSynthEvent stays free of any target field.
  void setSequencedSource(int voiceIndex, SequencedSource source);
  SequencedSource currentSequencedSource(int voiceIndex) const;
  bool setPhraseLength(int voiceIndex, uint8_t barCount);
  PhraseRuntime::RuntimeSynthEventBuffer& currentPhraseBuffer(int voiceIndex);
  const PhraseRuntime::RuntimeSynthEventBuffer& currentPhraseBuffer(
      int voiceIndex) const;

  void generateAudioBuffer(int16_t *buffer, size_t numSamples);

private:
  void updateTickIncrement();
  void advanceTick();
  void processSequencerEvents(uint32_t absoluteTick);
  void triggerSynthStep_(int synthIdx,
                         const PhraseRuntime::RuntimeSynthEvent& event,
                         uint32_t absoluteStartSubtick);
  void consumePatternPlaybackActions_(
      int synthIdx,
      const PhraseRuntime::RuntimeSynthPlaybackActions& actions);
  void hardBarrierPatternPlayback_(int synthIdx);
  void hardBarrierPatternPlayback_();
  void cleanupLiveNotesForTransportBarrier_(uint8_t patternAuthorityAtEntry);
  uint32_t currentAbsoluteSubtick_() const;
  void publishPatternNoteOn_(int synthIdx, uint8_t note, uint8_t velocity);
  void publishPatternNoteOff_(int synthIdx, uint8_t velocity = 0);
  void publishPatternAllNotesOff_();
  void triggerDrumVoice_(int voiceIdx, int stepIdx);
  void advanceSongBar_();

  int timingTicksForStep_(int stepIndex) const;
  int grooveOverrideTicksForStep_(const DrumPatternSet& patternSet, int stepIndex) const;
  float evaluateAutomationLaneAtStep_(const AutomationLane& lane, int step) const;
  void applyDrumAutomationLanesForStep_(const DrumPatternSet& patternSet, int step);
  float noteToFreq(int note);
  int clamp303Voice(int voiceIndex) const;
  TB303Voice* tb303Voice(int voiceIndex);
  const TB303Voice* tb303Voice(int voiceIndex) const;
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

  OutputOwnedSynthVoiceSlot synthVoices_[NUM_303_VOICES] = {
      OutputOwnedSynthVoiceSlot(GroovePuterOutput::Track::SynthA),
      OutputOwnedSynthVoiceSlot(GroovePuterOutput::Track::SynthB),
  };
  std::string synthEngineNames_[NUM_303_VOICES];

  PatternPublishingDrumVoice drums;
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

  VocalMixer vocalMixer_;
  VoiceCompressor voiceCompressor_;

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

  int currentTimingOffset_ = 0;

  volatile float bpmValue;
  volatile int currentStepIndex;
  uint64_t tickPhaseAccum_ = 0;
  uint64_t tickPhaseInc_ = 0;
  uint32_t currentTick_ = 0;
  float samplesPerStep_ = 10000.0f;

  // P2 compatibility fields remain physically present for this cutover
  // commit, but they no longer own Pattern backend lifetime decisions.
  long gateCountdownA_ = 0;
  long gateCountdownB_ = 0;
  ClampedLiveNoteIdentity liveNotes_[NUM_303_VOICES] = {-1, -1};
  PhraseRuntime::RuntimePatternEventBank patternRuntimeBank_;
  PhraseRuntime::RuntimeSynthPlaybackState patternPlaybackState_[NUM_303_VOICES]{};
  PhraseRuntime::RuntimeSynthEvent patternRetrigEvent_[NUM_303_VOICES]{};
  PatternEventQueueHandle patternEventQueue_;
  int16_t patternMidiNotes_[NUM_303_VOICES] = {-1, -1};
  std::atomic<uint8_t> patternOwnedMask_{0};
  uint32_t liveInputEpoch_ = 0;

  // P3: Bounded Phrase Source, owned per synth voice.
  SequencedSource sequencedSource_[NUM_303_VOICES]{};
  PhraseRuntime::RuntimeSynthEventBuffer currentPhrase_[NUM_303_VOICES]{};

  bool songMode_;
  int drumCycleIndex_;
  int songPlayheadPosition_;
  int songPlaybackSlot_ = 0;
  bool liveMixMode_ = false;
  int songBarIndex_ = -1;
  std::atomic<int8_t> currentPage_{0};
  std::atomic<int8_t> targetPage_{-1};
  std::atomic<bool> pageLoading_{false};
  bool approachB_Enabled_ = true;
  volatile uint32_t cyclePulseCounter_ = 0;

  int patternModeDrumPatternIndex_;
  int patternModeDrumBankIndex_;
  int patternModeSynthPatternIndex_[NUM_303_VOICES];
  int patternModeSynthBankIndex_[NUM_303_VOICES];

  struct RetrigState {
    int interval = 0;
    int counter = 0;
    int countRemaining = 0;
    bool active = false;
    uint8_t flamGhostVelocity = 0;
    int rollTotal = 0;
  };

  RetrigState retrigA_;
  RetrigState retrigB_;
  RetrigState retrigDrums_[NUM_DRUM_VOICES];
  void setupDrumStepFx_(int voiceIdx, uint8_t fx, uint8_t fxParam, uint8_t velocity);

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
    float f_coeff = 80.0f / (float)kSampleRate;
    float boost = 1.25f;
    float lpf = 0.0f;
    float process(float in) {
      lpf += f_coeff * (in - lpf);
      return in + lpf * (boost - 1.0f);
    }
  } masterBass;

  struct HighShelfCut {
    float lpf = 0.0f;
    float coeff = 0.08f;
    float process(float in) {
      lpf += coeff * (in - lpf);
      return lpf;
    }
  } masterHighCut;

  TempoDelay delay303;
  TempoDelay delay3032;
  TubeDistortion distortion303;
  TubeDistortion distortion3032;

  OneKnobCompressor drumCompressor;
  TransientShaper drumTransientShaper;
  DrumReverb drumReverb;

  float dcBlockPrev_ = 0.0f;
  float dcBlockOut_ = 0.0f;

  WaveformBuffer waveformBuffers_[2];
  std::atomic<int> displayBufferIndex_{0};
  int writeBufferIndex_ = 1;

  FormantSynth vocalSynth_;
  VoiceCache voiceCache_;
  bool voiceTrackMuted_ = false;

  void loadSceneFromStorage();
  bool saveSceneToStorage();
  void applySceneStateFromManager();
  void syncSceneStateToManager();

  Parameter params[static_cast<int>(MiniAcidParamId::Count)];

public:
  PerfStats perfStats;
  ISampleStore* sampleStore = nullptr;
  SampleIndex sampleIndex;
  std::unique_ptr<DrumSamplerTrack> samplerTrack;
  std::unique_ptr<TapeFX> tapeFX;
  std::unique_ptr<TapeLooper> tapeLooper;

  SceneManager& sceneManager() { return sceneManager_; }
  const SceneManager& sceneManager() const { return sceneManager_; }

private:
  GrooveboxModeManager modeManager_{*this};
  GenreManager genreManager_{sceneManager_};

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

  bool waitingForRehearsal_ = false;
  bool rehearsalAcknowledged_ = false;

  float dcBlockX1_ = 0.0f;
  float dcBlockY1_ = 0.0f;

  static constexpr float kMasterHighCutHz = 16000.0f;
  float masterOutputHighCutHz_ = 16000.0f;
  float masterOutputLpState_ = 0.0f;
  float masterOutputLpAlpha_ = 1.0f;
  void setMasterOutputHighCutHz(float hz);

  bool testToneEnabled_ = false;
  float testTonePhase_ = 0.0f;
  bool deviceMasterVolumeOverride_ = false;
  bool lastSceneLoadRecoveredAutosave_ = false;

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
#endif // MINIACID_ENGINE_H
