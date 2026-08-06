#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>

#include "src/dsp/chip_tuning.h"
#include "src/dsp/clamped_live_note_identity.h"

namespace {

float midiToHz(int note) {
  return 440.0f * std::pow(2.0f, static_cast<float>(note - 69) / 12.0f);
}

float centsBetween(float actualHz, float expectedHz) {
  return 1200.0f * std::log2(actualHz / expectedHz);
}

float pitchClassErrorCents(float actualHz, float expectedHz) {
  const float raw = centsBetween(actualHz, expectedHz);
  return raw - std::round(raw / 1200.0f) * 1200.0f;
}

void testAyChromaticRange() {
  float previous = 0.0f;
  for (int note = 24; note <= 71; ++note) {
    const float expected = midiToHz(note);
    const float actual = ChipTuning::quantizeAyToneFrequency(expected);
    assert(std::fabs(centsBetween(actual, expected)) <= 5.0f);
    assert(actual > previous);
    previous = actual;
  }

  previous = 0.0f;
  for (int note = 63; note <= 67; ++note) {
    const float actual = ChipTuning::quantizeAyToneFrequency(midiToHz(note));
    assert(actual > previous);
    previous = actual;
  }
}

void testSnLowRegisterPolicy() {
  constexpr std::array<int, 5> kNotes = {24, 33, 36, 42, 45};
  std::array<float, kNotes.size()> actual{};

  for (std::size_t i = 0; i < kNotes.size(); ++i) {
    const float expected = midiToHz(kNotes[i]);
    actual[i] = ChipTuning::quantizeSnToneFrequency(expected);
    assert(actual[i] >= ChipTuning::kSnMinimumToneHz - 0.01f);
    assert(std::fabs(pitchClassErrorCents(actual[i], expected)) <= 5.0f);
  }

  assert(std::fabs(actual[0] - actual[1]) > 1.0f);
  assert(std::fabs(actual[1] - actual[3]) > 1.0f);
  assert(std::fabs(actual[3] - actual[4]) > 1.0f);

  float ratios[3]{};
  ChipTuning::snStackRatios(1, ratios);
  assert(ratios[0] == 1.0f);
  assert(ratios[1] == 2.0f);
  assert(ratios[2] == 4.0f);

  ChipTuning::snStackRatios(2, ratios);
  assert(ratios[0] == 1.0f);
  assert(ratios[1] == 1.5f);
  assert(ratios[2] == 2.0f);
}

void testSnStackFoldsRootOnce() {
  float frequencies[3]{};
  const float requestedRoot = midiToHz(24);
  const float playableRoot =
      ChipTuning::quantizeSnToneFrequency(requestedRoot);

  ChipTuning::snStackFrequencies(requestedRoot, 1, frequencies);

  assert(frequencies[0] < frequencies[1]);
  assert(frequencies[1] < frequencies[2]);
  assert(std::fabs(centsBetween(frequencies[0], playableRoot)) <= 0.01f);
  assert(std::fabs(centsBetween(frequencies[1], playableRoot * 2.0f)) <= 5.0f);
  assert(std::fabs(centsBetween(frequencies[2], playableRoot * 4.0f)) <= 5.0f);
}

void testClampedLiveNoteIdentity() {
  ClampedLiveNoteIdentity note;

  note = ClampedLiveNoteIdentity::normalize(84);
  assert(static_cast<int16_t>(note) == 71);
  assert(note == 84);
  assert(!(note != 84));

  note = ClampedLiveNoteIdentity::normalize(12);
  assert(static_cast<int16_t>(note) == 24);
  assert(note == 12);
  assert(!(note != 12));

  note = -1;
  assert(note != 84);
}

}  // namespace

int main() {
  testAyChromaticRange();
  testSnLowRegisterPolicy();
  testSnStackFoldsRootOnce();
  testClampedLiveNoteIdentity();
  std::puts("chip tuning and live-note identity: OK");
  return 0;
}
