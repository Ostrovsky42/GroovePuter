#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "src/generation/migration/live_song_arrangement_activation.h"
text = path.read_text(encoding="utf-8")


def replace_once(old: str, new: str) -> None:
    global text
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one exact anchor, found {count}: {old[:80]!r}")
    text = text.replace(old, new, 1)

replace_once(
"""inline void clearSongActivationMetadata(int slot) {
  if (slot < 0 || slot > 1) return;
  g_songActivation[slot] = SongActivationMetadata{};
}
""",
"""inline void clearSongActivationMetadata(int slot) {
  if (slot < 0 || slot > 1) return;
  g_songActivation[slot] = SongActivationMetadata{};
}

// Terminal lifecycle cleanup. Once publication has been cancelled or claimed,
// neither the old audible bytes nor their D3 metadata may survive as a source
// for a later Song activation.
inline void clearSongActivationPayload(int slot) {
  if (slot < 0 || slot > 1) return;
  QuantizedGenerationDetail::g_slots[slot] = PendingGeneration{};
  clearSongActivationMetadata(slot);
}
""",
)

replace_once(
"""  QuantizedGenerationDetail::abortArmedActivation(lease.slot, status);
  clearSongActivationMetadata(lease.slot);
}""",
"""  QuantizedGenerationDetail::abortArmedActivation(lease.slot, status);
  clearSongActivationPayload(lease.slot);
}""",
)

replace_once(
"""  QuantizedGenerationDetail::abortArmedActivation(
      slot, QuantizedGenerationStatus::CancelledExplicit);
  clearSongActivationMetadata(slot);
  return true;
}""",
"""  QuantizedGenerationDetail::abortArmedActivation(
      slot, QuantizedGenerationStatus::CancelledExplicit);
  clearSongActivationPayload(slot);
  return true;
}""",
)

old_terminal = """  pending = PendingGeneration{};
  clearSongActivationMetadata(slot);
  QuantizedGenerationDetail::g_slotState[slot].store(
"""
new_terminal = """  clearSongActivationPayload(slot);
  QuantizedGenerationDetail::g_slotState[slot].store(
"""
count = text.count(old_terminal)
if count != 2:
    raise RuntimeError(f"expected two ACTIVATE/STOP terminal cleanup anchors, found {count}")
text = text.replace(old_terminal, new_terminal)

path.write_text(text, encoding="utf-8")
print("D3 terminal Song activation payload cleanup applied")
