#pragma once
#ifndef GROOVEPUTER_PHRASE_CROSSBAR_LIFETIME_RUNTIME_H
#define GROOVEPUTER_PHRASE_CROSSBAR_LIFETIME_RUNTIME_H

#include <cstdint>
#include <type_traits>

namespace GroovePuterPhraseRuntime {

constexpr uint16_t kUnspecifiedPhraseGenerationIdentity = 0xFFFFu;
constexpr uint8_t kMaxPhraseBars = 8u;
constexpr uint8_t kTerminalLogicalStep = 15u;

enum class LogicalBoundaryDecision : uint8_t {
  Release = 0,
  Continue,
};

struct PhraseCrossBarLifetimeContext {
  uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  uint8_t phraseBars = 0;
  uint8_t currentPhraseBarOrdinal = 0;
  uint8_t continuesMask = 0;
  uint8_t entersMask = 0;
  bool valid = false;
};

struct PhraseCrossBarHeldState {
  int16_t note = -1;
  uint16_t phraseGenerationIdentity = kUnspecifiedPhraseGenerationIdentity;
  uint8_t outgoingPhraseBarOrdinal = 0;
  uint8_t expectedIncomingPhraseBarOrdinal = 0;
  bool active = false;
  bool boundaryCrossed = false;
};

struct PhraseBoundaryRuntimeResult {
  LogicalBoundaryDecision decision = LogicalBoundaryDecision::Release;
  int16_t noteToRelease = -1;
  bool ordinarySequentialAccepted = false;
};

class PhraseCrossBarLifetimeExecutor {
 public:
  bool activate(const PhraseCrossBarLifetimeContext& candidate) {
    reset();
    if (!validContext(candidate)) return false;
    context_ = candidate;
    held_ = PhraseCrossBarHeldState{};
    return true;
  }

  void reset() {
    context_ = PhraseCrossBarLifetimeContext{};
    held_ = PhraseCrossBarHeldState{};
  }

  bool contextActive() const { return context_.valid; }
  bool heldActive() const { return held_.active; }
  bool heldCrossedBoundary() const {
    return held_.active && held_.boundaryCrossed;
  }
  int16_t heldNote() const { return held_.active ? held_.note : -1; }
  const PhraseCrossBarLifetimeContext& context() const { return context_; }
  const PhraseCrossBarHeldState& heldState() const { return held_; }

  bool armOutgoingNote(uint8_t logicalStep, int16_t note) {
    if (!context_.valid || held_.active || note < 0 ||
        logicalStep != kTerminalLogicalStep) {
      return false;
    }
    const uint8_t outgoing = context_.currentPhraseBarOrdinal;
    if (outgoing + 1u >= context_.phraseBars) return false;
    const uint8_t incoming = static_cast<uint8_t>(outgoing + 1u);
    if (!pairedCarrier(context_, outgoing, incoming)) return false;

    held_.note = note;
    held_.phraseGenerationIdentity = context_.phraseGenerationIdentity;
    held_.outgoingPhraseBarOrdinal = outgoing;
    held_.expectedIncomingPhraseBarOrdinal = incoming;
    held_.active = true;
    held_.boundaryCrossed = false;
    return true;
  }

  bool suppressOrdinaryGateExpiry() const {
    return context_.valid && held_.active;
  }

  // The first PatternPlayer Synth-B NoteOn after a successful boundary is the
  // C2-proven terminator. Returning the old note lets the runtime perform one
  // explicit Release before processing the new NoteOn. The phrase context is
  // retained so a later terminal onset in the same incoming bar may arm the
  // next ordinary phrase boundary.
  int16_t consumeTerminatorBeforeNoteOn() {
    if (!context_.valid || !held_.active || !held_.boundaryCrossed) return -1;
    const int16_t note = held_.note;
    held_ = PhraseCrossBarHeldState{};
    return note;
  }

  PhraseBoundaryRuntimeResult advanceOrdinarySequentialBoundary() {
    PhraseBoundaryRuntimeResult result{};
    if (!context_.valid) return result;

    // A voice already crossed one boundary but never observed the C2-promised
    // incoming terminator. Never hold it into a second bar.
    if (held_.active && held_.boundaryCrossed) {
      result.noteToRelease = held_.note;
      reset();
      return result;
    }

    const uint8_t outgoing = context_.currentPhraseBarOrdinal;
    if (outgoing + 1u >= context_.phraseBars) {
      result.noteToRelease = held_.active ? held_.note : -1;
      reset();
      return result;
    }
    const uint8_t incoming = static_cast<uint8_t>(outgoing + 1u);

    if (held_.active) {
      if (!pairedCarrier(context_, outgoing, incoming) ||
          held_.phraseGenerationIdentity != context_.phraseGenerationIdentity ||
          held_.outgoingPhraseBarOrdinal != outgoing ||
          held_.expectedIncomingPhraseBarOrdinal != incoming) {
        result.noteToRelease = held_.note;
        reset();
        return result;
      }
      held_.boundaryCrossed = true;
      context_.currentPhraseBarOrdinal = incoming;
      result.decision = LogicalBoundaryDecision::Continue;
      result.ordinarySequentialAccepted = true;
      return result;
    }

    // No held voice is present, but a valid phrase context still advances on
    // the same ordinary sequential boundary. Existing physical cleanup remains
    // Release for all ordinary notes.
    context_.currentPhraseBarOrdinal = incoming;
    result.ordinarySequentialAccepted = true;
    return result;
  }

  // Every abnormal transition is fail-closed. The caller owns the actual
  // internal/MIDI Release; this method only returns the one note that may need
  // that Release and clears all runtime lifetime metadata.
  int16_t hardBarrierRelease() {
    const int16_t note = held_.active ? held_.note : -1;
    reset();
    return note;
  }

  static bool validContext(const PhraseCrossBarLifetimeContext& value) {
    if (!value.valid ||
        value.phraseGenerationIdentity == kUnspecifiedPhraseGenerationIdentity ||
        value.phraseBars == 0 || value.phraseBars > kMaxPhraseBars ||
        value.currentPhraseBarOrdinal >= value.phraseBars) {
      return false;
    }

    const uint8_t validMask = value.phraseBars == 8u
        ? 0xFFu
        : static_cast<uint8_t>((1u << value.phraseBars) - 1u);
    if ((value.continuesMask & static_cast<uint8_t>(~validMask)) != 0u ||
        (value.entersMask & static_cast<uint8_t>(~validMask)) != 0u) {
      return false;
    }
    if ((value.entersMask & 0x01u) != 0u) return false;
    const uint8_t finalBit = static_cast<uint8_t>(
        1u << static_cast<uint8_t>(value.phraseBars - 1u));
    if ((value.continuesMask & finalBit) != 0u) return false;

    for (uint8_t outgoing = 0; outgoing + 1u < value.phraseBars; ++outgoing) {
      const uint8_t incoming = static_cast<uint8_t>(outgoing + 1u);
      const bool continues =
          (value.continuesMask & static_cast<uint8_t>(1u << outgoing)) != 0u;
      const bool enters =
          (value.entersMask & static_cast<uint8_t>(1u << incoming)) != 0u;
      if (continues != enters) return false;
    }
    return true;
  }

 private:
  static bool pairedCarrier(const PhraseCrossBarLifetimeContext& value,
                            uint8_t outgoing,
                            uint8_t incoming) {
    if (incoming != static_cast<uint8_t>(outgoing + 1u) ||
        incoming >= value.phraseBars) {
      return false;
    }
    const uint8_t outgoingBit = static_cast<uint8_t>(1u << outgoing);
    const uint8_t incomingBit = static_cast<uint8_t>(1u << incoming);
    return (value.continuesMask & outgoingBit) != 0u &&
           (value.entersMask & incomingBit) != 0u;
  }

  PhraseCrossBarLifetimeContext context_{};
  PhraseCrossBarHeldState held_{};
};

static_assert(std::is_trivially_copyable<PhraseCrossBarLifetimeContext>::value,
              "R1 runtime metadata must remain fixed-capacity data");
static_assert(std::is_trivially_copyable<PhraseCrossBarHeldState>::value,
              "R1 held state must remain fixed-capacity data");
static_assert(std::is_trivially_copyable<PhraseCrossBarLifetimeExecutor>::value,
              "R1 executor must remain fixed-capacity state");

}  // namespace GroovePuterPhraseRuntime

#endif  // GROOVEPUTER_PHRASE_CROSSBAR_LIFETIME_RUNTIME_H
