// Shared helpers for the per-Song-cell voice-source override tests.
#pragma once
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <set>
#include <vector>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private
#include "platform_sdl/scene_storage_sdl.h"
#include "src/platform/cardputer_material_publication_session.h"
#include "src/audio/pattern_paging.h"
#include "src/state/melody_promotion.h"

namespace cellsrc {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

inline Buffer makeMelody(uint8_t firstNote) {
  Buffer melody{};
  melody.lengthTicks = static_cast<uint16_t>(PhraseRuntime::kTicksPerBar);
  melody.count = 4;
  for (uint16_t i = 0; i < melody.count; ++i) {
    auto& event = melody.events[i];
    event.startTick = static_cast<uint16_t>(i * 96);
    event.durationSubticks = 24 * PhraseRuntime::kSubticksPerTick;
    event.note = static_cast<uint8_t>(firstNote + i * 2);
    event.velocity = 100;
    event.probability = 100;
  }
  return melody;
}

inline void writePattern(MiniAcid& engine, int slot, int8_t note) {
  SynthPattern& pattern =
      engine.sceneManager().currentScene().synthABanks[0].patterns[slot];
  for (auto& step : pattern.steps) step = SynthStep{};
  for (int s = 0; s < 16; s += 4) pattern.steps[s].note = note;
}

inline void bindPattern(MiniAcid& engine, int slot) {
  auto& d = engine.sceneManager().currentScene().materialSlots[0][slot];
  d.kind = GroovePuterMaterial::MaterialKind::Pattern;
  if (!d.id.valid()) d.id = PatternPagingService::allocateMaterialId();
  assert(d.id.valid());
}

inline void acceptMelody(MiniAcid& engine, int slot, const Buffer& melody) {
  if (engine.current303BankIndex(0) != 0) engine.set303BankIndex(0, 0);
  engine.set303PatternIndex(0, slot);
  engine.workingMaterial_[0].storeMelody(melody);
  assert(engine.acceptMaterialWorking(0) == MiniAcid::AcceptResult::Accepted);
  assert(engine.isMelodySlot(0, 0, slot));
}

inline void writeSong(MiniAcid& engine, const std::vector<int>& rows) {
  for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
    engine.setSongPattern(row, SongTrack::SynthA, static_cast<int16_t>(rows[row]));
  }
  engine.setSongLength(static_cast<int>(rows.size()));
}

inline void uiFrame(MiniAcid& engine) {
  if (engine.songMaterialServiceDue()) engine.serviceSongMaterial();
}

// Notes heard on voice A in each bar.
inline std::vector<std::set<int>> play(MiniAcid& engine, int bars) {
  std::vector<std::set<int>> heard(static_cast<size_t>(bars));
  uiFrame(engine);
  engine.start();
  int lastNote = -1;
  bool lastActive = false;
  for (int tick = 0; tick < bars * 384; ++tick) {
    ++engine.currentTick_;
    engine.advanceTick();
    const auto& state = engine.patternPlaybackState_[0];
    if (state.active() &&
        (!lastActive || state.activeNote() != lastNote || tick % 96 == 0)) {
      heard[tick / 384].insert(state.activeNote());
    }
    lastActive = state.active();
    lastNote = state.activeNote();
    if (tick % 48 == 47) uiFrame(engine);
  }
  engine.stop();
  return heard;
}

inline void setupProject(const char* dir, const std::string& proj) {
  const auto root = std::filesystem::temp_directory_path() / dir;
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
  std::filesystem::create_directories(root);
  std::filesystem::current_path(root);
  SD.setRoot(root);
  GroovePuterPlatform::clearMaterialPublication(proj, 0);
  PatternPagingService::setProjectName(proj);
}

}  // namespace cellsrc
