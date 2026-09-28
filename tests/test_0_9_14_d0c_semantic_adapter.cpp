#include "src/dsp/development_semantic_adapter.h"

#include <cassert>
#include <cstdio>

using PhraseRuntime::RuntimeSynthEventBuffer;
namespace Sem = GroovePuterDevelopmentSemantic;
namespace Dev = GroovePuterDevelopment;

namespace {

RuntimeSynthEventBuffer makeLine(uint8_t n0, uint16_t t0,
                                 uint8_t n1, uint16_t t1) {
  RuntimeSynthEventBuffer b{};
  b.lengthTicks = PhraseRuntime::kTicksPerBar;
  b.count = 2;

  b.events[0].startTick = t0;
  b.events[0].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[0].note = n0;
  b.events[0].velocity = 100;
  b.events[0].probability = 100;

  b.events[1].startTick = t1;
  b.events[1].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[1].note = n1;
  b.events[1].velocity = 100;
  b.events[1].probability = 100;
  return b;
}

void exact_repeat_and_return_are_distinct() {
  const auto a = makeLine(48, 0, 52, 96);
  Dev::DevelopmentEvidence evidence{};
  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::None;

  Sem::SemanticAssessmentInput repeat{};
  repeat.lineagePreservation = Sem::ClaimEvidenceStatus::Pass;
  repeat.relations[1] = {
      Sem::ReferenceRole::Predecessor, &a, Sem::ClaimEvidenceStatus::Pass};
  repeat.trajectoryContextAvailable = true;
  repeat.trajectory = Sem::TrajectoryRole::Repeat;

  const auto repeatFacts =
      Sem::adaptLegacyEvidence(a, a, evidence, request, repeat);
  assert(Sem::stateRelationFor(repeatFacts, Sem::ReferenceRole::Predecessor) ==
         Sem::StateRelation::Exact);
  assert(repeatFacts.lineage == Sem::LineageStatus::Continues);
  assert(repeatFacts.trajectory == Sem::TrajectoryRole::Repeat);

  Sem::SemanticAssessmentInput ret{};
  ret.lineagePreservation = Sem::ClaimEvidenceStatus::Pass;
  ret.relations[2] = {
      Sem::ReferenceRole::ReturnTarget, &a, Sem::ClaimEvidenceStatus::Pass};
  ret.trajectoryContextAvailable = true;
  ret.trajectory = Sem::TrajectoryRole::Return;

  const auto returnFacts =
      Sem::adaptLegacyEvidence(a, a, evidence, request, ret);
  assert(Sem::stateRelationFor(returnFacts, Sem::ReferenceRole::ReturnTarget) ==
         Sem::StateRelation::Exact);
  assert(returnFacts.lineage == Sem::LineageStatus::Continues);
  assert(returnFacts.trajectory == Sem::TrajectoryRole::Return);
}

void transformed_return_uses_explicit_references() {
  const auto source = makeLine(48, 0, 52, 96);
  const auto predecessor = makeLine(55, 24, 59, 120);
  const auto candidate = makeLine(60, 0, 64, 96);

  Dev::DevelopmentEvidence evidence{};
  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::None;

  Sem::SemanticAssessmentInput in{};
  in.lineagePreservation = Sem::ClaimEvidenceStatus::Pass;
  in.relations[0] = {
      Sem::ReferenceRole::Source, &source, Sem::ClaimEvidenceStatus::Pass};
  in.relations[1] = {
      Sem::ReferenceRole::Predecessor, &predecessor,
      Sem::ClaimEvidenceStatus::Pass};
  in.relations[2] = {
      Sem::ReferenceRole::ReturnTarget, &source,
      Sem::ClaimEvidenceStatus::Pass};
  in.trajectoryContextAvailable = true;
  in.trajectory = Sem::TrajectoryRole::Return;

  const auto facts =
      Sem::adaptLegacyEvidence(predecessor, candidate, evidence, request, in);

  assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::Source) ==
         Sem::StateRelation::Variation);
  assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::Predecessor) ==
         Sem::StateRelation::Variation);
  assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::ReturnTarget) ==
         Sem::StateRelation::Variation);
  assert(facts.lineage == Sem::LineageStatus::Continues);
  assert(facts.trajectory == Sem::TrajectoryRole::Return);
}

void same_local_change_can_be_none_or_development() {
  const auto a = makeLine(48, 0, 52, 96);
  const auto a1 = makeLine(48, 0, 55, 96);
  Dev::DevelopmentEvidence evidence{};
  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::None;

  Sem::SemanticAssessmentInput isolated{};
  isolated.lineagePreservation = Sem::ClaimEvidenceStatus::Pass;
  isolated.relations[0] = {
      Sem::ReferenceRole::Source, &a, Sem::ClaimEvidenceStatus::Pass};
  isolated.trajectoryContextAvailable = true;
  isolated.trajectory = Sem::TrajectoryRole::None;

  Sem::SemanticAssessmentInput directional = isolated;
  directional.trajectory = Sem::TrajectoryRole::Development;

  const auto isolatedFacts =
      Sem::adaptLegacyEvidence(a, a1, evidence, request, isolated);
  const auto developmentFacts =
      Sem::adaptLegacyEvidence(a, a1, evidence, request, directional);

  assert(Sem::stateRelationFor(isolatedFacts, Sem::ReferenceRole::Source) ==
         Sem::StateRelation::Variation);
  assert(Sem::stateRelationFor(developmentFacts, Sem::ReferenceRole::Source) ==
         Sem::StateRelation::Variation);
  assert(isolatedFacts.trajectory == Sem::TrajectoryRole::None);
  assert(developmentFacts.trajectory == Sem::TrajectoryRole::Development);
}

void b7_tiny_genre_violation_does_not_force_new_idea() {
  const auto source = makeLine(48, 0, 52, 96);
  auto candidate = source;
  candidate.events[0].startTick = 12;

  Dev::DevelopmentEvidence evidence{};
  evidence.rhythm.onsetsChanged = true;
  evidence.rhythm.theOnePreserved = false;

  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::Displace;
  request.genreId = static_cast<uint8_t>(GenerativeMode::FunkSoul);

  Sem::SemanticAssessmentInput in{};
  in.lineagePreservation = Sem::ClaimEvidenceStatus::Pass;
  in.relations[0] = {
      Sem::ReferenceRole::Source, &source, Sem::ClaimEvidenceStatus::Pass};

  const auto facts =
      Sem::adaptLegacyEvidence(source, candidate, evidence, request, in);

  assert(facts.lineage == Sem::LineageStatus::Continues);
  assert(facts.genre == Sem::GenreStatus::Violation);
  assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::Source) ==
         Sem::StateRelation::Variation);
}

void b8_unavailable_root_capability_stays_local() {
  const auto source = makeLine(48, 0, 52, 96);
  Dev::DevelopmentEvidence evidence{};
  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::Extend;

  const auto facts =
      Sem::adaptLegacyEvidence(source, source, evidence, request);

  assert(Sem::capabilityFor(
             facts, Sem::CapabilityClaim::HarmonicRootPreservation) ==
         Sem::CapabilityStatus::Unavailable);
  assert(facts.genre == Sem::GenreStatus::Unknown);
  assert(facts.operation == Sem::OperationConformance::Unknown);
  assert(facts.lineage == Sem::LineageStatus::Unknown);
}

void b9_operation_violation_is_not_genre_violation() {
  const auto source = makeLine(60, 0, 80, 96);
  RuntimeSynthEventBuffer candidate{};
  Dev::DevelopmentEvidence evidence{};
  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::Revoice;
  request.genreId = static_cast<uint8_t>(GenerativeMode::Acid);

  Dev::transformRevoice(source, request, candidate, evidence);
  assert(evidence.bass.contourPreserved == Dev::TriState::Fail);

  Sem::SemanticAssessmentInput in{};
  in.genreRequirements = Sem::ClaimEvidenceStatus::Pass;

  const auto facts =
      Sem::adaptLegacyEvidence(source, candidate, evidence, request, in);

  assert(facts.operation == Sem::OperationConformance::Violated);
  assert(facts.genre == Sem::GenreStatus::Allowed);
  assert(Sem::capabilityFor(
             facts, Sem::CapabilityClaim::ContourPreservation) ==
         Sem::CapabilityStatus::Available);
}

void b10_genre_allowed_new_idea_is_representable() {
  const auto source = makeLine(48, 0, 52, 96);
  const auto candidate = makeLine(61, 24, 67, 120);
  Dev::DevelopmentEvidence evidence{};
  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::None;

  Sem::SemanticAssessmentInput in{};
  in.lineagePreservation = Sem::ClaimEvidenceStatus::Fail;
  in.genreRequirements = Sem::ClaimEvidenceStatus::Pass;
  in.relations[0] = {
      Sem::ReferenceRole::Source, &source, Sem::ClaimEvidenceStatus::Fail};

  const auto facts =
      Sem::adaptLegacyEvidence(source, candidate, evidence, request, in);

  assert(facts.lineage == Sem::LineageStatus::NewIdea);
  assert(facts.genre == Sem::GenreStatus::Allowed);
  assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::Source) ==
         Sem::StateRelation::Unknown);
}

void unknown_is_local() {
  const auto source = makeLine(48, 0, 52, 96);
  Dev::DevelopmentEvidence evidence{};
  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::Extend;

  Sem::SemanticAssessmentInput in{};
  in.lineagePreservation = Sem::ClaimEvidenceStatus::Pass;
  in.relations[0] = {
      Sem::ReferenceRole::Source, &source, Sem::ClaimEvidenceStatus::Pass};

  const auto facts =
      Sem::adaptLegacyEvidence(source, source, evidence, request, in);

  assert(facts.lineage == Sem::LineageStatus::Continues);
  assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::Source) ==
         Sem::StateRelation::Exact);
  assert(Sem::capabilityFor(
             facts, Sem::CapabilityClaim::HarmonicRootPreservation) ==
         Sem::CapabilityStatus::Unavailable);
  assert(facts.genre == Sem::GenreStatus::Unknown);
}

}  // namespace

int main() {
  exact_repeat_and_return_are_distinct();
  transformed_return_uses_explicit_references();
  same_local_change_can_be_none_or_development();
  b7_tiny_genre_violation_does_not_force_new_idea();
  b8_unavailable_root_capability_stays_local();
  b9_operation_violation_is_not_genre_violation();
  b10_genre_allowed_new_idea_is_representable();
  unknown_is_local();
  std::puts("0.9.14 D0-C semantic adapter: PASS");
  return 0;
}
