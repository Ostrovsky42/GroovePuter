// GroovePuter 0.9.14 D0-F
// Test-local P0 preservation contract.
//
// This is deliberately NOT a production lineage evaluator. It proves the
// minimum one-sided contract required before the generated Synth A provenance
// seam is implemented.

#include "src/dsp/development_semantics.h"
#include "src/dsp/musical_development.h"
#include "src/phrase/runtime_phrase_edit.h"

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace D0F {

namespace Sem = GroovePuterDevelopmentSemantic;
namespace Dev = GroovePuterDevelopment;
using PhraseRuntime::RuntimeSynthEventBuffer;

enum class ClaimResult : uint8_t {
  Unknown = 0,
  Pass,
  Fail,
};

struct PreservationClaims {
  ClaimResult barExtent = ClaimResult::Unknown;
  ClaimResult bassOnsetTopology = ClaimResult::Unknown;
  ClaimResult tonalPitchClassAtOnset = ClaimResult::Unknown;
};

struct Assessment {
  PreservationClaims claims{};
  Sem::LineageStatus lineage = Sem::LineageStatus::Unknown;
  Sem::StateRelation relation = Sem::StateRelation::Unknown;
};

// P0 deliberately proves only a sufficient CONTINUES case.
// FAIL does not mean NEW_IDEA; it means the sufficient proof no longer applies.
Assessment assess(const RuntimeSynthEventBuffer& source,
                  const RuntimeSynthEventBuffer& candidate,
                  bool ownerOriginAvailable,
                  bool exactMaterialBindingAvailable) {
  Assessment out{};
  if (!ownerOriginAvailable || !exactMaterialBindingAvailable) return out;

  out.claims.barExtent =
      source.lengthTicks == candidate.lengthTicks
          ? ClaimResult::Pass
          : ClaimResult::Fail;

  bool onsetPass = source.count == candidate.count;
  if (onsetPass) {
    for (uint16_t i = 0; i < source.count; ++i) {
      if (source.events[i].startTick != candidate.events[i].startTick) {
        onsetPass = false;
        break;
      }
    }
  }
  out.claims.bassOnsetTopology =
      onsetPass ? ClaimResult::Pass : ClaimResult::Fail;

  bool pitchClassPass = onsetPass;
  if (pitchClassPass) {
    for (uint16_t i = 0; i < source.count; ++i) {
      if ((source.events[i].note % 12u) !=
          (candidate.events[i].note % 12u)) {
        pitchClassPass = false;
        break;
      }
    }
  }
  out.claims.tonalPitchClassAtOnset =
      pitchClassPass ? ClaimResult::Pass : ClaimResult::Fail;

  const bool allRequiredPass =
      out.claims.barExtent == ClaimResult::Pass &&
      out.claims.bassOnsetTopology == ClaimResult::Pass &&
      out.claims.tonalPitchClassAtOnset == ClaimResult::Pass;

  if (allRequiredPass) {
    out.lineage = Sem::LineageStatus::Continues;
    out.relation = RuntimePhraseEdit::same(source, candidate)
        ? Sem::StateRelation::Exact
        : Sem::StateRelation::Variation;
  }

  return out;
}

RuntimeSynthEventBuffer sourceLine() {
  RuntimeSynthEventBuffer b{};
  b.lengthTicks = PhraseRuntime::kTicksPerBar;
  b.count = 3;

  b.events[0].startTick = 0;
  b.events[0].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[0].note = 48;
  b.events[0].velocity = 100;
  b.events[0].probability = 100;

  b.events[1].startTick = 96;
  b.events[1].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[1].note = 52;
  b.events[1].velocity = 100;
  b.events[1].probability = 100;

  b.events[2].startTick = 240;
  b.events[2].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[2].note = 55;
  b.events[2].velocity = 100;
  b.events[2].probability = 100;
  return b;
}

Dev::DevelopmentRequest request(Dev::TransformationKind kind) {
  Dev::DevelopmentRequest r{};
  r.transformation = kind;
  r.genreId = static_cast<uint8_t>(GenerativeMode::Acid);
  return r;
}

Dev::DevelopmentResult transform(const RuntimeSynthEventBuffer& source,
                                 Dev::DevelopmentRequest r) {
  return Dev::developCandidate(source, r, &source);
}

void exact_repeat_is_continuous_exact() {
  const auto source = sourceLine();
  const auto a = assess(source, source, true, true);
  assert(a.lineage == Sem::LineageStatus::Continues);
  assert(a.relation == Sem::StateRelation::Exact);
}

void revoice_can_prove_continuity_without_exact_register() {
  const auto source = sourceLine();
  auto r = request(Dev::TransformationKind::Revoice);
  r.octaveShift = 1;
  const auto dev = transform(source, r);
  assert(dev.success);

  const auto a = assess(source, dev.candidate, true, true);
  assert(a.claims.bassOnsetTopology == ClaimResult::Pass);
  assert(a.claims.tonalPitchClassAtOnset == ClaimResult::Pass);
  assert(a.lineage == Sem::LineageStatus::Continues);
  assert(a.relation == Sem::StateRelation::Variation);
}

void hold_can_prove_continuity_without_exact_lifetime() {
  const auto source = sourceLine();
  const auto dev = transform(source, request(Dev::TransformationKind::Hold));
  assert(dev.success);
  assert(dev.candidate.events[0].durationSubticks !=
         source.events[0].durationSubticks);

  const auto a = assess(source, dev.candidate, true, true);
  assert(a.lineage == Sem::LineageStatus::Continues);
  assert(a.relation == Sem::StateRelation::Variation);
}

void connect_can_prove_continuity_without_exact_articulation() {
  const auto source = sourceLine();
  const auto dev = transform(source, request(Dev::TransformationKind::Connect));
  assert(dev.success);

  const auto a = assess(source, dev.candidate, true, true);
  assert(a.lineage == Sem::LineageStatus::Continues);
  assert(a.relation == Sem::StateRelation::Variation);
}

void octave_move_can_prove_continuity() {
  const auto source = sourceLine();
  auto r = request(Dev::TransformationKind::Move);
  r.octaveShift = 1;
  const auto dev = transform(source, r);
  assert(dev.success);

  const auto a = assess(source, dev.candidate, true, true);
  assert(a.claims.tonalPitchClassAtOnset == ClaimResult::Pass);
  assert(a.lineage == Sem::LineageStatus::Continues);
}

void displace_loses_sufficient_continuity_proof_not_idea_identity() {
  const auto source = sourceLine();
  auto r = request(Dev::TransformationKind::Displace);
  r.displaceTicks = 12;
  const auto dev = transform(source, r);
  assert(dev.success);

  const auto a = assess(source, dev.candidate, true, true);
  assert(a.claims.bassOnsetTopology == ClaimResult::Fail);
  assert(a.lineage == Sem::LineageStatus::Unknown);
  assert(a.relation == Sem::StateRelation::Unknown);
  assert(a.lineage != Sem::LineageStatus::NewIdea);
}

void thin_loses_sufficient_continuity_proof_not_idea_identity() {
  const auto source = sourceLine();
  const auto dev = transform(source, request(Dev::TransformationKind::Thin));
  assert(dev.success);

  const auto a = assess(source, dev.candidate, true, true);
  assert(a.claims.bassOnsetTopology == ClaimResult::Fail);
  assert(a.lineage == Sem::LineageStatus::Unknown);
  assert(a.lineage != Sem::LineageStatus::NewIdea);
}

void chromatic_pitch_change_loses_tonal_proof_only() {
  const auto source = sourceLine();
  auto candidate = source;
  ++candidate.events[1].note;

  const auto a = assess(source, candidate, true, true);
  assert(a.claims.bassOnsetTopology == ClaimResult::Pass);
  assert(a.claims.tonalPitchClassAtOnset == ClaimResult::Fail);
  assert(a.lineage == Sem::LineageStatus::Unknown);
  assert(a.lineage != Sem::LineageStatus::NewIdea);
}

void phrase_growth_is_outside_one_bar_p0() {
  const auto source = sourceLine();
  auto candidate = source;
  candidate.lengthTicks = PhraseRuntime::kTicksPerBar * 2u;

  const auto a = assess(source, candidate, true, true);
  assert(a.claims.barExtent == ClaimResult::Fail);
  assert(a.lineage == Sem::LineageStatus::Unknown);
}

void missing_origin_or_binding_stays_unknown() {
  const auto source = sourceLine();

  const auto noOrigin = assess(source, source, false, true);
  assert(noOrigin.lineage == Sem::LineageStatus::Unknown);

  const auto staleBinding = assess(source, source, true, false);
  assert(staleBinding.lineage == Sem::LineageStatus::Unknown);
}

void transformation_name_cannot_force_preservation_result() {
  const auto source = sourceLine();
  auto r = request(Dev::TransformationKind::Revoice);
  r.octaveShift = 1;
  r.forceDisplaceTheOne = true;
  const auto dev = transform(source, r);
  assert(dev.success);

  const auto a = assess(source, dev.candidate, true, true);
  assert(a.claims.bassOnsetTopology == ClaimResult::Fail);
  assert(a.lineage == Sem::LineageStatus::Unknown);
}

}  // namespace D0F

int main() {
  D0F::exact_repeat_is_continuous_exact();
  D0F::revoice_can_prove_continuity_without_exact_register();
  D0F::hold_can_prove_continuity_without_exact_lifetime();
  D0F::connect_can_prove_continuity_without_exact_articulation();
  D0F::octave_move_can_prove_continuity();
  D0F::displace_loses_sufficient_continuity_proof_not_idea_identity();
  D0F::thin_loses_sufficient_continuity_proof_not_idea_identity();
  D0F::chromatic_pitch_change_loses_tonal_proof_only();
  D0F::phrase_growth_is_outside_one_bar_p0();
  D0F::missing_origin_or_binding_stays_unknown();
  D0F::transformation_name_cannot_force_preservation_result();

  std::puts("0.9.14 D0-F P0 preservation contract: PASS");
  return 0;
}
