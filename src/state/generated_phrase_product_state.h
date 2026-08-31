#pragma once

#include <cstdint>
#include <type_traits>

namespace GroovePuterState {

enum class GeneratedPhraseOutcome : uint8_t {
  None = 0,
  Accepted,
  TypedRejection,
  ExecutionFailure,
};

struct GeneratedPhraseAcceptedSnapshot {
  bool valid = false;
  bool usedP1r = false;
  bool pendingNextBar = false;
  uint8_t bars = 0;
  int8_t songSlot = -1;
  int8_t pageIndex = -1;
  int8_t firstLocalSlot = -1;
  int16_t songStart = -1;
  uint16_t phraseGenerationIdentity = 0xFFFFu;
  uint8_t progression = 0;
  uint8_t harmonicEventPositions = 0;
};

struct GeneratedPhraseProductState {
  GeneratedPhraseOutcome lastOutcome = GeneratedPhraseOutcome::None;
  uint8_t lastRequestedBars = 0;
  GeneratedPhraseAcceptedSnapshot accepted{};
};

static_assert(std::is_trivially_copyable<GeneratedPhraseAcceptedSnapshot>::value,
              "generated Phrase UI snapshot must remain fixed-capacity");
static_assert(std::is_trivially_copyable<GeneratedPhraseProductState>::value,
              "generated Phrase product state must remain fixed-capacity");
static_assert(sizeof(GeneratedPhraseProductState) <= 24,
              "generated Phrase product state must stay small");

namespace generated_phrase_product_detail {
inline GeneratedPhraseProductState& storage() {
  static GeneratedPhraseProductState state{};
  return state;
}
}  // namespace generated_phrase_product_detail

inline const GeneratedPhraseProductState& generatedPhraseProductState() {
  return generated_phrase_product_detail::storage();
}

inline const char* generatedPhraseOutcomeName(GeneratedPhraseOutcome outcome) {
  switch (outcome) {
    case GeneratedPhraseOutcome::None: return "NO RESULT";
    case GeneratedPhraseOutcome::Accepted: return "ACCEPTED";
    case GeneratedPhraseOutcome::TypedRejection: return "REJECTED";
    case GeneratedPhraseOutcome::ExecutionFailure: return "EXEC FAILURE";
  }
  return "NO RESULT";
}

inline void publishGeneratedPhraseAccepted(
    uint8_t requestedBars,
    uint8_t effectiveBars,
    int songSlot,
    int pageIndex,
    int firstLocalSlot,
    int songStart,
    bool usedP1r,
    bool pendingNextBar,
    uint16_t phraseGenerationIdentity,
    uint8_t progression,
    uint8_t harmonicEventPositions) {
  auto& state = generated_phrase_product_detail::storage();
  state.lastOutcome = GeneratedPhraseOutcome::Accepted;
  state.lastRequestedBars = requestedBars;
  state.accepted.valid = true;
  state.accepted.usedP1r = usedP1r;
  state.accepted.pendingNextBar = pendingNextBar;
  state.accepted.bars = effectiveBars;
  state.accepted.songSlot = static_cast<int8_t>(songSlot);
  state.accepted.pageIndex = static_cast<int8_t>(pageIndex);
  state.accepted.firstLocalSlot = static_cast<int8_t>(firstLocalSlot);
  state.accepted.songStart = static_cast<int16_t>(songStart);
  state.accepted.phraseGenerationIdentity = phraseGenerationIdentity;
  state.accepted.progression = progression;
  state.accepted.harmonicEventPositions = harmonicEventPositions;
}

inline void publishGeneratedPhraseTypedRejection(uint8_t requestedBars) {
  auto& state = generated_phrase_product_detail::storage();
  state.lastOutcome = GeneratedPhraseOutcome::TypedRejection;
  state.lastRequestedBars = requestedBars;
}

inline void publishGeneratedPhraseExecutionFailure(uint8_t requestedBars) {
  auto& state = generated_phrase_product_detail::storage();
  state.lastOutcome = GeneratedPhraseOutcome::ExecutionFailure;
  state.lastRequestedBars = requestedBars;
}

inline void resetGeneratedPhraseProductState() {
  generated_phrase_product_detail::storage() = GeneratedPhraseProductState{};
}

}  // namespace GroovePuterState
