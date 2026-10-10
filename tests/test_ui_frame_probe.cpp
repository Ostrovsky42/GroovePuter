// 0.9.19 UI frame probe: stage split, clamping, worst frame, window reset,
// and the AudioTask busy clock.
#include <cassert>
#include <cstdio>

#include "src/ui/ui_frame_probe.h"

using UiFrameProbe::AudioBusyClock;
using UiFrameProbe::FrameAccumulator;
using UiFrameProbe::Reading;
using UiFrameProbe::Stage;

namespace {

Reading at(uint32_t now, uint32_t audio = 0, uint32_t wait = 0) {
  Reading r;
  r.nowUs = now;
  r.audioBusyUs = audio;
  r.guardWaitUs = wait;
  return r;
}

constexpr uint8_t idx(Stage s) { return static_cast<uint8_t>(s); }

void testStagesSplitWallAudioWait() {
  FrameAccumulator p;
  p.beginFrame(at(1000, 500, 70));
  p.mark(Stage::Key, at(1100, 500, 70));          // 100 own
  p.mark(Stage::Song, at(31100, 20500, 10070));   // 30000 = 20000 audio + 10000 wait
  p.mark(Stage::Draw, at(33100, 20500, 10070));   // 2000 own
  p.mark(Stage::Flush, at(36100, 21500, 10070));  // 3000 = 1000 audio
  p.endFrame(at(36100, 21500, 10070));
  const auto w = p.take();
  assert(w.frames == 1);
  assert(w.wallUs == 35100 && w.wallMaxUs == 35100);
  assert(w.audioUs == 21000 && w.guardUs == 10000);
  assert(w.stageWallUs[idx(Stage::Song)] == 30000);
  assert(w.worst.ownUs() == 4100);
  assert(w.worstStages[idx(Stage::Song)].audioUs == 20000);
  assert(w.worstStages[idx(Stage::Song)].guardUs == 10000);
  assert(w.worstStages[idx(Stage::Song)].ownUs() == 0);
  assert(w.worstStages[idx(Stage::Flush)].audioUs == 1000);
  std::puts("UI frame probe: a stage splits into own, audio and gate wait: PASS");
}

void testPartsNeverExceedWall() {
  // Racy readings may report more audio than wall time; the split clamps.
  const auto s = UiFrameProbe::between(at(0, 0, 0), at(1000, 1500, 400));
  assert(s.wallUs == 1000 && s.audioUs == 1000 && s.guardUs == 0 && s.ownUs() == 0);
  const auto t = UiFrameProbe::between(at(0, 0, 0), at(1000, 700, 900));
  assert(t.audioUs == 700 && t.guardUs == 300 && t.ownUs() == 0);
  // Counters may wrap.
  const auto u = UiFrameProbe::between(at(0xFFFFFF00u, 0xFFFFFFF0u, 0), at(0x100u, 0x10u, 0));
  assert(u.wallUs == 0x200u && u.audioUs == 0x20u);
  std::puts("UI frame probe: parts stay inside the wall time, counters may wrap: PASS");
}

void testWorstFrameAndRepeatedStage() {
  FrameAccumulator p;
  p.beginFrame(at(0));
  p.mark(Stage::Draw, at(5000));
  p.endFrame(at(5000));

  p.beginFrame(at(10000));
  p.mark(Stage::Persist, at(900000));    // the slow one
  p.mark(Stage::Status, at(901000));
  p.mark(Stage::Status, at(902000));     // marked twice: accumulates
  p.endFrame(at(902000));

  p.beginFrame(at(910000));
  p.mark(Stage::Draw, at(917000));
  p.endFrame(at(917000));

  const auto w = p.take();
  assert(w.frames == 3);
  assert(w.wallMaxUs == 892000);
  assert(w.worstStages[idx(Stage::Persist)].wallUs == 890000);
  assert(w.worstStages[idx(Stage::Status)].wallUs == 2000);
  assert(w.worstStages[idx(Stage::Draw)].wallUs == 0);
  assert(w.stageMaxUs[idx(Stage::Draw)] == 7000);
  assert(w.stageWallUs[idx(Stage::Draw)] == 12000);
  std::puts("UI frame probe: the worst frame is kept whole; a stage marked twice adds up: PASS");
}

void testTakeResetsAndMarksOutsideAFrameAreIgnored() {
  FrameAccumulator p;
  p.mark(Stage::Draw, at(100));
  p.endFrame(at(200));
  assert(p.take().frames == 0);
  p.beginFrame(at(0));
  p.mark(Stage::Draw, at(10));
  p.endFrame(at(10));
  assert(p.take().frames == 1);
  const auto empty = p.take();
  assert(empty.frames == 0 && empty.wallUs == 0 && empty.wallMaxUs == 0);
  std::puts("UI frame probe: take() starts a new window; marks outside a frame are ignored: PASS");
}

void testAudioBusyClockCountsTheBlockInProgress() {
  AudioBusyClock c;
  assert(c.busyUs(0) == 0);
  c.beginRender(1000);
  assert(c.busyUs(1500) == 500);
  c.endRender(800);
  assert(c.busyUs(5000) == 800);
  c.beginRender(10000);
  assert(c.busyUs(10300) == 1100);
  c.endRender(400);
  assert(c.busyUs(20000) == 1200);
  std::puts("UI frame probe: audio busy clock = finished blocks + the block in progress: PASS");
}

}  // namespace

int main() {
  testStagesSplitWallAudioWait();
  testPartsNeverExceedWall();
  testWorstFrameAndRepeatedStage();
  testTakeResetsAndMarksOutsideAFrameAreIgnored();
  testAudioBusyClockCountsTheBlockInProgress();
  std::puts("UI frame probe: GREEN");
  return 0;
}
