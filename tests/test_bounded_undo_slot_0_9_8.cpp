#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/state/bounded_undo_slot.h"

namespace {

struct SmallReceipt {
  uint32_t address = 0;
  uint8_t bytes[12]{};
};

struct SecondReceipt {
  uint16_t row = 0;
  int16_t references[4]{};
};

struct OversizedReceipt {
  uint8_t bytes[33]{};
};

static_assert(std::is_trivially_copyable<SmallReceipt>::value,
              "test receipt must be trivially copyable");
static_assert(std::is_trivially_copyable<SecondReceipt>::value,
              "test receipt must be trivially copyable");

}  // namespace

int main() {
  using GroovePuterState::SceneRevisionState;
  using GroovePuterUndo::BoundedUndoSlot;
  using GroovePuterUndo::UndoKind;

  using Slot = BoundedUndoSlot<32>;
  static_assert(Slot::capacity() == 32, "capacity contract changed");
  static_assert(sizeof(Slot) <= 48,
                "bounded slot metadata grew beyond the R1 host budget");

  Slot slot;
  assert(!slot.hasUndo());
  assert(slot.kind() == UndoKind::None);
  assert(slot.payloadSize() == 0);

  SmallReceipt first{};
  first.address = 0x10203040u;
  for (uint8_t i = 0; i < sizeof(first.bytes); ++i) first.bytes[i] = i + 1u;
  const SceneRevisionState first_revision{7u, 5u};

  assert(slot.publish(UndoKind::Pattern, first, first_revision));
  assert(slot.hasUndo());
  assert(slot.kind() == UndoKind::Pattern);
  assert(slot.payloadSize() == sizeof(SmallReceipt));
  assert(slot.revisionBefore().currentRevision == 7u);
  assert(slot.revisionBefore().persistedRevision == 5u);

  SmallReceipt restored_first{};
  assert(slot.read(UndoKind::Pattern, restored_first));
  assert(std::memcmp(&first, &restored_first, sizeof(first)) == 0);

  SmallReceipt wrong_kind{};
  assert(!slot.read(UndoKind::Song, wrong_kind));
  assert(slot.hasUndo());

  SecondReceipt second{};
  second.row = 12;
  second.references[0] = 3;
  second.references[1] = 8;
  second.references[2] = -1;
  second.references[3] = 21;
  const SceneRevisionState second_revision{11u, 9u};

  assert(slot.publish(UndoKind::Song, second, second_revision));
  assert(slot.kind() == UndoKind::Song);
  assert(slot.payloadSize() == sizeof(SecondReceipt));
  assert(slot.revisionBefore().currentRevision == 11u);
  assert(slot.revisionBefore().persistedRevision == 9u);

  // The second successful mutation replaces the first one-level receipt.
  assert(!slot.read(UndoKind::Pattern, restored_first));
  SecondReceipt restored_second{};
  assert(slot.read(UndoKind::Song, restored_second));
  assert(std::memcmp(&second, &restored_second, sizeof(second)) == 0);

  // Admission failure must not destroy the previous valid Undo.
  OversizedReceipt too_large{};
  assert(!slot.publish(UndoKind::Generation, too_large,
                       SceneRevisionState{99u, 99u}));
  assert(slot.kind() == UndoKind::Song);
  assert(slot.revisionBefore().currentRevision == 11u);
  assert(slot.read(UndoKind::Song, restored_second));
  assert(std::memcmp(&second, &restored_second, sizeof(second)) == 0);

  // None is never a publishable mutation and likewise preserves history.
  assert(!slot.publish(UndoKind::None, first, SceneRevisionState{42u, 42u}));
  assert(slot.kind() == UndoKind::Song);
  assert(slot.read(UndoKind::Song, restored_second));

  slot.clear();
  assert(!slot.hasUndo());
  assert(slot.kind() == UndoKind::None);
  assert(slot.payloadSize() == 0);
  assert(slot.revisionBefore().currentRevision == 0u);
  assert(slot.revisionBefore().persistedRevision == 0u);
  assert(!slot.read(UndoKind::Song, restored_second));

  return 0;
}
