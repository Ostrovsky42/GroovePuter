#include <cstddef>
#include <cstdio>
#include <type_traits>

#include "scenes.h"
#include "src/phrase/phrase_types.h"
#include "src/state/bounded_undo_slot.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

namespace {

struct SongRectUndoPayload {
  uint8_t songSlot = 0;
  uint8_t firstRow = 0;
  uint8_t rowCount = 0;
  uint8_t trackCount = 0;
  uint8_t oldLength = 1;
  uint8_t oldReverse = 0;
  uint8_t tracks[SongPosition::kTrackCount]{};
  int16_t refs[Song::kMaxPositions * SongPosition::kTrackCount]{};
};

struct PhraseSlotUndoPayload {
  uint8_t slot = 0;
  uint8_t reserved = 0;
  PhraseCore::PhraseSlot before{};
};

struct MaterializedRowUndoPayload {
  uint8_t songSlot = 0;
  uint8_t row = 0;
  uint8_t trackMask = 0;
  uint8_t oldSongLength = 1;
  int16_t oldSongRefs[3] = {-1, -1, -1};
  SynthPattern synthA{};
  SynthPattern synthB{};
  DrumPatternSet drums{};
};

template <typename T>
void printType(const char* name) {
  std::printf("%-36s %5zu bytes trivial=%d\n", name, sizeof(T),
              std::is_trivially_copyable<T>::value ? 1 : 0);
}

}  // namespace

int main() {
  std::puts("0.9.8 R2 Undo payload characterization");
  printType<GroovePuterState::SceneRevisionState>("SceneRevisionState");
  printType<SynthStep>("SynthStep");
  printType<SynthPattern>("SynthPattern");
  printType<DrumStep>("DrumStep");
  printType<DrumPattern>("DrumPattern");
  printType<DrumPatternSet>("DrumPatternSet");
  printType<SongPosition>("SongPosition");
  printType<Song>("Song");
  printType<PhraseCore::PhraseSlot>("PhraseSlot");
  printType<PhraseCore::PhraseBank>("PhraseBank");
  printType<Scene>("Scene");

  std::puts("candidate receipts");
  printType<GroovePuterUndo::SynthPatternUndoPayload>("SynthPatternUndoPayload");
  printType<SongRectUndoPayload>("SongRectUndoPayload");
  printType<PhraseSlotUndoPayload>("PhraseSlotUndoPayload");
  printType<MaterializedRowUndoPayload>("MaterializedRowUndoPayload");

  std::puts("owner footprints");
  printType<GroovePuterUndo::BoundedUndoSlot<256>>("BoundedUndoSlot<256>");
  printType<GroovePuterUndo::BoundedUndoSlot<512>>("BoundedUndoSlot<512>");
  printType<GroovePuterUndo::BoundedUndoSlot<1024>>("BoundedUndoSlot<1024>");
  printType<GroovePuterUndo::BoundedUndoSlot<1536>>("BoundedUndoSlot<1536>");
  printType<GroovePuterUndo::BoundedUndoSlot<2048>>("BoundedUndoSlot<2048>");
  printType<GroovePuterUndo::UndoOwner>("UndoOwner<1536>");

  static_assert(std::is_trivially_copyable<GroovePuterUndo::SynthPatternUndoPayload>::value,
                "Pattern Undo receipt must be a fixed value");
  static_assert(std::is_trivially_copyable<SongRectUndoPayload>::value,
                "Song Undo receipt must be a fixed value");
  static_assert(std::is_trivially_copyable<PhraseSlotUndoPayload>::value,
                "Phrase Undo receipt must be a fixed value");
  static_assert(std::is_trivially_copyable<MaterializedRowUndoPayload>::value,
                "generation/materialization Undo receipt must be a fixed value");

  return 0;
}
