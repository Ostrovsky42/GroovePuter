#include <cassert>
#include <cmath>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

SerialMock Serial;
SDMock SD;

namespace {

bool closeEnough(float lhs, float rhs) {
  return std::fabs(lhs - rhs) < 0.001f;
}

void testExternalClockDoesNotBecomeProjectTempoDuringSceneSync() {
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager_.setBpm(120.0f);
  engine.setBpm(120.0f);

  engine.setExternalClockBpm(137.0f);
  assert(closeEnough(engine.bpm(), 137.0f));

  engine.syncSceneStateToManager();
  assert(closeEnough(engine.sceneManager_.getBpm(), 120.0f));
}

// Persistence being correct is not enough: the effective render tempo must
// also return to the project tempo once clock ownership leaves external
// follow, or the engine keeps playing at the followed BPM until reboot.
void testRestoreProjectBpmRevertsEffectiveTempoAfterFollow() {
  MiniAcid engine{44100.0f, nullptr};
  engine.setBpm(120.0f);

  engine.setExternalClockBpm(137.0f);
  assert(closeEnough(engine.bpm(), 137.0f));

  engine.restoreProjectBpm();
  assert(closeEnough(engine.bpm(), 120.0f));
}

// restoreProjectBpm() must not disturb the durable project tempo itself --
// it only resyncs the effective value, never re-derives projectBpmValue.
void testRestoreProjectBpmDoesNotChangeProjectTempo() {
  MiniAcid engine{44100.0f, nullptr};
  engine.sceneManager_.setBpm(120.0f);
  engine.setBpm(120.0f);
  engine.setExternalClockBpm(137.0f);

  engine.restoreProjectBpm();
  engine.syncSceneStateToManager();
  assert(closeEnough(engine.sceneManager_.getBpm(), 120.0f));
}

}  // namespace

int main() {
  testExternalClockDoesNotBecomeProjectTempoDuringSceneSync();
  testRestoreProjectBpmRevertsEffectiveTempoAfterFollow();
  testRestoreProjectBpmDoesNotChangeProjectTempo();
  return 0;
}
