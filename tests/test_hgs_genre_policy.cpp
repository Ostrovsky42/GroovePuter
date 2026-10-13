#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/harmony/genre_harmonic_policy.h"
#include "src/generation/harmony/generated/genre_harmonic_policy_data.h"
#include "scenes.h"

using namespace GroovePuterHarmony;

namespace {
bool expectedRecipe(uint8_t recipe, HarmonicStyleId& style) {
  switch (recipe) {
    case 1: case 8: case 9: style = HarmonicStyleId::UkGarage; return true;
    case 2: case 18: case 19: case 20: case 21: style = HarmonicStyleId::DnB; return true;
    case 3: style = HarmonicStyleId::Footwork; return true;
    case 5: case 10: case 11: style = HarmonicStyleId::DubTechno; return true;
    case 7: style = HarmonicStyleId::Acid; return true;
    case 12: case 13: case 15: style = HarmonicStyleId::LoFi; return true;
    case 16: case 17: style = HarmonicStyleId::HipHop; return true;
    default: return false;
  }
}

bool expectedBase(uint8_t mode, HarmonicStyleId& style) {
  switch (mode) {
    case 0: style = HarmonicStyleId::Acid; return true;
    case 1: style = HarmonicStyleId::Synthwave; return true;
    case 3: style = HarmonicStyleId::Electro; return true;
    case 11: style = HarmonicStyleId::HipHop; return true;
    case 13: style = HarmonicStyleId::UkGarage; return true;
    case 14: style = HarmonicStyleId::DnB; return true;
    case 15: style = HarmonicStyleId::LoFi; return true;
    default: return false;
  }
}

void expectMapping(const GenreSettings& settings, bool expected,
                   HarmonicStyleId expectedStyle) {
  HarmonicStyleId actual = HarmonicStyleId::Salsa;
  const HarmonyStatus status = harmonicStyleFor(settings, actual);
  assert(status == (expected ? HarmonyStatus::Ready : HarmonyStatus::Unsupported));
  if (expected) {
    assert(actual == expectedStyle);
  } else {
    assert(actual == HarmonicStyleId::Salsa);  // Failure leaves output untouched.
  }
}
}  // namespace

int main() {
  static_assert(static_cast<uint8_t>(GenerativeMode::Acid) == 0);
  static_assert(static_cast<uint8_t>(GenerativeMode::Outrun) == 1);
  static_assert(static_cast<uint8_t>(GenerativeMode::Darksynth) == 2);
  static_assert(static_cast<uint8_t>(GenerativeMode::Electro) == 3);
  static_assert(static_cast<uint8_t>(GenerativeMode::LoFi) == 15);
  assert(GeneratedGenrePolicy::kPolicyCount == 17);

  for (uint8_t id = 0; id < 17; ++id) {
    const auto style = static_cast<HarmonicStyleId>(id);
    const GenreHarmonicPolicy* policy = policyFor(style);
    assert(policy != nullptr);
    assert(static_cast<uint8_t>(policy->style) == id);
    assert(policy->qualityMask != 0);
    assert(policy->scaleMask != 0);
    assert(policy->maxOutsideTakeKeep <= 1);
    assert(policy->maxOutsideDevelop <= 4);
    assert(policy->maxOutsideBreak <= 4);
    assert(policy->maxModulationsDevelop <= 2);
    assert(policy->maxModulationsBreak <= 2);
    assert(policy->changesPerSection >= 1 && policy->changesPerSection <= 4);

    HarmonicCandidatePolicyFacts within{
        0, 0, SectionIntent::Develop, 0, 1, 0, 0, 0, 0, 1, 0};
    while ((policy->qualityMask & (1u << within.qualityId)) == 0) ++within.qualityId;
    while ((policy->scaleMask & (1u << within.scaleId)) == 0) ++within.scaleId;
    assert(policyAllowsCandidate(*policy, within));

    auto over = within;
    over.qualityId = 31;
    assert(!policyAllowsCandidate(*policy, over));
    over = within;
    over.scaleId = 31;
    assert(!policyAllowsCandidate(*policy, over));
    over = within;
    over.targetOutOfScaleTones =
        static_cast<uint8_t>(policy->maxOutsideDevelop + 1);
    assert(!policyAllowsCandidate(*policy, over));
    over = within;
    over.changesInSection = static_cast<uint8_t>(policy->changesPerSection + 1);
    assert(!policyAllowsCandidate(*policy, over));
    over = within;
    over.modulationsInSection =
        static_cast<uint8_t>(policy->maxModulationsDevelop + 1);
    assert(!policyAllowsCandidate(*policy, over));
    over = within;
    over.colorCount = static_cast<uint8_t>(policy->colorBudget + 1);
    assert(!policyAllowsCandidate(*policy, over));
    over = within;
    over.usesPortal = 1;
    assert(policyAllowsCandidate(*policy, over) == (policy->allowPortals != 0));
    over.section = SectionIntent::Take;
    assert(!policyAllowsCandidate(*policy, over));
    over = within;
    over.structuralMotionAllowed = 0;
    assert(!policyAllowsCandidate(*policy, over));
    over = within;
    over.requestsFunctionalPreparation = 1;
    assert(policyAllowsCandidate(*policy, over) ==
           (policy->functionalPreparation != 0));
  }
  assert(policyFor(static_cast<HarmonicStyleId>(17)) == nullptr);
  assert(policyFor(static_cast<HarmonicStyleId>(255)) == nullptr);
  assert(!policyFor(HarmonicStyleId::Acid)->functionalPreparation);
  assert(policyFor(HarmonicStyleId::Salsa)->functionalPreparation);
  assert(policyFor(HarmonicStyleId::Salsa)->priors.prl == 0);
  assert(policyFor(HarmonicStyleId::Salsa)->qualityMask != 0);

  HarmonicCandidatePolicyFacts salsaCToAm{
      1, 1, SectionIntent::Develop, 0, 1, 0, 0, 0, 0, 1, 0};
  assert(policyAllowsCandidate(*policyFor(HarmonicStyleId::Salsa), salsaCToAm));
  HarmonicCandidatePolicyFacts acidCandidate{
      0, 1, SectionIntent::Take, 1, 1, 0, 0, 0, 0, 1, 0};
  assert(policyAllowsCandidate(*policyFor(HarmonicStyleId::Acid), acidCandidate));
  acidCandidate.requestsFunctionalPreparation = 1;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Acid), acidCandidate));
  acidCandidate.requestsFunctionalPreparation = 0;
  acidCandidate.qualityId = 2;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Acid), acidCandidate));
  acidCandidate.qualityId = 0;
  acidCandidate.structuralMotionAllowed = 0;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Acid), acidCandidate));
  acidCandidate.isHold = 1;
  acidCandidate.changesInSection = 0;
  assert(policyAllowsCandidate(*policyFor(HarmonicStyleId::Acid), acidCandidate));
  salsaCToAm.targetOutOfScaleTones = 2;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Salsa), salsaCToAm));
  salsaCToAm.targetOutOfScaleTones = 0;
  salsaCToAm.section = SectionIntent::Take;
  salsaCToAm.modulationsInSection = 1;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Salsa), salsaCToAm));
  salsaCToAm.modulationsInSection = 0;
  salsaCToAm.scaleId = 6;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Salsa), salsaCToAm));
  salsaCToAm.scaleId = 1;
  salsaCToAm.changesInSection = 2;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Salsa), salsaCToAm));
  salsaCToAm.changesInSection = 1;
  salsaCToAm.usesPortal = 1;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Salsa), salsaCToAm));
  salsaCToAm.section = SectionIntent::Develop;
  salsaCToAm.usesPortal = 0;
  salsaCToAm.colorCount = 2;
  assert(!policyAllowsCandidate(*policyFor(HarmonicStyleId::Salsa), salsaCToAm));
  salsaCToAm.colorCount = 1;
  assert(policyAllowsCandidate(*policyFor(HarmonicStyleId::Salsa), salsaCToAm));

  constexpr uint8_t morphAmounts[] = {0, 128, 255};
  for (uint8_t mode = 0; mode < 16; ++mode) {
    for (uint8_t recipe = 0; recipe < 22; ++recipe) {
      for (uint8_t amount : morphAmounts) {
        GenreSettings settings{};
        settings.generativeMode = mode;
        settings.recipe = recipe;
        settings.morphTarget = static_cast<uint8_t>((recipe + 1) % 22);
        settings.morphAmount = amount;
        if (amount > 0) {
          expectMapping(settings, false, HarmonicStyleId::Acid);
        } else {
          HarmonicStyleId expected{};
          const bool mapped = recipe == 0 ? expectedBase(mode, expected)
                                          : expectedRecipe(recipe, expected);
          expectMapping(settings, mapped, expected);
        }
      }

      GenreSettings sameTarget{};
      sameTarget.generativeMode = mode;
      sameTarget.recipe = recipe;
      sameTarget.morphTarget = recipe;
      sameTarget.morphAmount = 255;
      HarmonicStyleId expected{};
      const bool mapped = recipe == 0 ? expectedBase(mode, expected)
                                      : expectedRecipe(recipe, expected);
      expectMapping(sameTarget, mapped, expected);
    }
  }

  GenreSettings invalid{};
  invalid.generativeMode = 16;
  HarmonicStyleId output = HarmonicStyleId::Salsa;
  assert(harmonicStyleFor(invalid, output) == HarmonyStatus::Invalid);
  assert(output == HarmonicStyleId::Salsa);
  invalid.generativeMode = 0;
  invalid.recipe = 22;
  assert(harmonicStyleFor(invalid, output) == HarmonyStatus::Invalid);
  invalid.recipe = 0;
  invalid.morphTarget = 22;
  assert(harmonicStyleFor(invalid, output) == HarmonyStatus::Invalid);
  assert(output == HarmonicStyleId::Salsa);

  std::cout << "HGS genre policy and catalog binding: PASS\n";
  return 0;
}
