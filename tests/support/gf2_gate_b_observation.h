#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include "../../scenes.h"
#include "../../src/dsp/mini_drumvoices.h"
#include "../../src/generation/migration/strong_rhythm_migration.h"

namespace GroovePuterRhythm {
namespace GF2GateB {

struct NeutralMaterialObservation {
  uint16_t kickOnsets = 0;
  uint16_t backbeatOnsets = 0;
  uint16_t hatOnsets = 0;
  uint16_t supportOnsets = 0;
  uint16_t kickAccents = 0;
  uint16_t backbeatAccents = 0;
  uint16_t hatAccents = 0;
  uint16_t supportAccents = 0;
  uint16_t synthAOnsets = 0;
  uint16_t synthBOnsets = 0;
  uint16_t synthAAccents = 0;
  uint16_t synthBAccents = 0;
  uint16_t synthAGhosts = 0;
  uint16_t synthBGhosts = 0;
  uint16_t harmonicEventOnsets = 0;
  uint16_t chordOnsets = 0;
  uint16_t melodicFillOnsets = 0;
  uint16_t silenceMask = 0;
  uint16_t physicalEventCount = 0;
  uint8_t harmonicEventCount = 0;
  bool chordApplied = false;
  bool melodicApplied = false;
  SemanticSynthBRole synthBRole = SemanticSynthBRole::Count;
  std::string drumTiming;
  std::string synthATiming;
  std::string synthBTiming;
  std::string synthAPitchClass;
  std::string synthBPitchClass;
  std::string synthAContour;
  std::string synthBContour;
};

inline uint16_t physicalStepBit(int step) {
  return step >= 0 && step < static_cast<int>(kStepsPerBar)
      ? static_cast<uint16_t>(1u << (kStepsPerBar - 1u - static_cast<unsigned>(step)))
      : 0u;
}

inline int drumRoleBucket(int voice) {
  if (voice == static_cast<int>(KICK)) return 0;
  if (voice == static_cast<int>(SNARE) || voice == static_cast<int>(CLAP)) return 1;
  if (voice == static_cast<int>(CLOSED_HAT) || voice == static_cast<int>(OPEN_HAT)) return 2;
  return 3;
}

inline void appendTuple(std::ostringstream& stream, bool& first,
                        int step, int value) {
  if (!first) stream << ',';
  stream << step << ':' << value;
  first = false;
}

inline int positivePitchClass(int note, uint8_t rootPitchClass) {
  int value = (note - static_cast<int>(rootPitchClass)) % 12;
  if (value < 0) value += 12;
  return value;
}

inline void observeSynth(const SynthPattern& pattern,
                         uint8_t rootPitchClass,
                         uint16_t& onsetMask,
                         uint16_t& accentMask,
                         uint16_t& ghostMask,
                         std::string& timing,
                         std::string& pitchClass,
                         std::string& contour,
                         uint16_t& eventCount) {
  std::ostringstream timingStream;
  std::ostringstream pitchStream;
  std::ostringstream contourStream;
  bool firstTiming = true;
  bool firstPitch = true;
  bool firstContour = true;
  int previousNote = 0;
  bool havePreviousNote = false;

  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    const SynthStep& event = pattern.steps[step];
    if (event.note < 0) continue;
    const uint16_t bit = physicalStepBit(step);
    onsetMask = static_cast<uint16_t>(onsetMask | bit);
    if (event.accent) accentMask = static_cast<uint16_t>(accentMask | bit);
    if (event.ghost) ghostMask = static_cast<uint16_t>(ghostMask | bit);
    ++eventCount;

    appendTuple(timingStream, firstTiming, step, static_cast<int>(event.timing));
    if (!firstPitch) pitchStream << ',';
    pitchStream << step << ':' << positivePitchClass(event.note, rootPitchClass);
    firstPitch = false;

    if (havePreviousNote) {
      if (!firstContour) contourStream << ',';
      contourStream << step << ':' << (static_cast<int>(event.note) - previousNote);
      firstContour = false;
    }
    previousNote = static_cast<int>(event.note);
    havePreviousNote = true;
  }

  timing = timingStream.str();
  pitchClass = pitchStream.str();
  contour = contourStream.str();
}

inline NeutralMaterialObservation observeNeutralMaterial(
    const DrumPatternSet& drums,
    const SynthPattern& synthA,
    const SynthPattern& synthB,
    const StrongRhythmMigrationResult& migration,
    uint8_t rootPitchClass) {
  NeutralMaterialObservation observation{};
  std::ostringstream drumTimingStream;
  bool firstDrumTiming = true;
  uint16_t allOnsets = 0;

  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    const int bucket = drumRoleBucket(voice);
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      const DrumStep& event = drums.voices[voice].steps[step];
      if (!event.hit) continue;
      const uint16_t bit = physicalStepBit(step);
      uint16_t* onsetMask = nullptr;
      uint16_t* accentMask = nullptr;
      switch (bucket) {
        case 0:
          onsetMask = &observation.kickOnsets;
          accentMask = &observation.kickAccents;
          break;
        case 1:
          onsetMask = &observation.backbeatOnsets;
          accentMask = &observation.backbeatAccents;
          break;
        case 2:
          onsetMask = &observation.hatOnsets;
          accentMask = &observation.hatAccents;
          break;
        default:
          onsetMask = &observation.supportOnsets;
          accentMask = &observation.supportAccents;
          break;
      }
      *onsetMask = static_cast<uint16_t>(*onsetMask | bit);
      if (event.accent) *accentMask = static_cast<uint16_t>(*accentMask | bit);
      allOnsets = static_cast<uint16_t>(allOnsets | bit);
      ++observation.physicalEventCount;
      if (event.timing != 0) {
        if (!firstDrumTiming) drumTimingStream << ',';
        drumTimingStream << voice << ':' << step << ':' << static_cast<int>(event.timing);
        firstDrumTiming = false;
      }
    }
  }
  observation.drumTiming = drumTimingStream.str();

  observeSynth(synthA, rootPitchClass, observation.synthAOnsets,
               observation.synthAAccents, observation.synthAGhosts,
               observation.synthATiming, observation.synthAPitchClass,
               observation.synthAContour, observation.physicalEventCount);
  observeSynth(synthB, rootPitchClass, observation.synthBOnsets,
               observation.synthBAccents, observation.synthBGhosts,
               observation.synthBTiming, observation.synthBPitchClass,
               observation.synthBContour, observation.physicalEventCount);

  allOnsets = static_cast<uint16_t>(allOnsets | observation.synthAOnsets |
                                    observation.synthBOnsets);
  observation.silenceMask = static_cast<uint16_t>(~allOnsets);
  observation.harmonicEventOnsets = migration.harmonicEventOnsets;
  observation.harmonicEventCount = migration.harmonicEventCount;
  observation.chordOnsets = migration.chordOnsets;
  observation.melodicFillOnsets = migration.melodicFillOnsets;
  observation.chordApplied = migration.chordRhythmApplied;
  observation.melodicApplied = migration.melodicRhythmApplied;
  observation.synthBRole = migration.synthBRole;
  return observation;
}

inline std::string compactNeutralMaterial(const NeutralMaterialObservation& value) {
  std::ostringstream stream;
  stream << std::hex;
  stream << "k=" << value.kickOnsets
         << ",b=" << value.backbeatOnsets
         << ",h=" << value.hatOnsets
         << ",d=" << value.supportOnsets
         << ",ka=" << value.kickAccents
         << ",ba=" << value.backbeatAccents
         << ",ha=" << value.hatAccents
         << ",da=" << value.supportAccents
         << ",a=" << value.synthAOnsets
         << ",z=" << value.synthBOnsets
         << ",aa=" << value.synthAAccents
         << ",za=" << value.synthBAccents
         << ",ag=" << value.synthAGhosts
         << ",zg=" << value.synthBGhosts
         << ",he=" << value.harmonicEventOnsets
         << ",co=" << value.chordOnsets
         << ",mo=" << value.melodicFillOnsets
         << std::dec
         << ",hc=" << static_cast<unsigned>(value.harmonicEventCount)
         << ",ca=" << (value.chordApplied ? 1 : 0)
         << ",ma=" << (value.melodicApplied ? 1 : 0)
         << ",r=" << static_cast<unsigned>(value.synthBRole)
         << ",dt=" << value.drumTiming
         << ",at=" << value.synthATiming
         << ",zt=" << value.synthBTiming
         << ",ap=" << value.synthAPitchClass
         << ",zp=" << value.synthBPitchClass
         << ",ac=" << value.synthAContour
         << ",zc=" << value.synthBContour;
  return stream.str();
}

}  // namespace GF2GateB
}  // namespace GroovePuterRhythm
