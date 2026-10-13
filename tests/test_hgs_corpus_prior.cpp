#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/generation/harmony/corpus_prior.h"
#include "src/generation/harmony/generated/corpus_prior_data.h"

using namespace GroovePuterHarmony;

namespace {
constexpr CanonicalFunctionToken kI{0, 0, 0, 0, 2, 255, 0};
constexpr CanonicalFunctionToken kV{0, 4, -5, 0, 2, 255, 0};
constexpr CanonicalFunctionToken kVi{0, 5, -3, 1, 2, 255, 0};
constexpr CanonicalFunctionToken kUnknownPrevious{0, 2, 2, 0, 2, 255, 0};
constexpr CanonicalFunctionToken kUnknownTarget{0, 1, -4, 1, 2, 255, 0};

// IDs are little-endian. Fixed record sizes are 7, 5, and 4 bytes.
constexpr uint8_t kTrigrams[] = {0, 0, 1, 0, 2, 0, 12};
constexpr uint8_t kBigrams[] = {1, 0, 2, 0, 9};
constexpr uint8_t kCoarse[] = {0, 1, 1, 6};
constexpr CanonicalFunctionToken kTokens[] = {kI, kV, kVi};
constexpr CorpusModelView kFixture{
    kTokens, 3, kTrigrams, 1, kBigrams, 1, kCoarse, 1};

uint16_t read16(const uint8_t* bytes) {
  return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}
}  // namespace

int main() {
  static_assert(sizeof(CanonicalFunctionToken) == 8);
  assert(corpusPrior(kI, kV, kVi, kFixture) == 12);  // Exact trigram wins.
  assert(corpusPrior(kUnknownPrevious, kV, kVi, kFixture) == 9);  // Bigram backoff.
  assert(corpusPrior(kUnknownPrevious, kV, kUnknownTarget, kFixture) == 6);
  assert(corpusPrior(kI, kUnknownPrevious, kUnknownTarget, kFixture) == 0);

  CanonicalFunctionToken invalid = kI;
  invalid.rootOffset = 6;
  assert(corpusPrior(invalid, kV, kVi, kFixture) == 0);

  // The generated corpus model is a real bounded view, not a placeholder.
  assert(GeneratedCorpusPrior::kModel.tokenCount > 0);
  assert(GeneratedCorpusPrior::kModel.trigramCount > 0);
  assert(GeneratedCorpusPrior::kModel.bigramCount > 0);
  assert(GeneratedCorpusPrior::kModel.coarseCount > 0);
  const uint8_t* generatedRow = GeneratedCorpusPrior::kTrigramBytes;
  const uint16_t previous2Id = read16(generatedRow);
  const uint16_t previous1Id = read16(generatedRow + 2);
  const uint16_t targetId = read16(generatedRow + 4);
  assert(corpusPrior(GeneratedCorpusPrior::kTokens[previous2Id],
                     GeneratedCorpusPrior::kTokens[previous1Id],
                     GeneratedCorpusPrior::kTokens[targetId],
                     GeneratedCorpusPrior::kModel) == generatedRow[6]);

  std::cout << "HGS corpus prior lookup: PASS\n";
  return 0;
}
