#include "rhythm_audition_session.h"

#include <cstdio>

namespace GroovePuterRhythm {
namespace Audition {

bool Session::commitController(const Controller& candidate,
                               DrumPatternSet& drums,
                               SynthPattern& synthA,
                               SynthPattern& synthB) {
  if (!active_) return false;

  Controller nextController = candidate;
  DrumPatternSet nextDrums{};
  SynthPattern nextA{};
  SynthPattern nextB{};
  if (!nextController.render(nextDrums, nextA, nextB)) return false;

  controller_ = nextController;
  drums = nextDrums;
  synthA = nextA;
  synthB = nextB;
  return true;
}

bool Session::activate(DrumPatternSet& drums,
                       SynthPattern& synthA,
                       SynthPattern& synthB) {
  if (active_) return rerender(drums, synthA, synthB);

  Controller candidate = controller_;
  DrumPatternSet nextDrums{};
  SynthPattern nextA{};
  SynthPattern nextB{};
  if (!candidate.render(nextDrums, nextA, nextB)) return false;

  backupDrums_ = drums;
  backupA_ = synthA;
  backupB_ = synthB;
  controller_ = candidate;
  drums = nextDrums;
  synthA = nextA;
  synthB = nextB;
  active_ = true;
  return true;
}

void Session::deactivate(DrumPatternSet& drums,
                         SynthPattern& synthA,
                         SynthPattern& synthB) {
  if (!active_) return;
  drums = backupDrums_;
  synthA = backupA_;
  synthB = backupB_;
  active_ = false;
}

bool Session::selectDefinition(uint8_t index,
                               DrumPatternSet& drums,
                               SynthPattern& synthA,
                               SynthPattern& synthB) {
  if (!active_) return false;
  Controller candidate = controller_;
  candidate.selectDefinition(index);
  return commitController(candidate, drums, synthA, synthB);
}

bool Session::shiftSeed(int delta,
                        DrumPatternSet& drums,
                        SynthPattern& synthA,
                        SynthPattern& synthB) {
  if (!active_) return false;
  Controller candidate = controller_;
  candidate.shiftSeed(delta);
  return commitController(candidate, drums, synthA, synthB);
}

bool Session::cycleLevel(DrumPatternSet& drums,
                         SynthPattern& synthA,
                         SynthPattern& synthB) {
  if (!active_) return false;
  Controller candidate = controller_;
  candidate.cycleLevel();
  return commitController(candidate, drums, synthA, synthB);
}

bool Session::toggleBass(DrumPatternSet& drums,
                         SynthPattern& synthA,
                         SynthPattern& synthB) {
  if (!active_) return false;
  Controller candidate = controller_;
  candidate.toggleBass();
  return commitController(candidate, drums, synthA, synthB);
}

bool Session::rerender(DrumPatternSet& drums,
                       SynthPattern& synthA,
                       SynthPattern& synthB) {
  if (!active_) return false;
  return commitController(controller_, drums, synthA, synthB);
}

void Session::formatStatus(char* out, size_t capacity) const {
  if (!out || capacity == 0) return;
  const Definition& def = controller_.currentDefinition();
  std::snprintf(out,
                capacity,
                "AUD %s S%lu %s B%s",
                def.name,
                static_cast<unsigned long>(controller_.seed()),
                levelName(controller_.level()),
                controller_.bassEnabled() ? "ON" : "OFF");
}

}  // namespace Audition
}  // namespace GroovePuterRhythm
