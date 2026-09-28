#include "src/dsp/development_semantic_adapter.h"

#include <cassert>
#include <cstdio>

using PhraseRuntime::RuntimeSynthEventBuffer;
namespace Sem = GroovePuterDevelopmentSemantic;
namespace Dev = GroovePuterDevelopment;

namespace {

RuntimeSynthEventBuffer line(uint16_t firstTick,
                             uint8_t firstNote = 48,
                             uint16_t secondTick = 96,
                             uint8_t secondNote = 52) {
  RuntimeSynthEventBuffer b{};
  b.lengthTicks = PhraseRuntime::kTicksPerBar;
  b.count = 2;
  b.events[0].startTick = firstTick;
  b.events[0].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[0].note = firstNote;
  b.events[0].velocity = 100;
  b.events[0].probability = 100;
  b.events[1].startTick = secondTick;
  b.events[1].durationSubticks = 24u * PhraseRuntime::kSubticksPerTick;
  b.events[1].note = secondNote;
  b.events[1].velocity = 100;
  b.events[1].probability = 100;
  return b;
}

Dev::DevelopmentRequest funkRequest() {
  Dev::DevelopmentRequest request{};
  request.transformation = Dev::TransformationKind::Displace;
  request.genreId = static_cast<uint8_t>(GenerativeMode::FunkSoul);
  request.requireTheOne = true;
  return request;
}

void a_presence_on_both_is_only_an_observable() {
  const auto source = line(0);
  const auto candidate = line(0, 55);
  Dev::DevelopmentEvidence evidence{};
  const auto request = funkRequest();

  assert(Sem::hasPrimaryDownbeatOnset(source));
  assert(Sem::hasPrimaryDownbeatOnset(candidate));

  const auto facts =
      Sem::adaptLegacyEvidence(source, candidate, evidence, request);

  assert(Sem::capabilityFor(
             facts, Sem::CapabilityClaim::PrimaryDownbeatOnsetPresence) ==
         Sem::CapabilityStatus::Available);
  assert(facts.genre == Sem::GenreStatus::Unknown);
  assert(facts.lineage == Sem::LineageStatus::Unknown);
  assert(facts.trajectory == Sem::TrajectoryRole::Unknown);
}

void b_lost_downbeat_does_not_become_complete_genre_violation() {
  const auto source = line(0);
  const auto candidate = line(12);
  Dev::DevelopmentEvidence evidence{};
  evidence.rhythm.onsetsChanged = true;
  evidence.rhythm.theOnePreserved = false;
  const auto request = funkRequest();

  assert(Sem::hasPrimaryDownbeatOnset(source));
  assert(!Sem::hasPrimaryDownbeatOnset(candidate));

  const auto facts =
      Sem::adaptLegacyEvidence(source, candidate, evidence, request);

  assert(Sem::capabilityFor(
             facts, Sem::CapabilityClaim::PrimaryDownbeatOnsetPresence) ==
         Sem::CapabilityStatus::Available);
  assert(facts.genre == Sem::GenreStatus::Unknown);
  assert(facts.lineage == Sem::LineageStatus::Unknown);
  assert(facts.trajectory == Sem::TrajectoryRole::Unknown);
}

void c_absence_on_both_has_no_invented_negative_meaning() {
  const auto source = line(12);
  const auto candidate = line(24);
  Dev::DevelopmentEvidence evidence{};
  const auto request = funkRequest();

  assert(!Sem::hasPrimaryDownbeatOnset(source));
  assert(!Sem::hasPrimaryDownbeatOnset(candidate));

  const auto facts =
      Sem::adaptLegacyEvidence(source, candidate, evidence, request);

  assert(Sem::capabilityFor(
             facts, Sem::CapabilityClaim::PrimaryDownbeatOnsetPresence) ==
         Sem::CapabilityStatus::Available);
  assert(facts.genre == Sem::GenreStatus::Unknown);
  assert(facts.lineage == Sem::LineageStatus::Unknown);
  assert(facts.trajectory == Sem::TrajectoryRole::Unknown);
}

void d_candidate_downbeat_does_not_certify_relation_or_genre() {
  const auto source = line(24, 36, 144, 39);
  const auto unrelatedCandidate = line(0, 73, 168, 80);
  Dev::DevelopmentEvidence evidence{};
  const auto request = funkRequest();

  assert(!Sem::hasPrimaryDownbeatOnset(source));
  assert(Sem::hasPrimaryDownbeatOnset(unrelatedCandidate));

  const auto facts =
      Sem::adaptLegacyEvidence(source, unrelatedCandidate, evidence, request);

  assert(Sem::capabilityFor(
             facts, Sem::CapabilityClaim::PrimaryDownbeatOnsetPresence) ==
         Sem::CapabilityStatus::Available);
  assert(facts.genre == Sem::GenreStatus::Unknown);
  assert(facts.lineage == Sem::LineageStatus::Unknown);
  assert(facts.trajectory == Sem::TrajectoryRole::Unknown);
  assert(Sem::stateRelationFor(facts, Sem::ReferenceRole::Source) ==
         Sem::StateRelation::NotApplicable);
}

}  // namespace

int main() {
  a_presence_on_both_is_only_an_observable();
  b_lost_downbeat_does_not_become_complete_genre_violation();
  c_absence_on_both_has_no_invented_negative_meaning();
  d_candidate_downbeat_does_not_certify_relation_or_genre();
  std::puts("0.9.14 D0-D1 claim strength runtime witnesses: PASS");
  return 0;
}
