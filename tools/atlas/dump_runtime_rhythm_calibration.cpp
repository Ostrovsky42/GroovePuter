#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/relationship_resolver.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint8_t kSamplesPerArchetype = 64;
constexpr uint8_t kDrumRoleCount = 5;
constexpr double kRoleWeights[kDrumRoleCount] = {3.0, 3.0, 1.5, 1.0, 1.0};

struct DrumSample {
  StepMask masks[kDrumRoleCount]{};
};

unsigned bitCount(StepMask mask) {
  return static_cast<unsigned>(__builtin_popcount(static_cast<unsigned>(mask)));
}

double jaccard(StepMask lhs, StepMask rhs) {
  const StepMask unionMask = static_cast<StepMask>(lhs | rhs);
  if (unionMask == 0) return 0.0;
  const StepMask intersection = static_cast<StepMask>(lhs & rhs);
  return 1.0 - static_cast<double>(bitCount(intersection)) /
                   static_cast<double>(bitCount(unionMask));
}

double drumDistance(const DrumSample& lhs, const DrumSample& rhs) {
  double total = 0.0;
  double weightTotal = 0.0;
  for (uint8_t role = 0; role < kDrumRoleCount; ++role) {
    total += kRoleWeights[role] * jaccard(lhs.masks[role], rhs.masks[role]);
    weightTotal += kRoleWeights[role];
  }
  return total / weightTotal;
}

double quantile(std::vector<double> values, double p) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  if (values.size() == 1) return values.front();
  const double position = static_cast<double>(values.size() - 1) * p;
  const size_t low = static_cast<size_t>(std::floor(position));
  const size_t high = static_cast<size_t>(std::ceil(position));
  if (low == high) return values[low];
  return values[low] * (static_cast<double>(high) - position) +
         values[high] * (position - static_cast<double>(low));
}

uint64_t sampleFingerprint(const DrumSample& sample) {
  uint64_t hash = 1469598103934665603ull;
  for (uint8_t role = 0; role < kDrumRoleCount; ++role) {
    hash ^= sample.masks[role];
    hash *= 1099511628211ull;
  }
  return hash;
}

bool coveredBy(const RhythmArchetype& target,
               const RhythmPhrasePlan& plan) {
  return planRespectsProtectedSpace(target, plan) &&
         planRespectsLaneBounds(target, plan) &&
         hardRelationshipsSatisfied(target, structuralOccupancy(plan));
}

}  // namespace

int main() {
  using namespace GroovePuterRhythm::ReferenceVocabulary;

  const RhythmCatalogView& vocabulary = catalog();
  if (vocabulary.archetypeCount != 20 || definitionCount() != 20) return 2;

  std::array<std::array<DrumSample, kSamplesPerArchetype>, 20> samples{};
  std::array<std::array<RhythmPhrasePlan, kSamplesPerArchetype>, 20> plans{};
  std::array<std::array<uint8_t, 20>, 20> confusion{};
  std::array<std::vector<double>, 20> selfDistances{};
  std::vector<double> allSelfDistances;

  std::cout << "FORMAT\tGROOVEPUTER_RUNTIME_RHYTHM_CALIBRATION_V1\n";
  std::cout << std::fixed << std::setprecision(6);

  for (uint8_t sourceIndex = 0; sourceIndex < definitionCount(); ++sourceIndex) {
    const Definition& sourceDefinition = definition(sourceIndex);
    const RhythmArchetype* source = archetypeFor(sourceDefinition.key);
    if (source == nullptr) return 3;

    std::set<uint64_t> distinct;
    uint8_t valid = 0;
    for (uint8_t seedIndex = 0; seedIndex < kSamplesPerArchetype; ++seedIndex) {
      RhythmRealizationRequest request{};
      request.catalog = &vocabulary;
      request.archetypeId = source->id;
      request.phraseBars = 1;
      request.level = RealizationLevel::P1Canonical;
      request.generation = GenerationContext{
          static_cast<uint32_t>(seedIndex + 1u),
          static_cast<uint16_t>(sourceIndex)};

      const RhythmRealizationResult result = realizeRhythmPhrase(request);
      if (result.status == RealizationStatus::InvalidConstraintSet) return 4;
      ++valid;
      plans[sourceIndex][seedIndex] = result.plan;
      const PhraseOccupancy occupancy = structuralOccupancy(result.plan);
      for (uint8_t role = 0; role < kDrumRoleCount; ++role) {
        samples[sourceIndex][seedIndex].masks[role] = occupancy.roleMasks[0][role];
      }
      distinct.insert(sampleFingerprint(samples[sourceIndex][seedIndex]));

      for (uint8_t targetIndex = 0; targetIndex < definitionCount(); ++targetIndex) {
        const RhythmArchetype* target = archetypeFor(definition(targetIndex).key);
        if (target == nullptr) return 5;
        if (coveredBy(*target, result.plan)) {
          ++confusion[sourceIndex][targetIndex];
        }
      }
    }

    for (uint8_t left = 0; left < kSamplesPerArchetype; ++left) {
      for (uint8_t right = static_cast<uint8_t>(left + 1);
           right < kSamplesPerArchetype;
           ++right) {
        const double distance = drumDistance(
            samples[sourceIndex][left], samples[sourceIndex][right]);
        selfDistances[sourceIndex].push_back(distance);
        allSelfDistances.push_back(distance);
      }
    }

    const uint8_t ownCovered = confusion[sourceIndex][sourceIndex];
    if (ownCovered != kSamplesPerArchetype) return 6;
    std::cout << "SELF\t" << sourceDefinition.name << '\t'
              << static_cast<unsigned>(valid) << '\t'
              << distinct.size() << '\t'
              << static_cast<unsigned>(ownCovered) << '\t'
              << quantile(selfDistances[sourceIndex], 0.50) << '\t'
              << quantile(selfDistances[sourceIndex], 0.90) << '\t'
              << quantile(selfDistances[sourceIndex], 1.00) << '\n';
  }

  uint32_t crossCovered = 0;
  uint32_t crossTotal = 0;
  for (uint8_t sourceIndex = 0; sourceIndex < definitionCount(); ++sourceIndex) {
    for (uint8_t targetIndex = 0; targetIndex < definitionCount(); ++targetIndex) {
      if (sourceIndex == targetIndex) continue;
      const uint8_t covered = confusion[sourceIndex][targetIndex];
      crossCovered += covered;
      crossTotal += kSamplesPerArchetype;
      std::cout << "CONF\t" << definition(sourceIndex).name << '\t'
                << definition(targetIndex).name << '\t'
                << static_cast<unsigned>(covered) << '\t'
                << static_cast<unsigned>(kSamplesPerArchetype) << '\n';
    }
  }

  std::cout << "AGG_SELF\t" << allSelfDistances.size() << '\t'
            << quantile(allSelfDistances, 0.50) << '\t'
            << quantile(allSelfDistances, 0.90) << '\t'
            << quantile(allSelfDistances, 0.95) << '\t'
            << quantile(allSelfDistances, 1.00) << '\n';
  std::cout << "AGG_CONF\t" << crossCovered << '\t' << crossTotal << '\t'
            << (crossTotal ? static_cast<double>(crossCovered) /
                                 static_cast<double>(crossTotal)
                           : 0.0)
            << '\n';
  std::cout << "COUNT\t" << static_cast<unsigned>(definitionCount()) << '\n';
  return 0;
}
