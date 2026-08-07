#include "mini_tb303.h"
#include "audio_wavetables.h"
#include "../audio/audio_config.h"

#include <math.h>
#include <stdlib.h>

namespace {
const char* const kOscillatorOptions[] = {"saw", "sqr", "super", "pulse", "sub"};
const char* const kFilterTypeOptions[] = {"lp1", "acid", "moog", "warm", "soft", "retro", "drive"};

inline float fastSaturate(float x) {
  return x / (1.0f + fabsf(x));
}

inline float clamp01(float value) {
  if (value < 0.0f) return 0.0f;
  if (value > 1.0f) return 1.0f;
  return value;
}

enum class FilterCore : uint8_t { Chamberlin, Diode, Ladder };
enum class PreProcessType : uint8_t { None, TanhDrive, Quantize };

struct FilterProfile {
  FilterCore core;
  float cutoffMul;
  float resMul;
  float resOffset;
  PreProcessType preType;
  float preDrive;
  float makeup;
  float postLpfAlpha;
};

constexpr FilterProfile kFilterProfiles[] = {
  {FilterCore::Chamberlin, 1.00f, 1.00f, 0.00f, PreProcessType::None,      0.0f, 1.00f, 1.0f},
  {FilterCore::Diode,      1.08f, 1.18f, 0.03f, PreProcessType::None,      0.0f, 1.45f, 0.9f},
  {FilterCore::Ladder,     0.88f, 0.90f, 0.00f, PreProcessType::None,      0.0f, 2.05f, 1.0f},
  {FilterCore::Chamberlin, 0.92f, 0.55f, 0.00f, PreProcessType::TanhDrive, 1.8f, 1.35f, 0.9f},
  {FilterCore::Ladder,     0.80f, 0.72f, 0.00f, PreProcessType::None,      0.0f, 2.35f, 1.0f},
  {FilterCore::Chamberlin, 0.55f, 0.25f, 0.00f, PreProcessType::Quantize, 32.0f, 2.30f, 0.85f},
  {FilterCore::Diode,      0.85f, 0.60f, 0.00f, PreProcessType::TanhDrive, 2.5f, 1.55f, 0.85f},
};

const TB303Preset kLoFiMinimalPresets[] = {
    {400.0f, 0.25f, 150.0f, 400.0f, true, false, "DEEP"},
    {550.0f, 0.30f, 200.0f, 300.0f, true, true,  "DUSTY"},
    {500.0f, 0.20f,  80.0f, 800.0f, false, true, "WARM"},
    {480.0f, 0.35f, 180.0f, 350.0f, true, false, "GRIT"},
};

constexpr float kAmpAttackSeconds = 0.003f;
constexpr float kAmpDecaySeconds = 0.080f;
constexpr float kAmpReleaseSeconds = 0.150f;
constexpr float kAmpSustain = 0.78f;
constexpr float kAmpSilence = 0.00001f;
}  // namespace

TB303Voice::TB303Voice(float sampleRateHz)
    : sampleRate(sampleRateHz),
      invSampleRate(0.0f),
      nyquist(0.0f),
      filter(std::make_unique<ChamberlinFilter>(sampleRateHz)) {
  setSampleRate(sampleRateHz);
  reset();
}

void TB303Voice::reset() {
  initParameters();
  if (!Wavetable::isInitialized()) Wavetable::init();

  phase = 0.0f;
  phaseAcc_ = 0;
  subPhaseAcc_ = 0;
  for (int i = 0; i < kSuperSawOscCount; ++i) {
    const float seed = (static_cast<float>(i) + 1.0f) * 0.137f;
    superPhases[i] = seed - floorf(seed);
    superPhasesAcc_[i] = static_cast<uint32_t>(superPhases[i] * 4294967296.0f);
  }

  freq = 110.0f;
  targetFreq = 110.0f;
  slideSpeed = 0.001f;
  env = 0.0f;
  gate = false;
  slide = false;
  ampStage_ = AmpStage::Idle;
  ampEnvelope_ = 0.0f;
  ampVelocity_ = 0.0f;
  subPhase_ = 0.0f;
  subLPF_prev_ = 0.0f;
  postLPF_ = 0.0f;
  bassBoost_.reset();
  if (filter) filter->reset();
}

void TB303Voice::setSampleRate(float sampleRateHz) {
  if (sampleRateHz <= 0.0f) sampleRateHz = 44100.0f;
  sampleRate = sampleRateHz;
  invSampleRate = 1.0f / sampleRate;
  nyquist = sampleRate * 0.5f;
  updateAmplitudeCoefficients();
  if (filter) filter->setSampleRate(sampleRate);
}

void TB303Voice::updateAmplitudeCoefficients() {
  const float attackSamples = fmaxf(1.0f, sampleRate * kAmpAttackSeconds);
  const float decaySamples = fmaxf(1.0f, sampleRate * kAmpDecaySeconds);
  const float releaseSamples = fmaxf(1.0f, sampleRate * kAmpReleaseSeconds);
  ampAttackCoeff_ = 1.0f - expf(-1.0f / attackSamples);
  ampDecayCoeff_ = 1.0f - expf(-1.0f / decaySamples);
  ampReleaseCoeff_ = expf(logf(kAmpSilence) / releaseSamples);
}

void TB303Voice::startNote(float freqHz, bool accent, bool slideFlag,
                           uint8_t velocity) {
  if (!isfinite(freqHz) || freqHz <= 0.0f) return;

  const bool legatoSlide = slideFlag && gate && isVoiceActive();
  slide = slideFlag;
  if (!legatoSlide) freq = freqHz;
  targetFreq = freqHz;
  gate = true;

  if (legatoSlide) {
    return;
  }

  env = accent ? 2.0f : 1.0f;
  if (velocity < 1) velocity = 1;
  if (velocity > 127) velocity = 127;
  ampVelocity_ = 0.375f * (static_cast<float>(velocity) / 100.0f);
  ampEnvelope_ = 0.0f;
  ampStage_ = AmpStage::Attack;
}

void TB303Voice::release() {
  gate = false;
  if (ampStage_ != AmpStage::Idle) ampStage_ = AmpStage::Release;
}

bool TB303Voice::isVoiceActive() const {
  return ampStage_ != AmpStage::Idle;
}

float TB303Voice::advanceAmplitudeEnvelope() {
  if (!gate && ampStage_ != AmpStage::Idle && ampStage_ != AmpStage::Release) {
    ampStage_ = AmpStage::Release;
  }

  switch (ampStage_) {
    case AmpStage::Attack:
      ampEnvelope_ += (1.0f - ampEnvelope_) * ampAttackCoeff_;
      if (ampEnvelope_ >= 0.999f) {
        ampEnvelope_ = 1.0f;
        ampStage_ = gate ? AmpStage::Decay : AmpStage::Release;
      }
      break;
    case AmpStage::Decay:
      ampEnvelope_ += (kAmpSustain - ampEnvelope_) * ampDecayCoeff_;
      if (ampEnvelope_ <= kAmpSustain + 0.001f) {
        ampEnvelope_ = kAmpSustain;
        ampStage_ = gate ? AmpStage::Sustain : AmpStage::Release;
      }
      break;
    case AmpStage::Sustain:
      ampEnvelope_ = kAmpSustain;
      if (!gate) ampStage_ = AmpStage::Release;
      break;
    case AmpStage::Release:
      ampEnvelope_ *= ampReleaseCoeff_;
      if (!isfinite(ampEnvelope_) || ampEnvelope_ <= kAmpSilence) {
        ampEnvelope_ = 0.0f;
        ampVelocity_ = 0.0f;
        ampStage_ = AmpStage::Idle;
      }
      break;
    case AmpStage::Idle:
    default:
      ampEnvelope_ = 0.0f;
      break;
  }
  return ampEnvelope_;
}

float TB303Voice::oscSaw() {
  const float output = Wavetable::lookupSaw(phaseAcc_);
  const uint32_t phaseInc = static_cast<uint32_t>(freq * 4294967296.0f * invSampleRate);
  phaseAcc_ += phaseInc;
  return output;
}

float TB303Voice::oscSquare(float saw) {
  return saw >= 0.0f ? 1.0f : -1.0f;
}

float TB303Voice::oscPulse() {
  const float output = Wavetable::lookupSquare(phaseAcc_);
  const uint32_t phaseInc = static_cast<uint32_t>(freq * 4294967296.0f * invSampleRate);
  phaseAcc_ += phaseInc;
  return output;
}

float TB303Voice::oscSub() {
  const uint32_t phaseInc = static_cast<uint32_t>(freq * 4294967296.0f * invSampleRate);
  const uint32_t subInc = static_cast<uint32_t>(freq * 0.5f * 4294967296.0f * invSampleRate);
  const float saw = Wavetable::lookupSaw(phaseAcc_);
  const float sub = Wavetable::lookupSquare(subPhaseAcc_);
  phaseAcc_ += phaseInc;
  subPhaseAcc_ += subInc;
  return saw * 0.7f + sub * 0.3f;
}

float TB303Voice::oscSuperSaw() {
  static const float kDetune[kSuperSawOscCount] = {
      -0.019f, 0.019f, -0.012f, 0.012f, -0.0065f, 0.0065f};
  float sum = Wavetable::lookupSaw(phaseAcc_);
  phaseAcc_ += static_cast<uint32_t>(freq * 4294967296.0f * invSampleRate);
  for (int i = 0; i < kSuperSawOscCount; ++i) {
    const float detunedFreq = freq * (1.0f + kDetune[i]);
    superPhasesAcc_[i] += static_cast<uint32_t>(
        detunedFreq * 4294967296.0f * invSampleRate);
    sum += Wavetable::lookupSaw(superPhasesAcc_[i]);
  }
  return sum * (1.0f / static_cast<float>(kSuperSawOscCount + 1));
}

float TB303Voice::oscillatorSample() {
  const int oscIdx = oscillatorIndex();
  float out = 0.0f;
  switch (oscIdx) {
    case 1: {
      const float saw = oscSaw();
      out = oscSquare(saw);
      break;
    }
    case 2:
      out = oscSuperSaw();
      break;
    case 3:
      out = oscPulse();
      break;
    case 4:
      out = oscSub();
      break;
    default:
      out = oscSaw();
      if (mode_ == GrooveboxMode::Minimal || mode_ == GrooveboxMode::Dub) {
        if (out > 0.5f) out = 0.5f + (out - 0.5f) * 0.2f;
        else if (out < -0.5f) out = -0.5f + (out + 0.5f) * 0.2f;
      }
      break;
  }

  if (subEnabled_ && oscIdx != 4) {
    subPhase_ += freq * 0.5f * invSampleRate;
    if (subPhase_ >= 1.0f) subPhase_ -= floorf(subPhase_);
    const float rawSub = subPhase_ < 0.5f ? 1.0f : -1.0f;
    subLPF_prev_ += 0.2f * (rawSub - subLPF_prev_);
    out = out * (1.0f - subMix_) + subLPF_prev_ * subMix_;
  }
  return out;
}

float TB303Voice::svfProcess(float input) {
  updateFilterModel();

  freq += (targetFreq - freq) * slideSpeed;
  if (!isfinite(freq)) freq = targetFreq;

  if (gate || env > 0.0001f) {
    float decaySamples = parameterValue(TB303ParamId::EnvDecay) * sampleRate * 0.001f;
    if (decaySamples < 1.0f) decaySamples = 1.0f;
    constexpr float kDecayTargetLog = -4.60517019f;
    env *= expf(kDecayTargetLog / decaySamples);
  }

  const float maxCutoff = fminf(nyquist * 0.9f, 8000.0f);
  const float baseCutoff = parameterValue(TB303ParamId::Cutoff);
  float envMod = parameterValue(TB303ParamId::EnvAmount) * env;
  const float headroom = maxCutoff - baseCutoff;
  if (headroom > 0.0f && envMod > headroom * 0.7f) {
    envMod = headroom * 0.7f + (envMod - headroom * 0.7f) * 0.25f;
  }

  float cutoffHz = baseCutoff + envMod;
  if (cutoffHz < 50.0f) cutoffHz = 50.0f;
  if (cutoffHz > maxCutoff) cutoffHz = maxCutoff;

  float resonance = parameterValue(TB303ParamId::Resonance);
  const int filterType = params[static_cast<int>(TB303ParamId::FilterType)].optionIndex();
  constexpr int kProfileCount = static_cast<int>(sizeof(kFilterProfiles) / sizeof(kFilterProfiles[0]));
  const FilterProfile& profile = kFilterProfiles[
      filterType >= 0 && filterType < kProfileCount ? filterType : 0];

  cutoffHz *= profile.cutoffMul;
  resonance = resonance * profile.resMul + profile.resOffset;
  if (cutoffHz > maxCutoff) cutoffHz = maxCutoff;
  if (cutoffHz < 50.0f) cutoffHz = 50.0f;
  if (resonance < 0.0f) resonance = 0.0f;
  if (resonance > 0.95f) resonance = 0.95f;

  switch (profile.preType) {
    case PreProcessType::TanhDrive:
      input = fastSaturate(input * profile.preDrive);
      break;
    case PreProcessType::Quantize: {
      noiseState_ = noiseState_ * 1664525u + 1013904223u;
      const float dither = static_cast<float>((noiseState_ >> 16) & 0x7FFFu) /
                               32768.0f -
                           0.5f;
      input += dither * (1.0f / profile.preDrive);
      input = floorf(input * profile.preDrive + 0.5f) / profile.preDrive;
      break;
    }
    case PreProcessType::None:
    default:
      break;
  }

  const float filtered = filter->process(input, cutoffHz, resonance);
  float out = fastSaturate(filtered * profile.makeup);
  if (profile.postLpfAlpha < 1.0f) {
    postLPF_ += profile.postLpfAlpha * (out - postLPF_);
    out = postLPF_;
  }
  return out;
}

float TB303Voice::applyLoFiDegradation(float input) {
  if (loFiAmount_ <= 0.001f) return input;
  float out = floorf(input * cachedLoFiLevels_ + 0.5f) * cachedRecipLoFiLevels_;
  noiseState_ = noiseState_ * 1664525u + 1013904223u;
  const float noise = static_cast<float>((noiseState_ >> 16) & 0x7FFFu) /
                          32768.0f -
                      0.5f;
  out += noise * 0.01f * loFiAmount_;
  out += 0.005f * loFiAmount_;
  if (out > 0.4f) out = 0.4f + (out - 0.4f) * 0.3f;
  else if (out < -0.4f) out = -0.4f + (out + 0.4f) * 0.3f;
  return out;
}

float TB303Voice::process() {
  if (!isVoiceActive()) return 0.0f;

  const float envelopeForSample = ampEnvelope_;
  float out = svfProcess(oscillatorSample());
  if (loFiAmount_ > 0.001f) out = applyLoFiDegradation(out);

  if (noiseAmount_ > 0.001f) {
    noiseState_ = noiseState_ * 1664525u + 1013904223u;
    const float noise = static_cast<float>(static_cast<int16_t>(noiseState_ >> 16)) /
                        32768.0f;
    out += noise * noiseAmount_;
    out += 0.01f * noiseAmount_;
  }

  out = bassBoost_.process(out);
  const float volume = clamp01(parameterValue(TB303ParamId::MainVolume));
  const float result = out * ampVelocity_ * envelopeForSample * volume;
  advanceAmplitudeEnvelope();
  return isfinite(result) ? result : 0.0f;
}

uint8_t TB303Voice::parameterCount() const { return 4; }

const Parameter& TB303Voice::parameter(TB303ParamId id) const {
  return params[static_cast<int>(id)];
}

void TB303Voice::setParameter(TB303ParamId id, float value) {
  params[static_cast<int>(id)].setValue(value);
}

void TB303Voice::setParameterNormalized(TB303ParamId id, float norm) {
  params[static_cast<int>(id)].setNormalized(norm);
}

void TB303Voice::setParameterNormalized(uint8_t index, float norm) {
  switch (index) {
    case 0: setParameterNormalized(TB303ParamId::Cutoff, norm); break;
    case 1: setParameterNormalized(TB303ParamId::Resonance, norm); break;
    case 2: setParameterNormalized(TB303ParamId::EnvAmount, norm); break;
    case 3: setParameterNormalized(TB303ParamId::EnvDecay, norm); break;
    default: break;
  }
}

float TB303Voice::getParameterNormalized(uint8_t index) const {
  switch (index) {
    case 0: return params[static_cast<int>(TB303ParamId::Cutoff)].normalized();
    case 1: return params[static_cast<int>(TB303ParamId::Resonance)].normalized();
    case 2: return params[static_cast<int>(TB303ParamId::EnvAmount)].normalized();
    case 3: return params[static_cast<int>(TB303ParamId::EnvDecay)].normalized();
    default: return 0.0f;
  }
}

const Parameter& TB303Voice::getParameter(uint8_t index) const {
  switch (index) {
    case 1: return params[static_cast<int>(TB303ParamId::Resonance)];
    case 2: return params[static_cast<int>(TB303ParamId::EnvAmount)];
    case 3: return params[static_cast<int>(TB303ParamId::EnvDecay)];
    case 0:
    default:
      return params[static_cast<int>(TB303ParamId::Cutoff)];
  }
}

void TB303Voice::adjustParameter(TB303ParamId id, int steps) {
  params[static_cast<int>(id)].addSteps(steps);
}

float TB303Voice::parameterValue(TB303ParamId id) const {
  return params[static_cast<int>(id)].value();
}

int TB303Voice::oscillatorIndex() const {
  return params[static_cast<int>(TB303ParamId::Oscillator)].optionIndex();
}

void TB303Voice::applyLoFiPreset(int index) {
  if (index < 0 || index >= 4) return;
  const TB303Preset& preset = kLoFiMinimalPresets[index];
  setParameter(TB303ParamId::Cutoff, preset.cutoff);
  setParameter(TB303ParamId::Resonance, preset.resonance);
  setParameter(TB303ParamId::EnvAmount, preset.envAmount);
  setParameter(TB303ParamId::EnvDecay, preset.decay);
}

void TB303Voice::setMode(GrooveboxMode mode) { mode_ = mode; }

void TB303Voice::setLoFiAmount(float amount) {
  amount = clamp01(amount);
  if (fabsf(amount - loFiAmount_) <= 0.001f) return;
  loFiAmount_ = amount;
  const float bits = 12.0f - loFiAmount_ * 6.0f;
  cachedLoFiLevels_ = powf(2.0f, bits);
  cachedRecipLoFiLevels_ = 1.0f / cachedLoFiLevels_;
}

void TB303Voice::setSubOscillator(bool enabled) {
  subEnabled_ = enabled;
  if (!enabled) {
    subPhase_ = 0.0f;
    subLPF_prev_ = 0.0f;
  }
}

void TB303Voice::setNoiseAmount(float amount) {
  noiseAmount_ = clamp01(amount);
}

void TB303Voice::initParameters() {
  params[static_cast<int>(TB303ParamId::Cutoff)] =
      Parameter("Cutoff", "Hz", 60.0f, 2500.0f, 800.0f,
                (2500.0f - 60.0f) / 128.0f);
  params[static_cast<int>(TB303ParamId::Resonance)] =
      Parameter("Reso", "", 0.0f, 0.85f, 0.0f, 0.85f / 128.0f);
  params[static_cast<int>(TB303ParamId::EnvAmount)] =
      Parameter("Env", "Hz", 0.0f, 2000.0f, 400.0f, 2000.0f / 128.0f);
  params[static_cast<int>(TB303ParamId::EnvDecay)] =
      Parameter("Decay", "ms", 20.0f, 2200.0f, 420.0f,
                (2200.0f - 20.0f) / 128.0f);
  params[static_cast<int>(TB303ParamId::Oscillator)] =
      Parameter("Oscillator", "", kOscillatorOptions, 5, 0);
  params[static_cast<int>(TB303ParamId::FilterType)] =
      Parameter("Filter", "", kFilterTypeOptions, 7, 0);
  params[static_cast<int>(TB303ParamId::MainVolume)] =
      Parameter("Volume", "", 0.0f, 1.0f, 0.8f, 1.0f / 128.0f);
}

void TB303Voice::updateFilterModel() {
  const int currentType =
      params[static_cast<int>(TB303ParamId::FilterType)].optionIndex();
  if (currentType == lastFilterType_) return;

  constexpr int kProfileCount = static_cast<int>(sizeof(kFilterProfiles) / sizeof(kFilterProfiles[0]));
  const FilterCore core = kFilterProfiles[
      currentType >= 0 && currentType < kProfileCount ? currentType : 0]
                              .core;
  switch (core) {
    case FilterCore::Diode:
      filter = std::make_unique<DiodeFilter>(sampleRate);
      break;
    case FilterCore::Ladder:
      filter = std::make_unique<LadderFilter>(sampleRate);
      break;
    case FilterCore::Chamberlin:
    default:
      filter = std::make_unique<ChamberlinFilter>(sampleRate);
      break;
  }
  lastFilterType_ = currentType;
  postLPF_ = 0.0f;
}
