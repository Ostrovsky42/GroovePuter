#ifndef GROOVEPUTER_GENERATION_HARMONY_CORPUS_PRIOR_H
#define GROOVEPUTER_GENERATION_HARMONY_CORPUS_PRIOR_H

#include <cstdint>

namespace GroovePuterHarmony {

// v2 canonicalizes sounded relative pitch, not unrecoverable Roman spelling.
// Degrees use a fixed major reference; chromatic roots use the next degree
// lowered by one semitone (C# == bII). This is independent of key/mode.
constexpr uint16_t kCanonicalFunctionCodecVersion = 2;
inline uint8_t canonicalRootDegree(uint8_t relativePc) {
  static constexpr uint8_t degrees[12] = {0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6};
  return degrees[relativePc % 12u];
}
inline uint8_t canonicalRootAlteration(uint8_t relativePc) {
  static constexpr uint8_t alterations[12] = {2, 1, 2, 1, 2, 2, 1, 2, 1, 2, 1, 2};
  return alterations[relativePc % 12u];
}

constexpr uint8_t kNoFunctionDegree = 255;
constexpr uint8_t kNoSlashBassClass = 255;

struct CanonicalFunctionToken {
  uint16_t extensionFlags = 0;
  uint8_t degree = kNoFunctionDegree;
  int8_t rootOffset = 0;
  uint8_t qualityGroup = 0;
  uint8_t alterationFlags = 0;
  uint8_t slashBassClass = kNoSlashBassClass;
  uint8_t spellingFlags = 0;
};

struct CorpusModelView {
  const CanonicalFunctionToken* tokens = nullptr;
  uint16_t tokenCount = 0;
  const uint8_t* trigramBytes = nullptr;
  uint16_t trigramCount = 0;
  const uint8_t* bigramBytes = nullptr;
  uint16_t bigramCount = 0;
  const uint8_t* coarseBytes = nullptr;
  uint16_t coarseCount = 0;
};

static_assert(sizeof(CanonicalFunctionToken) == 8,
              "canonical function token must remain exactly 8 bytes");

// Returns a scoring prior in 0..15. A zero prior is absence of corpus evidence,
// not a musical or genre prohibition.
uint8_t corpusPrior(const CanonicalFunctionToken& previous2,
                    const CanonicalFunctionToken& previous1,
                    const CanonicalFunctionToken& target,
                    const CorpusModelView& model);

}  // namespace GroovePuterHarmony

#endif  // GROOVEPUTER_GENERATION_HARMONY_CORPUS_PRIOR_H
