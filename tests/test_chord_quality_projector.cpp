#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/tonal/chord_quality_projector.h"

using namespace GroovePuterRhythm;

namespace {

ChordQualityProjectionRequest baseRequest() {
  ChordQualityProjectionRequest request{};
  request.event = HarmonicEvent{0, ChordQuality::Triad, 0};
  request.triadPolarity = TriadPolarity::Major;
  request.rootPitchClass = 0;
  request.scaleTypeValue = kScaleMajor;
  request.minMidi = 36;
  request.maxMidi = 84;
  return request;
}

uint8_t normalizePitchClass(int value) {
  value %= 12;
  if (value < 0) value += 12;
  return static_cast<uint8_t>(value);
}

void assertNotes(const ChordQualityProjectionResult& result,
                 const uint8_t* expected,
                 uint8_t count) {
  assert(result.status == ChordQualityProjectionStatus::Ok);
  assert(result.projectionStatus == TonalProjectionStatus::Ok);
  assert(result.plan.toneCount == count);
  for (uint8_t index = 0; index < count; ++index)
    assert(result.plan.midiNotes[index] == expected[index]);
}

void testSameRootMajorVsMinorIsAudiblyDifferent() {
  ChordQualityProjectionRequest major = baseRequest();
  const ChordQualityProjectionResult majorResult =
      projectChordQualityPitchSet(major);
  const uint8_t expectedMajor[] = {60, 64, 67};
  assertNotes(majorResult, expectedMajor, 3);

  ChordQualityProjectionRequest minor = major;
  minor.triadPolarity = TriadPolarity::Minor;
  const ChordQualityProjectionResult minorResult =
      projectChordQualityPitchSet(minor);
  const uint8_t expectedMinor[] = {60, 63, 67};
  assertNotes(minorResult, expectedMinor, 3);

  assert(majorResult.plan.rootAnchorMidi == minorResult.plan.rootAnchorMidi);
  assert(majorResult.plan.midiNotes[0] == minorResult.plan.midiNotes[0]);
  assert(majorResult.plan.midiNotes[1] != minorResult.plan.midiNotes[1]);
  assert(majorResult.plan.midiNotes[2] == minorResult.plan.midiNotes[2]);
}

void testTriadPolarityIsExplicitNotInferredFromScale() {
  ChordQualityProjectionRequest request = baseRequest();
  request.scaleTypeValue = kScaleMinor;
  request.triadPolarity = TriadPolarity::Major;
  const ChordQualityProjectionResult majorOnMinorScale =
      projectChordQualityPitchSet(request);
  assert(majorOnMinorScale.status == ChordQualityProjectionStatus::Ok);
  assert(majorOnMinorScale.plan.midiNotes[1] -
             majorOnMinorScale.plan.midiNotes[0] == 4);

  request.scaleTypeValue = kScaleMajor;
  request.triadPolarity = TriadPolarity::Minor;
  const ChordQualityProjectionResult minorOnMajorScale =
      projectChordQualityPitchSet(request);
  assert(minorOnMajorScale.status == ChordQualityProjectionStatus::Ok);
  assert(minorOnMajorScale.plan.midiNotes[1] -
             minorOnMajorScale.plan.midiNotes[0] == 3);
}

void testSeventhQualitiesDifferWithoutChangingRoot() {
  ChordQualityProjectionRequest request = baseRequest();
  request.event.quality = ChordQuality::Major7;
  const ChordQualityProjectionResult major7 =
      projectChordQualityPitchSet(request);
  const uint8_t expectedMajor7[] = {60, 64, 67, 71};
  assertNotes(major7, expectedMajor7, 4);

  request.event.quality = ChordQuality::Dominant7;
  const ChordQualityProjectionResult dominant7 =
      projectChordQualityPitchSet(request);
  const uint8_t expectedDominant7[] = {60, 64, 67, 70};
  assertNotes(dominant7, expectedDominant7, 4);

  request.event.quality = ChordQuality::Minor7;
  const ChordQualityProjectionResult minor7 =
      projectChordQualityPitchSet(request);
  const uint8_t expectedMinor7[] = {60, 63, 67, 70};
  assertNotes(minor7, expectedMinor7, 4);
}

void testNinthQualitySignatureStaysBounded() {
  ChordQualityProjectionRequest request = baseRequest();
  request.event.quality = ChordQuality::Minor9;
  const ChordQualityProjectionResult minor9 =
      projectChordQualityPitchSet(request);
  const uint8_t expectedMinor9[] = {60, 63, 70, 74};
  assertNotes(minor9, expectedMinor9, 4);

  request.event.quality = ChordQuality::Major9;
  const ChordQualityProjectionResult major9 =
      projectChordQualityPitchSet(request);
  const uint8_t expectedMajor9[] = {60, 64, 71, 74};
  assertNotes(major9, expectedMajor9, 4);
}

void testAllCurrentQualitiesAreBoundedAndRooted() {
  for (uint8_t raw = 0;
       raw < static_cast<uint8_t>(ChordQuality::Count); ++raw) {
    ChordQualityProjectionRequest request = baseRequest();
    request.event.quality = static_cast<ChordQuality>(raw);
    const ChordQualityProjectionResult result =
        projectChordQualityPitchSet(request);
    assert(result.status == ChordQualityProjectionStatus::Ok);
    assert(result.plan.toneCount >= 3);
    assert(result.plan.toneCount <= kMaxChordQualityTones);
    assert(result.plan.midiNotes[0] == result.plan.rootAnchorMidi);
    for (uint8_t index = 1; index < result.plan.toneCount; ++index)
      assert(result.plan.midiNotes[index] > result.plan.midiNotes[index - 1]);
  }
}

void testGlobalRootScaleAndChromaticOffsetProjectTogether() {
  ChordQualityProjectionRequest request = baseRequest();
  request.rootPitchClass = 2;  // D
  request.scaleTypeValue = kScaleMajor;
  request.event = HarmonicEvent{3, ChordQuality::Triad, 1};  // IV + 1 semitone
  const ChordQualityProjectionResult result =
      projectChordQualityPitchSet(request);
  const uint8_t expected[] = {56, 60, 63};  // G#3 major
  assertNotes(result, expected, 3);

  request.rootPitchClass = 3;
  const ChordQualityProjectionResult transposed =
      projectChordQualityPitchSet(request);
  assert(transposed.status == ChordQualityProjectionStatus::Ok);
  assert(transposed.plan.toneCount == result.plan.toneCount);
  for (uint8_t index = 0; index < result.plan.toneCount; ++index)
    assert(transposed.plan.midiNotes[index] == result.plan.midiNotes[index] + 1);
}

void testAllScaleTypesPreserveQualityIntervals() {
  for (ScaleTypeValue scale = 0; scale < kScaleTypeCount; ++scale) {
    for (uint8_t degree = 0; degree <= 6; ++degree) {
      ChordQualityProjectionRequest request = baseRequest();
      request.rootPitchClass = 4;  // E
      request.scaleTypeValue = scale;
      request.event = HarmonicEvent{degree, ChordQuality::Dominant7, -1};
      request.minMidi = 24;
      request.maxMidi = 108;

      const ChordQualityProjectionResult result =
          projectChordQualityPitchSet(request);
      assert(result.status == ChordQualityProjectionStatus::Ok);
      assert(result.plan.toneCount == 4);
      assert(result.plan.midiNotes[1] - result.plan.midiNotes[0] == 4);
      assert(result.plan.midiNotes[2] - result.plan.midiNotes[0] == 7);
      assert(result.plan.midiNotes[3] - result.plan.midiNotes[0] == 10);

      const uint8_t expectedRootPitchClass = normalizePitchClass(
          static_cast<int>(request.rootPitchClass) +
          scaleDegreeToSemitone(scale, degree) - 1);
      assert(result.plan.rootAnchorMidi % 12 == expectedRootPitchClass);
    }
  }
}

void testRegisterFailureIsAtomic() {
  ChordQualityProjectionRequest request = baseRequest();
  request.minMidi = 60;
  request.maxMidi = 65;
  const ChordQualityProjectionResult result =
      projectChordQualityPitchSet(request);
  assert(result.status == ChordQualityProjectionStatus::ProjectionFailed);
  assert(result.projectionStatus == TonalProjectionStatus::NoteOutOfRegister);
  assert(result.plan.toneCount == 0);
  for (uint8_t note : result.plan.midiNotes) assert(note == 0);
}

void testInvalidRequestsFailClosed() {
  ChordQualityProjectionRequest request = baseRequest();
  request.event.degree = 7;
  assert(projectChordQualityPitchSet(request).status ==
         ChordQualityProjectionStatus::InvalidRequest);

  request = baseRequest();
  request.event.rootOffsetSemitones = 3;
  assert(projectChordQualityPitchSet(request).status ==
         ChordQualityProjectionStatus::InvalidRequest);

  request = baseRequest();
  request.triadPolarity = TriadPolarity::Count;
  assert(projectChordQualityPitchSet(request).status ==
         ChordQualityProjectionStatus::InvalidRequest);

  request = baseRequest();
  request.rootPitchClass = 12;
  assert(projectChordQualityPitchSet(request).status ==
         ChordQualityProjectionStatus::InvalidRequest);
}

}  // namespace

int main() {
  testSameRootMajorVsMinorIsAudiblyDifferent();
  testTriadPolarityIsExplicitNotInferredFromScale();
  testSeventhQualitiesDifferWithoutChangingRoot();
  testNinthQualitySignatureStaysBounded();
  testAllCurrentQualitiesAreBoundedAndRooted();
  testGlobalRootScaleAndChromaticOffsetProjectTogether();
  testAllScaleTypesPreserveQualityIntervals();
  testRegisterFailureIsAtomic();
  testInvalidRequestsFailClosed();
  std::cout << "P1 audible chord quality projection host matrix: OK\n";
  return 0;
}
