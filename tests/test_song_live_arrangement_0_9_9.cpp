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

  const WriteLease lease = acquireWriteLease();
  assert(lease.slot >= 0 && lease.slot < 2);
  g_slots[lease.slot].bpm = kSongActivationBpmSentinel;
  g_songActivation[lease.slot].active = true;
  g_songActivation[lease.slot].sourcePlaybackSlot = 0;
  g_songActivation[lease.slot].sourceRow = 7;
  g_songActivation[lease.slot].targetPlaybackSlot = 1;
  g_songActivation[lease.slot].kind = SongActivationKind::PlaybackSlotSwitch;
  assert(isSongActivationSlot(lease.slot));

  // One pending owner means a second intent cannot acquire a slot.
  armActivationSlot(lease.slot);
  const WriteLease busy = acquireWriteLease();
  assert(busy.slot < 0);

  abortArmedActivation(lease.slot,
                       QuantizedGenerationStatus::CancelledExplicit);
  clearSongActivationMetadata(lease.slot);
  assert(!isSongActivationSlot(lease.slot));

  const WriteLease reusable = acquireWriteLease();
  assert(reusable.slot >= 0);
  releaseWriteSlot(reusable.slot);

  std::printf(
      "0.9.9-D3 song-meta=%zu C-slot=%zu C-slots=2\n",
      pendingSongActivationMetadataBytes(), sizeof(PendingGeneration));
  return 0;
}
