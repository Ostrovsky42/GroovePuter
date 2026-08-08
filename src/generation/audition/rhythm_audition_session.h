#pragma once

#include <cstddef>
#include <cstdint>

#include "rhythm_audition_controller.h"

namespace GroovePuterRhythm {
namespace Audition {

class Session {
public:
  bool active() const { return active_; }
  const Controller& controller() const { return controller_; }

  bool activate(DrumPatternSet& drums,
                SynthPattern& synthA,
                SynthPattern& synthB);
  void deactivate(DrumPatternSet& drums,
                  SynthPattern& synthA,
                  SynthPattern& synthB);

  bool selectDefinition(uint8_t index,
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
  bool toggleBass(DrumPatternSet& drums,
                  SynthPattern& synthA,
                  SynthPattern& synthB);
  bool rerender(DrumPatternSet& drums,
                SynthPattern& synthA,
                SynthPattern& synthB);

  void formatStatus(char* out, size_t capacity) const;

private:
  bool commitController(const Controller& candidate,
                        DrumPatternSet& drums,
                        SynthPattern& synthA,
                        SynthPattern& synthB);

  Controller controller_{};
  bool active_ = false;
  DrumPatternSet backupDrums_{};
  SynthPattern backupA_{};
  SynthPattern backupB_{};
};

}  // namespace Audition
}  // namespace GroovePuterRhythm
