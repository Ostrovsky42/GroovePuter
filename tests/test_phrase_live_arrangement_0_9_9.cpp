#include "../src/dsp/generated_phrase_song.h"

#include <cassert>
#include <cstdio>
#include <type_traits>

int main() {
  using namespace GeneratedPhraseSong;
  using namespace GroovePuterRhythm;
  using namespace GroovePuterRhythm::PhraseLiveArrangementDetail;
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;

  static_assert(std::is_trivially_copyable<PreparedPhraseArrangement>::value,
                "D2 PREPARE staging must stay fixed-size");
  static_assert(std::is_trivially_copyable<GeneratedPhraseUndoPayload>::value,
                "D2 Undo receipt must stay fixed-size");
  static_assert(sizeof(GeneratedPhraseUndoPayload) <=
                    GroovePuterUndo::kUndoPayloadBytes,
                "D2 Undo must fit the canonical one-level owner");
  static_assert(kMaxPreparedBars == 8,
                "D2 must remain bounded to the accepted Phrase length set");
  static_assert(sizeof(g_phraseActivation) <= 128,
                "D2 pending metadata exceeded its small fixed-state budget");

  g_publishedSlot.store(-1, std::memory_order_release);
  for (int slot = 0; slot < 2; ++slot) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    g_slots[slot] = PendingGeneration{};
    clearPhraseActivationMetadata(slot);
  }

  const WriteLease lease = acquireWriteLease();
  assert(lease.slot >= 0 && lease.slot < 2);

  g_slots[lease.slot].bpm = kPhraseActivationBpmSentinel;
  g_phraseActivation[lease.slot].active = true;
  assert(isPhraseActivationSlot(lease.slot));

  g_phraseActivation[lease.slot].songSlot = 1;
  g_phraseActivation[lease.slot].songStart = 23;
  g_phraseActivation[lease.slot].bars = 8;
  g_phraseActivation[lease.slot].audibleSongRow = 7;
  assert(g_phraseActivation[lease.slot].songSlot == 1);
  assert(g_phraseActivation[lease.slot].songStart == 23);
  assert(g_phraseActivation[lease.slot].bars == 8);
  assert(g_phraseActivation[lease.slot].audibleSongRow == 7);

  clearPhraseActivationMetadata(lease.slot);
  assert(!isPhraseActivationSlot(lease.slot));
  releaseWriteSlot(lease.slot);

  const WriteLease next = acquireWriteLease();
  assert(next.slot >= 0);
  releaseWriteSlot(next.slot);

  std::printf(
      "0.9.9-D2 staging=%zu undo=%zu pending-meta=%zu canonical-pending-slots=2\n",
      preparedPhraseArrangementSize(),
      generatedPhraseUndoPayloadSize(),
      pendingPhraseActivationMetadataBytes());
  return 0;
}
