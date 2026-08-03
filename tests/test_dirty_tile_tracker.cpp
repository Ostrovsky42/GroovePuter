#include "src/ui/dirty_tile_tracker.h"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

std::vector<DirtyTileRun> scan(DirtyTileTracker& tracker,
                               const std::vector<uint16_t>& frame,
                               int width) {
  std::vector<DirtyTileRun> runs;
  tracker.scan(frame.data(), width, [&](const DirtyTileRun& run) {
    runs.push_back(run);
  });
  return runs;
}

}  // namespace

int main() {
  constexpr int kWidth = 40;
  constexpr int kHeight = 33;
  std::vector<uint16_t> frame(kWidth * kHeight, 0);
  DirtyTileTracker tracker;
  assert(tracker.reset(kWidth, kHeight));

  auto runs = scan(tracker, frame, kWidth);
  assert(runs.size() == 1);
  assert(runs[0].x == 0 && runs[0].y == 0);
  assert(runs[0].w == kWidth && runs[0].h == kHeight);

  runs = scan(tracker, frame, kWidth);
  assert(runs.empty());

  frame[2 * kWidth + 3] = 1;
  runs = scan(tracker, frame, kWidth);
  assert(runs.size() == 1);
  assert(runs[0].x == 0 && runs[0].y == 0);
  assert(runs[0].w == 16 && runs[0].h == 16);

  frame[2 * kWidth + 17] = 2;
  runs = scan(tracker, frame, kWidth);
  assert(runs.size() == 1);
  assert(runs[0].x == 16 && runs[0].y == 0);
  assert(runs[0].w == 16 && runs[0].h == 16);

  frame[18 * kWidth + 2] = 3;
  frame[18 * kWidth + 18] = 4;
  runs = scan(tracker, frame, kWidth);
  assert(runs.size() == 1);
  assert(runs[0].x == 0 && runs[0].y == 16);
  assert(runs[0].w == 32 && runs[0].h == 16);

  frame[32 * kWidth + 39] = 5;
  runs = scan(tracker, frame, kWidth);
  assert(runs.size() == 1);
  assert(runs[0].x == 32 && runs[0].y == 32);
  assert(runs[0].w == 8 && runs[0].h == 1);

  tracker.forceFullRefresh();
  runs = scan(tracker, frame, kWidth);
  assert(runs.size() == 1);
  assert(runs[0].w == kWidth && runs[0].h == kHeight);

  runs.clear();
  for (auto& pixel : frame) pixel ^= 0xFFFFu;
  const auto result = tracker.scan(frame.data(), kWidth, [&](const DirtyTileRun& run) {
    runs.push_back(run);
  });
  assert(result.full_refresh);
  assert(runs.size() == 1);
  assert(runs[0].w == kWidth && runs[0].h == kHeight);

  DirtyTileTracker invalid;
  assert(!invalid.reset(DirtyTileTracker::kMaxScreenWidth + 1, 10));
  runs.clear();
  const auto invalidResult = invalid.scan(frame.data(), kWidth, [&](const DirtyTileRun& run) {
    runs.push_back(run);
  });
  assert(invalidResult.total_tiles == 0);
  assert(runs.empty());

  return 0;
}
