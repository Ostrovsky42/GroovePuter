#include <cassert>
#include <cstdint>

#include "src/generation/generation_backend.h"
#include "src/generation/materialization/pattern_materializer.h"
#include "src/generation/shadow/pattern_shadow_metrics.h"

using namespace GroovePuterRhythm;

namespace {

bool sameDrumStep(const DrumStep& a, const DrumStep& b) {
  return a.hit == b.hit && a.accent == b.accent &&
         a.velocity == b.velocity && a.timing == b.timing &&
         a.fx == b.fx && a.fxParam == b.fxParam &&
         a.probability == b.probability;
}

bool sameDrums(const DrumPatternSet& a, const DrumPatternSet& b) {
  for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
    for (int step = 0; step < DrumPattern::kSteps; ++step) {
      if (!sameDrumStep(a.voices[voice].steps[step],
                        b.voices[voice].steps[step])) {
        return false;
      }
    }
  }
  if (a.groove.swing != b.groove.swing ||
      a.groove.humanize != b.groove.humanize) {
    return false;
  }
  return true;
}

bool sameSynthStep(const SynthStep& a, const SynthStep& b) {
  return a.note == b.note && a.slide == b.slide &&
         a.accent == b.accent && a.ghost == b.ghost &&
         a.velocity == b.velocity && a.timing == b.timing &&
         a.fx == b.fx && a.fxParam == b.fxParam &&
         a.probability == b.probability;
}

bool sameSynth(const SynthPattern& a, const SynthPattern& b) {
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    if (!sameSynthStep(a.steps[step], b.steps[step])) return false;
  }
  return true;
}

bool sameMaterialized(const MaterializedPatterns& a,
                      const MaterializedPatterns& b) {
  return sameDrums(a.drums, b.drums) &&
         sameSynth(a.synthA, b.synthA) &&
         sameSynth(a.synthB, b.synthB);
}

bool sameDiagnostics(const PatternMaterializationDiagnostics& a,
                     const PatternMaterializationDiagnostics& b) {
  return a.structuralEvents == b.structuralEvents &&
         a.secondaryEvents == b.secondaryEvents &&
         a.ghostEvents == b.ghostEvents &&
         a.accentEvents == b.accentEvents &&
         a.ignoredEvents == b.ignoredEvents &&
         a.deferredGateEvents == b.deferredGateEvents &&
         a.emittedRoles == b.emittedRoles &&
         a.ignoredRoles == b.ignoredRoles;
}

RhythmPhrasePlan basePlan() {
  RhythmPhrasePlan plan{};
  plan.barCount = 1;
  plan.level = RealizationLevel::P2Variation;
  plan.bars[0].function = BarFunction::Statement;

  RoleRhythmPlan& kick =
      plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)];
  kick.structural = stepBit(0) | stepBit(4);
  kick.secondary = stepBit(10);
  kick.ghosts = stepBit(14);
  kick.accents = stepBit(0);

  RoleRhythmPlan& bass =
      plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::BassRhythm)];
  bass.structural = stepBit(0);
  bass.secondary = stepBit(6);
  bass.heldGate = stepBit(0);
  bass.tieGate = stepBit(6);
  bass.accents = stepBit(6);

  return plan;
}

MaterializedPatterns sentinelDestination() {
  MaterializedPatterns value{};
  value.drums.voices[CLAP].steps[15].hit = true;
  value.drums.voices[CLAP].steps[15].accent = true;
  value.drums.voices[CLAP].steps[15].velocity = 77;
  value.drums.groove.swing = 0.25f;
  value.drums.groove.humanize = 0.5f;
  value.synthA.steps[2].note = 64;
  value.synthA.steps[2].accent = true;
  value.synthB.steps[3].note = 67;
  value.synthB.steps[3].ghost = true;
  return value;
}

void test_failure_is_transactional() {
  RhythmPhrasePlan plan = basePlan();
  PatternMaterializerBinding binding = standardDrumPatternBinding();

  MaterializedPatterns destination = sentinelDestination();
  const MaterializedPatterns before = destination;
  PatternMaterializationDiagnostics diagnostics{};
  diagnostics.structuralEvents = 91;
  diagnostics.emittedRoles = kAllRhythmRoles;
  const PatternMaterializationDiagnostics diagnosticsBefore = diagnostics;

  const PatternMaterializeStatus status = materializeRhythmPattern(
      plan, binding, destination, &diagnostics);
  assert(status == PatternMaterializeStatus::UnboundRole);
  assert(sameMaterialized(destination, before));
  assert(sameDiagnostics(diagnostics, diagnosticsBefore));

  plan.barCount = 2;
  const PatternMaterializeStatus badPlanStatus = materializeRhythmPattern(
      plan, binding, destination, &diagnostics);
  assert(badPlanStatus == PatternMaterializeStatus::InvalidPlan);
  assert(sameMaterialized(destination, before));
  assert(sameDiagnostics(diagnostics, diagnosticsBefore));
}

void test_explicit_synth_binding_and_deferred_gate() {
  const RhythmPhrasePlan plan = basePlan();
  PatternMaterializerBinding binding = standardDrumPatternBinding();
  SynthRhythmBinding& bass =
      binding.synthByRole[static_cast<uint8_t>(RhythmRole::BassRhythm)];
  bass.target = SynthPatternTarget::SynthB;
  bass.note = 36;

  MaterializedPatterns destination = sentinelDestination();
  PatternMaterializationDiagnostics diagnostics{};
  const PatternMaterializeStatus status = materializeRhythmPattern(
      plan, binding, destination, &diagnostics);
  assert(status == PatternMaterializeStatus::Ok);

  assert(destination.drums.voices[KICK].steps[0].hit);
  assert(destination.drums.voices[KICK].steps[0].accent);
  assert(destination.drums.voices[KICK].steps[0].velocity == 110);
  assert(destination.drums.voices[KICK].steps[10].hit);
  assert(destination.drums.voices[KICK].steps[10].velocity == 86);
  assert(destination.drums.voices[KICK].steps[14].hit);
  assert(destination.drums.voices[KICK].steps[14].velocity == 52);

  assert(destination.synthA.steps[0].note == -1);
  assert(destination.synthB.steps[0].note == 36);
  assert(destination.synthB.steps[6].note == 36);
  assert(!destination.synthB.steps[0].slide);
  assert(!destination.synthB.steps[6].slide);
  assert(destination.synthB.steps[6].accent);

  // Materializer owns rhythm topology only; FEEL remains inherited globally.
  assert(destination.drums.groove.swing == -1.0f);
  assert(destination.drums.groove.humanize == -1.0f);

  assert(diagnostics.structuralEvents == 3);
  assert(diagnostics.secondaryEvents == 2);
  assert(diagnostics.ghostEvents == 1);
  assert(diagnostics.accentEvents == 2);
  assert(diagnostics.deferredGateEvents == 2);
  assert((diagnostics.emittedRoles & rhythmRoleBit(RhythmRole::Kick)) != 0);
  assert((diagnostics.emittedRoles & rhythmRoleBit(RhythmRole::BassRhythm)) != 0);
}

void test_ignored_role_is_explicit() {
  const RhythmPhrasePlan plan = basePlan();
  PatternMaterializerBinding binding = standardDrumPatternBinding(
      rhythmRoleBit(RhythmRole::BassRhythm));

  MaterializedPatterns destination{};
  PatternMaterializationDiagnostics diagnostics{};
  const PatternMaterializeStatus status = materializeRhythmPattern(
      plan, binding, destination, &diagnostics);
  assert(status == PatternMaterializeStatus::Ok);
  assert(diagnostics.ignoredEvents == 2);
  assert(diagnostics.ignoredRoles == rhythmRoleBit(RhythmRole::BassRhythm));
  assert(destination.synthA.steps[0].note == -1);
  assert(destination.synthB.steps[0].note == -1);
}

void test_binding_collisions_are_rejected() {
  RhythmPhrasePlan plan{};
  plan.barCount = 1;
  plan.bars[0].function = BarFunction::Statement;
  plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::Kick)].structural =
      stepBit(0);
  plan.bars[0].roles[static_cast<uint8_t>(RhythmRole::Backbeat)].structural =
      stepBit(4);

  PatternMaterializerBinding binding{};
  binding.drumVoiceByRole[static_cast<uint8_t>(RhythmRole::Kick)] = KICK;
  binding.drumVoiceByRole[static_cast<uint8_t>(RhythmRole::Backbeat)] = KICK;

  MaterializedPatterns destination = sentinelDestination();
  const MaterializedPatterns before = destination;
  assert(materializeRhythmPattern(plan, binding, destination) ==
         PatternMaterializeStatus::InvalidBinding);
  assert(sameMaterialized(destination, before));
}

void test_shadow_metrics_are_observational() {
  DrumPatternSet legacyDrums{};
  SynthPattern legacyA{};
  SynthPattern legacyB{};
  legacyDrums.voices[KICK].steps[0].hit = true;
  legacyDrums.voices[SNARE].steps[4].hit = true;
  legacyDrums.voices[SNARE].steps[4].accent = true;
  legacyA.steps[2].note = 60;

  MaterializedPatterns vocabulary{};
  vocabulary.drums.voices[KICK].steps[0].hit = true;
  vocabulary.drums.voices[SNARE].steps[12].hit = true;
  vocabulary.drums.voices[SNARE].steps[12].accent = true;
  vocabulary.synthA.steps[2].note = 36;

  const DrumPatternSet legacyDrumsBefore = legacyDrums;
  const SynthPattern legacyABefore = legacyA;
  const MaterializedPatterns vocabularyBefore = vocabulary;

  const ShadowPatternMetrics drumsOnly = compareShadowPatterns(
      legacyDrums, legacyA, legacyB, vocabulary, ShadowDrums);
  assert(drumsOnly.legacyOnsets == 2);
  assert(drumsOnly.vocabularyOnsets == 2);
  assert(drumsOnly.sharedOnsets == 1);
  assert(drumsOnly.differingSlots == 2);
  assert(drumsOnly.legacyAccents == 1);
  assert(drumsOnly.vocabularyAccents == 1);
  assert(drumsOnly.sharedAccents == 0);

  const ShadowPatternMetrics withSynth = compareShadowPatterns(
      legacyDrums, legacyA, legacyB, vocabulary,
      static_cast<uint8_t>(ShadowDrums | ShadowSynthA));
  assert(withSynth.legacyOnsets == 3);
  assert(withSynth.vocabularyOnsets == 3);
  assert(withSynth.sharedOnsets == 2);

  assert(sameDrums(legacyDrums, legacyDrumsBefore));
  assert(sameSynth(legacyA, legacyABefore));
  assert(sameMaterialized(vocabulary, vocabularyBefore));
}

void test_backend_route_is_migration_only_value() {
  constexpr GenerationBackendRoute legacyOnly{
      GenerationBackend::LegacyAtlas,
      GenerationBackend::Vocabulary,
      false};
  static_assert(validGenerationBackendRoute(legacyOnly),
                "legacy route should be valid");

  constexpr GenerationBackendRoute shadow{
      GenerationBackend::LegacyProcedural,
      GenerationBackend::Vocabulary,
      true};
  static_assert(validGenerationBackendRoute(shadow),
                "shadow route should be valid");

  constexpr GenerationBackendRoute invalid{
      GenerationBackend::Vocabulary,
      GenerationBackend::Vocabulary,
      true};
  static_assert(!validGenerationBackendRoute(invalid),
                "shadow backend must differ from applied backend");
}

}  // namespace

int main() {
  test_failure_is_transactional();
  test_explicit_synth_binding_and_deferred_gate();
  test_ignored_role_is_explicit();
  test_binding_collisions_are_rejected();
  test_shadow_metrics_are_observational();
  test_backend_route_is_migration_only_value();
  return 0;
}
