#include "src/phrase/pattern_lease_owner.h"
#include "src/state/undo_receipts.h"

#include <cstdint>
#include <cstdio>

using Slot = GroovePuterUndo::BoundedUndoSlot<GroovePuterUndo::kUndoPayloadBytes>;

static_assert(sizeof(Slot) == 1548,
              "P1b canonical bounded Undo slot resident size changed");
static_assert(sizeof(GroovePuterUndo::UndoOwner) == 1552,
              "P1b canonical UndoOwner resident size changed");
static_assert(GroovePuterUndo::UndoOwner::payloadCapacity() == 1536,
              "P1b ordinary Undo payload capacity changed");
static_assert(GroovePuterUndo::UndoOwner::lifecyclePayloadCapacity() == 1424,
              "P1b lifecycle-aware payload capacity changed");
static_assert(GroovePuterUndo::kUndoLifecycleTailBytes == 112,
              "P1b reserved lifecycle tail changed");
static_assert(sizeof(PhrasePatternLease::PatternLease) == 14,
              "P1a2 PatternLease size changed");
static_assert(sizeof(PhrasePatternLease::PatternLeaseOwner) == 28,
              "P1a2 PatternLeaseOwner size changed");

#if UINTPTR_MAX == UINT64_MAX
static_assert(sizeof(GroovePuterUndo::UndoLifecycleMetadata) == 112,
              "P1b 64-bit host lifecycle metadata size changed");
#elif UINTPTR_MAX == UINT32_MAX
static_assert(sizeof(GroovePuterUndo::UndoLifecycleMetadata) == 96,
              "P1b 32-bit target lifecycle metadata size changed");
#else
#error "P1b memory contract supports only 32-bit and 64-bit pointer ABIs"
#endif

int main() {
  std::printf("BoundedUndoSlot=%zu\n", sizeof(Slot));
  std::printf("UndoOwner=%zu\n", sizeof(GroovePuterUndo::UndoOwner));
  std::printf("UndoPayloadCapacity=%zu\n",
              GroovePuterUndo::UndoOwner::payloadCapacity());
  std::printf("LifecyclePayloadCapacity=%zu\n",
              GroovePuterUndo::UndoOwner::lifecyclePayloadCapacity());
  std::printf("LifecycleTailReserved=%zu\n",
              GroovePuterUndo::kUndoLifecycleTailBytes);
  std::printf("UndoLifecycleMetadata=%zu\n",
              sizeof(GroovePuterUndo::UndoLifecycleMetadata));
  std::printf("PhraseUndoPayload=%zu\n",
              sizeof(GroovePuterUndo::PhraseUndoPayload));
  std::printf("PatternLease=%zu\n",
              sizeof(PhrasePatternLease::PatternLease));
  std::printf("PatternLeaseOwner=%zu\n",
              sizeof(PhrasePatternLease::PatternLeaseOwner));
  return 0;
}
