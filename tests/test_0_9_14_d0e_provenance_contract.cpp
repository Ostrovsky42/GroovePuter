// D0-E test-local research model.
//
// This does NOT define production provenance structs or production enum names.
// It proves that a bounded generated-origin basis plus claim-local current
// applicability can represent the required P0 semantics without reverse
// analysing arbitrary note buffers.

#include "src/generation/generation_context.h"
#include "src/generation/rhythm/rhythm_types.h"
#include "src/generation/roles/bass_rhythm.h"
#include "src/generation/roles/chord_progression.h"
#include "src/generation/roles/harmonic_rhythm.h"
#include "src/state/material_identity.h"
#include "src/state/material_version.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <type_traits>

namespace D0E {

using namespace GroovePuterRhythm;
using GroovePuterMaterial::MaterialReference;
using GroovePuterMaterial::MaterialVersionToken;

// Candidate P0 storage shape only. The important property is that the common
// origin facts are stored once per generated phrase while bar-local evidence is
// bounded to the existing 8-bar semantic phrase capacity.
struct P0GeneratedOriginKey {
  uint16_t phraseGenerationIdentity = 0;
  GenerationContext realizationGeneration{};
  RhythmArchetypeId rhythmArchetypeId = kNoArchetypeId;
  uint8_t structuralDensityTarget = 0;
  RealizationLevel level = RealizationLevel::P1Canonical;
  uint8_t phraseBars = 0;

  // Generation-program provenance only. This must never be read as the D0-C
  // semantic TrajectoryRole verdict.
  TrajectoryId generationTrajectoryId = kNoTrajectoryId;

  uint8_t rootPitchClass = 0;
  uint8_t scaleTypeValue = 0;
  ChordProgressionSource progressionSource{};
};

struct P0BarOrigin {
  MaterialReference material{};
  MaterialVersionToken originVersion{};
  uint8_t phraseBarOrdinal = 0;
  BassRhythmPlan bassRhythm{};
  HarmonicRhythmPlan harmonicRhythm{};
};

constexpr uint8_t kP0Bars = 8;

struct P0GeneratedPhraseOrigin {
  P0GeneratedOriginKey common{};
  P0BarOrigin bars[kP0Bars]{};
  uint8_t barCount = 0;
};

static_assert(std::is_trivially_copyable<P0GeneratedPhraseOrigin>::value,
              "D0-E candidate origin must stay a bounded value object");
static_assert(sizeof(P0GeneratedPhraseOrigin) <= 384,
              "D0-E candidate session origin is no longer compact");

enum Claim : uint16_t {
  BassRhythmTopology = 1u << 0u,
  BassDrumRelation = 1u << 1u,
  TonalContextApplicability = 1u << 2u,
  HarmonicSourceApplicability = 1u << 3u,
  HarmonicTimingApplicability = 1u << 4u,
  PitchContourApplicability = 1u << 5u,
  ArticulationLifetimeApplicability = 1u << 6u,
  PhraseExtentApplicability = 1u << 7u,
};

constexpr uint16_t kAllClaims =
    BassRhythmTopology |
    BassDrumRelation |
    TonalContextApplicability |
    HarmonicSourceApplicability |
    HarmonicTimingApplicability |
    PitchContourApplicability |
    ArticulationLifetimeApplicability |
    PhraseExtentApplicability;

enum MutationEffect : uint16_t {
  NoEffect = 0,
  PitchContent = 1u << 0u,
  OnsetTopology = 1u << 1u,
  Duration = 1u << 2u,
  Articulation = 1u << 3u,
  MetricTransform = 1u << 4u,
  PhraseLength = 1u << 5u,
  FxOnly = 1u << 6u,
  VelocityOnly = 1u << 7u,
  UnknownMutation = 1u << 15u,
};

struct CurrentClaimApplicability {
  // A set bit means the owner-derived origin witness may still be applied to
  // CURRENT. A cleared bit means UNKNOWN, not FAIL.
  uint16_t applicable = kAllClaims;

  bool has(Claim claim) const {
    return (applicable & static_cast<uint16_t>(claim)) != 0;
  }
};

void applyMutationEffect(CurrentClaimApplicability& state, uint16_t effects) {
  if ((effects & UnknownMutation) != 0) {
    state.applicable = 0;
    return;
  }

  if ((effects & PitchContent) != 0) {
    state.applicable &= static_cast<uint16_t>(
        ~(TonalContextApplicability |
          HarmonicSourceApplicability |
          PitchContourApplicability));
  }

  if ((effects & (OnsetTopology | MetricTransform)) != 0) {
    state.applicable &= static_cast<uint16_t>(
        ~(BassRhythmTopology |
          BassDrumRelation |
          HarmonicTimingApplicability));
  }

  if ((effects & (Duration | Articulation)) != 0) {
    state.applicable &= static_cast<uint16_t>(
        ~ArticulationLifetimeApplicability);
  }

  if ((effects & PhraseLength) != 0) {
    state.applicable &= static_cast<uint16_t>(
        ~PhraseExtentApplicability);
  }

  // FX-only / velocity-only changes deliberately do not invalidate the P0
  // structural claim set.
}

struct SidecarGuard {
  MaterialVersionToken originVersion{};
  MaterialVersionToken lastKnownVersion{};
  uint16_t originApplicable = kAllClaims;
  CurrentClaimApplicability current{};
};

// A typed mutation updates the version and applies only its known effect. If a
// version changes outside this boundary, fail closed. Exact return to the
// immutable origin bytes may re-arm origin-derived applicability.
void observeVersion(SidecarGuard& guard,
                    MaterialVersionToken current,
                    bool typedTransitionObserved,
                    uint16_t effects = NoEffect) {
  if (current == guard.originVersion) {
    guard.current.applicable = guard.originApplicable;
    guard.lastKnownVersion = current;
    return;
  }

  if (current == guard.lastKnownVersion) return;

  if (!typedTransitionObserved) {
    guard.current.applicable = 0;
    guard.lastKnownVersion = current;
    return;
  }

  applyMutationEffect(guard.current, effects);
  guard.lastKnownVersion = current;
}

MaterialVersionToken version(uint32_t low) {
  return MaterialVersionToken{low, 0};
}

void pitch_edit_is_local() {
  CurrentClaimApplicability state{};
  applyMutationEffect(state, PitchContent);

  assert(state.has(BassRhythmTopology));
  assert(state.has(BassDrumRelation));
  assert(!state.has(TonalContextApplicability));
  assert(!state.has(HarmonicSourceApplicability));
  assert(!state.has(PitchContourApplicability));
  assert(state.has(ArticulationLifetimeApplicability));
  assert(state.has(PhraseExtentApplicability));
}

void onset_edit_is_local() {
  CurrentClaimApplicability state{};
  applyMutationEffect(state, OnsetTopology);

  assert(!state.has(BassRhythmTopology));
  assert(!state.has(BassDrumRelation));
  assert(!state.has(HarmonicTimingApplicability));
  assert(state.has(TonalContextApplicability));
  assert(state.has(HarmonicSourceApplicability));
  assert(state.has(ArticulationLifetimeApplicability));
}

void lifetime_edit_is_local() {
  CurrentClaimApplicability state{};
  applyMutationEffect(state, Duration);

  assert(!state.has(ArticulationLifetimeApplicability));
  assert(state.has(BassRhythmTopology));
  assert(state.has(TonalContextApplicability));
  assert(state.has(HarmonicTimingApplicability));
}

void metric_transform_invalidates_metric_claims_not_pitch_origin() {
  CurrentClaimApplicability state{};
  applyMutationEffect(state, MetricTransform);

  assert(!state.has(BassRhythmTopology));
  assert(!state.has(BassDrumRelation));
  assert(!state.has(HarmonicTimingApplicability));
  assert(state.has(TonalContextApplicability));
  assert(state.has(HarmonicSourceApplicability));
}

void phrase_length_edit_invalidates_phrase_scope_only() {
  CurrentClaimApplicability state{};
  applyMutationEffect(state, PhraseLength);

  assert(!state.has(PhraseExtentApplicability));
  assert(state.has(BassRhythmTopology));
  assert(state.has(TonalContextApplicability));
}

void expression_only_edit_preserves_structural_claims() {
  CurrentClaimApplicability fx{};
  applyMutationEffect(fx, FxOnly);
  assert(fx.applicable == kAllClaims);

  CurrentClaimApplicability velocity{};
  applyMutationEffect(velocity, VelocityOnly);
  assert(velocity.applicable == kAllClaims);
}

void unknown_compound_edit_fails_closed_without_claim_failure() {
  CurrentClaimApplicability state{};
  applyMutationEffect(state, UnknownMutation);
  assert(state.applicable == 0);
}

void origin_is_immutable_while_current_applicability_changes() {
  P0GeneratedOriginKey origin{};
  origin.phraseGenerationIdentity = 42;
  origin.rhythmArchetypeId = 416;
  origin.rootPitchClass = 5;
  origin.phraseBars = 4;

  const uint16_t phraseIdentityBefore = origin.phraseGenerationIdentity;
  const RhythmArchetypeId archetypeBefore = origin.rhythmArchetypeId;
  const uint8_t rootBefore = origin.rootPitchClass;
  const uint8_t phraseBarsBefore = origin.phraseBars;

  CurrentClaimApplicability state{};
  applyMutationEffect(state, PitchContent | Duration);

  assert(origin.phraseGenerationIdentity == phraseIdentityBefore);
  assert(origin.rhythmArchetypeId == archetypeBefore);
  assert(origin.rootPitchClass == rootBefore);
  assert(origin.phraseBars == phraseBarsBefore);
  assert(!state.has(TonalContextApplicability));
  assert(!state.has(ArticulationLifetimeApplicability));
}

void stale_version_fails_closed_but_exact_origin_can_rearm() {
  SidecarGuard guard{};
  guard.originVersion = version(10);
  guard.lastKnownVersion = guard.originVersion;

  observeVersion(guard, version(11), true, PitchContent);
  assert(guard.current.has(BassRhythmTopology));
  assert(!guard.current.has(TonalContextApplicability));

  // Unknown external mutation: nothing may be confidently carried forward.
  observeVersion(guard, version(12), false);
  assert(guard.current.applicable == 0);

  // Byte-exact return to the retained origin is authoritative again.
  observeVersion(guard, version(10), false);
  assert(guard.current.applicable == guard.originApplicable);
}

void compound_known_effects_invalidate_union_only() {
  CurrentClaimApplicability state{};
  applyMutationEffect(state, PitchContent | OnsetTopology | Articulation);

  assert(!state.has(BassRhythmTopology));
  assert(!state.has(BassDrumRelation));
  assert(!state.has(TonalContextApplicability));
  assert(!state.has(HarmonicSourceApplicability));
  assert(!state.has(HarmonicTimingApplicability));
  assert(!state.has(PitchContourApplicability));
  assert(!state.has(ArticulationLifetimeApplicability));
  assert(state.has(PhraseExtentApplicability));
}

}  // namespace D0E

int main() {
  using namespace D0E;

  pitch_edit_is_local();
  onset_edit_is_local();
  lifetime_edit_is_local();
  metric_transform_invalidates_metric_claims_not_pitch_origin();
  phrase_length_edit_invalidates_phrase_scope_only();
  expression_only_edit_preserves_structural_claims();
  unknown_compound_edit_fails_closed_without_claim_failure();
  origin_is_immutable_while_current_applicability_changes();
  stale_version_fails_closed_but_exact_origin_can_rearm();
  compound_known_effects_invalidate_union_only();

  std::printf("D0-E candidate sizes: common=%zu bar=%zu phrase=%zu\n",
              sizeof(P0GeneratedOriginKey),
              sizeof(P0BarOrigin),
              sizeof(P0GeneratedPhraseOrigin));
  std::printf("D0-E RuntimePhraseUndo: used=%zu capacity=%zu slack=%zu\n",
              sizeof(GroovePuterUndo::RuntimePhraseUndoPayload),
              GroovePuterUndo::UndoOwner::payloadCapacity(),
              GroovePuterUndo::UndoOwner::payloadCapacity() -
                  sizeof(GroovePuterUndo::RuntimePhraseUndoPayload));
  std::puts("0.9.14 D0-E provenance/invalidation research model: PASS");
  return 0;
}
