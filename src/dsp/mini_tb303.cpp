#include "mini_tb303.h"
#include "audio_wavetables.h"

#include <math.h>
#include <stdlib.h>

namespace {
const char* const kOscillatorOptions[] = {"saw", "sqr", "super", "pulse", "sub"};
// Keep legacy names for compatibility and add lightweight voicing variants.
const char* const kFilterTypeOptions[] = {"lp1", "acid", "moog", "warm", "soft", "retro", "drive"};

inline float fastSaturate(float x) {
  return x / (1.0f + fabsf(x));
}

enum class FilterCore : uint8_t { Chamberlin, Diode, Ladder };
enum class PreProcessType : uint8_t { None, TanhDrive, Quantize };

struct FilterProfile {
  FilterCore core;
  float cutoffMul;
  float resMul;
  float resOffset;
  PreProcessType preType;
  float preDrive;      // tanh drive amount, or quantize levels
  float makeup;
  float postLpfAlpha;  // 1-pole post-LPF coefficient (1.0 = bypass)
};

// Indexed by filter type option index.
//                        core              cutMul resMul resOfs preType              drive  makeup postLPF
constexpr FilterProfile kFilterProfiles[] = {
  // 0: lp1 — neutral baseline (no post-LPF, preserve brightness)
  { FilterCore::Chamberlin, 1.00f, 1.00f, 0.00f, PreProcessType::None,      0.0f,  1.00f, 1.0f },
  // 1: acid — classic acid squelch (post-LPF tames aliasing from diode nonlinearity)
  { FilterCore::Diode,      1.08f, 1.18f, 0.03f, PreProcessType::None,      0.0f,  1.45f, 0.9f },
  // 2: moog — smooth round (no post-LPF, ladder is already smooth)
  { FilterCore::Ladder,     0.88f, 0.90f, 0.00f, PreProcessType::None,      0.0f,  2.05f, 1.0f },
  // 3: warm — synthwave warmth (post-LPF rounds off tanh harmonics)
  { FilterCore::Chamberlin, 0.92f, 0.55f, 0.00f, PreProcessType::TanhDrive, 1.8f,  1.35f, 0.9f },
  // 4: soft — dark smooth (no post-LPF, already very dark)
  { FilterCore::Ladder,     0.80f, 0.72f, 0.00f, PreProcessType::None,      0.0f,  2.35f, 1.0f },
  // 5: retro — retro game console (post-LPF smooths quantization steps)
  { FilterCore::Chamberlin, 0.55f, 0.25f, 0.00f, PreProcessType::Quantize,  32.0f, 2.30f, 0.85f },
  // 6: drive — overdriven growl (post-LPF tames heavy tanh + diode harmonics)
  { FilterCore::Diode,      0.85f, 0.60f, 0.00f, PreProcessType::TanhDrive, 2.5f,  1.55f, 0.85f },
};

const TB303Preset kLoFiMinimalPresets[] = {
    // DEEP BASS
    {400.0f, 0.25f, 150.0f, 400.0f, true, false, "DEEP"},
    // DUSTY KEYS
    {550.0f, 0.3f, 200.0f, 300.0f, true, true, "DUSTY"},
    // WARM PAD
    {500.0f, 0.2f, 80.0f, 800.0f, false, true, "WARM"},
    // GRITTY
    {480.0f, 0.35f, 180.0f, 350.0f, true, false, "GRIT"}
};
} // namespace

TB303Voice::TB303Voice(float sampleRate)
  : sampleRate(sampleRate),
    invSampleRate(0.0f),
    nyquist(0.0f),
    filter(std::make_unique<ChamberlinFilter>(sampleRate)) {
  setSampleRate(sampleRate);
  reset();
}

void TB303Voice::reset() {
  initParameters(); // Initialize wavetables once
  if (!Wavetable::isInitialized()) {
    Wavetable::init();
  }
  
  phase = 0.0f;
  phaseAcc_ = 0;
  for (int i = 0; i < kSuperSawOscCount; ++i) {
    float seed = (static_cast<float>(i) + 1.0f) * 0.137f;
    superPhases[i] = seed - floorf(seed);
    superPhasesAcc_[i] = static_cast<uint32_t>(seed * 4294967296.0f); // Initialize fixed-point
  }
  freq = 110.0f;
  targetFreq = 110.0f;
  slideSpeed = 0.001f;
  env = 0.0f;
  gate = false;
  slide = false;
  amp = 0.3f;
  postLPF_ = 0.0f;
  filter->reset();
}

void TB303Voice::setSampleRate(float sampleRateHz) {
  if (sampleRateHz <= 0.0f) sampleRateHz = 44100.0f;
  sampleRate = sampleRateHz;
  invSampleRate = 1.0f / sampleRate;
  nyquist = sampleRate * 0.5f;
  filter->setSampleRate(sampleRate);
}

void TB303Voice::startNote(float freqHz, bool accent, bool slideFlag, uint8_t velocity) {
  slide = slideFlag;

  if (!slide) {
    freq = freqHz;
  }
  targetFreq = freqHz;

  gate = true;
  env = accent ? 2.0f : 1.0f;
  
  // Velocity scaling (0.3f is base gain)
  amp = 0.3f * (velocity / 100.0f);
}

void TB303Voice::release() { gate = false; }

float TB303Voice::oscSaw() {
  // Wavetable lookup - 3x faster than generating sawtooth
  float output = Wavetable::lookupSaw(phaseAcc_);
  
  // Advance phase using fixed-point for precision
  uint32_t phaseInc = static_cast<uint32_t>(freq * 190359.1689f); // 2^32 / 22050
  phaseAcc_ += phaseInc;
  
  return output;
}

float TB303Voice::oscSquare(float saw) {
  return saw >= 0.0f ? 1.0f : -1.0f;
}

float TB303Voice::oscPulse() {
  // Square wave lookup with 30% duty cycle
  float output = Wavetable::lookupSquare(phaseAcc_);
  
  uint32_t phaseInc = static_cast<uint32_t>(freq * 190359.1689f);
  phaseAcc_ += phaseInc;
  
  return output;
}

float TB303Voice::oscSub() {
  // Saw + Sub octave square
  float saw = Wavetable::lookupSaw(phaseAcc_);
  
  static uint32_t subPhase = 0;
  uint32_t subInc = static_cast<uint32_t>((freq * 0.5f) * 190359.1689f);
  subPhase += subInc;
  
  float sub = Wavetable::lookupSquare(subPhase);
  
  uint32_t phaseInc = static_cast<uint32_t>(freq * 190359.1689f);
  phaseAcc_ += phaseInc;
  
  return saw * 0.7f + sub * 0.3f;
}

float TB303Voice::oscSuperSaw() {
  static const float kSuperSawDetune[kSuperSawOscCount] = {
    -0.019f, 0.019f, -0.012f, 0.012f, -0.0065f, 0.0065f
  };

  // Main oscillator with wavetable
  float sum = Wavetable::lookupSaw(phaseAcc_);
  
  uint32_t baseInc = static_cast<uint32_t>(freq * 190359.1689f);
  phaseAcc_ += baseInc;

  // Detuned oscillators
  for (int i = 0; i < kSuperSawOscCount; ++i) {
    float detunedFreq = freq * (1.0f + kSuperSawDetune[i]);
    uint32_t detunedInc = static_cast<uint32_t>(detunedFreq * 190359.1689f);
    superPhasesAcc_[i] += detunedInc;
    sum += Wavetable::lookupSaw(superPhasesAcc_[i]);
  }

  constexpr float kGain = 1.0f / (TB303Voice::kSuperSawOscCount - 5);
  return sum * kGain;
}

float TB303Voice::oscillatorSample() {
  int oscIdx = oscillatorIndex();
  float out = 0.0f;
  switch (oscIdx) {
    case 1: {
      float saw = oscSaw();
      out = oscSquare(saw);
      break;
    }
    case 2: out = oscSuperSaw(); break;
    case 3: out = oscPulse(); break;
    case 4: out = oscSub(); break;
    default: {
      out = oscSaw();
      if (mode_ == GrooveboxMode::Minimal) {
        // Soft clipping for warmth
        if (out > 0.5f) out = 0.5f + (out - 0.5f) * 0.2f;
        else if (out < -0.5f) out = -0.5f + (out + 0.5f) * 0.2f;
      }
      break;
    }
  }

  // Add dedicated sub oscillator layer if enabled via mode config
  if (subEnabled_ && oscIdx != 4) {
    subPhase_ += (freq * 0.5f) * invSampleRate;
    if (subPhase_ >= 1.0f) subPhase_ -= 1.0f;
    float subSquare = subPhase_ < 0.5f ? 1.0f : -1.0f;
    out = out * 0.7f + subSquare * 0.3f;
  }

  return out;
}

float TB303Voice::svfProcess(float input) {
  // Update filter model if needed
  updateFilterModel();

  // Slide toward target frequency
  freq += (targetFreq - freq) * slideSpeed;
  if (!isfinite(freq))
    freq = targetFreq;

  // Envelope decay
  if (gate || env > 0.0001f) {
    float decayMs = parameterValue(TB303ParamId::EnvDecay);
    float decaySamples = decayMs * sampleRate * 0.001f;
    if (decaySamples < 1.0f)
      decaySamples = 1.0f;
    // 0.01 represents roughly -40 dB, a practical "off" point for the envelope.
    constexpr float kDecayTargetLog = -4.60517019f; // ln(0.01f)
    float decayCoeff = expf(kDecayTargetLog / decaySamples);
    env *= decayCoeff;
  }

  // Hard cap: never let the filter cutoff above 8 kHz regardless of sample rate.
  // At 22050 Hz SR, nyquist*0.9 ≈ 9.9 kHz — too close to fold-back zone.
  float maxCutoff = fminf(nyquist * 0.9f, 8000.0f);

  float baseCutoff = parameterValue(TB303ParamId::Cutoff);
  float envMod = parameterValue(TB303ParamId::EnvAmount) * env;

  // Soft-scale envelope when base cutoff is already high to prevent
  // cutoff + env from slamming into the ceiling and creating harsh peaks.
  float headroom = maxCutoff - baseCutoff;
  if (headroom > 0.0f && envMod > headroom * 0.7f) {
    // 4:1 compression above 70% of remaining headroom.
    envMod = headroom * 0.7f + (envMod - headroom * 0.7f) * 0.25f;
  }

  float cutoffHz = baseCutoff + envMod;
  if (cutoffHz < 50.0f)
    cutoffHz = 50.0f;
  if (cutoffHz > maxCutoff)
    cutoffHz = maxCutoff;
  
  float resonance = parameterValue(TB303ParamId::Resonance);
  const int fltType = params[static_cast<int>(TB303ParamId::FilterType)].optionIndex();
  constexpr int kNumProfiles = sizeof(kFilterProfiles) / sizeof(kFilterProfiles[0]);
  const FilterProfile& prof = kFilterProfiles[fltType < kNumProfiles ? fltType : 0];

  // Apply character from profile table.
  cutoffHz *= prof.cutoffMul;
  resonance = resonance * prof.resMul + prof.resOffset;

  if (cutoffHz > maxCutoff) cutoffHz = maxCutoff;
  if (cutoffHz < 50.0f) cutoffHz = 50.0f;
  if (resonance < 0.0f) resonance = 0.0f;
  if (resonance > 0.95f) resonance = 0.95f;

  // Pre-processing (saturation / quantization) before the filter.
  switch (prof.preType) {
    case PreProcessType::TanhDrive:
      input = fastSaturate(input * prof.preDrive);
      break;
    case PreProcessType::Quantize: {
      // Dither: tiny noise before quantize to soften staircase artifacts.
      noiseState_ = noiseState_ * 1664525 + 1013904223;
      float dither = ((noiseState_ >> 16) & 0x7FFF) / 32768.0f - 0.5f;
      input += dither * (1.0f / prof.preDrive); // ±0.5 LSB
      input = floorf(input * prof.preDrive + 0.5f) / prof.preDrive;
      break;
    }
    default:
      break;
  }

  float filtered = filter->process(input, cutoffHz, resonance);

  float makeup = prof.makeup;

  // Soft-limit after makeup to avoid harsh clipping at high resonance.
  float out = fastSaturate(filtered * makeup);

  // Per-profile 1-pole post-filter to tame aliasing from nonlinearities.
  // α=1.0 bypasses; α=0.9 ≈ 8 kHz; α=0.85 ≈ 6.6 kHz at 22050 Hz SR.
  if (prof.postLpfAlpha < 1.0f) {
    postLPF_ += prof.postLpfAlpha * (out - postLPF_);
    out = postLPF_;
  }
  return out;
}

float TB303Voice::applyLoFiDegradation(float input) {
  if (loFiAmount_ <= 0.001f) return input;

  float out = input;
  
  // 1. Bit reduction
  out = floorf(input * cachedLoFiLevels_ + 0.5f) * cachedRecipLoFiLevels_;

  // 2. Micro-detuning / Jitter
  noiseState_ = noiseState_ * 1664525 + 1013904223;
  float noise = ((noiseState_ >> 16) & 0x7FFF) / 32768.0f - 0.5f;
  out += noise * 0.01f * loFiAmount_;

  // 3. DC Offset / Tape feel
  out += 0.005f * loFiAmount_;

  // 4. Soft Saturation
  if (out > 0.4f) out = 0.4f + (out - 0.4f) * 0.3f;
  else if (out < -0.4f) out = -0.4f + (out + 0.4f) * 0.3f;

  return out;
}

float TB303Voice::process() {
  if (!gate && env < 0.0001f) {
    return 0.0f;
  }

  float mainOsc = oscillatorSample();
  
  // === SUB OSCILLATOR (NEW) ===
  float finalOsc = mainOsc;
  if (subEnabled_) {
    subPhase_ += (freq * 0.5f) * invSampleRate;
    if (subPhase_ >= 1.0f) subPhase_ -= 1.0f;
    float sub = (subPhase_ < 0.5f) ? 1.0f : -1.0f;
    
    // Simple LPF for sub to avoid clicks
    subLPF_prev_ += 0.2f * (sub - subLPF_prev_);
    sub = subLPF_prev_;
    
    finalOsc = mainOsc * (1.0f - subMix_) + sub * subMix_;
  }

  float out = svfProcess(finalOsc);
  
  if (loFiAmount_ > 0.001f) {
    out = applyLoFiDegradation(out);
  }

  // Minimal mode extra character: Noise + DC offset
  if (noiseAmount_ > 0.001f) {
    noiseState_ = noiseState_ * 1664525 + 1013904223;
    float noise = (float)(int16_t(noiseState_ >> 16)) / 32768.0f;
    out += noise * noiseAmount_;
    out += 0.01f * noiseAmount_;
  }

  // === BASS BOOST (NEW) ===
  out = bassBoost_.process(out);

  return out * amp;
}

const Parameter& TB303Voice::parameter(TB303ParamId id) const {
  return params[static_cast<int>(id)];
}

void TB303Voice::setParameter(TB303ParamId id, float value) {
  params[static_cast<int>(id)].setValue(value);
}

void TB303Voice::setParameterNormalized(TB303ParamId id, float norm) {
  params[static_cast<int>(id)].setNormalized(norm);
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
  const TB303Preset& p = kLoFiMinimalPresets[index];
  setParameter(TB303ParamId::Cutoff, p.cutoff);
  setParameter(TB303ParamId::Resonance, p.resonance);
  setParameter(TB303ParamId::EnvAmount, p.envAmount);
  setParameter(TB303ParamId::EnvDecay, p.decay);
  // distortion and delay flags will be handled by MiniAcid
}

void TB303Voice::setMode(GrooveboxMode mode) {
  mode_ = mode;
}

void TB303Voice::setLoFiAmount(float amount) {
  if (fabsf(amount - loFiAmount_) > 0.001f) {
    loFiAmount_ = amount;
    // Precalculate bit reduction levels to avoid per-sample powf/div
    float bits = 12.0f - loFiAmount_ * 6.0f;
    cachedLoFiLevels_ = powf(2.0f, bits);
    cachedRecipLoFiLevels_ = 1.0f / cachedLoFiLevels_;
  }
}

void TB303Voice::setSubOscillator(bool enabled) {
  subEnabled_ = enabled;
  if (!enabled) subPhase_ = 0;
}

void TB303Voice::setNoiseAmount(float amount) {
  noiseAmount_ = amount;
}

void TB303Voice::initParameters() {
  params[static_cast<int>(TB303ParamId::Cutoff)] = Parameter("cut", "Hz", 60.0f, 2500.0f, 800.0f, (2500.f - 60.0f) / 128);
  params[static_cast<int>(TB303ParamId::Resonance)] = Parameter("res", "", 0.0f, 0.85f, 0.0f, 0.85f / 128);
  params[static_cast<int>(TB303ParamId::EnvAmount)] = Parameter("env", "Hz", 0.0f, 2000.0f, 400.0f, (2000.0f - 0.0f) / 128);
  params[static_cast<int>(TB303ParamId::EnvDecay)] = Parameter("dec", "ms", 20.0f, 2200.0f, 420.0f, (2200.0f - 20.0f) / 128);
  params[static_cast<int>(TB303ParamId::Oscillator)] = Parameter("osc", "", kOscillatorOptions, 5, 0);
  params[static_cast<int>(TB303ParamId::FilterType)] = Parameter("flt", "", kFilterTypeOptions, 7, 0);
  params[static_cast<int>(TB303ParamId::MainVolume)] = Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);
}

void TB303Voice::updateFilterModel() {
  int currentType = params[static_cast<int>(TB303ParamId::FilterType)].optionIndex();
  if (currentType == lastFilterType_) return;

  constexpr int kNumProfiles = sizeof(kFilterProfiles) / sizeof(kFilterProfiles[0]);
  FilterCore core = kFilterProfiles[currentType < kNumProfiles ? currentType : 0].core;

  switch (core) {
    case FilterCore::Diode:      filter = std::make_unique<DiodeFilter>(sampleRate); break;
    case FilterCore::Ladder:     filter = std::make_unique<LadderFilter>(sampleRate); break;
    case FilterCore::Chamberlin:
    default:                     filter = std::make_unique<ChamberlinFilter>(sampleRate); break;
  }
  lastFilterType_ = currentType;
}
