// Per-cell source override: the same Melody slot plays Melody on a row whose
// bit is 0 and steps on a row whose bit is 1; a Pattern slot always plays steps.
#include "song_cell_source_common.h"

SerialMock Serial;
SDMock SD;

using namespace cellsrc;

int main() {
  const std::string proj = "cell_src_play";
  setupProject("gp_test_cell_src_play", proj);
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  constexpr int kX = 0, kY = 1;

  MiniAcid engine{44100.0f, &storage};
  engine.init();
  engine.setSongMode(false);
  writePattern(engine, kX, 40);
  bindPattern(engine, kX);
  acceptMelody(engine, kY, makeMelody(60));
  writePattern(engine, kY, 77);  // the steps under the Melody slot
  engine.set303PatternIndex(0, kX);
  engine.releaseSavedWorkingMelody_(0);
  assert(engine.rebuildPatternRuntimeEventBank());
  writeSong(engine, {kY, kY, kX, kX});
  // Row 1: override Melody slot Y to steps. Row 3: bit on a Pattern slot.
  Song& song = engine.sceneManager().currentScene().songs[0];
  song.setStepsOverride(0, 1, true);
  song.setStepsOverride(0, 3, true);
  engine.setSongMode(true);
  engine.setSongPosition(0);

  const auto bars = play(engine, 4);
  for (size_t i = 0; i < bars.size(); ++i) {
    std::printf("bar %zu:", i);
    for (int n : bars[i]) std::printf(" %d", n);
    std::printf("\n");
  }
  assert((bars[0] == std::set<int>{60, 62, 64, 66}));  // bit 0: Melody
  assert((bars[1] == std::set<int>{77}));              // bit 1: steps of the same slot
  assert((bars[2] == std::set<int>{40}));              // Pattern slot
  assert((bars[3] == std::set<int>{40}));              // Pattern slot with bit 1 still steps
  assert(engine.songCellMaterialKind(0, kY, 0) == MiniAcid::SongCellMaterialKind::Melody);
  assert(engine.songCellMaterialKind(0, kY, 1) == MiniAcid::SongCellMaterialKind::Pattern);
  std::puts("test_song_cell_source_playback PASS");
  return 0;
}
