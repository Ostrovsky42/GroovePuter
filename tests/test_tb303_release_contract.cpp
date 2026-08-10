#include <cassert>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <vector>

#include "../src/dsp/mini_tb303.h"
#include "../src/dsp/tube_distortion.h"

namespace {
constexpr float kSampleRate = 22050.0f;

float renderRms(TB303Voice& voice, int sampleCount) {
  double energy = 0.0;
  for (int i = 0; i < sampleCount; ++i) {
    const float sample = voice.process();
    assert(std::isfinite(sample));
    energy += static_cast<double>(sample) * static_cast<double>(sample);
  }
  return static_cast<float>(std::sqrt(energy / static_cast<double>(sampleCount)));
}

float rmsAtVolume(float volume) {
  TB303Voice voice(kSampleRate);
  voice.setParameter(TB303ParamId::MainVolume, volume);
  voice.startNote(110.0f, false, false, 100);
  renderRms(voice, 256);
  return renderRms(voice, 1024);
}

void testAmplitudeLifecycle() {
  TB303Voice voice(kSampleRate);
  voice.startNote(110.0f, false, false, 100);

  const float first = voice.process();
  assert(std::isfinite(first));
  assert(std::fabs(first) < 0.02f);
  assert(voice.isVoiceActive());

  const float sustainRms = renderRms(voice, 4096);
  assert(sustainRms > 0.0001f);

  voice.release();
  float previousEnvelope = voice.amplitudeEnvelope();
  const int releaseLimit = static_cast<int>(kSampleRate * 0.30f);
  for (int i = 0; i < releaseLimit && voice.isVoiceActive(); ++i) {
    const float sample = voice.process();
    assert(std::isfinite(sample));
    const float envelope = voice.amplitudeEnvelope();
    assert(envelope <= previousEnvelope + 0.000001f);
    previousEnvelope = envelope;
  }
  assert(!voice.isVoiceActive());
  assert(voice.amplitudeEnvelope() == 0.0f);
  assert(std::fabs(voice.process()) < 0.000001f);
}

void testSlideAndRetrigger() {
  TB303Voice voice(kSampleRate);
  voice.startNote(110.0f, false, false, 100);
  renderRms(voice, 1024);
  const float beforeSlide = voice.amplitudeEnvelope();

  voice.startNote(146.832f, false, true, 100);
  const float afterSlide = voice.amplitudeEnvelope();
  assert(std::fabs(afterSlide - beforeSlide) < 0.000001f);
  assert(std::isfinite(voice.process()));

  voice.startNote(164.814f, true, false, 100);
  assert(voice.amplitudeEnvelope() == 0.0f);
  assert(std::fabs(voice.process()) < 0.02f);

  voice.reset();
  assert(!voice.isVoiceActive());
  assert(voice.amplitudeEnvelope() == 0.0f);
}

void testVolumeAndSubOwnership() {
  const float silent = rmsAtVolume(0.0f);
  const float low = rmsAtVolume(0.25f);
  const float medium = rmsAtVolume(0.50f);
  const float high = rmsAtVolume(1.0f);
  assert(silent < 0.000001f);
  assert(low > silent);
  assert(medium > low);
  assert(high > medium);
  assert(std::isfinite(high));

  TB303Voice withoutSub(kSampleRate);
  withoutSub.setSubOscillator(false);
  withoutSub.startNote(110.0f, false, false, 100);
  renderRms(withoutSub, 512);
  const float dryRms = renderRms(withoutSub, 2048);

  TB303Voice withSub(kSampleRate);
  withSub.setSubOscillator(true);
  withSub.startNote(110.0f, false, false, 100);
  renderRms(withSub, 512);
  const float subRms = renderRms(withSub, 2048);

  assert(std::fabs(subRms - dryRms) > 0.000001f);
  assert(subRms < dryRms * 2.0f + 0.000001f);
}

void testDistortionDriveRestoration() {
  TubeDistortion first;
  first.setDrive(0.1f);
  first.setEnabled(true);
  assert(std::fabs(first.drive() - 8.0f) < 0.000001f);
  const float enabledOutput = first.process(0.1f);
  assert(std::isfinite(enabledOutput));
  assert(std::fabs(enabledOutput) > 0.01f);

  first.setDrive(3.0f);
  first.setEnabled(true);
  assert(std::fabs(first.drive() - 3.0f) < 0.000001f);
  first.setEnabled(false);
  assert(std::fabs(first.drive() - 3.0f) < 0.000001f);
  first.setEnabled(true);
  assert(std::fabs(first.drive() - 3.0f) < 0.000001f);

  TubeDistortion second;
  second.setDrive(5.0f);
  second.setEnabled(true);
  assert(std::fabs(second.drive() - 5.0f) < 0.000001f);
  assert(std::fabs(first.drive() - 3.0f) < 0.000001f);
}
}  // namespace

int main() {
  testAmplitudeLifecycle();
  testSlideAndRetrigger();
  testVolumeAndSubOwnership();
  testDistortionDriveRestoration();
  std::cout << "TB303 release and distortion contracts passed\n";
  return 0;
}
