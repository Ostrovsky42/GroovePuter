#include "genre_harmonic_policy.h"

#include "scenes.h"
#include "generated/genre_harmonic_policy_data.h"

namespace GroovePuterHarmony {
namespace {

constexpr uint8_t kMaxCurrentRecipeId = 21;

bool mapRecipe(uint8_t recipe, HarmonicStyleId& out) {
  switch (recipe) {
    case 1:
    case 8:
    case 9:
      out = HarmonicStyleId::UkGarage;
      return true;
    case 2:
    case 18:
    case 19:
    case 20:
    case 21:
      out = HarmonicStyleId::DnB;
      return true;
    case 3:
      out = HarmonicStyleId::Footwork;
      return true;
    case 5:
    case 10:
    case 11:
      out = HarmonicStyleId::DubTechno;
      return true;
    case 7:
      out = HarmonicStyleId::Acid;
      return true;
    case 12:
    case 13:
    case 15:
      out = HarmonicStyleId::LoFi;
      return true;
    case 16:
    case 17:
      out = HarmonicStyleId::HipHop;
      return true;
    default:
      return false;
  }
}

bool mapBaseMode(uint8_t mode, HarmonicStyleId& out) {
  switch (mode) {
    case 0:
      out = HarmonicStyleId::Acid;
      return true;
    case 1:
      out = HarmonicStyleId::Synthwave;
      return true;
    case 3:
      out = HarmonicStyleId::Electro;
      return true;
    case 11:
      out = HarmonicStyleId::HipHop;
      return true;
    case 13:
      out = HarmonicStyleId::UkGarage;
      return true;
    case 14:
      out = HarmonicStyleId::DnB;
      return true;
    case 15:
      out = HarmonicStyleId::LoFi;
      return true;
    default:
      return false;
  }
}

}  // namespace

const GenreHarmonicPolicy* policyFor(HarmonicStyleId style) {
  const uint8_t id = static_cast<uint8_t>(style);
  if (id >= GeneratedGenrePolicy::kPolicyCount) return nullptr;
  return &GeneratedGenrePolicy::kPolicies[id];
}

bool policyAllowsCandidate(const GenreHarmonicPolicy& policy,
                           const HarmonicCandidatePolicyFacts& facts) {
  if (facts.qualityId >= 32 || facts.scaleId >= 32 ||
      static_cast<uint8_t>(facts.section) >
          static_cast<uint8_t>(SectionIntent::Return) || facts.usesPortal > 1 ||
      facts.isHold > 1 || facts.structuralMotionAllowed > 1 ||
      facts.requestsFunctionalPreparation > 1) {
    return false;
  }
  if ((policy.qualityMask & (1u << facts.qualityId)) == 0 ||
      (policy.scaleMask & (1u << facts.scaleId)) == 0 ||
      facts.colorCount > policy.colorBudget ||
      (facts.requestsFunctionalPreparation && !policy.functionalPreparation)) {
    return false;
  }
  if (!facts.structuralMotionAllowed && !facts.isHold) return false;
  if (facts.usesPortal &&
      (!policy.allowPortals || facts.section == SectionIntent::Take ||
       facts.section == SectionIntent::Keep)) {
    return false;
  }

  uint8_t outsideLimit = policy.maxOutsideBreak;
  uint8_t changeLimit = policy.changesPerSection;
  uint8_t modulationLimit = policy.maxModulationsBreak;
  switch (facts.section) {
    case SectionIntent::Take:
      outsideLimit = policy.maxOutsideTakeKeep;
      changeLimit = changeLimit < 1 ? changeLimit : 1;
      modulationLimit = 0;
      break;
    case SectionIntent::Keep:
      outsideLimit = policy.maxOutsideTakeKeep;
      changeLimit = changeLimit < 2 ? changeLimit : 2;
      modulationLimit = 0;
      break;
    case SectionIntent::Develop:
      outsideLimit = policy.maxOutsideDevelop;
      modulationLimit = policy.maxModulationsDevelop;
      break;
    case SectionIntent::Break:
      outsideLimit = policy.maxOutsideBreak;
      modulationLimit = policy.maxModulationsBreak;
      break;
    case SectionIntent::Return:
      outsideLimit = policy.maxOutsideBreak;
      changeLimit = 255;  // RETURN depth is bounded by the directed path planner.
      modulationLimit = policy.maxModulationsBreak;
      break;
  }
  return facts.targetOutOfScaleTones <= outsideLimit &&
         facts.changesInSection <= changeLimit &&
         facts.modulationsInSection <= modulationLimit;
}

HarmonyStatus harmonicStyleFor(const ::GenreSettings& settings,
                               HarmonicStyleId& out) {
  if (settings.generativeMode >= 16 || settings.recipe > kMaxCurrentRecipeId ||
      settings.morphTarget > kMaxCurrentRecipeId) {
    return HarmonyStatus::Invalid;
  }
  if (settings.morphAmount > 0 && settings.morphTarget != settings.recipe)
    return HarmonyStatus::Unsupported;

  HarmonicStyleId result{};
  const bool mapped = settings.recipe == 0
                          ? mapBaseMode(settings.generativeMode, result)
                          : mapRecipe(settings.recipe, result);
  if (!mapped) return HarmonyStatus::Unsupported;
  out = result;
  return HarmonyStatus::Ready;
}

}  // namespace GroovePuterHarmony
