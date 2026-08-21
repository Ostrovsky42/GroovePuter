#include "../src/generation/migration/quantized_generation_commit.h"

#include <cassert>
#include <cstdio>
#include <type_traits>

int main() {
  using namespace GroovePuterRhythm;
  using namespace GroovePuterRhythm::LiveSongArrangementDetail;
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;

  static_assert(std::is_trivially_copyable<SongActivationMetadata>::value,
                "D3 pending Song metadata must remain fixed value state");
  static_assert(std::is_trivially_copyable<SongMutationLease>::value,
                "D3 Song lease must remain fixed value state");
  static_assert(sizeof(g_songActivation) <= 192,
                "D3 resident pending metadata exceeded the accepted small budget");
  static_assert(sizeof(g_slots) == sizeof(PendingGeneration) * 2,
                "D3 must reuse exactly the accepted C two-slot owner");
  static_assert(SongActivationKind::PersistentMutation !=
                    SongActivationKind::PlaybackSlotSwitch,
                "persistent Song COMMIT and runtime PLAY switch must remain distinguishable");

  SongPosition a{};
  SongPosition b{};
  assert(sameSongPosition(a, b));
  b.patterns[static_cast<int>(SongTrack::SynthA)] = 12;
  assert(!sameSongPosition(a, b));
  a.patterns[static_cast<int>(SongTrack::SynthA)] = 12;
  assert(sameSongPosition(a, b));
  b.patterns[static_cast<int>(SongTrack::SynthB)] = 27;
  assert(!sameSongPosition(a, b));
  a.patterns[static_cast<int>(SongTrack::SynthB)] = 27;
  b.patterns[static_cast<int>(SongTrack::Drums)] = 31;
  assert(!sameSongPosition(a, b));

  g_publishedSlot.store(-1, std::memory_order_release);
  for (int slot = 0; slot < 2; ++slot) {
    g_slotState[slot].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    g_slots[slot] = PendingGeneration{};
    clearSongActivationMetadata(slot);
  }

  // Mutation A: publish one old-audible snapshot and prove the one-pending
  // admission policy rejects a second intent while A is authoritative.
  const WriteLease first = acquireWriteLease();
  assert(first.slot >= 0 && first.slot < 2);
  g_slots[first.slot].bpm = kSongActivationBpmSentinel;
  g_slots[first.slot].synth[0].steps[0].note = 12;
  g_songActivation[first.slot].active = true;
  g_songActivation[first.slot].sourcePlaybackSlot = 0;
  g_songActivation[first.slot].sourceRow = 7;
  g_songActivation[first.slot].targetPlaybackSlot = 0;
  g_songActivation[first.slot].kind = SongActivationKind::PersistentMutation;
  assert(isSongActivationSlot(first.slot));
  armActivationSlot(first.slot);
  const WriteLease busy = acquireWriteLease();
  assert(busy.slot < 0);

  // Terminal A uses the D3 race-safe claim: Armed/Ready -> Reading ->
  // unpublish -> clear bytes+metadata -> Empty. A is fully dead before a new
  // writer can acquire the slot.
  assert(cancelSongActivationSlot(
      first.slot, QuantizedGenerationStatus::CancelledExplicit));
  assert(g_publishedSlot.load(std::memory_order_acquire) < 0);
  assert(!isSongActivationSlot(first.slot));
  assert(!g_songActivation[first.slot].active);
  assert(g_slots[first.slot].owner == nullptr);
  assert(g_slots[first.slot].synth[0].steps[0].note == -1);
  assert(static_cast<SlotState>(g_slotState[first.slot].load(
             std::memory_order_acquire)) == SlotState::Empty);

  // Mutation B must start from clean state, not from A's row/material. This is
  // the host analogue of A -> boundary -> B -> boundary lifecycle ownership.
  const WriteLease second = acquireWriteLease();
  assert(second.slot >= 0 && second.slot < 2);
  g_slots[second.slot].bpm = kSongActivationBpmSentinel;
  g_slots[second.slot].synth[0].steps[0].note = 27;
  g_songActivation[second.slot].active = true;
  g_songActivation[second.slot].sourcePlaybackSlot = 0;
  g_songActivation[second.slot].sourceRow = 8;
  g_songActivation[second.slot].targetPlaybackSlot = 1;
  g_songActivation[second.slot].kind = SongActivationKind::PlaybackSlotSwitch;
  assert(isSongActivationSlot(second.slot));
  assert(g_songActivation[second.slot].sourceRow == 8);
  assert(g_slots[second.slot].synth[0].steps[0].note == 27);
  armActivationSlot(second.slot);

  assert(cancelSongActivationSlot(
      second.slot, QuantizedGenerationStatus::CancelledExplicit));
  assert(!isSongActivationSlot(second.slot));
  assert(!g_songActivation[second.slot].active);

  const WriteLease reusable = acquireWriteLease();
  assert(reusable.slot >= 0);
  releaseWriteSlot(reusable.slot);

  std::printf(
      "0.9.9-D3 song-meta=%zu C-slot=%zu C-slots=2 lifecycle=A-dead-B-clean\n",
      pendingSongActivationMetadataBytes(), sizeof(PendingGeneration));
  return 0;
}
