#include <stdlib.h>
#include <math.h>
#include "mini_drumvoices.h"

LoFiDrumFX::LoFiDrumFX() : noiseState_(12345) {}

void LoFiDrumFX::setEnabled(bool enabled) { enabled_ = enabled; }
void LoFiDrumFX::setAmount(float amount) { amount_ = amount; }

float LoFiDrumFX::process(float input, DrumVoiceType voice) {
  if (!enabled_ || amount_ <= 0.001f) return input;
  
  float output = input;
  
  // 1. Bit reduction
  int bits = 12 - (int)(amount_ * 6.0f);
  output = bitcrush(output, bits);
  
  // 2. Soft saturation
  output = fastTanh(output * (1.0f + amount_ * 0.5f));
  
  // 3. Highpass (subtle)
  output = hipass_.process(output, 60.0f + amount_ * 100.0f, 22050.0f);
  
  // 4. Vinyl noise (very quiet)
  output += vinyl() * 0.01f * amount_;
  
  // 5. Drift
  output *= (1.0f + drift() * 0.002f * amount_);
  
  return output;
}

float LoFiDrumFX::bitcrush(float input, int bits) {
  float levels = powf(2.0f, (float)bits);
  return floorf(input * levels + 0.5f) / levels;
}

inline float LoFiDrumFX::fastTanh(float x) {
  if (x < -3.0f) return -1.0f;
  if (x > 3.0f) return 1.0f;
  float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

float LoFiDrumFX::vinyl() {
  noiseState_ = noiseState_ * 1664525 + 1013904223;
  float noise = ((noiseState_ >> 16) & 0x7FFF) / 32768.0f - 0.5f;
  if ((noiseState_ & 0xFF) < 2) noise *= 4.0f; // occasional pop
  return noise;
}

float LoFiDrumFX::drift() {
  driftPhase_ += 0.0002f;
  if (driftPhase_ > 1.0f) driftPhase_ -= 1.0f;
  return (driftPhase_ < 0.5f) ? (driftPhase_ * 4.0f - 1.0f) : (3.0f - driftPhase_ * 4.0f);
}

TR808DrumSynthVoice::TR808DrumSynthVoice(float sampleRate)
  : sampleRate(sampleRate),
    invSampleRate(0.0f) {
  setSampleRate(sampleRate);
  reset();
}

void TR808DrumSynthVoice::reset() {
  kickPhase = 0.0f;
  kickFreq = 60.0f;
  kickEnvAmp = 0.0f;
  kickEnvPitch = 0.0f;
  kickActive = false;
  kickAccentGain = 1.0f;
  kickAccentDistortion = false;
  kickAmpDecay = 0.9995f;
  kickBaseFreq = 42.0f;

  snareEnvAmp = 0.0f;
  snareToneEnv = 0.0f;
  snareActive = false;
  snareBp = 0.0f;
  snareLp = 0.0f;
  snareTonePhase = 0.0f;
  snareTonePhase2 = 0.0f;
  snareAccentGain = 1.0f;
  snareToneGain = 1.0f;
  snareAccentDistortion = false;

  hatEnvAmp = 0.0f;
  hatToneEnv = 0.0f;
  hatActive = false;
  hatHp = 0.0f;
  hatPrev = 0.0f;
  hatPhaseA = 0.0f;
  hatPhaseB = 0.0f;
  hatAccentGain = 1.0f;
  hatBrightness = 1.0f;
  hatAccentDistortion = false;

  openHatEnvAmp = 0.0f;
  openHatToneEnv = 0.0f;
  openHatActive = false;
  openHatHp = 0.0f;
  openHatPrev = 0.0f;
  openHatPhaseA = 0.0f;
  openHatPhaseB = 0.0f;
  openHatAccentGain = 1.0f;
  openHatBrightness = 1.0f;
  openHatAccentDistortion = false;

  midTomPhase = 0.0f;
  midTomEnv = 0.0f;
  midTomActive = false;
  midTomAccentGain = 1.0f;
  midTomAccentDistortion = false;

  highTomPhase = 0.0f;
  highTomEnv = 0.0f;
  highTomActive = false;
  highTomAccentGain = 1.0f;
  highTomAccentDistortion = false;

  rimPhase = 0.0f;
  rimEnv = 0.0f;
  rimActive = false;
  rimAccentGain = 1.0f;
  rimAccentDistortion = false;

  clapEnv = 0.0f;
  clapTrans = 0.0f;
  clapNoise = 0.0f;
  clapActive = false;
  clapDelay = 0.0f;
  clapTime = 0.0f;
  clapAccentAmount = 0.0f;
  clapAccentGain = 1.0f;
  clapAccentDistortion = false;
  clapBandpass.reset();

  cymbalEnv = 0.0f;
  cymbalToneEnv = 0.0f;
  cymbalActive = false;
  cymbalHp = 0.0f;
  cymbalPrev = 0.0f;
  cymbalPhaseA = 0.0f;
  cymbalPhaseB = 0.0f;
  cymbalAccentGain = 1.0f;
  cymbalBrightness = 1.0f;
  cymbalAccentDistortion = false;
  clapLowpass.reset();

  cymbalEnv = 0.0f;
  cymbalToneEnv = 0.0f;
  cymbalActive = false;
  cymbalHp = 0.0f;
  cymbalPrev = 0.0f;
  cymbalPhaseA = 0.0f;
  cymbalPhaseB = 0.0f;
  cymbalAccentGain = 1.0f;
  cymbalBrightness = 1.0f;
  cymbalAccentDistortion = false;

  accentDistortion.setEnabled(true);
  accentDistortion.setDrive(3.0f);

  params[static_cast<int>(DrumParamId::MainVolume)] = Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);

  updateClapFilters(clapAccentAmount);
}

void TR808DrumSynthVoice::setSampleRate(float sampleRateHz) {
  if (sampleRateHz <= 0.0f) sampleRateHz = 44100.0f;
  sampleRate = sampleRateHz;
  invSampleRate = 1.0f / sampleRate;
  updateClapFilters(clapAccentAmount);
}

void TR808DrumSynthVoice::triggerKick(bool accent) {
  kickActive = true;
  kickPhase = 0.0f;
  kickEnvAmp = accent ? 1.4f : 1.2f;
  kickEnvPitch = 1.0f;
  kickFreq = 55.0f;
  kickAccentGain = accent ? 1.15f : 1.0f;
  kickAccentDistortion = accent;
  kickAmpDecay = accent ? 0.99965f : 0.9995f;
  kickBaseFreq = accent ? 36.0f : 42.0f;
}

void TR808DrumSynthVoice::triggerSnare(bool accent) {
  snareActive = true;
  snareEnvAmp = accent ? 1.4f : 1.0f;
  snareToneEnv = accent ? 1.35f : 1.0f;
  snareTonePhase = 0.0f;
  snareTonePhase2 = 0.0f;
  snareAccentGain = accent ? 1.15f : 1.0f;
  snareToneGain = accent ? 1.2f : 1.0f;
  snareAccentDistortion = accent;
}

void TR808DrumSynthVoice::triggerHat(bool accent) {
  hatActive = true;
  hatEnvAmp = accent ? 0.7f : 0.5f;
  hatToneEnv = 1.0f;
  hatPhaseA = 0.0f;
  hatPhaseB = 0.25f;
  hatAccentGain = accent ? 1.4f : 1.0f;
  hatBrightness = accent ? 1.45f : 1.0f;
  hatAccentDistortion = accent;
  // closing the hat chokes any ringing open-hat tail
  openHatEnvAmp *= 0.3f;
}

void TR808DrumSynthVoice::triggerOpenHat(bool accent) {
  openHatActive = true;
  openHatEnvAmp = accent ? 0.999f : 0.9f;
  openHatToneEnv = 1.0f;
  openHatPhaseA = 0.0f;
  openHatPhaseB = 0.37f;
  openHatAccentGain = accent ? 1.3f : 1.0f;
  openHatBrightness = accent ? 1.25f : 1.0f;
  openHatAccentDistortion = accent;
}

void TR808DrumSynthVoice::triggerMidTom(bool accent) {
  midTomActive = true;
  midTomEnv = 1.0f;
  midTomPhase = 0.0f;
  midTomAccentGain = accent ? 1.45f : 1.0f;
  midTomAccentDistortion = accent;
}

void TR808DrumSynthVoice::triggerHighTom(bool accent) {
  highTomActive = true;
  highTomEnv = 1.0f;
  highTomPhase = 0.0f;
  highTomAccentGain = accent ? 1.45f : 1.0f;
  highTomAccentDistortion = accent;
}

void TR808DrumSynthVoice::triggerRim(bool accent) {
  rimActive = true;
  rimEnv = 1.0f;
  rimPhase = 0.0f;
  rimAccentGain = accent ? 1.4f : 1.0f;
  rimAccentDistortion = accent;
}

void TR808DrumSynthVoice::triggerClap(bool accent) {
  clapActive = true;
  clapEnv = 1.0f;
  clapTrans = 1.0f;
  clapNoise = frand();
  clapDelay = 0.0f;
  clapTime = 0.0f;
  clapAccentAmount = accent ? 0.2f: 0.0f;
  clapAccentGain = accent ? 1.45f : 1.0f;
  clapAccentDistortion = accent;
  clapBandpass.reset();
  clapLowpass.reset();
  updateClapFilters(clapAccentAmount);
}

void TR808DrumSynthVoice::triggerCymbal(bool accent) {
  cymbalActive = true;
  cymbalEnv = accent ? 0.85f : 0.7f;
  cymbalToneEnv = 1.0f;
  cymbalPhaseA = 0.0f;
  cymbalPhaseB = 0.35f;
  cymbalAccentGain = accent ? 1.25f : 1.0f;
  cymbalBrightness = accent ? 1.4f : 1.0f;
  cymbalAccentDistortion = accent;
}

float TR808DrumSynthVoice::frand() {
  return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

float TR808DrumSynthVoice::applyAccentDistortion(float input, bool accent) {
  if (!accent) {
    return input;
  }
  return accentDistortion.process(input);
}

void TR808DrumSynthVoice::updateClapFilters(float accentAmount) {
  float bpFreq = 1200.0f + 700.0f * accentAmount;
  float bpQ = 0.6f;
  float bpW0 = 2.0f * 3.14159265f * bpFreq * invSampleRate;
  float bpAlpha = sinf(bpW0) / (2.0f * bpQ);
  float bpCos = cosf(bpW0);

  float bpB0 = bpAlpha;
  float bpB1 = 0.0f;
  float bpB2 = -bpAlpha;
  float bpA0 = 1.0f + bpAlpha;
  float bpA1 = -2.0f * bpCos;
  float bpA2 = 1.0f - bpAlpha;

  clapBandpass.a0 = bpB0 / bpA0;
  clapBandpass.a1 = bpB1 / bpA0;
  clapBandpass.a2 = bpB2 / bpA0;
  clapBandpass.b1 = bpA1 / bpA0;
  clapBandpass.b2 = bpA2 / bpA0;

  float lpFreq = 4500.0f + 2000.0f * accentAmount;
  float lpQ = 0.7f;
  float lpW0 = 2.0f * 3.14159265f * lpFreq * invSampleRate;
  float lpAlpha = sinf(lpW0) / (2.0f * lpQ);
  float lpCos = cosf(lpW0);

  float lpB0 = (1.0f - lpCos) * 0.5f;
  float lpB1 = 1.0f - lpCos;
  float lpB2 = (1.0f - lpCos) * 0.5f;
  float lpA0 = 1.0f + lpAlpha;
  float lpA1 = -2.0f * lpCos;
  float lpA2 = 1.0f - lpAlpha;

  clapLowpass.a0 = lpB0 / lpA0;
  clapLowpass.a1 = lpB1 / lpA0;
  clapLowpass.a2 = lpB2 / lpA0;
  clapLowpass.b1 = lpA1 / lpA0;
  clapLowpass.b2 = lpA2 / lpA0;
}

float TR808DrumSynthVoice::processKick() {
  if (!kickActive)
    return 0.0f;

  // Longer amp tail with faster pitch drop for a punchy thump
  kickEnvAmp *= kickAmpDecay;
  kickEnvPitch *= 0.997f;
  if (kickEnvAmp < 0.0008f) {
    kickActive = false;
    return 0.0f;
  }

  float pitchFactor = kickEnvPitch * kickEnvPitch;
  float f = kickBaseFreq + 170.0f * pitchFactor;
  kickFreq = f;
  kickPhase += kickFreq * invSampleRate;
  if (kickPhase >= 1.0f)
    kickPhase -= 1.0f;

  float body = sinf(2.0f * 3.14159265f * kickPhase);
  float transient = sinf(2.0f * 3.14159265f * kickPhase * 3.0f) * pitchFactor * 0.25f;
  float driven = tanhf(body * (2.8f + 0.6f * kickEnvAmp));

  float out = (driven * 0.85f + transient) * kickEnvAmp * kickAccentGain;
  float res = applyAccentDistortion(out, kickAccentDistortion);
  return lofiEnabled ? lofi.process(res, KICK) : res;
}


float TR808DrumSynthVoice::processSnare() {
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
  float tone = (toneA * 0.55f + toneB * 0.45f) * snareToneEnv * snareToneGain;

  // 808: tone only supports transient, noise dominates sustain
  float out = noiseOut * 0.75f + tone * 0.65f;
  out *= snareEnvAmp * snareAccentGain;
  float res = applyAccentDistortion(out, snareAccentDistortion);
  return lofiEnabled ? lofi.process(res, SNARE) : res;
}

float TR808DrumSynthVoice::processHat() {
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
  float tone = (sinf(2.0f * 3.14159265f * hatPhaseA) + sinf(2.0f * 3.14159265f * hatPhaseB)) *
               0.5f * hatToneEnv * hatBrightness;

  float out = hatHp * 0.65f + tone * 0.7f;
  out *= hatEnvAmp * 0.6f * hatAccentGain;
  float res = applyAccentDistortion(out, hatAccentDistortion);
  return lofiEnabled ? lofi.process(res, CLOSED_HAT) : res;
}

float TR808DrumSynthVoice::processOpenHat() {
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
  float tone =
    (sinf(2.0f * 3.14159265f * openHatPhaseA) + sinf(2.0f * 3.14159265f * openHatPhaseB)) *
    0.5f * openHatToneEnv * openHatBrightness;

  float out = openHatHp * 0.55f + tone * 0.95f;
  out *= openHatEnvAmp * 0.7f * openHatAccentGain;
  float res = applyAccentDistortion(out, openHatAccentDistortion);
  return lofiEnabled ? lofi.process(res, OPEN_HAT) : res;
}

float TR808DrumSynthVoice::processMidTom() {
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
  float out = (tone * 0.9f + slightNoise) * midTomEnv * 0.8f * midTomAccentGain;
  float res = applyAccentDistortion(out, midTomAccentDistortion);
  return lofiEnabled ? lofi.process(res, MID_TOM) : res;
}

float TR808DrumSynthVoice::processHighTom() {
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
  float out = (tone * 0.88f + slightNoise) * highTomEnv * 0.75f * highTomAccentGain;
  float res = applyAccentDistortion(out, highTomAccentDistortion);
  return lofiEnabled ? lofi.process(res, HIGH_TOM) : res;
}

float TR808DrumSynthVoice::processRim() {
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
  float out = (tone * 0.5f + click) * rimEnv * 0.8f * rimAccentGain;
  float res = applyAccentDistortion(out, rimAccentDistortion);
  return lofiEnabled ? lofi.process(res, RIM) : res;
}

float TR808DrumSynthVoice::processClap() {
  if (!clapActive)
    return 0.0f;

  clapEnv *= 0.9992f;
  clapTrans *= 0.9985f;
  clapDelay += invSampleRate;
  clapTime += invSampleRate;
  if (clapEnv < 0.0002f) {
    clapActive = false;
    return 0.0f;
  }

  float decayScale = 1.0f + 0.5f * clapAccentAmount;
  float accentGain = 1.0f + 0.6f * clapAccentAmount;

  float env1 = clapTime < 0.0f ? 0.0f : expf(-(clapTime - 0.0f) / (0.007f * decayScale));
  float env2 = clapTime < 0.008f ? 0.0f : expf(-(clapTime - 0.008f) / (0.011f * decayScale));
  float env3 = clapTime < 0.015f ? 0.0f : expf(-(clapTime - 0.015f) / (0.015f * decayScale));
  float body = frand() * (env1 + env2 + env3);

  float tail = 0.0f;
  if (clapTime >= 0.02f) {
    tail = frand() * expf(-(clapTime - 0.02f) / (0.120f * decayScale));
  }

  float out = (body + tail) * accentGain;
  out = clapBandpass.process(out);
  out = clapLowpass.process(out);
  out *= (clapEnv * clapAccentGain) * 0.8f;
  float res = applyAccentDistortion(out, clapAccentDistortion);
  return lofiEnabled ? lofi.process(res, CLAP) : res;
}

float TR808DrumSynthVoice::processCymbal() {
  if (!cymbalActive)
    return 0.0f;

  cymbalEnv *= 0.99945f;
  cymbalToneEnv *= 0.975f;
  if (cymbalEnv < 0.0003f) {
    cymbalActive = false;
    return 0.0f;
  }

  float n = frand();
  float alpha = 0.94f;
  cymbalHp = alpha * (cymbalHp + n - cymbalPrev);
  cymbalPrev = n;

  cymbalPhaseA += 5200.0f * invSampleRate;
  if (cymbalPhaseA >= 1.0f)
    cymbalPhaseA -= 1.0f;
  cymbalPhaseB += 7900.0f * invSampleRate;
  if (cymbalPhaseB >= 1.0f)
    cymbalPhaseB -= 1.0f;
  float tone =
    (sinf(2.0f * 3.14159265f * cymbalPhaseA) + sinf(2.0f * 3.14159265f * cymbalPhaseB)) *
    0.5f * cymbalToneEnv * cymbalBrightness;

  float out = cymbalHp * 0.6f + tone * 0.9f;
  out *= cymbalEnv * cymbalAccentGain;
  float res = applyAccentDistortion(out, cymbalAccentDistortion);
  return lofiEnabled ? lofi.process(res, CYMBAL) : res;
}

const Parameter& TR808DrumSynthVoice::parameter(DrumParamId id) const {
  return params[static_cast<int>(id)];
}

void TR808DrumSynthVoice::setParameter(DrumParamId id, float value) {
  params[static_cast<int>(id)].setValue(value);
}

TR909DrumSynthVoice::TR909DrumSynthVoice(float sampleRate)
  : sampleRate(sampleRate),
    invSampleRate(0.0f) {
  setSampleRate(sampleRate);
  reset();
}

void TR909DrumSynthVoice::reset() {
  kickPhase = 0.0f;
  kickFreq = 60.0f;
  kickEnvAmp = 0.0f;
  kickEnvPitch = 0.0f;
  kickActive = false;
  kickAccentGain = 1.0f;
  kickAccentDistortion = false;
  kickAmpDecay = 0.99925f;
  kickBaseFreq = 48.0f;
  kickClickEnv = 0.0f;

  snareEnvAmp = 0.0f;
  snareToneEnv = 0.0f;
  snareActive = false;
  snareBp = 0.0f;
  snareLp = 0.0f;
  snareTonePhase = 0.0f;
  snareTonePhase2 = 0.0f;
  snareAccentGain = 1.0f;
  snareToneGain = 1.0f;
  snareAccentDistortion = false;
  snareNoiseColor = 0.0f;

  hatEnvAmp = 0.0f;
  hatToneEnv = 0.0f;
  hatActive = false;
  hatHp = 0.0f;
  hatPrev = 0.0f;
  hatPhaseA = 0.0f;
  hatPhaseB = 0.0f;
  hatAccentGain = 1.0f;
  hatBrightness = 1.0f;
  hatAccentDistortion = false;

  openHatEnvAmp = 0.0f;
  openHatToneEnv = 0.0f;
  openHatActive = false;
  openHatHp = 0.0f;
  openHatPrev = 0.0f;
  openHatPhaseA = 0.0f;
  openHatPhaseB = 0.0f;
  openHatAccentGain = 1.0f;
  openHatBrightness = 1.0f;
  openHatAccentDistortion = false;

  midTomPhase = 0.0f;
  midTomEnv = 0.0f;
  midTomActive = false;
  midTomAccentGain = 1.0f;
  midTomAccentDistortion = false;

  highTomPhase = 0.0f;
  highTomEnv = 0.0f;
  highTomActive = false;
  highTomAccentGain = 1.0f;
  highTomAccentDistortion = false;

  rimPhase = 0.0f;
  rimEnv = 0.0f;
  rimActive = false;
  rimAccentGain = 1.0f;
  rimAccentDistortion = false;

  clapEnv = 0.0f;
  clapTrans = 0.0f;
  clapNoise = 0.0f;
  clapActive = false;
  clapDelay = 0.0f;
  clapTime = 0.0f;
  clapAccentGain = 1.0f;
  clapAccentDistortion = false;
  clapBandpass.reset();

  accentDistortion.setEnabled(true);
  accentDistortion.setDrive(2.2f);

  params[static_cast<int>(DrumParamId::MainVolume)] =
    Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);

  updateClapFilter();
}

void TR909DrumSynthVoice::setSampleRate(float sampleRateHz) {
  if (sampleRateHz <= 0.0f) sampleRateHz = 44100.0f;
  sampleRate = sampleRateHz;
  invSampleRate = 1.0f / sampleRate;
  updateClapFilter();
}

void TR909DrumSynthVoice::triggerKick(bool accent) {
  kickActive = true;
  kickPhase = 0.0f;
  kickEnvAmp = accent ? 1.35f : 1.15f;
  kickEnvPitch = 0.85f;
  kickFreq = 58.0f;
  kickAccentGain = accent ? 1.2f : 1.0f;
  kickAccentDistortion = accent;
  kickAmpDecay = accent ? 0.99935f : 0.99925f;
  kickBaseFreq = accent ? 46.0f : 48.0f;
  kickClickEnv = accent ? 1.0f : 0.85f;
}

void TR909DrumSynthVoice::triggerSnare(bool accent) {
  snareActive = true;
  snareEnvAmp = accent ? 1.25f : 1.0f;
  snareToneEnv = accent ? 1.25f : 1.0f;
  snareTonePhase = 0.0f;
  snareTonePhase2 = 0.0f;
  snareAccentGain = accent ? 1.15f : 1.0f;
  snareToneGain = accent ? 1.35f : 1.0f;
  snareAccentDistortion = accent;
  snareNoiseColor = 0.0f;
}

void TR909DrumSynthVoice::triggerHat(bool accent) {
  hatActive = true;
  hatEnvAmp = accent ? 0.6f : 0.42f;
  hatToneEnv = 1.0f;
  hatPhaseA = 0.0f;
  hatPhaseB = 0.33f;
  hatAccentGain = accent ? 1.35f : 1.0f;
  hatBrightness = accent ? 1.6f : 1.25f;
  hatAccentDistortion = accent;
  openHatEnvAmp *= 0.25f;
}

void TR909DrumSynthVoice::triggerOpenHat(bool accent) {
  openHatActive = true;
  openHatEnvAmp = accent ? 0.9995f : 0.95f;
  openHatToneEnv = 1.0f;
  openHatPhaseA = 0.0f;
  openHatPhaseB = 0.29f;
  openHatAccentGain = accent ? 1.25f : 1.0f;
  openHatBrightness = accent ? 1.35f : 1.1f;
  openHatAccentDistortion = accent;
}

void TR909DrumSynthVoice::triggerMidTom(bool accent) {
  midTomActive = true;
  midTomEnv = 1.0f;
  midTomPhase = 0.0f;
  midTomAccentGain = accent ? 1.3f : 1.0f;
  midTomAccentDistortion = accent;
}

void TR909DrumSynthVoice::triggerHighTom(bool accent) {
  highTomActive = true;
  highTomEnv = 1.0f;
  highTomPhase = 0.0f;
  highTomAccentGain = accent ? 1.3f : 1.0f;
  highTomAccentDistortion = accent;
}

void TR909DrumSynthVoice::triggerRim(bool accent) {
  rimActive = true;
  rimEnv = 1.0f;
  rimPhase = 0.0f;
  rimAccentGain = accent ? 1.35f : 1.0f;
  rimAccentDistortion = accent;
}

void TR909DrumSynthVoice::triggerClap(bool accent) {
  clapActive = true;
  clapEnv = 1.0f;
  clapTrans = 1.0f;
  clapNoise = frand();
  clapDelay = 0.0f;
  clapTime = 0.0f;
  clapAccentGain = accent ? 1.35f : 1.0f;
  clapAccentDistortion = accent;
  clapBandpass.reset();
}

void TR909DrumSynthVoice::triggerCymbal(bool accent) {
  cymbalActive = true;
  cymbalEnv = accent ? 0.95f : 0.75f;
  cymbalToneEnv = 1.0f;
  cymbalPhaseA = 0.0f;
  cymbalPhaseB = 0.27f;
  cymbalAccentGain = accent ? 1.3f : 1.0f;
  cymbalBrightness = accent ? 1.55f : 1.25f;
  cymbalAccentDistortion = accent;
}

float TR909DrumSynthVoice::frand() {
  return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

float TR909DrumSynthVoice::applyAccentDistortion(float input, bool accent) {
  if (!accent) {
    return input;
  }
  return accentDistortion.process(input);
}

void TR909DrumSynthVoice::updateClapFilter() {
  float freq = 1800.0f;
  float q = 1.0f;
  float w0 = 2.0f * 3.14159265f * freq * invSampleRate;
  float alpha = sinf(w0) / (2.0f * q);
  float cosw = cosf(w0);

  float b0 = alpha;
  float b1 = 0.0f;
  float b2 = -alpha;
  float a0 = 1.0f + alpha;
  float a1 = -2.0f * cosw;
  float a2 = 1.0f - alpha;

  clapBandpass.a0 = b0 / a0;
  clapBandpass.a1 = b1 / a0;
  clapBandpass.a2 = b2 / a0;
  clapBandpass.b1 = a1 / a0;
  clapBandpass.b2 = a2 / a0;
}

float TR909DrumSynthVoice::processKick() {
  if (!kickActive)
    return 0.0f;

  kickEnvAmp *= kickAmpDecay;
  kickEnvPitch *= 0.996f;
  kickClickEnv *= 0.94f;
  if (kickEnvAmp < 0.0008f) {
    kickActive = false;
    return 0.0f;
  }

  float pitchFactor = kickEnvPitch * kickEnvPitch;
  kickFreq = kickBaseFreq + 140.0f * pitchFactor;
  kickPhase += kickFreq * invSampleRate;
  if (kickPhase >= 1.0f)
    kickPhase -= 1.0f;

  float body = sinf(2.0f * 3.14159265f * kickPhase);
  float transient = sinf(2.0f * 3.14159265f * kickPhase * 4.0f) * pitchFactor * 0.2f;
  float click = (frand() * 0.4f + 0.6f) * kickClickEnv * 0.2f;
  float driven = tanhf(body * (2.4f + 0.7f * kickEnvAmp));

  float out = (driven * 0.9f + transient + click) * kickEnvAmp * kickAccentGain;
  float res = applyAccentDistortion(out, kickAccentDistortion);
  return lofiEnabled ? lofi.process(res, KICK) : res;
}

float TR909DrumSynthVoice::processSnare() {
  if (!snareActive)
    return 0.0f;

  snareEnvAmp *= 0.9976f;
  snareToneEnv *= 0.99965f;

  if (snareEnvAmp < 0.00025f) {
    snareActive = false;
    return 0.0f;
  }

  float n = frand();
  float f = 0.32f;
  snareBp += f * (n - snareLp - 0.18f * snareBp);
  snareLp += f * snareBp;

  float noiseHP = n - snareLp;
  snareNoiseColor = 0.92f * snareNoiseColor + 0.08f * noiseHP;
  float noiseOut = snareBp * 0.25f + snareNoiseColor * 0.75f;

  snareTonePhase += 330.0f * invSampleRate;
  if (snareTonePhase >= 1.0f) snareTonePhase -= 1.0f;
  snareTonePhase2 += 200.0f * invSampleRate;
  if (snareTonePhase2 >= 1.0f) snareTonePhase2 -= 1.0f;

  float toneA = sinf(2.0f * 3.14159265f * snareTonePhase);
  float toneB = sinf(2.0f * 3.14159265f * snareTonePhase2);
  float tone = (toneA * 0.6f + toneB * 0.4f) * snareToneEnv * snareToneGain;

  float out = (noiseOut * 0.6f + tone * 0.85f) * 1.25f;
  out *= snareEnvAmp * snareAccentGain;
  return applyAccentDistortion(out, snareAccentDistortion);
}

float TR909DrumSynthVoice::processHat() {
  if (!hatActive)
    return 0.0f;

  hatEnvAmp *= 0.996f;
  hatToneEnv *= 0.9f;
  if (hatEnvAmp < 0.00045f) {
    hatActive = false;
    return 0.0f;
  }

  float n = frand();
  float alpha = 0.95f;
  hatHp = alpha * (hatHp + n - hatPrev);
  hatPrev = n;

  hatPhaseA += 8000.0f * invSampleRate;
  if (hatPhaseA >= 1.0f)
    hatPhaseA -= 1.0f;
  hatPhaseB += 10400.0f * invSampleRate;
  if (hatPhaseB >= 1.0f)
    hatPhaseB -= 1.0f;
  float tone =
    (sinf(2.0f * 3.14159265f * hatPhaseA) + sinf(2.0f * 3.14159265f * hatPhaseB)) *
    0.5f * hatToneEnv * hatBrightness;

  float out = hatHp * 0.6f + tone * 0.85f;
  out *= hatEnvAmp * 0.55f * hatAccentGain;
  return applyAccentDistortion(out, hatAccentDistortion);
}

float TR909DrumSynthVoice::processOpenHat() {
  if (!openHatActive)
    return 0.0f;

  openHatEnvAmp *= 0.99955f;
  openHatToneEnv *= 0.93f;
  if (openHatEnvAmp < 0.00035f) {
    openHatActive = false;
    return 0.0f;
  }

  float n = frand();
  float alpha = 0.955f;
  openHatHp = alpha * (openHatHp + n - openHatPrev);
  openHatPrev = n;

  openHatPhaseA += 6200.0f * invSampleRate;
  if (openHatPhaseA >= 1.0f)
    openHatPhaseA -= 1.0f;
  openHatPhaseB += 8200.0f * invSampleRate;
  if (openHatPhaseB >= 1.0f)
    openHatPhaseB -= 1.0f;
  float tone =
    (sinf(2.0f * 3.14159265f * openHatPhaseA) + sinf(2.0f * 3.14159265f * openHatPhaseB)) *
    0.5f * openHatToneEnv * openHatBrightness;

  float out = openHatHp * 0.5f + tone * 1.05f;
  out *= openHatEnvAmp * 0.65f * openHatAccentGain;
  return applyAccentDistortion(out, openHatAccentDistortion);
}

float TR909DrumSynthVoice::processMidTom() {
  if (!midTomActive)
    return 0.0f;

  midTomEnv *= 0.9989f;
  if (midTomEnv < 0.0003f) {
    midTomActive = false;
    return 0.0f;
  }

  float freq = 200.0f;
  midTomPhase += freq * invSampleRate;
  if (midTomPhase >= 1.0f)
    midTomPhase -= 1.0f;

  float tone = sinf(2.0f * 3.14159265f * midTomPhase);
  float slightNoise = frand() * 0.03f;
  float out = (tone * 0.92f + slightNoise) * midTomEnv * 0.8f * midTomAccentGain;
  return applyAccentDistortion(out, midTomAccentDistortion);
}

float TR909DrumSynthVoice::processHighTom() {
  if (!highTomActive)
    return 0.0f;

  highTomEnv *= 0.9988f;
  if (highTomEnv < 0.0003f) {
    highTomActive = false;
    return 0.0f;
  }

  float freq = 280.0f;
  highTomPhase += freq * invSampleRate;
  if (highTomPhase >= 1.0f)
    highTomPhase -= 1.0f;

  float tone = sinf(2.0f * 3.14159265f * highTomPhase);
  float slightNoise = frand() * 0.025f;
  float out = (tone * 0.9f + slightNoise) * highTomEnv * 0.78f * highTomAccentGain;
  return applyAccentDistortion(out, highTomAccentDistortion);
}

float TR909DrumSynthVoice::processRim() {
  if (!rimActive)
    return 0.0f;

  rimEnv *= 0.9975f;
  if (rimEnv < 0.00035f) {
    rimActive = false;
    return 0.0f;
  }

  rimPhase += 1200.0f * invSampleRate;
  if (rimPhase >= 1.0f)
    rimPhase -= 1.0f;
  float tone = sinf(2.0f * 3.14159265f * rimPhase);
  float click = (frand() * 0.5f + 0.5f) * rimEnv;
  float out = (tone * 0.6f + click) * rimEnv * 0.85f * rimAccentGain;
  return applyAccentDistortion(out, rimAccentDistortion);
}

float TR909DrumSynthVoice::processClap() {
  if (!clapActive)
    return 0.0f;

  clapEnv *= 0.9988f;
  clapDelay += invSampleRate;
  clapTime += invSampleRate;
  if (clapEnv < 0.0002f) {
    clapActive = false;
    return 0.0f;
  }

  float bursts = 0.0f;
  const float burstSpacing = 0.006f;
  const float burstLength = 0.0008f;
  for (int i = 0; i < 6; ++i) {
    float start = i * burstSpacing;
    if (clapTime >= start && clapTime < start + burstLength) {
      float localT = (clapTime - start) / burstLength;
      bursts += frand() * (1.0f - localT);
    }
  }

  float tail = 0.0f;
  if (clapTime >= 0.02f) {
    float t = clapTime - 0.02f;
    float env = expf(-t * 18.0f);
    tail = frand() * env;
  }

  float out = clapBandpass.process(bursts + tail);
  out *= clapEnv * clapAccentGain;
  return applyAccentDistortion(out, clapAccentDistortion);
}

float TR909DrumSynthVoice::processCymbal() {
  if (!cymbalActive)
    return 0.0f;

  cymbalEnv *= 0.99935f;
  cymbalToneEnv *= 0.97f;
  if (cymbalEnv < 0.00025f) {
    cymbalActive = false;
    return 0.0f;
  }

  float n = frand();
  float alpha = 0.955f;
  cymbalHp = alpha * (cymbalHp + n - cymbalPrev);
  cymbalPrev = n;

  cymbalPhaseA += 6400.0f * invSampleRate;
  if (cymbalPhaseA >= 1.0f)
    cymbalPhaseA -= 1.0f;
  cymbalPhaseB += 9800.0f * invSampleRate;
  if (cymbalPhaseB >= 1.0f)
    cymbalPhaseB -= 1.0f;
  float tone =
    (sinf(2.0f * 3.14159265f * cymbalPhaseA) + sinf(2.0f * 3.14159265f * cymbalPhaseB)) *
    0.5f * cymbalToneEnv * cymbalBrightness;

  float out = cymbalHp * 0.55f + tone * 1.05f;
  out *= cymbalEnv * cymbalAccentGain;
  return applyAccentDistortion(out, cymbalAccentDistortion);
}

const Parameter& TR909DrumSynthVoice::parameter(DrumParamId id) const {
  return params[static_cast<int>(id)];
}

void TR909DrumSynthVoice::setParameter(DrumParamId id, float value) {
  params[static_cast<int>(id)].setValue(value);
}

TR606DrumSynthVoice::TR606DrumSynthVoice(float sampleRate)
  : sampleRate(sampleRate),
    invSampleRate(0.0f) {
  setSampleRate(sampleRate);
  reset();
}

void TR606DrumSynthVoice::reset() {
  kickPhase = 0.0f;
  kickAmpEnv = 0.0f;
  kickFmEnv = 0.0f;
  kickActive = false;
  kickAmpDecay = decayCoeff(0.180f);
  kickFmDecay = decayCoeff(0.012f);

  snareTonePhaseA = 0.0f;
  snareTonePhaseB = 0.0f;
  snareToneEnv = 0.0f;
  snareNoiseEnv = 0.0f;
  snareActive = false;
  snareToneDecay = decayCoeff(0.075f);
  snareNoiseDecay = decayCoeff(0.115f);
  snareNoiseLp.reset();
  snareNoiseLpCoeff = onePoleCoeff(2200.0f);

  midTomPhase = 0.0f;
  midTomAmpEnv = 0.0f;
  midTomFmEnv = 0.0f;
  midTomActive = false;
  midTomAmpDecay = decayCoeff(0.120f);
  midTomFmDecay = decayCoeff(0.010f);

  highTomPhase = 0.0f;
  highTomAmpEnv = 0.0f;
  highTomFmEnv = 0.0f;
  highTomActive = false;
  highTomAmpDecay = decayCoeff(0.095f);
  highTomFmDecay = decayCoeff(0.009f);

  hatEnv = 0.0f;
  openHatEnv = 0.0f;
  hatActive = false;
  openHatActive = false;
  hatDecay = decayCoeff(0.040f);
  openHatDecay = decayCoeff(0.280f);
  hatNoiseLp.reset();
  hatMetalLp.reset();
  hatNoiseLpCoeff = onePoleCoeff(8000.0f);
  hatMetalLpCoeff = onePoleCoeff(6000.0f);

  cymbalEnv = 0.0f;
  cymbalActive = false;
  cymbalDecay = decayCoeff(0.600f);
  cymbalBandpass.reset();

  accentEnv = 0.35f;
  accentDecay = decayCoeff(0.110f);

  for (int i = 0; i < 6; ++i) {
    metalPhases[i] = 0.0f;
  }
  metalSignal = 0.0f;

  params[static_cast<int>(DrumParamId::MainVolume)] =
    Parameter("vol", "", 0.0f, 1.0f, 0.8f, 1.0f / 128);

  updateHatFilters(accentEnv);
  updateCymbalFilter(accentEnv, cymbalBandpass);
}

void TR606DrumSynthVoice::setSampleRate(float sampleRateHz) {
  if (sampleRateHz <= 0.0f) sampleRateHz = 44100.0f;
  sampleRate = sampleRateHz;
  invSampleRate = 1.0f / sampleRate;
  kickAmpDecay = decayCoeff(0.180f);
  kickFmDecay = decayCoeff(0.012f);
  snareToneDecay = decayCoeff(0.075f);
  snareNoiseDecay = decayCoeff(0.115f);
  snareNoiseLpCoeff = onePoleCoeff(2200.0f);
  midTomAmpDecay = decayCoeff(0.120f);
  midTomFmDecay = decayCoeff(0.010f);
  highTomAmpDecay = decayCoeff(0.095f);
  highTomFmDecay = decayCoeff(0.009f);
  hatDecay = decayCoeff(0.040f);
  openHatDecay = decayCoeff(0.280f);
  hatNoiseLpCoeff = onePoleCoeff(8000.0f);
  hatMetalLpCoeff = onePoleCoeff(6000.0f);
  cymbalDecay = decayCoeff(0.600f);
  accentDecay = decayCoeff(0.110f);
  updateHatFilters(accentEnv);
  updateCymbalFilter(accentEnv, cymbalBandpass);
}

void TR606DrumSynthVoice::triggerKick(bool accent) {
  setAccent(accent);
  kickActive = true;
  kickPhase = 0.0f;
  kickAmpEnv = 1.0f + accentEnv * 0.7f;
  kickFmEnv = 1.0f + accentEnv * 0.4f;
}

void TR606DrumSynthVoice::triggerSnare(bool accent) {
  setAccent(accent);
  snareActive = true;
  snareToneEnv = 1.0f + accentEnv * 0.4f;
  snareNoiseEnv = 1.0f + accentEnv * 0.8f;
  snareTonePhaseA = 0.0f;
  snareTonePhaseB = 0.0f;
}

void TR606DrumSynthVoice::triggerHat(bool accent) {
  setAccent(accent);
  hatActive = true;
  hatEnv = 1.0f + accentEnv * 0.6f;
  updateHatFilters(accentEnv);
  openHatEnv *= 0.25f;
}

void TR606DrumSynthVoice::triggerOpenHat(bool accent) {
  setAccent(accent);
  openHatActive = true;
  openHatEnv = 1.0f + accentEnv * 0.6f;
  updateHatFilters(accentEnv);
}

void TR606DrumSynthVoice::triggerMidTom(bool accent) {
  setAccent(accent);
  midTomActive = true;
  midTomPhase = 0.0f;
  midTomAmpEnv = 1.0f + accentEnv * 0.5f;
  midTomFmEnv = 1.0f;
}

void TR606DrumSynthVoice::triggerHighTom(bool accent) {
  setAccent(accent);
  highTomActive = true;
  highTomPhase = 0.0f;
  highTomAmpEnv = 1.0f + accentEnv * 0.5f;
  highTomFmEnv = 1.0f;
}

void TR606DrumSynthVoice::triggerRim(bool accent) {
  triggerCymbal(accent);
}

void TR606DrumSynthVoice::triggerCymbal(bool accent) {
  setAccent(accent);
  cymbalActive = true;
  cymbalEnv = 1.0f + accentEnv * 0.5f;
  cymbalDecay = decayCoeff(0.600f * (1.0f + accentEnv * 0.5f));
  updateCymbalFilter(accentEnv, cymbalBandpass);
}

void TR606DrumSynthVoice::triggerClap(bool accent) {
  setAccent(accent);
}

float TR606DrumSynthVoice::processKick() {
  accentEnv *= accentDecay;
  updateMetalBank();

  if (!kickActive)
    return 0.0f;

  kickAmpEnv *= kickAmpDecay;
  kickFmEnv *= kickFmDecay;
  if (kickAmpEnv < 0.0003f) {
    kickActive = false;
    return 0.0f;
  }

  float baseFreq = 58.0f;
  float fmHz = 120.0f * kickFmEnv;
  kickPhase += (baseFreq + fmHz) * invSampleRate;
  if (kickPhase >= 1.0f) kickPhase -= 1.0f;

  float out = sinf(2.0f * 3.14159265f * kickPhase) * kickAmpEnv;
  return out;
}

float TR606DrumSynthVoice::processSnare() {
  if (!snareActive)
    return 0.0f;

  snareToneEnv *= snareToneDecay;
  snareNoiseEnv *= snareNoiseDecay;
  if (snareNoiseEnv < 0.0002f) {
    snareActive = false;
    return 0.0f;
  }

  snareTonePhaseA += 180.0f * invSampleRate;
  if (snareTonePhaseA >= 1.0f) snareTonePhaseA -= 1.0f;
  snareTonePhaseB += 330.0f * invSampleRate;
  if (snareTonePhaseB >= 1.0f) snareTonePhaseB -= 1.0f;

  float tone =
    (sinf(2.0f * 3.14159265f * snareTonePhaseA) +
     sinf(2.0f * 3.14159265f * snareTonePhaseB)) * 0.5f * snareToneEnv;

  float noise = frand();
  snareNoiseLp.a = snareNoiseLpCoeff;
  float noiseHp = noise - snareNoiseLp.process(noise);
  float noiseOut = noiseHp * snareNoiseEnv;

  return tone * 0.45f + noiseOut * 0.55f;
}

float TR606DrumSynthVoice::processHat() {
  if (!hatActive)
    return 0.0f;

  hatEnv *= hatDecay;
  if (hatEnv < 0.0002f) {
    hatActive = false;
    return 0.0f;
  }

  float noise = frand();
  hatNoiseLp.a = hatNoiseLpCoeff;
  float noiseHp = noise - hatNoiseLp.process(noise);
  hatMetalLp.a = hatMetalLpCoeff;
  float metalHp = metalSignal - hatMetalLp.process(metalSignal);

  float out = (noiseHp * 0.6f + metalHp * 0.4f) * hatEnv;
  return out;
}

float TR606DrumSynthVoice::processOpenHat() {
  if (!openHatActive)
    return 0.0f;

  openHatEnv *= openHatDecay;
  if (openHatEnv < 0.0002f) {
    openHatActive = false;
    return 0.0f;
  }

  float noise = frand();
  hatNoiseLp.a = hatNoiseLpCoeff;
  float noiseHp = noise - hatNoiseLp.process(noise);
  hatMetalLp.a = hatMetalLpCoeff;
  float metalHp = metalSignal - hatMetalLp.process(metalSignal);

  float out = (noiseHp * 0.6f + metalHp * 0.4f) * openHatEnv;
  return out;
}

float TR606DrumSynthVoice::processMidTom() {
  if (!midTomActive)
    return 0.0f;

  midTomAmpEnv *= midTomAmpDecay;
  midTomFmEnv *= midTomFmDecay;
  if (midTomAmpEnv < 0.0002f) {
    midTomActive = false;
    return 0.0f;
  }

  float baseFreq = 110.0f * (1.0f + accentEnv * 0.07f);
  float fmHz = 60.0f * midTomFmEnv;
  midTomPhase += (baseFreq + fmHz) * invSampleRate;
  if (midTomPhase >= 1.0f) midTomPhase -= 1.0f;

  return sinf(2.0f * 3.14159265f * midTomPhase) * midTomAmpEnv;
}

float TR606DrumSynthVoice::processHighTom() {
  if (!highTomActive)
    return 0.0f;

  highTomAmpEnv *= highTomAmpDecay;
  highTomFmEnv *= highTomFmDecay;
  if (highTomAmpEnv < 0.0002f) {
    highTomActive = false;
    return 0.0f;
  }

  float baseFreq = 170.0f * (1.0f + accentEnv * 0.07f);
  float fmHz = 70.0f * highTomFmEnv;
  highTomPhase += (baseFreq + fmHz) * invSampleRate;
  if (highTomPhase >= 1.0f) highTomPhase -= 1.0f;

  return sinf(2.0f * 3.14159265f * highTomPhase) * highTomAmpEnv;
}

float TR606DrumSynthVoice::processRim() {
  return processCymbal();
}

float TR606DrumSynthVoice::processCymbal() {
  if (!cymbalActive)
    return 0.0f;

  cymbalEnv *= cymbalDecay;
  if (cymbalEnv < 0.0002f) {
    cymbalActive = false;
    return 0.0f;
  }

  float clipped = tanhf(metalSignal * 2.2f);
  float out = cymbalBandpass.process(clipped) * cymbalEnv;
  return out;
}

float TR606DrumSynthVoice::processClap() {
  return 0.0f;
}

const Parameter& TR606DrumSynthVoice::parameter(DrumParamId id) const {
  return params[static_cast<int>(id)];
}

void TR606DrumSynthVoice::setParameter(DrumParamId id, float value) {
  params[static_cast<int>(id)].setValue(value);
}

float TR606DrumSynthVoice::frand() {
  return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

float TR606DrumSynthVoice::decayCoeff(float timeSeconds) const {
  return expf(-1.0f / (timeSeconds * sampleRate));
}

float TR606DrumSynthVoice::onePoleCoeff(float cutoffHz) const {
  float omega = 2.0f * 3.14159265f * cutoffHz * invSampleRate;
  return 1.0f - expf(-omega);
}

float TR606DrumSynthVoice::square(float phase) const {
  return phase < 0.5f ? 1.0f : -1.0f;
}

void TR606DrumSynthVoice::setAccent(bool accent) {
  accentEnv = accent ? 1.0f : 0.35f;
}

void TR606DrumSynthVoice::updateMetalBank() {
  float sum = 0.0f;
  for (int i = 0; i < 6; ++i) {
    metalPhases[i] += metalFreqs[i] * invSampleRate;
    if (metalPhases[i] >= 1.0f)
      metalPhases[i] -= 1.0f;
    sum += square(metalPhases[i]);
  }
  metalSignal = sum / 6.0f;
}

void TR606DrumSynthVoice::updateHatFilters(float accent) {
  float noiseCutoff = 8000.0f * (1.0f + 0.4f * accent);
  float metalCutoff = 6000.0f * (1.0f + 0.4f * accent);
  hatNoiseLpCoeff = onePoleCoeff(noiseCutoff);
  hatMetalLpCoeff = onePoleCoeff(metalCutoff);
}

void TR606DrumSynthVoice::updateCymbalFilter(float accent, Biquad& filter) {
  float freq = 8000.0f * (1.0f + 0.2f * accent);
  float q = 0.9f;
  float w0 = 2.0f * 3.14159265f * freq * invSampleRate;
  float alpha = sinf(w0) / (2.0f * q);
  float cosw = cosf(w0);

  float b0 = alpha;
  float b1 = 0.0f;
  float b2 = -alpha;
  float a0 = 1.0f + alpha;
  float a1 = -2.0f * cosw;
  float a2 = 1.0f - alpha;

  filter.a0 = b0 / a0;
  filter.a1 = b1 / a0;
  filter.a2 = b2 / a0;
  filter.b1 = a1 / a0;
  filter.b2 = a2 / a0;
}
