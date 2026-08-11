#include "chord_quality_projector.h"

namespace GroovePuterRhythm {
namespace {

struct IntervalSet {
  uint8_t count = 0;
  int8_t semitones[kMaxChordQualityTones]{};
};

bool validEvent(const HarmonicEvent& event) {
  return event.degree <= 6 &&
         static_cast<uint8_t>(event.quality) <
             static_cast<uint8_t>(ChordQuality::Count) &&
         event.rootOffsetSemitones >= -kMaxRootOffsetSemitones &&
         event.rootOffsetSemitones <= kMaxRootOffsetSemitones;
}

bool validTriadPolarity(TriadPolarity polarity) {
  return static_cast<uint8_t>(polarity) <
         static_cast<uint8_t>(TriadPolarity::Count);
}

uint16_t ordinalMask(uint8_t count) {
  if (count == 0) return 0;
  return static_cast<uint16_t>((1u << count) - 1u);
}

uint8_t normalizePitchClass(int value) {
  value %= 12;
  if (value < 0) value += 12;
  return static_cast<uint8_t>(value);
}

bool qualityIntervals(ChordQuality quality,
                      TriadPolarity triadPolarity,
                      IntervalSet& set) {
  switch (quality) {
    case ChordQuality::Triad:
      set.count = 3;
      set.semitones[0] = 0;
      set.semitones[1] =
          triadPolarity == TriadPolarity::Minor ? 3 : 4;
      set.semitones[2] = 7;
      return true;
    case ChordQuality::Minor7:
      set.count = 4;
      set.semitones[0] = 0;
      set.semitones[1] = 3;
      set.semitones[2] = 7;
      set.semitones[3] = 10;
      return true;
    case ChordQuality::Major7:
      set.count = 4;
      set.semitones[0] = 0;
      set.semitones[1] = 4;
      set.semitones[2] = 7;
      set.semitones[3] = 11;
      return true;
    case ChordQuality::Dominant7:
      set.count = 4;
      set.semitones[0] = 0;
      set.semitones[1] = 4;
      set.semitones[2] = 7;
      set.semitones[3] = 10;
      return true;
    case ChordQuality::Sus4:
      set.count = 3;
      set.semitones[0] = 0;
      set.semitones[1] = 5;
      set.semitones[2] = 7;
      return true;
    case ChordQuality::Minor9:
      // Fixed quality-signature set, not a general voicing engine. The fifth
      // is omitted so root/minor-third/minor-seventh/ninth fit the 4-tone cap.
      set.count = 4;
      set.semitones[0] = 0;
      set.semitones[1] = 3;
      set.semitones[2] = 10;
      set.semitones[3] = 14;
      return true;
    case ChordQuality::Major9:
      // Same bounded rule: root/major-third/major-seventh/ninth.
      set.count = 4;
      set.semitones[0] = 0;
      set.semitones[1] = 4;
      set.semitones[2] = 11;
      set.semitones[3] = 14;
      return true;
    case ChordQuality::Diminished:
      set.count = 3;
      set.semitones[0] = 0;
      set.semitones[1] = 3;
      set.semitones[2] = 6;
      return true;
    case ChordQuality::Count:
      return false;
  }
  return false;
}

}  // namespace

ChordQualityProjectionResult projectChordQualityPitchSet(
    const ChordQualityProjectionRequest& request) {
  ChordQualityProjectionResult result{};
  if (!validEvent(request.event) ||
      !validTriadPolarity(request.triadPolarity) ||
      request.rootPitchClass > 11 ||
      !isValidScaleTypeValue(request.scaleTypeValue) ||
      request.minMidi > request.maxMidi || request.maxMidi > 127) {
    return result;
  }

  IntervalSet intervals{};
  if (!qualityIntervals(request.event.quality, request.triadPolarity,
                        intervals)) {
    result.status = ChordQualityProjectionStatus::UnsupportedQuality;
    return result;
  }

  const int eventSemitone =
      scaleDegreeToSemitone(request.scaleTypeValue, request.event.degree) +
      static_cast<int>(request.event.rootOffsetSemitones);

  TonalProjectionRequest projection{};
  projection.onsetCount = intervals.count;
  projection.rootPitchClass = normalizePitchClass(
      static_cast<int>(request.rootPitchClass) + eventSemitone);
  projection.scaleTypeValue = request.scaleTypeValue;
  projection.minMidi = request.minMidi;
  projection.maxMidi = request.maxMidi;
  projection.maxAdjacentLeapSemitones = 127;
  projection.semitoneOffsetOrdinals = ordinalMask(intervals.count);
  for (uint8_t index = 0; index < intervals.count; ++index)
    projection.tonalOffsets[index] = intervals.semitones[index];

  const TonalProjectionResult projected = projectTonalIntent(projection);
  result.projectionStatus = projected.status;
  if (projected.status != TonalProjectionStatus::Ok ||
      projected.noteCount != intervals.count) {
    result.status = ChordQualityProjectionStatus::ProjectionFailed;
    return result;
  }

  result.plan.toneCount = projected.noteCount;
  result.plan.rootAnchorMidi = projected.rootAnchorMidi;
  for (uint8_t index = 0; index < projected.noteCount; ++index)
    result.plan.midiNotes[index] = projected.midiNotes[index];
  result.status = ChordQualityProjectionStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm

// Linker-only P1 resource probe. It is not declared in a public header and is
// dead-stripped from normal product builds. The feasibility workflow can force
// this symbol with -u to measure the actual ESP32-S3 linked code delta without
// wiring chord quality into live generation or creating runtime side effects.
extern "C" uint8_t grooveputerP1ChordQualityProbe() {
  GroovePuterRhythm::ChordQualityProjectionRequest request{};
  request.event = GroovePuterRhythm::HarmonicEvent{
      0, GroovePuterRhythm::ChordQuality::Major9, 0};
  request.triadPolarity = GroovePuterRhythm::TriadPolarity::Major;
  request.rootPitchClass = 0;
  request.scaleTypeValue = GroovePuterRhythm::kScaleMajor;
  request.minMidi = 36;
  request.maxMidi = 84;
  const GroovePuterRhythm::ChordQualityProjectionResult result =
      GroovePuterRhythm::projectChordQualityPitchSet(request);
  return result.status == GroovePuterRhythm::ChordQualityProjectionStatus::Ok
      ? result.plan.toneCount
      : 0;
}
