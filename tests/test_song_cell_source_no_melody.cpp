// Per-cell source override: bit 1 on a Pattern slot plays steps and is never
// Awaiting; a Melody whose payload cannot load still fails closed.
#include "song_cell_source_common.h"

SerialMock Serial;
SDMock SD;

using namespace cellsrc;

int main() {
  const std::string proj = "cell_src_nomel";
  setupProject("gp_test_cell_src_nomel", proj);
  SceneStorageSdl storage;
  storage.setCurrentSceneName("default");
  constexpr int kX = 0, kMissing = 4;

  {  // Pattern slot with bit 1: steps, InSync.
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    engine.setSongMode(false);
    writePattern(engine, kX, 40);
    bindPattern(engine, kX);
    assert(engine.rebuildPatternRuntimeEventBank());
    writeSong(engine, {kX});
    engine.sceneManager().currentScene().songs[0].setStepsOverride(0, 0, true);
    engine.setSongMode(true);
    engine.setSongPosition(0);
    engine.serviceSongMaterial();
    assert(engine.songVoiceState_[0] == MiniAcid::SongVoiceState::InSync);
    assert(engine.songVoiceDisplayState(0) == MiniAcid::SongVoiceDisplayState::Pattern);
    const auto bars = play(engine, 1);
    assert((bars[0] == std::set<int>{40}));
  }
  {  // Melody descriptor without payload, bit 0: fail closed and silent.
    MiniAcid engine{44100.0f, &storage};
    engine.init();
    engine.setSongMode(false);
    writePattern(engine, kMissing, 77);
    engine.sceneManager().currentScene().materialSlots[0][kMissing].kind =
        GroovePuterMaterial::MaterialKind::Melody;
    assert(engine.rebuildPatternRuntimeEventBank());
    writeSong(engine, {kMissing});
    engine.setSongMode(true);
    engine.setSongPosition(0);
    engine.serviceSongMaterial();
    assert(engine.songVoiceState_[0] == MiniAcid::SongVoiceState::Awaiting);
    assert(engine.songVoiceDisplayState(0) == MiniAcid::SongVoiceDisplayState::LoadFailed);
    const auto bars = play(engine, 1);
    assert(bars[0].empty());
  }
  std::puts("test_song_cell_source_no_melody PASS");
  return 0;
}
