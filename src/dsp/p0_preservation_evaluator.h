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

// D1-C1: keep ATTACKS and CONTINUATIONS distinct.
//   attackMask       = origin.bassRhythm.onsets          (R2/R3 subject)
//   continuationMask = origin.bassRhythm.continuations   (representation only)
//   physicalSkeleton = attackMask | continuationMask     (projection evidence)
// The owner's tonal adapter realizes a continuation as an extra physical
// Pattern step carrying the active note with slide set, so the Pattern->Runtime
// projection legitimately contains events at the physical skeleton. That is a
// representation fact, not a new attack: continuation events are never counted
// as attacks, and continuation steps are neither required nor compared by
// R2/R3.
inline GroovePuterRhythm::StepMask attackMask(
    const GroovePuterMaterial::GeneratedSynthABarOrigin& origin) {
  return origin.bassRhythm.onsets;
}

inline GroovePuterRhythm::StepMask continuationMask(
    const GroovePuterMaterial::GeneratedSynthABarOrigin& origin) {
  return origin.bassRhythm.continuations;
}

inline GroovePuterRhythm::StepMask physicalSkeleton(
    const GroovePuterMaterial::GeneratedSynthABarOrigin& origin) {
  return static_cast<GroovePuterRhythm::StepMask>(
      attackMask(origin) | continuationMask(origin));
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

  // ---- R2: BASS ATTACK topology --------------------------------------------
  // Classify every projected CURRENT event by its authoritative source step:
  // origin attack / known origin continuation / neither. Continuation events
  // are never attacks. Precedence: a direct contradiction (an origin attack is
  // missing or moved, or a corresponding attack tick differs) is Fail; missing
  // correspondence or unclassifiable events are Unknown, never an invented Fail.
  const StepMask attacks = P0Detail::attackMask(origin);
  const StepMask skeleton = P0Detail::physicalSkeleton(origin);
  bool mapWellFormed = source.count <= SynthPattern::kSteps;
  StepMask mapped = 0;
  bool unclassifiable = false;
  if (mapWellFormed) {
    for (uint16_t i = 0; i < source.count; ++i) {
      const uint8_t step = sourceSteps[i];
      if (step >= GroovePuterRhythm::kStepsPerBar ||
          P0Detail::stepIn(mapped, step)) {
        mapWellFormed = false;  // projection did not give a usable source-step map
        break;
      }
      mapped = static_cast<StepMask>(mapped | GroovePuterRhythm::stepBit(step));
      if (!P0Detail::stepIn(skeleton, step)) unclassifiable = true;
    }
  }

  if (!mapWellFormed) {
    out.r2BassOnsetTopology = ClaimEvidenceStatus::Unknown;
  } else {
    // Every origin attack must still exist at its own step (exactly one event:
    // uniqueness is guaranteed by the map check above).
    const bool allAttacksPresent = (mapped & attacks) == attacks;
    bool status_fail = !allAttacksPresent;
    bool status_unknown = false;
    if (allAttacksPresent) {
      // Candidate correspondence is only established when ordinals line up.
      if (candidate.count != source.count) {
        status_unknown = true;
      } else {
        for (uint16_t i = 0; i < source.count; ++i) {
          if (!P0Detail::stepIn(attacks, sourceSteps[i])) continue;  // continuation ticks are not compared
          if (candidate.events[i].startTick != source.events[i].startTick) {
            status_fail = true;
            break;
          }
        }
      }
      if (unclassifiable) status_unknown = true;  // events of unknown meaning: do not invent one
    }
    out.r2BassOnsetTopology = status_fail ? ClaimEvidenceStatus::Fail
        : status_unknown ? ClaimEvidenceStatus::Unknown
                         : ClaimEvidenceStatus::Pass;
  }

  // ---- R3: pitch class AT ATTACKS only (needs an established correspondence) -
  if (out.r2BassOnsetTopology != ClaimEvidenceStatus::Pass) {
    out.r3PitchClassAtOnset = ClaimEvidenceStatus::Unknown;
  } else {
    bool pass = true;
    for (uint16_t i = 0; i < source.count; ++i) {
      const uint8_t step = sourceSteps[i];
      if (!P0Detail::stepIn(attacks, step)) continue;  // continuation inheritance is not R3
      const uint8_t expected = origin.bassPitchClasses.pitchClassAt(step);
      if (expected >= 12 || (source.events[i].note % 12u) != expected ||
          (candidate.events[i].note % 12u) != expected) {
        pass = false;
        break;
      }
    }
    out.r3PitchClassAtOnset = pass ? ClaimEvidenceStatus::Pass : ClaimEvidenceStatus::Fail;
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
