#include "../src/generation/migration/quantized_generation_commit.h"
#include "../src/state/undo_owner.h"

#include <cassert>
#include <type_traits>

int main() {
  using GroovePuterRhythm::QuantizedGenerationDetail::GenerationUndoPayload;

  static_assert(std::is_trivially_copyable<GenerationUndoPayload>::value);
  static_assert(sizeof(GenerationUndoPayload) <= GroovePuterUndo::kUndoPayloadBytes);
  assert(GroovePuterRhythm::quantizedGenerationUndoPayloadSize() ==
         sizeof(GenerationUndoPayload));
  assert(GroovePuterRhythm::quantizedGenerationUndoPayloadSize() <=
         GroovePuterUndo::UndoOwner::payloadCapacity());
  return 0;
}
