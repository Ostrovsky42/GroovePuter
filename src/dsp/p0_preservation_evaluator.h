#pragma once
#ifndef GROOVEPUTER_DSP_P0_PRESERVATION_EVALUATOR_H
#define GROOVEPUTER_DSP_P0_PRESERVATION_EVALUATOR_H

#include <cstdint>
#include <type_traits>

#include "../../scenes.h"
#include "../generation/rhythm/rhythm_types.h"
#include "../phrase/runtime_phrase_edit.h"
#include "../phrase/runtime_synth_events.h"
#include "../state/generated_synth_a_origin.h"
#include "development_semantic_adapter.h"
#include "development_semantics.h"
#include "p0_preservation_types.h"

// D1-C: P0 GENERATED SYNTH A PRESERVATION EVALUATOR (frozen D0-F contract).
//
// Evaluates R1/R2/R3 for one bar of immutable generated origin evidence
// against CURRENT (as a Pattern projection with authoritative source steps)
// and a Development candidate. It is a *provider of one-sided facts* only:
//
//   lineageSummary == Pass     iff R1 && R2 && R3 all Pass   (=> CONTINUES)
//   lineageSummary == Unknown  otherwise                     (never Fail)
//
// A failed required claim means the sufficient continuity proof is
// unavailable; it never proves a new idea, so this provider can never feed
// Fail into the D0-C lineage adapter. It owns no publication policy, consumes
// none of the legacy idea, genre or publish-decision results, and
// performs no distance scoring, hashing or reverse harmonic analysis.
namespace GroovePuterDevelopmentSemantic {

namespace P0Detail {

inline bool stepIn(GroovePuterRhythm::StepMask mask, uint8_t step) {
  return step < GroovePuterRhythm::kStepsPerBar &&
         (mask & GroovePuterRhythm::stepBit(step)) != 0;
}

inline uint8_t popcount16(GroovePuterRhythm::StepMask mask) {
  uint8_t count = 0;
  for (uint8_t step = 0; step < GroovePuterRhythm::kStepsPerBar; ++step) {
    if (stepIn(mask, step)) ++count;
  }
  return count;
}

// Physical Pattern steps that hold a note for this bar. Bass continuations are
// realized by the owner's tonal adapter as real notes (copy of the active note
// with slide set), so the Pattern's physical skeleton -- and therefore its
// Runtime projection -- is onsets | continuations.
inline GroovePuterRhythm::StepMask physicalSkeleton(
    const GroovePuterMaterial::GeneratedSynthABarOrigin& origin) {
  return static_cast<GroovePuterRhythm::StepMask>(
      origin.bassRhythm.onsets | origin.bassRhythm.continuations);
}

// Owner-derived pitch class the origin authorises at a physical step: the
// witness nibble at an attack, or -- for a continuation step -- the attack it
// continues (the owner adapter copies the active note into continuations).
inline bool originPitchClassAt(
    const GroovePuterMaterial::GeneratedSynthABarOrigin& origin,
    uint8_t step,
    uint8_t& pitchClass) {
  uint8_t s = step;
  while (!stepIn(origin.bassRhythm.onsets, s)) {
    if (!stepIn(origin.bassRhythm.continuations, s) || s == 0) return false;
    --s;
  }
  pitchClass = origin.bassPitchClasses.pitchClassAt(s);
  return pitchClass < 12;
}

}  // namespace P0Detail

// `sourceSteps[i]` is the physical Pattern step that produced `source.events[i]`
// (from PhraseRuntime::projectPatternToRuntimeEventsWithSourceSteps).
inline P0PreservationAssessment evaluateP0Preservation(
    const GroovePuterMaterial::GeneratedSynthABarOrigin& origin,
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const uint8_t (&sourceSteps)[SynthPattern::kSteps],
    const PhraseRuntime::RuntimeSynthEventBuffer& candidate) {
  using GroovePuterRhythm::StepMask;
  P0PreservationAssessment out{};
  out.available = true;

  // ---- R1: bar extent ------------------------------------------------------
  if (source.lengthTicks != PhraseRuntime::kTicksPerBar) {
    out.r1BarExtent = ClaimEvidenceStatus::Unknown;  // outside the declared P0 scope
  } else {
    out.r1BarExtent = candidate.lengthTicks == source.lengthTicks
        ? ClaimEvidenceStatus::Pass
        : ClaimEvidenceStatus::Fail;
  }

  // ---- R2: origin applicability of CURRENT (authoritative source steps) ----
  const StepMask skeleton = P0Detail::physicalSkeleton(origin);
  bool mapWellFormed = source.count <= SynthPattern::kSteps;
  StepMask mapped = 0;
  if (mapWellFormed) {
    for (uint16_t i = 0; i < source.count; ++i) {
      const uint8_t step = sourceSteps[i];
      if (step >= GroovePuterRhythm::kStepsPerBar ||
          P0Detail::stepIn(mapped, step)) {
        mapWellFormed = false;  // projection did not give a usable source-step map
        break;
      }
      mapped = static_cast<StepMask>(mapped | GroovePuterRhythm::stepBit(step));
    }
  }

  if (!mapWellFormed) {
    out.r2BassOnsetTopology = ClaimEvidenceStatus::Unknown;
  } else {
    const bool currentMatchesOrigin =
        mapped == skeleton && source.count == P0Detail::popcount16(skeleton);
    bool candidatePreserves = candidate.count == source.count;
    if (candidatePreserves) {
      for (uint16_t i = 0; i < source.count; ++i) {
        if (candidate.events[i].startTick != source.events[i].startTick) {
          candidatePreserves = false;
          break;
        }
      }
    }
    out.r2BassOnsetTopology = (currentMatchesOrigin && candidatePreserves)
        ? ClaimEvidenceStatus::Pass
        : ClaimEvidenceStatus::Fail;
  }

  // ---- R3: pitch class at onset (needs an established correspondence) ------
  if (out.r2BassOnsetTopology != ClaimEvidenceStatus::Pass) {
    out.r3PitchClassAtOnset = ClaimEvidenceStatus::Unknown;
  } else {
    bool unavailable = false;
    bool pass = true;
    for (uint16_t i = 0; i < source.count; ++i) {
      uint8_t expected = 0;
      if (!P0Detail::originPitchClassAt(origin, sourceSteps[i], expected)) {
        unavailable = true;
        break;
      }
      if ((source.events[i].note % 12u) != expected ||
          (candidate.events[i].note % 12u) != expected) {
        pass = false;
      }
    }
    out.r3PitchClassAtOnset = unavailable ? ClaimEvidenceStatus::Unknown
        : pass ? ClaimEvidenceStatus::Pass
               : ClaimEvidenceStatus::Fail;
  }

  // ---- One-sided aggregation: Pass or Unknown, never Fail ------------------
  const bool allPass = out.r1BarExtent == ClaimEvidenceStatus::Pass &&
                       out.r2BassOnsetTopology == ClaimEvidenceStatus::Pass &&
                       out.r3PitchClassAtOnset == ClaimEvidenceStatus::Pass;
  out.lineageSummary =
      allPass ? ClaimEvidenceStatus::Pass : ClaimEvidenceStatus::Unknown;

  // ---- PREDECESSOR relation (the operation source), explicit reference ------
  if (allPass) {
    out.predecessorRelation = RuntimePhraseEdit::same(candidate, source)
        ? StateRelation::Exact
        : StateRelation::Variation;
  } else {
    out.predecessorRelation = StateRelation::Unknown;
  }
  return out;
}

// Provider-specific D0-C integration. The lineage summary is Pass/Unknown only,
// so the generic adapter can produce CONTINUES or UNKNOWN but never NEW_IDEA
// from this provider. SOURCE is left UNKNOWN (the origin is an anchor, not a
// byte-exact snapshot we hold); the operation source is PREDECESSOR.
inline SemanticFacts adaptP0Preservation(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    const GroovePuterDevelopment::DevelopmentEvidence& evidence,
    const GroovePuterDevelopment::DevelopmentRequest& request,
    const P0PreservationAssessment& assessment) {
  SemanticAssessmentInput input{};
  if (assessment.available) {
    // Never Fail: guard the invariant at the seam as well.
    input.lineagePreservation =
        assessment.lineageSummary == ClaimEvidenceStatus::Pass
            ? ClaimEvidenceStatus::Pass
            : ClaimEvidenceStatus::Unknown;
    input.relations[static_cast<uint8_t>(ReferenceRole::Predecessor)] = {
        ReferenceRole::Predecessor, &source, input.lineagePreservation};
  }
  SemanticFacts facts = adaptLegacyEvidence(source, candidate, evidence, request, input);
  if (assessment.available) {
    for (uint8_t i = 0; i < kReferenceFactCount; ++i) {
      if (facts.relations[i].reference == ReferenceRole::Source) {
        facts.relations[i].result = StateRelation::Unknown;
      } else if (facts.relations[i].reference == ReferenceRole::Predecessor) {
        // Spec: Exact/Variation only once lineage preservation PASSED.
        facts.relations[i].result = assessment.predecessorRelation;
      }
    }
    // The provider evaluated R2 only when the source-step map was usable.
    if (assessment.r2BassOnsetTopology != ClaimEvidenceStatus::Unknown) {
      setCapability(facts, CapabilityClaim::RhythmTopology,
                    CapabilityStatus::Available);
    }
  }
  return facts;
}

}  // namespace GroovePuterDevelopmentSemantic

#endif  // GROOVEPUTER_DSP_P0_PRESERVATION_EVALUATOR_H
