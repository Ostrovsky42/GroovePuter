#include "dsp_engine.h"

#include <math.h>
#include <stdlib.h>
#include <algorithm>
#include <string>

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
}

Parameter::Parameter()
  : _label(""), _unit(""), min_(0.0f), max_(1.0f),
    default_(0.0f), step_(0.0f), value_(0.0f) {}

Parameter::Parameter(const char* label, const char* unit, float minValue, float maxValue, float defaultValue, float step)
  : _label(label), _unit(unit), min_(minValue), max_(maxValue),
    default_(defaultValue), step_(step), value_(defaultValue) {}

const char* Parameter::label() const { return _label; }
const char* Parameter::unit() const { return _unit; }
float Parameter::value() const { return value_; }
float Parameter::min() const { return min_; }
float Parameter::max() const { return max_; }
float Parameter::step() const { return step_; }

float Parameter::normalized() const {
  if (max_ <= min_) return 0.0f;
  return (value_ - min_) / (max_ - min_);
}

void Parameter::setValue(float v) {
  if (v < min_) v = min_;
  if (v > max_) v = max_;
  value_ = v;
}

void Parameter::addSteps(int steps) {
  setValue(value_ + step_ * steps);
}

void Parameter::setNormalized(float norm) {
  if (norm < 0.0f) norm = 0.0f;
  if (norm > 1.0f) norm = 1.0f;
  value_ = min_ + norm * (max_ - min_);
}

void Parameter::reset() {
  value_ = default_;
}

ChamberlinFilter::ChamberlinFilter(float sampleRate) : _lp(0.0f), _bp(0.0f), _sampleRate(sampleRate) {
  if (_sampleRate <= 0.0f) _sampleRate = 44100.0f;
}

void ChamberlinFilter::reset() {
  _lp = 0.0f;
  _bp = 0.0f;
}

void ChamberlinFilter::setSampleRate(float sr) {
  if (sr <= 0.0f) sr = 44100.0f;
  _sampleRate = sr;
}

float ChamberlinFilter::process(float input, float cutoffHz, float resonance) {
  float f = 2.0f * sinf(3.14159265f * cutoffHz / _sampleRate);
  if (!isfinite(f))
    f = 0.0f;
  float q = 1.0f / (1.0f + resonance * 4.0f);
  if (q < 0.06f)
    q = 0.06f;
  

  float hp = input - _lp - q * _bp;
  _bp += f * hp;
  _lp += f * _bp;

  _bp = tanhf(_bp * 1.3f);

  // Keep states bounded to avoid numeric blowups
  const float kStateLimit = 50.0f;
  if (_lp > kStateLimit) _lp = kStateLimit;
  if (_lp < -kStateLimit) _lp = -kStateLimit;
  if (_bp > kStateLimit) _bp = kStateLimit;
  if (_bp < -kStateLimit) _bp = -kStateLimit;

  return _lp;
}

TB303Voice::TB303Voice(float sampleRate)
  : sampleRate(sampleRate),
    invSampleRate(0.0f),
    nyquist(0.0f),
    filter(sampleRate) {
  setSampleRate(sampleRate);
  reset();
}

void TB303Voice::reset() {
  initParameters();
  phase = 0.0f;
  freq = 110.0f;
  targetFreq = 110.0f;
  slideSpeed = 0.001f;
  env = 0.0f;
  gate = false;
  slide = false;
  amp = 0.3f;
  filter.reset();
}

void TB303Voice::setSampleRate(float sampleRateHz) {
  if (sampleRateHz <= 0.0f) sampleRateHz = 44100.0f;
  sampleRate = sampleRateHz;
  invSampleRate = 1.0f / sampleRate;
  nyquist = sampleRate * 0.5f;
  filter.setSampleRate(sampleRate);
}

void TB303Voice::startNote(float freqHz, bool accent, bool slideFlag) {
  slide = slideFlag;

  if (!slide) {
    freq = freqHz;
  }
  targetFreq = freqHz;

  gate = true;
  env = accent ? 2.0f : 1.0f;
}

void TB303Voice::release() { gate = false; }

float TB303Voice::oscSaw() {
  phase += freq * invSampleRate;
  if (phase >= 1.0f) {
    phase -= 1.0f;
  }
  return 2.0f * phase - 1.0f;
}

float TB303Voice::svfProcess(float input) {
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

  float cutoffHz = parameterValue(TB303ParamId::Cutoff) + parameterValue(TB303ParamId::EnvAmount) * env;
  if (cutoffHz < 50.0f)
    cutoffHz = 50.0f;
  float maxCutoff = nyquist * 0.9f;
  if (cutoffHz > maxCutoff)
    cutoffHz = maxCutoff;


  return filter.process(input, cutoffHz, parameterValue(TB303ParamId::Resonance));
}

float TB303Voice::process() {
  if (!gate && env < 0.0001f) {
    return 0.0f;
  }

  float osc = oscSaw();
  float out = svfProcess(osc);

  return out * amp;
}

const Parameter& TB303Voice::parameter(TB303ParamId id) const {
  return params[static_cast<int>(id)];
}

void TB303Voice::setParameter(TB303ParamId id, float value) {
  params[static_cast<int>(id)].setValue(value);
}

void TB303Voice::adjustParameter(TB303ParamId id, int steps) {
  params[static_cast<int>(id)].addSteps(steps);
}

float TB303Voice::parameterValue(TB303ParamId id) const {
  return params[static_cast<int>(id)].value();
}

void TB303Voice::initParameters() {
  params[static_cast<int>(TB303ParamId::Cutoff)] = Parameter("cut", "Hz", 60.0f, 2500.0f, 800.0f, (2500.f - 60.0f) / 128);
  params[static_cast<int>(TB303ParamId::Resonance)] = Parameter("res", "", 0.05f, 0.85f, 0.6f, (0.85f - 0.05f) / 128);
  params[static_cast<int>(TB303ParamId::EnvAmount)] = Parameter("env", "Hz", 0.0f, 2000.0f, 400.0f, (2000.0f - 0.0f) / 128);
  params[static_cast<int>(TB303ParamId::EnvDecay)] = Parameter("dec", "ms", 20.0f, 2200.0f, 420.0f, (2200.0f - 20.0f) / 128);
  params[static_cast<int>(TB303ParamId::MainVolume)] = Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);
}

DrumSynthVoice::DrumSynthVoice(float sampleRate)
  : sampleRate(sampleRate),
    invSampleRate(0.0f) {
  setSampleRate(sampleRate);
  reset();
}

void DrumSynthVoice::reset() {
  kickPhase = 0.0f;
  kickFreq = 60.0f;
  kickEnvAmp = 0.0f;
  kickEnvPitch = 0.0f;
  kickActive = false;

  snareEnvAmp = 0.0f;
  snareToneEnv = 0.0f;
  snareActive = false;
  snareBp = 0.0f;
  snareLp = 0.0f;
  snareTonePhase = 0.0f;
  snareTonePhase2 = 0.0f;

  hatEnvAmp = 0.0f;
  hatToneEnv = 0.0f;
  hatActive = false;
  hatHp = 0.0f;
  hatPrev = 0.0f;
  hatPhaseA = 0.0f;
  hatPhaseB = 0.0f;

  openHatEnvAmp = 0.0f;
  openHatToneEnv = 0.0f;
  openHatActive = false;
  openHatHp = 0.0f;
  openHatPrev = 0.0f;
  openHatPhaseA = 0.0f;
  openHatPhaseB = 0.0f;

  midTomPhase = 0.0f;
  midTomEnv = 0.0f;
  midTomActive = false;

  highTomPhase = 0.0f;
  highTomEnv = 0.0f;
  highTomActive = false;

  rimPhase = 0.0f;
  rimEnv = 0.0f;
  rimActive = false;

  clapEnv = 0.0f;
  clapTrans = 0.0f;
  clapNoise = 0.0f;
  clapActive = false;
  clapDelay = 0.0f;

  params[static_cast<int>(DrumParamId::MainVolume)] = Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);
}

void DrumSynthVoice::setSampleRate(float sampleRateHz) {
  if (sampleRateHz <= 0.0f) sampleRateHz = 44100.0f;
  sampleRate = sampleRateHz;
  invSampleRate = 1.0f / sampleRate;
}

void DrumSynthVoice::triggerKick() {
  kickActive = true;
  kickPhase = 0.0f;
  kickEnvAmp = 1.2f;
  kickEnvPitch = 1.0f;
  kickFreq = 55.0f;
}

void DrumSynthVoice::triggerSnare() {
  snareActive = true;
  snareEnvAmp = 1.1f;
  snareToneEnv = 1.0f;
  snareTonePhase = 0.0f;
  snareTonePhase2 = 0.0f;
}

void DrumSynthVoice::triggerHat() {
  hatActive = true;
  hatEnvAmp = 0.7f;
  hatToneEnv = 1.0f;
  hatPhaseA = 0.0f;
  hatPhaseB = 0.25f;
  // closing the hat chokes any ringing open-hat tail
  openHatEnvAmp *= 0.3f;
}

void DrumSynthVoice::triggerOpenHat() {
  openHatActive = true;
  openHatEnvAmp = 0.9f;
  openHatToneEnv = 1.0f;
  openHatPhaseA = 0.0f;
  openHatPhaseB = 0.37f;
}

void DrumSynthVoice::triggerMidTom() {
  midTomActive = true;
  midTomEnv = 1.0f;
  midTomPhase = 0.0f;
}

void DrumSynthVoice::triggerHighTom() {
  highTomActive = true;
  highTomEnv = 1.0f;
  highTomPhase = 0.0f;
}

void DrumSynthVoice::triggerRim() {
  rimActive = true;
  rimEnv = 1.0f;
  rimPhase = 0.0f;
}

void DrumSynthVoice::triggerClap() {
  clapActive = true;
  clapEnv = 1.0f;
  clapTrans = 1.0f;
  clapNoise = frand();
  clapDelay = 0.0f;
}

float DrumSynthVoice::frand() {
  return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

float DrumSynthVoice::processKick() {
  if (!kickActive)
    return 0.0f;

  // Longer amp tail with faster pitch drop for a punchy thump
  kickEnvAmp *= 0.9995f;
  kickEnvPitch *= 0.997f;
  if (kickEnvAmp < 0.0008f) {
    kickActive = false;
    return 0.0f;
  }

  float pitchFactor = kickEnvPitch * kickEnvPitch;
  float f = 42.0f + 170.0f * pitchFactor;
  kickFreq = f;
  kickPhase += kickFreq * invSampleRate;
  if (kickPhase >= 1.0f)
    kickPhase -= 1.0f;

  float body = sinf(2.0f * 3.14159265f * kickPhase);
  float transient = sinf(2.0f * 3.14159265f * kickPhase * 3.0f) * pitchFactor * 0.25f;
  float driven = tanhf(body * (2.8f + 0.6f * kickEnvAmp));

  return (driven * 0.85f + transient) * kickEnvAmp;
}


float DrumSynthVoice::processSnare() {
  if (!snareActive)
    return 0.0f;

  // --- ENVELOPES ---
  // 808: Long noise decay, short tone decay
  snareEnvAmp *= 0.9985f;   // slow decay, long tail
  snareToneEnv *= 0.99999f;    // short tone "tick"

  if (snareEnvAmp < 0.0002f) {
    snareActive = false;
    return 0.0f;
  }

  // --- NOISE PROCESSING ---
  float n = frand(); // assume 0.0–1.0 random

  // 808: Noise is brighter with a bit of highpass emphasis
  // simple bandpass around ~1–2 kHz
  float f = 0.28f;
  snareBp += f * (n - snareLp - 0.20f * snareBp);
  snareLp += f * snareBp;

  // high fizz (808 has a lot of it)
  float noiseHP = n - snareLp;    // crude highpass
  float noiseOut = snareBp * 0.35f + noiseHP * 0.65f;

  // --- TONE (two sines, tuned to classic 808) ---
  // ~330 Hz + ~180 Hz slight mix, short decay
  snareTonePhase += 330.0f * invSampleRate;
  if (snareTonePhase >= 1.0f) snareTonePhase -= 1.0f;
  snareTonePhase2 += 180.0f * invSampleRate;
  if (snareTonePhase2 >= 1.0f) snareTonePhase2 -= 1.0f;

  float toneA = sinf(2.0f * 3.14159265f * snareTonePhase);
  float toneB = sinf(2.0f * 3.14159265f * snareTonePhase2);
  float tone = (toneA * 0.55f + toneB * 0.45f) * snareToneEnv;

  // --- MIX ---
  // 808: tone only supports transient, noise dominates sustain
  float out = noiseOut * 0.75f + tone * 0.65f;
  return out * snareEnvAmp;
}

float DrumSynthVoice::processHat() {
  if (!hatActive)
    return 0.0f;

  // hatEnvAmp *= 0.994f;   // slower decay for a longer hat tail
  hatEnvAmp *= 0.998f;   // slower decay for a longer hat tail
  hatToneEnv *= 0.92f;
  if (hatEnvAmp < 0.0005f) {
    hatActive = false;
    return 0.0f;
  }

  float n = frand();
  // crude highpass
  float alpha = 0.92f;
  hatHp = alpha * (hatHp + n - hatPrev);
  hatPrev = n;

  // Simple metallic partials on top of noise
  hatPhaseA += 6200.0f * invSampleRate;
  if (hatPhaseA >= 1.0f)
    hatPhaseA -= 1.0f;
  hatPhaseB += 7400.0f * invSampleRate;
  if (hatPhaseB >= 1.0f)
    hatPhaseB -= 1.0f;
  float tone = (sinf(2.0f * 3.14159265f * hatPhaseA) + sinf(2.0f * 3.14159265f * hatPhaseB)) * 0.5f * hatToneEnv;

  float out = hatHp * 0.65f + tone * 0.7f;
  return out * hatEnvAmp * 0.6f;
}

float DrumSynthVoice::processOpenHat() {
  if (!openHatActive)
    return 0.0f;

  openHatEnvAmp *= 0.9993f;
  openHatToneEnv *= 0.94f;
  if (openHatEnvAmp < 0.0004f) {
    openHatActive = false;
    return 0.0f;
  }

  float n = frand();
  float alpha = 0.93f;
  openHatHp = alpha * (openHatHp + n - openHatPrev);
  openHatPrev = n;

  openHatPhaseA += 5100.0f * invSampleRate;
  if (openHatPhaseA >= 1.0f)
    openHatPhaseA -= 1.0f;
  openHatPhaseB += 6600.0f * invSampleRate;
  if (openHatPhaseB >= 1.0f)
    openHatPhaseB -= 1.0f;
  float tone = (sinf(2.0f * 3.14159265f * openHatPhaseA) + sinf(2.0f * 3.14159265f * openHatPhaseB)) * 0.5f * openHatToneEnv;

  float out = openHatHp * 0.55f + tone * 0.95f;
  return out * openHatEnvAmp * 0.7f;
}

float DrumSynthVoice::processMidTom() {
  if (!midTomActive)
    return 0.0f;

  midTomEnv *= 0.99925f;
  if (midTomEnv < 0.0003f) {
    midTomActive = false;
    return 0.0f;
  }

  float freq = 180.0f;
  midTomPhase += freq * invSampleRate;
  if (midTomPhase >= 1.0f)
    midTomPhase -= 1.0f;

  float tone = sinf(2.0f * 3.14159265f * midTomPhase);
  float slightNoise = frand() * 0.05f;
  return (tone * 0.9f + slightNoise) * midTomEnv * 0.8f;
}

float DrumSynthVoice::processHighTom() {
  if (!highTomActive)
    return 0.0f;

  highTomEnv *= 0.99915f;
  if (highTomEnv < 0.0003f) {
    highTomActive = false;
    return 0.0f;
  }

  float freq = 240.0f;

  highTomPhase += freq * invSampleRate;
  if (highTomPhase >= 1.0f)
    highTomPhase -= 1.0f;

  float tone = sinf(2.0f * 3.14159265f * highTomPhase);
  float slightNoise = frand() * 0.04f;
  return (tone * 0.88f + slightNoise) * highTomEnv * 0.75f;
}

float DrumSynthVoice::processRim() {
  if (!rimActive)
    return 0.0f;

  rimEnv *= 0.9985f;
  if (rimEnv < 0.0004f) {
    rimActive = false;
    return 0.0f;
  }

  rimPhase += 900.0f * invSampleRate;
  if (rimPhase >= 1.0f)
    rimPhase -= 1.0f;
  float tone = sinf(2.0f * 3.14159265f * rimPhase);
  float click = (frand() * 0.6f + 0.4f) * rimEnv;
  return (tone * 0.5f + click) * rimEnv * 0.8f;
}

float DrumSynthVoice::processClap() {
  if (!clapActive)
    return 0.0f;

  clapEnv *= 0.99992f;
  clapTrans *= 0.9985f;
  clapDelay += invSampleRate;
  if (clapEnv < 0.0002f) {
    clapActive = false;
    return 0.0f;
  }

  // simple three-burst clap feel
  float burst = 0.0f;
  if (clapDelay < 0.024f) burst = 1.0f;
  else if (clapDelay < 0.048f) burst = 0.8f;
  else if (clapDelay < 0.072f) burst = 0.6f;

  float noise = frand() * 0.7f + clapNoise * 0.3f;
  float tone = sinf(2.0f * 3.14159265f * 1100.0f * clapDelay);
  float out = (noise * 0.7f + tone * 0.3f) * clapTrans * burst;
  return out * clapEnv;
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
    drums(sampleRate),
    sampleRateValue(sampleRate),
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
    bpmValue(100.0f),
    currentStepIndex(-1),
    samplesIntoStep(0),
    samplesPerStep(0.0f),
    songMode_(false),
    songPlayheadPosition_(0),
    patternModeDrumPatternIndex_(0),
    patternModeSynthPatternIndex_{0, 0},
    delay303(sampleRate),
    delay3032(sampleRate) {
  if (sampleRateValue <= 0.0f) sampleRateValue = 44100.0f;
  reset();
}


void MiniAcid::init() {

  params[static_cast<int>(MiniAcidParamId::MainVolume)] = Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);

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
  drums.reset();
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
  lastBufferCount = 0;
  for (int i = 0; i < AUDIO_BUFFER_SAMPLES; ++i) lastBuffer[i] = 0;
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
  drums.reset();
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

int MiniAcid::currentDrumPatternIndex() const {
  return sceneManager_.getCurrentDrumPatternIndex();
}

int MiniAcid::current303PatternIndex(int voiceIndex) const {
  int idx = clamp303Voice(voiceIndex);
  return sceneManager_.getCurrentSynthPatternIndex(idx);
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

bool MiniAcid::songModeEnabled() const { return songMode_; }

void MiniAcid::setSongMode(bool enabled) {
  if (enabled == songMode_) return;
  if (enabled) {
    patternModeDrumPatternIndex_ = sceneManager_.getCurrentDrumPatternIndex();
    patternModeSynthPatternIndex_[0] = sceneManager_.getCurrentSynthPatternIndex(0);
    patternModeSynthPatternIndex_[1] = sceneManager_.getCurrentSynthPatternIndex(1);
    songPlayheadPosition_ = clampSongPosition(sceneManager_.getSongPosition());
    sceneManager_.setSongPosition(songPlayheadPosition_);
    applySongPositionSelection();
  } else {
    sceneManager_.setCurrentDrumPatternIndex(patternModeDrumPatternIndex_);
    sceneManager_.setCurrentSynthPatternIndex(0, patternModeSynthPatternIndex_[0]);
    sceneManager_.setCurrentSynthPatternIndex(1, patternModeSynthPatternIndex_[1]);
  }
  songMode_ = enabled;
  sceneManager_.setSongMode(songMode_);
}

void MiniAcid::toggleSongMode() { setSongMode(!songMode_); }

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
    int pat = sceneManager_.songPattern(sceneManager_.getSongPosition(),
                                        idx == 0 ? SongTrack::SynthA : SongTrack::SynthB);
    return pat;
  }
  return sceneManager_.getCurrentSynthPatternIndex(idx);
}

int MiniAcid::displayDrumPatternIndex() const {
  if (songMode_) {
    return sceneManager_.songPattern(sceneManager_.getSongPosition(), SongTrack::Drums);
  }
  return sceneManager_.getCurrentDrumPatternIndex();
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
  pattern.steps[step].accent = pattern.steps[step].hit;
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
  int pat = sceneManager_.songPattern(pos, track);
  if (pat < 0) return -1;
  int maxPatterns = track == SongTrack::Drums ? Bank<DrumPatternSet>::kPatterns
                                              : Bank<SynthPattern>::kPatterns;
  if (pat >= maxPatterns) pat = maxPatterns - 1;
  return pat;
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

  if (patA < 0) patA = patternModeSynthPatternIndex_[0];
  if (patB < 0) patB = patternModeSynthPatternIndex_[1];
  if (patD < 0) patD = patternModeDrumPatternIndex_;

  sceneManager_.setCurrentSynthPatternIndex(0, patA);
  sceneManager_.setCurrentSynthPatternIndex(1, patB);
  sceneManager_.setCurrentDrumPatternIndex(patD);
}

void MiniAcid::advanceSongPlayhead() {
  int len = sceneManager_.songLength();
  if (len < 1) len = 1;
  songPlayheadPosition_ = (songPlayheadPosition_ + 1) % len;
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

  if (kick.steps[currentStepIndex].hit && !muteKick && drumsActive)
    drums.triggerKick();
  if (snare.steps[currentStepIndex].hit && !muteSnare && drumsActive)
    drums.triggerSnare();
  if (hat.steps[currentStepIndex].hit && !muteHat && drumsActive)
    drums.triggerHat();
  if (openHat.steps[currentStepIndex].hit && !muteOpenHat && drumsActive)
    drums.triggerOpenHat();
  if (midTom.steps[currentStepIndex].hit && !muteMidTom && drumsActive)
    drums.triggerMidTom();
  if (highTom.steps[currentStepIndex].hit && !muteHighTom && drumsActive)
    drums.triggerHighTom();
  if (rim.steps[currentStepIndex].hit && !muteRim && drumsActive)
    drums.triggerRim();
  if (clap.steps[currentStepIndex].hit && !muteClap && drumsActive)
    drums.triggerClap();
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
    }

    float sample = 0.0f;
    if (playing) {
      float sample303 = 0.0f;
      if (!mute303) {
        float v = voice303.process() * 0.5f;
        sample303 += delay303.process(v);
      } else {
        // keep delay line ticking even while muted to let tails decay
        delay303.process(0.0f);
      }
      if (!mute303_2) {
        float v = voice3032.process() * 0.5f;
        sample303 += delay3032.process(v);
      } else {
        delay3032.process(0.0f);
      }
      if (!muteKick)
        sample += drums.processKick();
      if (!muteSnare)
        sample += drums.processSnare();
      if (!muteHat)
        sample += drums.processHat();
      if (!muteOpenHat)
        sample += drums.processOpenHat();
      if (!muteMidTom)
        sample += drums.processMidTom();
      if (!muteHighTom)
        sample += drums.processHighTom();
      if (!muteRim)
        sample += drums.processRim();
      if (!muteClap)
        sample += drums.processClap();
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
  params[static_cast<int>(id)].setValue(value);
}

void MiniAcid::adjustParameter(MiniAcidParamId id, int steps) {
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
    if (sceneStorage_->readScene(sceneManager_)) return;

    std::string serialized;
    if (sceneStorage_->readScene(serialized) && sceneManager_.loadScene(serialized)) {
      return;
    }
  }
  sceneManager_.loadDefaultScene();
}

void MiniAcid::saveSceneToStorage() {
  if (!sceneStorage_) return;
  syncSceneStateToManager();
  if (sceneStorage_->writeScene(sceneManager_)) return;

  std::string serialized = sceneManager_.dumpCurrentScene();
  sceneStorage_->writeScene(serialized);
}

void MiniAcid::applySceneStateFromManager() {
  setBpm(sceneManager_.getBpm());
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

  const SynthParameters& paramsA = sceneManager_.getSynthParameters(0);
  const SynthParameters& paramsB = sceneManager_.getSynthParameters(1);

  voice303.setParameter(TB303ParamId::Cutoff, paramsA.cutoff);
  voice303.setParameter(TB303ParamId::Resonance, paramsA.resonance);
  voice303.setParameter(TB303ParamId::EnvAmount, paramsA.envAmount);
  voice303.setParameter(TB303ParamId::EnvDecay, paramsA.envDecay);

  voice3032.setParameter(TB303ParamId::Cutoff, paramsB.cutoff);
  voice3032.setParameter(TB303ParamId::Resonance, paramsB.resonance);
  voice3032.setParameter(TB303ParamId::EnvAmount, paramsB.envAmount);
  voice3032.setParameter(TB303ParamId::EnvDecay, paramsB.envDecay);

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
  sceneManager_.setSongMode(songMode_);
  int songPosToStore = songMode_ ? songPlayheadPosition_ : sceneManager_.getSongPosition();
  sceneManager_.setSongPosition(clampSongPosition(songPosToStore));

  SynthParameters paramsA;
  paramsA.cutoff = voice303.parameterValue(TB303ParamId::Cutoff);
  paramsA.resonance = voice303.parameterValue(TB303ParamId::Resonance);
  paramsA.envAmount = voice303.parameterValue(TB303ParamId::EnvAmount);
  paramsA.envDecay = voice303.parameterValue(TB303ParamId::EnvDecay);
  sceneManager_.setSynthParameters(0, paramsA);

  SynthParameters paramsB;
  paramsB.cutoff = voice3032.parameterValue(TB303ParamId::Cutoff);
  paramsB.resonance = voice3032.parameterValue(TB303ParamId::Resonance);
  paramsB.envAmount = voice3032.parameterValue(TB303ParamId::EnvAmount);
  paramsB.envDecay = voice3032.parameterValue(TB303ParamId::EnvDecay);
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
      patternSet.voices[kDrumKickVoice].steps[i].accent = patternSet.voices[kDrumKickVoice].steps[i].hit;
    }

    if (drumVoiceCount > kDrumSnareVoice) {
      if (i % 4 == 2 || (rand() % 100) < 15) {
        patternSet.voices[kDrumSnareVoice].steps[i].hit = (rand() % 100) < 80;
      } else {
        patternSet.voices[kDrumSnareVoice].steps[i].hit = false;
      }
      patternSet.voices[kDrumSnareVoice].steps[i].accent = patternSet.voices[kDrumSnareVoice].steps[i].hit;
    }

    bool hatVal = false;
    if (drumVoiceCount > kDrumHatVoice) {
      if ((rand() % 100) < 90) {
        hatVal = (rand() % 100) < 80;
      } else {
        hatVal = false;
      }
      patternSet.voices[kDrumHatVoice].steps[i].hit = hatVal;
      patternSet.voices[kDrumHatVoice].steps[i].accent = hatVal;
    }

    bool openVal = false;
    if (drumVoiceCount > kDrumOpenHatVoice) {
      openVal = (i % 4 == 3 && (rand() % 100) < 65) || ((rand() % 100) < 20 && hatVal);
      patternSet.voices[kDrumOpenHatVoice].steps[i].hit = openVal;
      patternSet.voices[kDrumOpenHatVoice].steps[i].accent = openVal;
      if (openVal && drumVoiceCount > kDrumHatVoice) {
        patternSet.voices[kDrumHatVoice].steps[i].hit = false;
        patternSet.voices[kDrumHatVoice].steps[i].accent = false;
      }
    }

    if (drumVoiceCount > kDrumMidTomVoice) {
      bool midTom = (i % 8 == 4 && (rand() % 100) < 75) || ((rand() % 100) < 8);
      patternSet.voices[kDrumMidTomVoice].steps[i].hit = midTom;
      patternSet.voices[kDrumMidTomVoice].steps[i].accent = midTom;
    }

    if (drumVoiceCount > kDrumHighTomVoice) {
      bool highTom = (i % 8 == 6 && (rand() % 100) < 70) || ((rand() % 100) < 6);
      patternSet.voices[kDrumHighTomVoice].steps[i].hit = highTom;
      patternSet.voices[kDrumHighTomVoice].steps[i].accent = highTom;
    }

    if (drumVoiceCount > kDrumRimVoice) {
      bool rim = (i % 4 == 1 && (rand() % 100) < 25);
      patternSet.voices[kDrumRimVoice].steps[i].hit = rim;
      patternSet.voices[kDrumRimVoice].steps[i].accent = rim;
    }

    if (drumVoiceCount > kDrumClapVoice) {
      bool clap = false;
      if (i % 4 == 2) {
        clap = (rand() % 100) < 80;
      } else {
        clap = (rand() % 100) < 5;
      }
      patternSet.voices[kDrumClapVoice].steps[i].hit = clap;
      patternSet.voices[kDrumClapVoice].steps[i].accent = clap;
    }
  }
}
