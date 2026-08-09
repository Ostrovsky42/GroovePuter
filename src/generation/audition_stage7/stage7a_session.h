#pragma once

#include <cstddef>
#include <cstdint>

#include "stage7a_catalog.h"
#include "../rhythm/rhythm_realizer.h"
#include "../../../scenes.h"

namespace GroovePuterRhythm {
namespace Stage7AAudition {

class Session {
public:
  bool active() const { return active_; }
  uint8_t candidateIndex() const { return candidateIndex_; }
  uint32_t seed() const { return seed_; }
  RealizationLevel level() const { return level_; }
  bool identityValid() const { return identityValid_; }
  RealizationStatus lastStatus() const { return lastStatus_; }
  const PhraseRhythmIdentity& identity() const { return identity_; }
  const Definition& currentDefinition() const { return definition(candidateIndex_); }

  bool activate(DrumPatternSet& drums,
                SynthPattern& synthA,
                SynthPattern& synthB);
  void deactivate(DrumPatternSet& drums,
                  SynthPattern& synthA,
                  SynthPattern& synthB);

  bool selectCandidate(uint8_t index,
                       DrumPatternSet& drums,
                       SynthPattern& synthA,
                       SynthPattern& synthB);
  bool shiftSeed(int delta,
                 DrumPatternSet& drums,
                 SynthPattern& synthA,
                 SynthPattern& synthB);
  bool cycleLevel(DrumPatternSet& drums,
                  SynthPattern& synthA,
                  SynthPattern& synthB);
  bool rerender(DrumPatternSet& drums,
                SynthPattern& synthA,
                SynthPattern& synthB);

  void formatStatus(char* out, size_t capacity) const;

private:
  bool establishIdentity();
  bool renderScratch(DrumPatternSet& drums,
                     SynthPattern& synthA,
                     SynthPattern& synthB);
  bool commitCurrent(DrumPatternSet& drums,
                     SynthPattern& synthA,
                     SynthPattern& synthB);

  uint8_t candidateIndex_ = 0;
  uint32_t seed_ = 1;
  RealizationLevel level_ = RealizationLevel::P1Canonical;
  bool identityValid_ = false;
  RealizationStatus lastStatus_ = RealizationStatus::InvalidConstraintSet;
  PhraseRhythmIdentity identity_{};

  bool active_ = false;
  DrumPatternSet backupDrums_{};
  SynthPattern backupA_{};
  SynthPattern backupB_{};
};

const char* levelName(RealizationLevel level);

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
