#!/usr/bin/env python3
from pathlib import Path

# Trigger commit: workflow exists in the branch parent.
ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "src/generation/migration/live_song_arrangement_activation.h"
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one exact anchor, found {count}: {old[:100]!r}")
    text = text.replace(old, new, 1)

replace_once(
"""inline void clearSongActivationPayload(int slot) {
  if (slot < 0 || slot > 1) return;
  QuantizedGenerationDetail::g_slots[slot] = PendingGeneration{};
  clearSongActivationMetadata(slot);
}
""",
"""inline void clearSongActivationPayload(int slot) {
  if (slot < 0 || slot > 1) return;
  QuantizedGenerationDetail::g_slots[slot] = PendingGeneration{};
  clearSongActivationMetadata(slot);
}

// Race-safe terminal cancellation for D3-owned C slots. Claim the pending
// state before unpublishing and do not expose Empty until both captured bytes
// and metadata are dead, so a new writer can never be cleared by an old cancel.
inline bool cancelSongActivationSlot(
    int slot,
    QuantizedGenerationStatus status =
        QuantizedGenerationStatus::CancelledExplicit) {
  if (slot < 0 || slot > 1 || !isSongActivationSlot(slot)) return false;

  uint8_t state = QuantizedGenerationDetail::g_slotState[slot].load(
      std::memory_order_acquire);
  if (state != static_cast<uint8_t>(SlotState::Armed) &&
      state != static_cast<uint8_t>(SlotState::Ready)) {
    return false;
  }
  if (!QuantizedGenerationDetail::g_slotState[slot].compare_exchange_strong(
          state,
          static_cast<uint8_t>(SlotState::Reading),
          std::memory_order_acq_rel,
          std::memory_order_acquire)) {
    return false;
  }

  int8_t published = static_cast<int8_t>(slot);
  QuantizedGenerationDetail::g_publishedSlot.compare_exchange_strong(
      published, -1,
      std::memory_order_acq_rel,
      std::memory_order_acquire);
  clearSongActivationPayload(slot);
  QuantizedGenerationDetail::g_slotState[slot].store(
      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
  QuantizedGenerationDetail::g_status.store(
      static_cast<uint8_t>(status), std::memory_order_release);
  return true;
}
""",
)

replace_once(
"""  if (!lease.boundaryRequired || lease.slot < 0) return;
  QuantizedGenerationDetail::abortArmedActivation(lease.slot, status);
  clearSongActivationPayload(lease.slot);
}""",
"""  if (!lease.boundaryRequired || lease.slot < 0) return;
  cancelSongActivationSlot(lease.slot, status);
}""",
)

replace_once(
"""  QuantizedGenerationDetail::abortArmedActivation(
      slot, QuantizedGenerationStatus::CancelledExplicit);
  clearSongActivationPayload(slot);
  return true;
}""",
"""  return cancelSongActivationSlot(
      slot, QuantizedGenerationStatus::CancelledExplicit);
}""",
)

old_failure = """    QuantizedGenerationDetail::g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    clearSongActivationMetadata(slot);
"""
new_failure = """    clearSongActivationPayload(slot);
    QuantizedGenerationDetail::g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
"""
count = text.count(old_failure)
if count != 2:
    raise RuntimeError(f"expected two claimed failure cleanup anchors, found {count}")
text = text.replace(old_failure, new_failure)

path.write_text(text, encoding="utf-8")
print("D3 race-safe terminal cancellation applied")
