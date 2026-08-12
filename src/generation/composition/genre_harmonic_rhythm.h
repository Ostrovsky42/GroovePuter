#ifndef GROOVEPUTER_GENERATION_COMPOSITION_GENRE_HARMONIC_RHYTHM_H
#define GROOVEPUTER_GENERATION_COMPOSITION_GENRE_HARMONIC_RHYTHM_H

#include <cstdint>

#include "generation_profile.h"
#include "../roles/chord_rhythm_retrigger.h"
#include "../roles/chord_rhythm_timeline.h"

namespace GroovePuterRhythm {

enum class GenreHarmonicRhythmStatus : uint8_t {
  Ok = 0,
  ValidButEmpty,
  InvalidRequest,
  TimelineFailed,
  RetriggerFailed,
  Count,
};

// Production-facing composition of the frozen P2/P3 capabilities.
//
// This layer deliberately does not own rhythm generation, progression content,
// absolute pitch, transport, Song storage, or synth voices.  It receives the
// already-selected one-bar ChordRhythm topology, lifts that exact topology to a
// bounded phrase for P2 validation, then classifies existing audible onsets as
// either source-advance (N) or same-source retrigger (S) for P3.
//
// Normal Pattern/Genre generation remains a self-contained 16-step physical
// surface: the first audible onset in a non-empty bar is always N.  Therefore
// this planner never pretends that a standalone SynthPattern owns an incoming
// cross-bar gate/source.  A future Song phrase materializer may provide that
// physical owner while reusing the same P2/P3 contracts.
struct GenreHarmonicRhythmRequest {
  RhythmFamily family = RhythmFamily::Count;
  ChordRhythmPlan chord{};
  ProgressionId progression = ProgressionId::Auto;
  PhraseEvolutionLawId phraseLaw = PhraseEvolutionLawId::Loop;
  uint8_t phraseBars = 1;
  uint8_t barOrdinal = 0;
};

struct GenreHarmonicRhythmPlan {
  GenreHarmonicRhythmStatus status =
      GenreHarmonicRhythmStatus::InvalidRequest;
  ChordRhythmTimelineStatus timelineStatus =
      ChordRhythmTimelineStatus::InvalidRequest;
  ChordRhythmRetriggerStatus retriggerStatus =
      ChordRhythmRetriggerStatus::ValidButEmpty;

  uint8_t requestedPhraseBars = 1;
  uint8_t boundedPhraseBars = 1;
  uint8_t phraseBarOrdinal = 0;

  ChordRhythmTimelinePlan timeline{};
  ChordRhythmRetriggerPlan currentBar{};
};

namespace GenreHarmonicRhythmDetail {

inline uint8_t countOnsets(StepMask mask) {
  uint8_t count = 0;
  while (mask != 0) {
    count = static_cast<uint8_t>(count + (mask & 1u));
    mask = static_cast<StepMask>(mask >> 1u);
  }
  return count;
}

inline StepMask nthOnset(StepMask mask, uint8_t ordinal) {
  uint8_t seen = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((mask & bit) == 0) continue;
    if (seen == ordinal) return bit;
    ++seen;
  }
  return 0;
}

inline bool staticProgression(ProgressionId id) {
  return id == ProgressionId::StaticModal ||
         id == ProgressionId::PedalDrone;
}

inline uint8_t familyAdvanceBudget(RhythmFamily family) {
  switch (family) {
    case RhythmFamily::DubPulse:
    case RhythmFamily::SparsePulse:
    case RhythmFamily::HipHopBackbeat:
      return 1;
    case RhythmFamily::FourFloor:
    case RhythmFamily::MachineSyncopation:
    case RhythmFamily::Breakbeat:
    case RhythmFamily::UkTwoStep:
    case RhythmFamily::Funk16:
      return 2;
    case RhythmFamily::Count:
      break;
  }
  return 0;
}

inline uint8_t advanceBudget(const GenreHarmonicRhythmRequest& request,
                             uint8_t onsetCount,
                             uint8_t boundedBars,
                             uint8_t phraseBarOrdinal) {
  if (onsetCount == 0) return 0;
  if (staticProgression(request.progression)) return 1;

  uint8_t budget = familyAdvanceBudget(request.family);
  if (budget == 0) return 0;

  switch (request.chord.id) {
    case ChordRhythmId::HeldPad:
    case ChordRhythmId::WholeBarHold:
      budget = 1;
      break;
    case ChordRhythmId::HalfBarChange:
    case ChordRhythmId::AnticipatedChange:
      budget = 2;
      break;
    case ChordRhythmId::OffbeatStab:
    case ChordRhythmId::BackbeatStab:
    case ChordRhythmId::SparseChordReply:
    case ChordRhythmId::DubChordSpace:
    case ChordRhythmId::SyncopatedComp:
      break;
    case ChordRhythmId::Auto:
    case ChordRhythmId::Count:
      return 0;
  }

  // Phrase law changes only how frequently the already-audible chord rhythm
  // advances harmonic source.  It never adds an onset or infers a hold.
  switch (request.phraseLaw) {
    case PhraseEvolutionLawId::Loop:
      break;
    case PhraseEvolutionLawId::RepeatReply:
      if (boundedBars > 1 && (phraseBarOrdinal & 1u) != 0u && budget > 1)
        --budget;
      break;
    case PhraseEvolutionLawId::DevelopReturn:
      if (boundedBars >= 4 && phraseBarOrdinal == 2 && budget < 2 &&
          onsetCount >= 2) {
        budget = 2;
      }
      break;
    case PhraseEvolutionLawId::SparseDrift:
      budget = 1;
      break;
    case PhraseEvolutionLawId::Count:
      return 0;
  }

  if (budget > onsetCount) budget = onsetCount;
  return budget;
}

inline StepMask selectSourceAdvances(StepMask audibleOnsets, uint8_t budget) {
  const uint8_t count = countOnsets(audibleOnsets);
  if (count == 0 || budget == 0) return 0;
  if (budget >= count) return audibleOnsets;

  StepMask advances = nthOnset(audibleOnsets, 0);
  if (budget == 1) return advances;

  // Distribute a second source change around the middle of the audible onset
  // sequence.  Four offbeat stabs therefore become N,S,N,S (A,A,B,B) rather
  // than four unrelated harmonic events.
  const uint8_t middle = static_cast<uint8_t>(count / 2u);
  advances = static_cast<StepMask>(advances | nthOnset(audibleOnsets, middle));

  // The current production policy intentionally caps at two harmonic source
  // advances per bar.  Keep the loop bounded in case that policy is extended.
  for (uint8_t ordinal = 1;
       countOnsets(advances) < budget && ordinal < count; ++ordinal) {
    advances = static_cast<StepMask>(
        advances | nthOnset(audibleOnsets, ordinal));
  }
  return advances;
}

inline uint8_t boundedPhraseBars(uint8_t requested) {
  if (requested == 0) return 0;
  return requested > kMaxChordRhythmTimelineBars
      ? kMaxChordRhythmTimelineBars
      : requested;
}

}  // namespace GenreHarmonicRhythmDetail

inline GenreHarmonicRhythmPlan realizeGenreHarmonicRhythm(
    const GenreHarmonicRhythmRequest& request) {
  GenreHarmonicRhythmPlan result{};
  result.requestedPhraseBars = request.phraseBars;

  if (static_cast<uint8_t>(request.family) >=
          static_cast<uint8_t>(RhythmFamily::Count) ||
      !isValidChordRhythmId(request.chord.id, false) ||
      !isValidProgressionId(request.progression, false) ||
      static_cast<uint8_t>(request.phraseLaw) >=
          static_cast<uint8_t>(PhraseEvolutionLawId::Count)) {
    return result;
  }

  const uint8_t bars =
      GenreHarmonicRhythmDetail::boundedPhraseBars(request.phraseBars);
  if (bars == 0) return result;
  result.boundedPhraseBars = bars;
  result.phraseBarOrdinal = static_cast<uint8_t>(request.barOrdinal % bars);

  ChordRhythmTimelineRequest timelineRequest{};
  timelineRequest.barCount = bars;
  for (uint8_t bar = 0; bar < bars; ++bar) {
    timelineRequest.onsets |=
        chordRhythmTimelineFromBar(request.chord.onsets, bar);
    timelineRequest.continuations |=
        chordRhythmTimelineFromBar(request.chord.continuations, bar);
    timelineRequest.releasePoints |=
        chordRhythmTimelineFromBar(request.chord.releasePoints, bar);
  }

  result.timeline = realizeChordRhythmTimeline(timelineRequest);
  result.timelineStatus = result.timeline.status;
  if (result.timeline.status != ChordRhythmTimelineStatus::Ok &&
      result.timeline.status != ChordRhythmTimelineStatus::ValidButEmpty) {
    result.status = GenreHarmonicRhythmStatus::TimelineFailed;
    return result;
  }

  const uint8_t audibleCount =
      GenreHarmonicRhythmDetail::countOnsets(request.chord.onsets);
  const uint8_t budget = GenreHarmonicRhythmDetail::advanceBudget(
      request, audibleCount, bars, result.phraseBarOrdinal);
  if (audibleCount != 0 && budget == 0) return result;

  ChordRhythmRetriggerRequest retriggerRequest{};
  retriggerRequest.sourceAdvanceOnsets =
      GenreHarmonicRhythmDetail::selectSourceAdvances(
          request.chord.onsets, budget);
  retriggerRequest.sameChordRetriggers = static_cast<StepMask>(
      request.chord.onsets & ~retriggerRequest.sourceAdvanceOnsets);
  retriggerRequest.continuations = request.chord.continuations;
  retriggerRequest.releasePoints = request.chord.releasePoints;
  // A standalone production SynthPattern is self-contained.  Cross-bar
  // incoming-source ownership belongs to a future Song phrase materializer.
  retriggerRequest.sourceAvailableAtStart = false;

  result.currentBar = realizeChordRhythmRetriggers(retriggerRequest);
  result.retriggerStatus = result.currentBar.status;
  if (result.currentBar.status != ChordRhythmRetriggerStatus::Ok &&
      result.currentBar.status != ChordRhythmRetriggerStatus::ValidButEmpty) {
    result.status = GenreHarmonicRhythmStatus::RetriggerFailed;
    return result;
  }

  result.status = result.currentBar.status ==
                          ChordRhythmRetriggerStatus::ValidButEmpty
      ? GenreHarmonicRhythmStatus::ValidButEmpty
      : GenreHarmonicRhythmStatus::Ok;
  return result;
}

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_GENRE_HARMONIC_RHYTHM_H
