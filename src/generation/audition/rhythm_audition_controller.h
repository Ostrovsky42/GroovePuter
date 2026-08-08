#pragma once

#include <cstdint>

#include "rhythm_audition_catalog.h"
#include "rhythm_audition_materializer.h"

namespace GroovePuterRhythm {
namespace Audition {

class Controller {
public:
  Controller() = default;

  uint8_t definitionIndex() const { return definitionIndex_; }
  const Definition& currentDefinition() const;

  uint32_t seed() const { return seed_; }
  RealizationLevel level() const { return level_; }
  bool bassEnabled() const { return bassEnabled_; }
  bool identityValid() const { return identityValid_; }
  RealizationStatus lastStatus() const { return lastStatus_; }
  const PhraseRhythmIdentity& identity() const { return identity_; }

  void selectDefinition(uint8_t index);
  void setSeed(uint32_t seed);
  void shiftSeed(int delta);
  void cycleLevel();
  void setLevel(RealizationLevel level);
  void toggleBass() { bassEnabled_ = !bassEnabled_; }
  void setBassEnabled(bool enabled) { bassEnabled_ = enabled; }

  // Rebuilds the same deterministic identity on the next render. With the
  // same seed/archetype this intentionally yields the same phrase.
  void invalidateIdentity() { identityValid_ = false; }

  // Transactional: output patterns are assigned only after realization and
  // materialization both succeed. P2/P3 always reuse the P1 identity held by
  // this controller until archetype or seed changes.
  bool render(DrumPatternSet& drums,
              SynthPattern& synthA,
              SynthPattern& synthB);

private:
  bool establishIdentity();

  uint8_t definitionIndex_ = 0;
  uint32_t seed_ = 1;
  RealizationLevel level_ = RealizationLevel::P1Canonical;
  bool bassEnabled_ = false;
  bool identityValid_ = false;
  RealizationStatus lastStatus_ = RealizationStatus::InvalidConstraintSet;
  PhraseRhythmIdentity identity_{};
};

const char* levelName(RealizationLevel level);

}  // namespace Audition
}  // namespace GroovePuterRhythm
