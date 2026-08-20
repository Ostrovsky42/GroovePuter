#include "../src/generation/migration/quantized_generation_commit.h"
#include "../src/state/undo_owner.h"

#include <cassert>
#include <cstdio>
#include <type_traits>

int main() {
  using namespace GroovePuterRhythm;
  using namespace GroovePuterRhythm::QuantizedGenerationDetail;

  static_assert(std::is_trivially_copyable<PendingGeneration>::value,
                "pending activation storage must remain a fixed value");
  static_assert(static_cast<uint8_t>(SlotState::Armed) <
                    static_cast<uint8_t>(SlotState::Ready),
                "Armed must precede Ready publication");
  static_assert(static_cast<uint8_t>(QuantizedGenerationStatus::Activated) >
                    static_cast<uint8_t>(QuantizedGenerationStatus::AttemptUnavailable),
                "C statuses must append without renumbering A/B identities");

  g_publishedSlot.store(-1, std::memory_order_release);
  for (int i = 0; i < 2; ++i) {
    g_slotState[i].store(
        static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);
    g_slots[i] = PendingGeneration{};
  }
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::Idle),
      std::memory_order_release);

  // One prepared candidate can reserve the other fixed slot as its audible
  // overlay. No heap queue or third resident slot is required.
  const WriteLease prepared = acquireWriteLease();
  assert(prepared.slot >= 0 && prepared.slot < 2);
  assert(g_slotState[prepared.slot].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Writing));

  const int activation = acquireCompanionActivationSlot(prepared.slot);
  assert(activation >= 0 && activation < 2 && activation != prepared.slot);
  g_slots[activation].scope = QuantizedGenerationScope::SynthA;
  g_slots[activation].committedRevision = 0;
  armActivationSlot(activation);
  assert(g_publishedSlot.load(std::memory_order_acquire) == activation);
  assert(g_slotState[activation].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Armed));

  // Explicit C policy: another intent is rejected while a pending activation
  // exists. There is no hidden replacement queue.
  const WriteLease rejected = acquireWriteLease();
  assert(rejected.slot < 0);

  // The MiniAcid-dependent completeArmedActivation() installs the BAR_START
  // hook and is exercised by source + firmware gates. This standalone test
  // verifies the fixed storage state transition without pulling DSP linkage.
  g_slots[activation].committedRevision = 77;
  g_slotState[activation].store(
      static_cast<uint8_t>(SlotState::Ready), std::memory_order_release);
  g_status.store(
      static_cast<uint8_t>(QuantizedGenerationStatus::PendingNextBar),
      std::memory_order_release);
  assert(g_slots[activation].committedRevision == 77);
  assert(g_slotState[activation].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Ready));
  assert(static_cast<QuantizedGenerationStatus>(
             g_status.load(std::memory_order_acquire)) ==
         QuantizedGenerationStatus::PendingNextBar);

  abortArmedActivation(
      activation, QuantizedGenerationStatus::CancelledExplicit);
  assert(g_publishedSlot.load(std::memory_order_acquire) == -1);
  assert(g_slotState[activation].load(std::memory_order_acquire) ==
         static_cast<uint8_t>(SlotState::Empty));
  releaseWriteSlot(prepared.slot);

  const WriteLease next = acquireWriteLease();
  assert(next.slot >= 0);
  releaseWriteSlot(next.slot);

  std::printf("0.9.9-C PendingGeneration=%zu bytes, fixed slots=2\n",
              sizeof(PendingGeneration));
  return 0;
}
