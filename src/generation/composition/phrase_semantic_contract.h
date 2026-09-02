#ifndef GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_SEMANTIC_CONTRACT_H
#define GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_SEMANTIC_CONTRACT_H

#include <cstdint>
#include <type_traits>

#include "../roles/melodic_motif.h"

namespace GroovePuterRhythm {

// M2-T1 semantic carrier only. This describes the lifetime of the one
// monophonic note that may cross a logical phrase-bar boundary. It is not a
// playback token and deliberately does not encode the legacy SynthStep note
// sentinel used by MiniAcid compatibility playback.
struct MelodicCrossBarLifetime {
  bool entersFromPreviousBar = false;
  bool continuesIntoNextBar = false;
};

enum class PhraseNoteLifetimeState : uint8_t {
  None = 0,
  StartedHere = 1u << 0u,
  ContinuesIntoThisBar = 1u << 1u,
  ContinuesOutOfThisBar = 1u << 2u,
  EndsHere = 1u << 3u,
};

constexpr uint8_t phraseNoteLifetimeStateBits(
    const MelodicMotifPlan& plan,
    const MelodicCrossBarLifetime& lifetime) {
  uint8_t bits = 0;
  if (plan.onsets != 0) {
    bits |= static_cast<uint8_t>(PhraseNoteLifetimeState::StartedHere);
  }
  if (lifetime.entersFromPreviousBar) {
    bits |= static_cast<uint8_t>(PhraseNoteLifetimeState::ContinuesIntoThisBar);
  }
  if (lifetime.continuesIntoNextBar) {
    bits |= static_cast<uint8_t>(PhraseNoteLifetimeState::ContinuesOutOfThisBar);
  }
  const bool hasBoundaryNote =
      plan.onsets != 0 || lifetime.entersFromPreviousBar || plan.continuations != 0;
  if (hasBoundaryNote && !lifetime.continuesIntoNextBar) {
    bits |= static_cast<uint8_t>(PhraseNoteLifetimeState::EndsHere);
  }
  return bits;
}

constexpr bool phraseNoteLifetimeHas(
    uint8_t bits, PhraseNoteLifetimeState state) {
  return (bits & static_cast<uint8_t>(state)) != 0;
}

// Physical Song/pattern switches are intentionally not represented here.
// Future execution must classify the semantic boundary first; backends then
// consume the same logical decision.
enum class PhraseLifetimeBoundary : uint8_t {
  IntraPhraseBarAdvance = 0,
  Stop,
  OutsideLogicalPhrase,
  Count,
};

enum class PhraseLifetimeDecision : uint8_t {
  Continue = 0,
  Release,
  Count,
};

constexpr PhraseLifetimeDecision phraseLifetimeDecision(
    PhraseLifetimeBoundary boundary,
    const MelodicCrossBarLifetime& lifetime) {
  return boundary == PhraseLifetimeBoundary::IntraPhraseBarAdvance &&
                 lifetime.continuesIntoNextBar
             ? PhraseLifetimeDecision::Continue
             : PhraseLifetimeDecision::Release;
}

static_assert(std::is_trivially_copyable<MelodicCrossBarLifetime>::value,
              "cross-bar lifetime must remain fixed-capacity");
static_assert(sizeof(MelodicCrossBarLifetime) <= 2,
              "cross-bar lifetime exceeded its semantic budget");

}  // namespace GroovePuterRhythm

#endif  // GROOVEPUTER_GENERATION_COMPOSITION_PHRASE_SEMANTIC_CONTRACT_H
