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

}  // namespace

int main() {
  testExternalClockDoesNotBecomeProjectTempoDuringSceneSync();
  return 0;
}
