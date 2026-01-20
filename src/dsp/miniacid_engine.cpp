#include "miniacid_engine.h"

#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

#include "../miniacid_config.h"

namespace {
constexpr int kDrumKickVoice = 0;
constexpr int kDrumSnareVoice = 1;
constexpr int kDrumHatVoice = 2;
constexpr int kDrumOpenHatVoice = 3;
constexpr int kDrumMidTomVoice = 4;
constexpr int kDrumHighTomVoice = 5;
constexpr int kDrumRimVoice = 6;
constexpr int kDrumClapVoice = 7;
const char* const kDrumEngineOptions[] = {"808", "909", "606"};
constexpr int kDrumEngineOptionCount = 3;

SynthPattern makeEmptySynthPattern() {
  SynthPattern pattern{};
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    pattern.steps[i].note = -1;
    pattern.steps[i].accent = false;
    pattern.steps[i].slide = false;
  }
  for (int p = 0; p < static_cast<int>(TB303ParamId::Count); ++p) {
    pattern.automation[p].clear();
  }
  return pattern;
}

DrumPatternSet makeEmptyDrumPatternSet() {
  DrumPatternSet set{};
  for (int v = 0; v < DrumPatternSet::kVoices; ++v) {
    for (int s = 0; s < DrumPattern::kSteps; ++s) {
      set.voices[v].steps[s].hit = false;
    }
  }
  for (int s = 0; s < DrumPattern::kSteps; ++s) {
    set.accents[s] = false;
  }
  for (int i = 0; i < static_cast<int>(DrumAutomationParamId::Count); ++i) {
    set.automation[i].clear();
  }
  return set;
}

const SynthPattern kEmptySynthPattern = makeEmptySynthPattern();
const DrumPatternSet kEmptyDrumPatternSet = makeEmptyDrumPatternSet();
constexpr uint8_t kAutomationSampleStride = static_cast<uint8_t>(MINIACID_AUTOMATION_SAMPLE_STRIDE);
constexpr float kAutomationValueScale = 1.0f / 255.0f;

std::string toLowerCopy(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

int findOptionIndexByLabel(const Parameter& param, const char* label) {
  if (!label || !label[0]) return -1;
  int count = param.optionCount();
  for (int i = 0; i < count; ++i) {
    const char* optionLabel = param.optionLabelAt(i);
    if (optionLabel && std::strcmp(optionLabel, label) == 0) {
      return i;
    }
  }
  return -1;
}

void upgradeAutomationOptionLanesForPattern(MiniAcid& engine, SynthPattern& pattern, int voiceIndex) {
  for (int p = 0; p < static_cast<int>(TB303ParamId::Count); ++p) {
    AutomationLane& lane = pattern.automation[p];
    if (lane.nodeCount == 0) continue;
    if (lane.hasOptions()) continue;
    TB303ParamId paramId = static_cast<TB303ParamId>(p);
    const Parameter& param = engine.parameter303(paramId, voiceIndex);
    if (!param.hasOptions()) continue;
    int optionCount = param.optionCount();
    if (optionCount <= 0) continue;
    if (optionCount > kAutomationMaxOptions) optionCount = kAutomationMaxOptions;
    const char* labels[kAutomationMaxOptions]{};
    for (int i = 0; i < optionCount; ++i) {
      labels[i] = param.optionLabelAt(i);
    }
    lane.setOptions(labels, optionCount);
    AutomationNode* nodes = lane.nodes();
    if (!nodes) continue;
    int maxIndex = optionCount > 1 ? optionCount - 1 : 0;
    for (int i = 0; i < lane.nodeCount; ++i) {
      if (maxIndex <= 0) {
        nodes[i].y = 0;
        continue;
      }
      float norm = static_cast<float>(nodes[i].y) * kAutomationValueScale;
      int idx = static_cast<int>(norm * static_cast<float>(maxIndex) + 0.5f);
      if (idx < 0) idx = 0;
      if (idx > maxIndex) idx = maxIndex;
      nodes[i].y = static_cast<uint8_t>(idx);
    }
  }
}

void copyAutomationLane(const AutomationLane* src, AutomationLane& dst) {
  dst.clear();
  if (!src) return;
  dst.enabled = src->enabled;
  if (src->optionCount > 0) {
    const char* labels[kAutomationMaxOptions]{};
    int limit = src->optionCount;
    if (limit > kAutomationMaxOptions) limit = kAutomationMaxOptions;
    for (int i = 0; i < limit; ++i) {
      labels[i] = src->optionLabelAt(i);
    }
    dst.setOptions(labels, limit);
  }
  if (src->nodeCount == 0) return;
  if (!dst.ensureCapacity(src->nodeCount)) return;
  AutomationNode* dst_nodes = dst.nodes();
  const AutomationNode* src_nodes = src->nodes();
  if (!dst_nodes || !src_nodes) return;
  for (int i = 0; i < src->nodeCount; ++i) {
    dst_nodes[i] = src_nodes[i];
  }
  dst.nodeCount = src->nodeCount;
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
  setSampleRate(sampleRate);
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
  buffer.assign(static_cast<size_t>(maxDelaySamples), 0.0f);
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
  buffer[writeIndex] = input + delayed * feedback;

  writeIndex++;
  if (writeIndex >= maxDelaySamples)
    writeIndex = 0;

  return input + delayed * mix;
}

MiniAcid::MiniAcid(float sampleRate, SceneStorage* sceneStorage)
  : voice303(sampleRate),
    voice3032(sampleRate),
    drums(std::make_unique<TR808DrumSynthVoice>(sampleRate)),
    // drums(std::make_unique<TR909DrumSynthVoice>(sampleRate)),
    sampleRateValue(sampleRate),
    drumEngineName_("808"),
    sceneStorage_(sceneStorage),
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
    patternModeDrumPatternIndex_(0),
    patternModeDrumBankIndex_(0),
    patternModeSynthPatternIndex_{0, 0},
    patternModeSynthBankIndex_{0, 0},
    delay303(sampleRate),
    delay3032(sampleRate),
    distortion303(),
    distortion3032() {
  if (sampleRateValue <= 0.0f) sampleRateValue = 44100.0f;
  reset();
}


void MiniAcid::init() {

  params[static_cast<int>(MiniAcidParamId::MainVolume)] = Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);
  params[static_cast<int>(MiniAcidParamId::DrumEngine)] =
    Parameter("eng", "", kDrumEngineOptions, kDrumEngineOptionCount, 0);

  // maybe move everything from the constructor here later
  sceneStorage_->initializeStorage();
  loadSceneFromStorage();
  reset();
  applySceneStateFromManager();
}

void MiniAcid::reset() {
  voice303.reset();
  voice3032.reset();
  // make the second voice have different params
  voice3032.adjustParameter(TB303ParamId::Cutoff, -3);
  voice3032.adjustParameter(TB303ParamId::Resonance, -3);
  voice3032.adjustParameter(TB303ParamId::EnvAmount, -1);
  drums->reset();
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
  delay303Enabled = false;
  delay3032Enabled = false;
  distortion303Enabled = false;
  distortion3032Enabled = false;
  bpmValue = 100.0f;
  currentStepIndex = -1;
  samplesIntoStep = 0;
  updateSamplesPerStep();
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
  distortion303.setEnabled(distortion303Enabled);
  distortion3032.setEnabled(distortion3032Enabled);
  lastBufferCount = 0;
  for (int i = 0; i < AUDIO_BUFFER_SAMPLES; ++i) lastBuffer[i] = 0;
  automationSampleCountdown_ = 0;
  songMode_ = false;
  songPlayheadPosition_ = 0;
  patternModeDrumPatternIndex_ = 0;
  patternModeSynthPatternIndex_[0] = 0;
  patternModeSynthPatternIndex_[1] = 0;
}

void MiniAcid::start() {
  playing = true;
  currentStepIndex = -1;
  samplesIntoStep = static_cast<unsigned long>(samplesPerStep);
  automationSampleCountdown_ = 0;
  if (songMode_) {
    songPlayheadPosition_ = clampSongPosition(sceneManager_.getSongPosition());
    sceneManager_.setSongPosition(songPlayheadPosition_);
    applySongPositionSelection();
  }
}

void MiniAcid::stop() {
  playing = false;
  currentStepIndex = -1;
  samplesIntoStep = 0;
  voice303.release();
  voice3032.release();
  drums->reset();
  if (songMode_) {
    sceneManager_.setSongPosition(clampSongPosition(songPlayheadPosition_));
  }

  saveSceneToStorage();
}

void MiniAcid::setBpm(float bpm) {
  bpmValue = bpm;
  if (bpmValue < 40.0f)
    bpmValue = 40.0f;
  if (bpmValue > 200.0f)
    bpmValue = 200.0f;
  updateSamplesPerStep();
  delay303.setBpm(bpmValue);
  delay3032.setBpm(bpmValue);
}

float MiniAcid::bpm() const { return bpmValue; }
float MiniAcid::sampleRate() const { return sampleRateValue; }

bool MiniAcid::isPlaying() const { return playing; }

int MiniAcid::currentStep() const { return currentStepIndex; }

float MiniAcid::currentStepProgress() const {
  if (samplesPerStep <= 0.0f) return 0.0f;
  float frac = static_cast<float>(samplesIntoStep) / samplesPerStep;
  if (frac < 0.0f) frac = 0.0f;
  if (frac > 1.0f) frac = 1.0f;
  return frac;
}

int MiniAcid::currentDrumPatternIndex() const {
  return sceneManager_.getCurrentDrumPatternIndex();
}

int MiniAcid::current303PatternIndex(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  return sceneManager_.getCurrentSynthPatternIndex(idx);
}

int MiniAcid::currentDrumBankIndex() const {
  return sceneManager_.getCurrentBankIndex(0);
}

int MiniAcid::current303BankIndex(int voiceIndex) const {
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
const Parameter& MiniAcid::drumParameter(DrumAutomationParamId id) const {
  switch (id) {
    case DrumAutomationParamId::DrumEngine:
      return params[static_cast<int>(MiniAcidParamId::DrumEngine)];
    default:
      return params[static_cast<int>(MiniAcidParamId::DrumEngine)];
  }
}
const AutomationLane* MiniAcid::automationLane303(TB303ParamId id, int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  return activeSynthPattern(idx).automationLane(id);
}
AutomationLane* MiniAcid::editAutomationLane303(TB303ParamId id, int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  return editSynthPattern(idx).editAutomationLane(id);
}
void MiniAcid::clearAutomationLane303(TB303ParamId id, int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  editSynthPattern(idx).clearAutomationLane(id);
}
void MiniAcid::setAutomationLaneEnabled303(TB303ParamId id, bool enabled, int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  AutomationLane* lane = editSynthPattern(idx).editAutomationLane(id);
  if (!lane) return;
  lane->enabled = enabled;
}
bool MiniAcid::toggleAutomationLaneEnabled303(TB303ParamId id, int voiceIndex) {
  const AutomationLane* lane = automationLane303(id, voiceIndex);
  bool next_enabled = lane ? !lane->enabled : true;
  setAutomationLaneEnabled303(id, next_enabled, voiceIndex);
  return true;
}
void MiniAcid::copy303AutomationToPattern(SynthPattern& dst, int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  const SynthPattern& src = activeSynthPattern(idx);
  for (int p = 0; p < static_cast<int>(TB303ParamId::Count); ++p) {
    copyAutomationLane(&src.automation[p], dst.automation[p]);
  }
}
void MiniAcid::paste303AutomationFromPattern(const SynthPattern& src, int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  SynthPattern& dst = editSynthPattern(idx);
  for (int p = 0; p < static_cast<int>(TB303ParamId::Count); ++p) {
    copyAutomationLane(&src.automation[p], dst.automation[p]);
  }
}
const AutomationLane* MiniAcid::automationLaneDrum(DrumAutomationParamId id) const {
  return activeDrumPatternSet().automationLane(id);
}
AutomationLane* MiniAcid::editAutomationLaneDrum(DrumAutomationParamId id) {
  return editActiveDrumPatternSet().editAutomationLane(id);
}
void MiniAcid::clearAutomationLaneDrum(DrumAutomationParamId id) {
  editActiveDrumPatternSet().clearAutomationLane(id);
}
void MiniAcid::setAutomationLaneEnabledDrum(DrumAutomationParamId id, bool enabled) {
  AutomationLane* lane = editActiveDrumPatternSet().editAutomationLane(id);
  if (!lane) return;
  lane->enabled = enabled;
}
bool MiniAcid::toggleAutomationLaneEnabledDrum(DrumAutomationParamId id) {
  const AutomationLane* lane = automationLaneDrum(id);
  bool next_enabled = lane ? !lane->enabled : true;
  setAutomationLaneEnabledDrum(id, next_enabled);
  return true;
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
  return set.accents;
}
const bool* MiniAcid::patternKickAccentSteps() const {
  return patternDrumAccentSteps();
}
const bool* MiniAcid::patternSnareAccentSteps() const {
  return patternDrumAccentSteps();
}
const bool* MiniAcid::patternHatAccentSteps() const {
  return patternDrumAccentSteps();
}
const bool* MiniAcid::patternOpenHatAccentSteps() const {
  return patternDrumAccentSteps();
}
const bool* MiniAcid::patternMidTomAccentSteps() const {
  return patternDrumAccentSteps();
}
const bool* MiniAcid::patternHighTomAccentSteps() const {
  return patternDrumAccentSteps();
}
const bool* MiniAcid::patternRimAccentSteps() const {
  return patternDrumAccentSteps();
}
const bool* MiniAcid::patternClapAccentSteps() const {
  return patternDrumAccentSteps();
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

int MiniAcid::currentSongPosition() const { return sceneManager_.getSongPosition(); }

int MiniAcid::songPlayheadPosition() const { return songPlayheadPosition_; }

void MiniAcid::setSongPosition(int position) {
  int pos = clampSongPosition(position);
  sceneManager_.setSongPosition(pos);
  if (!playing) songPlayheadPosition_ = pos;
  if (songMode_) applySongPositionSelection();
}

void MiniAcid::setSongPattern(int position, SongTrack track, int patternIndex) {
  sceneManager_.setSongPattern(position, track, patternIndex);
  if (songMode_ && position == currentSongPosition()) {
    applySongPositionSelection();
  }
}

void MiniAcid::clearSongPattern(int position, SongTrack track) {
  sceneManager_.clearSongPattern(position, track);
  int pos = clampSongPosition(sceneManager_.getSongPosition());
  sceneManager_.setSongPosition(pos);
  if (songMode_ && position == pos) {
    applySongPositionSelection();
  }
}

int MiniAcid::songPatternAt(int position, SongTrack track) const {
  return sceneManager_.songPattern(position, track);
}

const Song& MiniAcid::song() const { return sceneManager_.song(); }

int MiniAcid::display303PatternIndex(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  if (songMode_) {
    int combined = sceneManager_.songPattern(sceneManager_.getSongPosition(),
                                             idx == 0 ? SongTrack::SynthA : SongTrack::SynthB);
    if (combined < 0) return -1;
    return songPatternIndexInBank(combined);
  }
  return sceneManager_.getCurrentSynthPatternIndex(idx);
}

int MiniAcid::displayDrumPatternIndex() const {
  if (songMode_) {
    int combined = sceneManager_.songPattern(sceneManager_.getSongPosition(), SongTrack::Drums);
    if (combined < 0) return -1;
    return songPatternIndexInBank(combined);
  }
  return sceneManager_.getCurrentDrumPatternIndex();
}

std::vector<std::string> MiniAcid::getAvailableDrumEngines() const {
  return {"808", "909", "606"};
}

void MiniAcid::setDrumEngine(const std::string& engineName) {
  std::string name = toLowerCopy(engineName);
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
    return;
  }
  drums->reset();
  int optionIndex = findOptionIndexByLabel(
    params[static_cast<int>(MiniAcidParamId::DrumEngine)],
    drumEngineName_.c_str());
  if (optionIndex >= 0) {
    params[static_cast<int>(MiniAcidParamId::DrumEngine)].setValue(static_cast<float>(optionIndex));
  }
}

std::string MiniAcid::currentDrumEngineName() const {
  return drumEngineName_;
}

size_t MiniAcid::copyLastAudio(int16_t *dst, size_t maxSamples) const {
  if (!dst || maxSamples == 0) return 0;
  size_t n = lastBufferCount;
  if (n > maxSamples) n = maxSamples;
  for (size_t i = 0; i < n; ++i) dst[i] = lastBuffer[i];
  return n;
}

void MiniAcid::toggleMute303(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  if (idx == 0)
    mute303 = !mute303;
  else
    mute303_2 = !mute303_2;
}
void MiniAcid::toggleMuteKick() { muteKick = !muteKick; }
void MiniAcid::toggleMuteSnare() { muteSnare = !muteSnare; }
void MiniAcid::toggleMuteHat() { muteHat = !muteHat; }
void MiniAcid::toggleMuteOpenHat() { muteOpenHat = !muteOpenHat; }
void MiniAcid::toggleMuteMidTom() { muteMidTom = !muteMidTom; }
void MiniAcid::toggleMuteHighTom() { muteHighTom = !muteHighTom; }
void MiniAcid::toggleMuteRim() { muteRim = !muteRim; }
void MiniAcid::toggleMuteClap() { muteClap = !muteClap; }
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

void MiniAcid::setDrumPatternIndex(int patternIndex) {
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
  if (idx == 0)
    voice303.setParameter(id, value);
  else
    voice3032.setParameter(id, value);
}
void MiniAcid::set303PatternIndex(int voiceIndex, int patternIndex) {
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
void MiniAcid::adjust303StepNote(int voiceIndex, int stepIndex, int semitoneDelta) {
  int idx = clamp303Voice(voiceIndex);
  int step = clamp303Step(stepIndex);
  SynthPattern& pattern = editSynthPattern(idx);
  int note = pattern.steps[step].note;
  if (note < 0) {
    if (semitoneDelta <= 0) return; // keep rests when moving downward
    note = kMin303Note;
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
  patternSet.accents[step] = !patternSet.accents[step];
}

void MiniAcid::setDrumAccentStep(int stepIndex, bool accent) {
  int step = stepIndex;
  if (step < 0) step = 0;
  if (step >= DrumPattern::kSteps) step = DrumPattern::kSteps - 1;
  DrumPatternSet& patternSet = sceneManager_.editCurrentDrumPattern();
  patternSet.accents[step] = accent;
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

const DrumPatternSet& MiniAcid::activeDrumPatternSet() const {
  int pat = songPatternIndexForTrack(SongTrack::Drums);
  if (pat < 0) return kEmptyDrumPatternSet;
  return sceneManager_.getDrumPatternSet(pat);
}

DrumPatternSet& MiniAcid::editActiveDrumPatternSet() {
  return sceneManager_.editCurrentDrumPattern();
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
  int combined = sceneManager_.songPattern(pos, track);
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
  int len = sceneManager_.songLength();
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
  int patA = sceneManager_.songPattern(pos, SongTrack::SynthA);
  int patB = sceneManager_.songPattern(pos, SongTrack::SynthB);
  int patD = sceneManager_.songPattern(pos, SongTrack::Drums);

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

void MiniAcid::advanceSongPlayhead() {
  int len = sceneManager_.songLength();
  if (len < 1) len = 1;
  int nextPos = (songPlayheadPosition_ + 1) % len;
  if (sceneManager_.loopMode()) {
    int loopStart = sceneManager_.loopStartRow();
    int loopEnd = sceneManager_.loopEndRow();
    if (loopStart < 0) loopStart = 0;
    if (loopEnd < 0) loopEnd = 0;
    if (loopStart >= len) loopStart = len - 1;
    if (loopEnd >= len) loopEnd = len - 1;
    if (loopStart > loopEnd) {
      int tmp = loopStart;
      loopStart = loopEnd;
      loopEnd = tmp;
    }
    if (songPlayheadPosition_ < loopStart || songPlayheadPosition_ > loopEnd) {
      nextPos = loopStart;
    } else if (songPlayheadPosition_ >= loopEnd) {
      nextPos = loopStart;
    } else {
      nextPos = songPlayheadPosition_ + 1;
    }
  }
  songPlayheadPosition_ = nextPos;
  sceneManager_.setSongPosition(songPlayheadPosition_);
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
  }
}

void MiniAcid::applySynthAutomation(int voiceIndex, float t) {
  int idx = clamp303Voice(voiceIndex);
  const SynthPattern& pattern = activeSynthPattern(idx);
  for (int p = 0; p < static_cast<int>(TB303ParamId::Count); ++p) {
    const AutomationLane& lane = pattern.automation[p];
    if (!lane.enabled || lane.nodeCount == 0) continue;
    uint8_t value = lane.evaluate(t);
    TB303ParamId paramId = static_cast<TB303ParamId>(p);
    const Parameter& param = parameter303(paramId, idx);
    float paramValue = 0.0f;
    if (param.hasOptions()) {
      int options = param.optionCount();
      if (options <= 1) {
        paramValue = 0.0f;
      } else if (lane.hasOptions()) {
        int laneIndex = lane.clampOptionIndex(static_cast<int>(value));
        const char* laneLabel = lane.optionLabelAt(laneIndex);
        int mappedIndex = findOptionIndexByLabel(param, laneLabel);
        if (mappedIndex < 0) {
          if (laneIndex >= options) laneIndex = options - 1;
          mappedIndex = laneIndex;
        }
        paramValue = static_cast<float>(mappedIndex);
      } else {
        float norm = static_cast<float>(value) * kAutomationValueScale;
        paramValue = norm * static_cast<float>(options - 1);
      }
    } else {
      float minVal = param.min();
      float maxVal = param.max();
      if (lane.hasOptions()) {
        int maxIndex = lane.optionCount > 1 ? lane.optionCount - 1 : 0;
        float laneNorm = maxIndex > 0
          ? static_cast<float>(lane.clampOptionIndex(static_cast<int>(value))) /
            static_cast<float>(maxIndex)
          : 0.0f;
        paramValue = minVal + laneNorm * (maxVal - minVal);
      } else {
        float norm = static_cast<float>(value) * kAutomationValueScale;
        paramValue = minVal + norm * (maxVal - minVal);
      }
    }
    set303Parameter(paramId, paramValue, voiceIndex);
  }
}

void MiniAcid::applyDrumAutomation(float t) {
  const DrumPatternSet& patternSet = activeDrumPatternSet();
  for (int p = 0; p < static_cast<int>(DrumAutomationParamId::Count); ++p) {
    const AutomationLane& lane = patternSet.automation[p];
    if (!lane.enabled || lane.nodeCount == 0) continue;
    uint8_t value = lane.evaluate(t);
    DrumAutomationParamId paramId = static_cast<DrumAutomationParamId>(p);
    const Parameter& param = drumParameter(paramId);
    float paramValue = 0.0f;
    if (param.hasOptions()) {
      int options = param.optionCount();
      if (options <= 1) {
        paramValue = 0.0f;
      } else if (lane.hasOptions()) {
        int laneIndex = lane.clampOptionIndex(static_cast<int>(value));
        const char* laneLabel = lane.optionLabelAt(laneIndex);
        int mappedIndex = findOptionIndexByLabel(param, laneLabel);
        if (mappedIndex < 0) {
          if (laneIndex >= options) laneIndex = options - 1;
          mappedIndex = laneIndex;
        }
        paramValue = static_cast<float>(mappedIndex);
      } else {
        float norm = static_cast<float>(value) * kAutomationValueScale;
        paramValue = norm * static_cast<float>(options - 1);
      }
    } else {
      float minVal = param.min();
      float maxVal = param.max();
      float norm = static_cast<float>(value) * kAutomationValueScale;
      paramValue = minVal + norm * (maxVal - minVal);
    }
    int optionIndex = static_cast<int>(paramValue + 0.5f);
    const char* label = param.optionLabelAt(optionIndex);
    if (label) {
      if (drumEngineName_ != label) {
        setDrumEngine(label);
      } else {
        params[static_cast<int>(MiniAcidParamId::DrumEngine)].setValue(paramValue);
      }
    } else {
      setParameter(MiniAcidParamId::DrumEngine, paramValue);
    }
  }
}

void MiniAcid::updateSamplesPerStep() {
  samplesPerStep = sampleRateValue * 60.0f / (bpmValue * 4.0f);
}

float MiniAcid::noteToFreq(int note) {
  return 440.0f * powf(2.0f, (note - 69) / 12.0f);
}

void MiniAcid::advanceStep() {
  int prevStep = currentStepIndex;
  currentStepIndex = (currentStepIndex + 1) % SEQ_STEPS;

  if (songMode_) {
    if (prevStep < 0) {
      songPlayheadPosition_ = clampSongPosition(sceneManager_.getSongPosition());
      sceneManager_.setSongPosition(songPlayheadPosition_);
      applySongPositionSelection();
    } else if (currentStepIndex == 0) {
      advanceSongPlayhead();
    }
  }
  applyDrumAutomation(static_cast<float>(currentStepIndex));

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
  int note = stepA.note;
  bool accent = stepA.accent;
  bool slide = stepA.slide;

  int note2 = stepB.note;
  bool accent2 = stepB.accent;
  bool slide2 = stepB.slide;

  if (!mute303 && songPatternA >= 0 && note >= 0)
    voice303.startNote(noteToFreq(note), accent, slide);
  else
    voice303.release();

  if (!mute303_2 && songPatternB >= 0 && note2 >= 0)
    voice3032.startNote(noteToFreq(note2), accent2, slide2);
  else
    voice3032.release();

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
  const DrumPatternSet& patternSet = activeDrumPatternSet();
  bool stepAccent = patternSet.accents[currentStepIndex];

  if (kick.steps[currentStepIndex].hit && !muteKick && drumsActive)
    drums->triggerKick(stepAccent);
  if (snare.steps[currentStepIndex].hit && !muteSnare && drumsActive)
    drums->triggerSnare(stepAccent);
  if (hat.steps[currentStepIndex].hit && !muteHat && drumsActive)
    drums->triggerHat(stepAccent);
  if (openHat.steps[currentStepIndex].hit && !muteOpenHat && drumsActive)
    drums->triggerOpenHat(stepAccent);
  if (midTom.steps[currentStepIndex].hit && !muteMidTom && drumsActive)
    drums->triggerMidTom(stepAccent);
  if (highTom.steps[currentStepIndex].hit && !muteHighTom && drumsActive)
    drums->triggerHighTom(stepAccent);
  if (rim.steps[currentStepIndex].hit && !muteRim && drumsActive)
    drums->triggerRim(stepAccent);
  if (clap.steps[currentStepIndex].hit && !muteClap && drumsActive)
    //drums->triggerCymbal(stepAccent);
    drums->triggerClap(stepAccent);
}

void MiniAcid::generateAudioBuffer(int16_t *buffer, size_t numSamples) {
  if (!buffer || numSamples == 0) {
    return;
  }

  updateSamplesPerStep();
  delay303.setBpm(bpmValue);
  delay3032.setBpm(bpmValue);

  for (size_t i = 0; i < numSamples; ++i) {
    if (playing) {
      if (samplesIntoStep >= (unsigned long)samplesPerStep) {
        samplesIntoStep = 0;
        advanceStep();
      }
      samplesIntoStep++;

      int step = currentStepIndex;
      if (step < 0) step = 0;
      float frac = samplesPerStep > 0.0f ? static_cast<float>(samplesIntoStep) / samplesPerStep : 0.0f;
      float t = static_cast<float>(step) + frac;
#if !MINIACID_DISABLE_AUTOMATION_APPLY
      if (automationSampleCountdown_ == 0) {
        automationSampleCountdown_ = kAutomationSampleStride - 1;
        applySynthAutomation(0, t);
        applySynthAutomation(1, t);
      } else {
        --automationSampleCountdown_;
      }
#endif
    }

    float sample = 0.0f;
    if (playing) {
      float sample303 = 0.0f;
      if (!mute303) {
        float v = voice303.process() * 0.5f;
        v = distortion303.process(v);
        sample303 += delay303.process(v);
      } else {
        // keep delay line ticking even while muted to let tails decay
        delay303.process(0.0f);
      }
      if (!mute303_2) {
        float v = voice3032.process() * 0.5f;
        v = distortion3032.process(v);
        sample303 += delay3032.process(v);
      } else {
        delay3032.process(0.0f);
      }
      if (!muteKick)
        sample += drums->processKick();
      if (!muteSnare)
        sample += drums->processSnare();
      if (!muteHat)
        sample += drums->processHat();
      if (!muteOpenHat)
        sample += drums->processOpenHat();
      if (!muteMidTom)
        sample += drums->processMidTom();
      if (!muteHighTom)
        sample += drums->processHighTom();
      if (!muteRim)
        sample += drums->processRim();
      if (!muteClap)
        // sample += drums->processCymbal();
        sample += drums->processClap();
      sample += sample303;
    }

    // Soft clipping/limiting
    sample *= 0.65f;
    if (sample > 1.0f)
      sample = 1.0f;
    if (sample < -1.0f)
      sample = -1.0f;


    float currentVolume = params[static_cast<int>(MiniAcidParamId::MainVolume)].value();
    buffer[i] = static_cast<int16_t>(sample * 32767.0f * currentVolume);

  }

  size_t copyCount = numSamples;
  if (copyCount > AUDIO_BUFFER_SAMPLES) copyCount = AUDIO_BUFFER_SAMPLES;
  for (size_t i = 0; i < copyCount; ++i) lastBuffer[i] = buffer[i];
  lastBufferCount = copyCount;
}

void MiniAcid::randomize303Pattern(int voiceIndex) {
  int idx = clamp303Voice(voiceIndex);
  PatternGenerator::generateRandom303Pattern(editSynthPattern(idx));
}

void MiniAcid::setParameter(MiniAcidParamId id, float value) {
  if (id == MiniAcidParamId::DrumEngine) {
    params[static_cast<int>(id)].setValue(value);
    int optionIndex = params[static_cast<int>(id)].optionIndex();
    const char* label = params[static_cast<int>(id)].optionLabelAt(optionIndex);
    if (label) {
      setDrumEngine(label);
    }
    return;
  }
  params[static_cast<int>(id)].setValue(value);
}

void MiniAcid::adjustParameter(MiniAcidParamId id, int steps) {
  if (id == MiniAcidParamId::DrumEngine) {
    params[static_cast<int>(id)].addSteps(steps);
    int optionIndex = params[static_cast<int>(id)].optionIndex();
    const char* label = params[static_cast<int>(id)].optionLabelAt(optionIndex);
    if (label) {
      setDrumEngine(label);
    }
    return;
  }
  params[static_cast<int>(id)].addSteps(steps);
}

void MiniAcid::randomizeDrumPattern() {
  PatternGenerator::generateRandomDrumPattern(sceneManager_.editCurrentDrumPattern());
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
  if (!sceneStorage_) return false;
  std::string previousName = sceneStorage_->getCurrentSceneName();
  sceneStorage_->setCurrentSceneName(name);

  bool loaded = sceneStorage_->readScene(sceneManager_);
  if (!loaded) {
    std::string serialized;
    loaded = sceneStorage_->readScene(serialized) && sceneManager_.loadScene(serialized);
  }
  if (!loaded) {
    sceneStorage_->setCurrentSceneName(previousName);
    return false;
  }
  upgradeAutomationOptionLanes();
  applySceneStateFromManager();
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
    if (sceneStorage_->readScene(sceneManager_)) {
      upgradeAutomationOptionLanes();
      return;
    }
  }
  sceneManager_.loadDefaultScene();
  upgradeAutomationOptionLanes();
}

void MiniAcid::saveSceneToStorage() {
  if (!sceneStorage_) return;
  syncSceneStateToManager();
  sceneStorage_->writeScene(sceneManager_);
}

void MiniAcid::upgradeAutomationOptionLanes() {
  Scene& scene = sceneManager_.currentScene();
  for (int bank = 0; bank < kBankCount; ++bank) {
    for (int patternIdx = 0; patternIdx < Bank<SynthPattern>::kPatterns; ++patternIdx) {
      upgradeAutomationOptionLanesForPattern(*this, scene.synthABanks[bank].patterns[patternIdx], 0);
      upgradeAutomationOptionLanesForPattern(*this, scene.synthBBanks[bank].patterns[patternIdx], 1);
    }
  }
}

void MiniAcid::applySceneStateFromManager() {
  setBpm(sceneManager_.getBpm());
  const std::string& drumEngineName = sceneManager_.getDrumEngineName();
  if (!drumEngineName.empty()) {
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

  patternModeDrumPatternIndex_ = sceneManager_.getCurrentDrumPatternIndex();
  patternModeSynthPatternIndex_[0] = sceneManager_.getCurrentSynthPatternIndex(0);
  patternModeSynthPatternIndex_[1] = sceneManager_.getCurrentSynthPatternIndex(1);
  songMode_ = sceneManager_.songMode();
  songPlayheadPosition_ = clampSongPosition(sceneManager_.getSongPosition());
  if (songMode_) {
    applySongPositionSelection();
  }
}

void MiniAcid::syncSceneStateToManager() {
  sceneManager_.setBpm(bpmValue);
  sceneManager_.setDrumEngineName(drumEngineName_);
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
    }
  }
  for (int i = 0; i < stepCount; ++i) {
    patternSet.accents[i] = false;
  }

  for (int i = 0; i < stepCount; ++i) {
    bool accentStep = false;
    if (drumVoiceCount > kDrumKickVoice) {
      if (i % 4 == 0 || (rand() % 100) < 20) {
        patternSet.voices[kDrumKickVoice].steps[i].hit = true;
      } else {
        patternSet.voices[kDrumKickVoice].steps[i].hit = false;
      }
      accentStep = accentStep ||
                   (patternSet.voices[kDrumKickVoice].steps[i].hit && (rand() % 100) < 35);
    }

    if (drumVoiceCount > kDrumSnareVoice) {
      if (i % 4 == 2 || (rand() % 100) < 15) {
        patternSet.voices[kDrumSnareVoice].steps[i].hit = (rand() % 100) < 80;
      } else {
        patternSet.voices[kDrumSnareVoice].steps[i].hit = false;
      }
      accentStep = accentStep ||
                   (patternSet.voices[kDrumSnareVoice].steps[i].hit && (rand() % 100) < 30);
    }

    bool hatVal = false;
    if (drumVoiceCount > kDrumHatVoice) {
      if ((rand() % 100) < 90) {
        hatVal = (rand() % 100) < 80;
      } else {
        hatVal = false;
      }
      patternSet.voices[kDrumHatVoice].steps[i].hit = hatVal;
      accentStep = accentStep || (hatVal && (rand() % 100) < 20);
    }

    bool openVal = false;
    if (drumVoiceCount > kDrumOpenHatVoice) {
      openVal = (i % 4 == 3 && (rand() % 100) < 65) || ((rand() % 100) < 20 && hatVal);
      patternSet.voices[kDrumOpenHatVoice].steps[i].hit = openVal;
      accentStep = accentStep || (openVal && (rand() % 100) < 25);
      if (openVal && drumVoiceCount > kDrumHatVoice) {
        patternSet.voices[kDrumHatVoice].steps[i].hit = false;
      }
    }

    if (drumVoiceCount > kDrumMidTomVoice) {
      bool midTom = (i % 8 == 4 && (rand() % 100) < 75) || ((rand() % 100) < 8);
      patternSet.voices[kDrumMidTomVoice].steps[i].hit = midTom;
      accentStep = accentStep || (midTom && (rand() % 100) < 35);
    }

    if (drumVoiceCount > kDrumHighTomVoice) {
      bool highTom = (i % 8 == 6 && (rand() % 100) < 70) || ((rand() % 100) < 6);
      patternSet.voices[kDrumHighTomVoice].steps[i].hit = highTom;
      accentStep = accentStep || (highTom && (rand() % 100) < 35);
    }

    if (drumVoiceCount > kDrumRimVoice) {
      bool rim = (i % 4 == 1 && (rand() % 100) < 25);
      patternSet.voices[kDrumRimVoice].steps[i].hit = rim;
      accentStep = accentStep || (rim && (rand() % 100) < 30);
    }

    if (drumVoiceCount > kDrumClapVoice) {
      bool clap = false;
      if (i % 4 == 2) {
        clap = (rand() % 100) < 80;
      } else {
        clap = (rand() % 100) < 5;
      }
      patternSet.voices[kDrumClapVoice].steps[i].hit = clap;
      accentStep = accentStep || (clap && (rand() % 100) < 30);
    }
    patternSet.accents[i] = accentStep;
  }
}
