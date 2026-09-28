#ifndef GROOVEPUTER_SRC_STATE_GENERATED_SYNTH_A_ORIGIN_H
#define GROOVEPUTER_SRC_STATE_GENERATED_SYNTH_A_ORIGIN_H

#include <cstdint>
#include <type_traits>

#include "../generation/roles/bass_rhythm.h"
#include "../generation/roles/chord_progression.h"
#include "../generation/roles/harmonic_rhythm.h"
#include "../generation/tonal/tonal_projector.h"
#include "material_identity.h"
#include "material_version.h"

// D1-B: immutable, owner-derived origin evidence for the most recently
// successfully committed P1R generated Phrase (Synth A only).
//
// This is EVIDENCE, not authority: it does not own Material, does not classify
// identity, and is not a lineage database. It records "this evidence generated
// this Material state" and never mutates afterwards, even when CURRENT is
// edited. Consumers compare `originPatternVersion` with the current exact
// version themselves; nothing here decides what a difference means musically.
//
// Only GeneratedPhraseSong publishes it (after commitPrepared succeeded); it
// is session-local (never persisted, never part of a Material payload).
namespace GroovePuterMaterial {

constexpr uint8_t kMaxGeneratedSynthAOriginBars = 8;

struct GeneratedSynthABarOrigin {
  MaterialReference material{};
  // versionForPattern() of the Pattern as committed, NOT an identity.
  MaterialVersionToken originPatternVersion{};
  // The exact plan consumed to build this bar's Synth A.
  GroovePuterRhythm::BassRhythmPlan bassRhythm{};
  // Copied from the PREPARE-time harmonic clock projection, never re-derived.
  GroovePuterRhythm::HarmonicRhythmPlan harmonicRhythm{};
  uint8_t phraseBarOrdinal = 0;
};

struct GeneratedSynthAOriginCommon {
  uint16_t phraseGenerationIdentity = 0;
  uint8_t barCount = 0;
  uint8_t rootPitchClass = 0;
  GroovePuterRhythm::ScaleTypeValue scaleTypeValue =
      GroovePuterRhythm::kDefaultScaleTypeValue;
  GroovePuterRhythm::ChordProgressionSource progressionSource{};
};

struct GeneratedSynthAOrigin {
  GeneratedSynthAOriginCommon common{};
  GeneratedSynthABarOrigin bars[kMaxGeneratedSynthAOriginBars]{};

  bool valid() const {
    if (common.barCount == 0 || common.barCount > kMaxGeneratedSynthAOriginBars) {
      return false;
    }
    for (uint8_t i = 0; i < common.barCount; ++i) {
      const auto& bar = bars[i];
      if (!bar.material.id.valid() || !bar.originPatternVersion.valid() ||
          bar.phraseBarOrdinal != i) {
        return false;
      }
    }
    return true;
  }

  // Lookup by canonical Material identity (address AND id). Returns nullptr
  // when the reference is not part of this origin. Does not compare versions.
  const GeneratedSynthABarOrigin* find(MaterialReference reference) const {
    if (!reference.id.valid()) return nullptr;
    for (uint8_t i = 0; i < common.barCount && i < kMaxGeneratedSynthAOriginBars;
         ++i) {
      const auto& bar = bars[i];
      if (materialReferenceMatches(reference, bar.material.address,
                                   bar.material.id)) {
        return &bar;
      }
    }
    return nullptr;
  }
};

// Unpublished candidate filled during COMMIT from real owner results. Only a
// `complete` candidate (every bar carried valid P1R evidence) may be published.
struct GeneratedSynthAOriginCandidate {
  GeneratedSynthAOrigin origin{};
  uint8_t filledBars = 0;
  bool failed = false;

  bool complete() const {
    return !failed && origin.common.barCount != 0 &&
           filledBars == origin.common.barCount && origin.valid();
  }
};

static_assert(std::is_trivially_copyable<GeneratedSynthABarOrigin>::value,
              "GeneratedSynthABarOrigin must stay fixed-capacity");
static_assert(std::is_trivially_copyable<GeneratedSynthAOrigin>::value,
              "GeneratedSynthAOrigin must stay fixed-capacity");
static_assert(sizeof(GeneratedSynthAOrigin) <= 296,
              "D1-B GeneratedSynthAOrigin exceeded its 296-byte target");

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_GENERATED_SYNTH_A_ORIGIN_H
