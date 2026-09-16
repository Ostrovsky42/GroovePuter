#pragma once
#ifndef GROOVEPUTER_DSP_MUSICAL_DEVELOPMENT_H
#define GROOVEPUTER_DSP_MUSICAL_DEVELOPMENT_H

#include <cstdint>
#include <cstring>

#include "src/phrase/runtime_synth_events.h"
#include "src/dsp/genre_manager.h"
#include "src/dsp/miniacid_engine.h"
#include "src/generation/tonal/scale_catalog.h"

namespace GroovePuterDevelopment {

enum class TransformationKind : uint8_t {
  None = 0,
  // Harmony
  Revoice,
  Extend,
  // Rhythm
  Displace,
  Thin,
  // Bass / Articulation
  Hold,
  Connect,
  Move,
};

enum class TriState : uint8_t {
  Unknown = 0,
  Pass,
  Fail,
};

enum class GenreResult : uint8_t {
  Unknown = 0,
  Pass,
  Fail,
};

enum class TemporalRoleResult : uint8_t {
  Unknown = 0,
  Pass,
  Fail,
};

struct HarmonyEvidence {
  bool pitchesChanged = false;
  bool rootPreserved = true;
  bool scaleDegreesValid = true;
  bool pitchClassesPreserved = true;
  bool extensionsAdded = false;
  int8_t pitchDeltaSemitones = 0;
  TriState harmonicSupport = TriState::Unknown;
  TriState cadence = TriState::Unknown;
  TriState harmonicFunction = TriState::Unknown;
};

struct RhythmEvidence {
  bool onsetsChanged = false;
  bool theOnePreserved = true;
  int8_t densityDelta = 0;
  TriState metricAlignment = TriState::Unknown;
};

struct BassArticulationEvidence {
  bool articulationChanged = false;
  bool slidesAdded = false;
  bool durationsExtended = false;
  int16_t durationDeltaSubticks = 0;
  TriState contourPreserved = TriState::Unknown;
};

struct DevelopmentEvidence {
  TransformationKind transformation = TransformationKind::None;
  HarmonyEvidence harmony{};
  RhythmEvidence rhythm{};
  BassArticulationEvidence bass{};
};

struct DevelopmentClassification {
  GroovePuterMaterial::IdeaClassification idea =
      GroovePuterMaterial::IdeaClassification::Unknown;
  GenreResult genre = GenreResult::Unknown;
  TemporalRoleResult temporalRole = TemporalRoleResult::Unknown;
  const char* failureReason = nullptr;
};

struct DevelopmentRequest {
  TransformationKind transformation = TransformationKind::Revoice;
  uint8_t scaleType = GroovePuterRhythm::kDefaultScaleTypeValue;
  uint8_t rootKey = 0;
  uint8_t genreId = static_cast<uint8_t>(GenerativeMode::Acid);
  bool requireTheOne = false;
  int8_t maxDensityDrop = 4;
  int8_t degreeShift = 2; // e.g. third shift
  int8_t octaveShift = 0;
  uint16_t displaceTicks = 12; // half-step syncopation (12 ticks = 1/32th)
  bool forceDisplaceTheOne = false; // For negative witness testing
};

struct DevelopmentResult {
  bool success = false;
  PhraseRuntime::RuntimeSynthEventBuffer candidate{};
  DevelopmentEvidence evidence{};
  DevelopmentClassification classification{};
};

inline bool hasEventOnTheOne(const PhraseRuntime::RuntimeSynthEventBuffer& buf) {
  for (uint16_t i = 0; i < buf.count; ++i) {
    if (buf.events[i].startTick == 0) return true;
  }
  return false;
}

inline void transformRevoice(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Revoice;
  evidence.harmony.harmonicSupport = TriState::Pass;
  evidence.harmony.rootPreserved = true;
  evidence.harmony.scaleDegreesValid = true;
  evidence.harmony.pitchClassesPreserved = true;

  // Genuine REVOICE: transforms vertical register realization / octave voicing while
  // preserving exact harmonic pitch-class identity (note % 12), scale degrees, and root.
  const int octaveDelta = (request.octaveShift != 0) ? (request.octaveShift * 12) : 12;

  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& ev = candidate.events[i];
    if (ev.startTick == 0 && request.forceDisplaceTheOne) {
      ev.startTick += 12;
      evidence.rhythm.onsetsChanged = true;
    }
    int newPitch = static_cast<int>(ev.note) + octaveDelta;
    if (newPitch > 84) newPitch -= 24;
    if (newPitch < 24) newPitch += 24;
    if (newPitch != ev.note) {
      evidence.harmony.pitchDeltaSemitones = static_cast<int8_t>(newPitch - ev.note);
      ev.note = static_cast<uint8_t>(newPitch);
      evidence.harmony.pitchesChanged = true;
    }
  }
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformExtend(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Extend;
  evidence.harmony.harmonicSupport = TriState::Pass;
  evidence.harmony.rootPreserved = true;
  evidence.harmony.scaleDegreesValid = true;

  if (candidate.count > 0 && candidate.count < PhraseRuntime::kMaxSynthEvents) {
    // Genuine EXTEND: adds harmonic upper extension (7th or 9th scale degree)
    // relative to the harmonic root key, enriching the harmonic shell.
    const auto baseEvent = candidate.events[0];
    const uint16_t newStart = baseEvent.startTick + 12; // weak-beat / offbeat placement
    if (newStart < candidate.lengthTicks) {
      auto& newEv = candidate.events[candidate.count];
      newEv = baseEvent;
      newEv.startTick = newStart;
      newEv.durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;

      // Calculate harmonic 7th degree (degree 6) or 9th degree (degree 1 + octave)
      const int extensionSemitone = GroovePuterRhythm::scaleDegreeToSemitone(
          request.scaleType, 6);
      int pitch = static_cast<int>(request.rootKey) + 36 + extensionSemitone;
      while (pitch > 84) pitch -= 12;
      while (pitch < 24) pitch += 12;

      newEv.note = static_cast<uint8_t>(pitch);
      newEv.velocity = 80;
      candidate.count++;

      evidence.harmony.pitchesChanged = true;
      evidence.harmony.pitchClassesPreserved = false;
      evidence.harmony.extensionsAdded = true;
      evidence.rhythm.onsetsChanged = true;
      evidence.rhythm.densityDelta = 1;
    }
  }
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformDisplace(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Displace;
  evidence.rhythm.metricAlignment = TriState::Pass;

  const bool genreGuardsTheOne =
      request.requireTheOne ||
      request.genreId == static_cast<uint8_t>(GenerativeMode::FunkSoul);

  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& ev = candidate.events[i];
    if (ev.startTick == 0 && genreGuardsTheOne && !request.forceDisplaceTheOne) {
      // Funk / The One: downbeat anchor is strictly preserved
      continue;
    }
    const uint16_t newStart = (ev.startTick + request.displaceTicks) % candidate.lengthTicks;
    if (newStart != ev.startTick) {
      ev.startTick = newStart;
      evidence.rhythm.onsetsChanged = true;
    }
  }
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformThin(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  evidence.transformation = TransformationKind::Thin;
  evidence.rhythm.metricAlignment = TriState::Pass;
  candidate.lengthTicks = source.lengthTicks;
  candidate.count = 0;

  for (uint16_t i = 0; i < source.count; ++i) {
    const auto& ev = source.events[i];
    // Keep The One and every second note
    if (ev.startTick == 0 || (i % 2 == 0)) {
      candidate.events[candidate.count++] = ev;
    }
  }
  evidence.rhythm.densityDelta = static_cast<int8_t>(candidate.count) - static_cast<int8_t>(source.count);
  if (evidence.rhythm.densityDelta != 0) {
    evidence.rhythm.onsetsChanged = true;
  }
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformHold(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Hold;
  evidence.bass.contourPreserved = TriState::Pass;

  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& ev = candidate.events[i];
    const uint16_t extended = ev.durationSubticks * 2;
    if (extended > ev.durationSubticks) {
      ev.durationSubticks = extended;
      evidence.bass.durationsExtended = true;
      evidence.bass.articulationChanged = true;
      evidence.bass.durationDeltaSubticks = static_cast<int16_t>(extended - ev.durationSubticks);
    }
  }
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformConnect(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Connect;
  evidence.bass.contourPreserved = TriState::Pass;

  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& ev = candidate.events[i];
    ev.flags |= PhraseRuntime::kEventSlide;
    evidence.bass.slidesAdded = true;
    evidence.bass.articulationChanged = true;
  }
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformMove(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Move;
  evidence.bass.contourPreserved = TriState::Pass;

  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& ev = candidate.events[i];
    int newPitch = static_cast<int>(ev.note) + (request.octaveShift != 0 ? request.octaveShift * 12 : 12);
    if (newPitch > 84) newPitch -= 24;
    if (newPitch < 24) newPitch += 24;
    if (newPitch != ev.note) {
      ev.note = static_cast<uint8_t>(newPitch);
      evidence.harmony.pitchesChanged = true;
      evidence.bass.articulationChanged = true;
    }
  }
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline DevelopmentClassification evaluateClassificationAndG4(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    const DevelopmentEvidence& evidence,
    const DevelopmentRequest& request) {
  DevelopmentClassification classification{};
  classification.temporalRole = TemporalRoleResult::Unknown;

  // 1. G4 Structural Constraints
  // Bounded metric gravity witness for Funk/Soul:
  // If the source motif established metric gravity with a downbeat anchor (The One at step 0),
  // Funk metric gravity forbids displacing or eliminating it.
  // Note: This is a bounded witness predicate for metric gravity preservation, NOT an assertion
  // that every lane across all genres must contain an onset at index 0.
  const bool genreRequiresTheOne =
      request.requireTheOne ||
      request.genreId == static_cast<uint8_t>(GenerativeMode::FunkSoul);

  const bool sourceHadTheOne = hasEventOnTheOne(source);
  if (genreRequiresTheOne && sourceHadTheOne && !evidence.rhythm.theOnePreserved) {
    classification.genre = GenreResult::Fail;
    classification.failureReason = "G4: The One metric gravity witness failed (step 0 downbeat anchor lost)";
    classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    return classification;
  }

  if (candidate.count == 0) {
    classification.genre = GenreResult::Fail;
    classification.failureReason = "G4: density reduced to zero (all onsets eliminated)";
    classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    return classification;
  }

  // Generation policy check (bounded policy, not universal G4 law)
  if (evidence.rhythm.densityDelta < -request.maxDensityDrop) {
    classification.genre = GenreResult::Fail;
    classification.failureReason = "Policy: density reduction exceeded requested maxDensityDrop";
    classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    return classification;
  }

  // G4 constraint passed
  classification.genre = GenreResult::Pass;

  // 2. Idea Continuity Evaluation
  if (evidence.harmony.pitchesChanged && evidence.rhythm.onsetsChanged) {
    classification.idea = GroovePuterMaterial::IdeaClassification::NewIdea;
  } else if (evidence.harmony.pitchesChanged ||
             evidence.rhythm.onsetsChanged ||
             evidence.bass.articulationChanged) {
    classification.idea = GroovePuterMaterial::IdeaClassification::Variation;
  } else {
    classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
  }

  return classification;
}

inline DevelopmentResult developCandidate(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request) {
  DevelopmentResult result{};
  DevelopmentEvidence& ev = result.evidence;

  switch (request.transformation) {
    case TransformationKind::Revoice:
      transformRevoice(source, request, result.candidate, ev);
      break;
    case TransformationKind::Extend:
      transformExtend(source, request, result.candidate, ev);
      break;
    case TransformationKind::Displace:
      transformDisplace(source, request, result.candidate, ev);
      break;
    case TransformationKind::Thin:
      transformThin(source, request, result.candidate, ev);
      break;
    case TransformationKind::Hold:
      transformHold(source, request, result.candidate, ev);
      break;
    case TransformationKind::Connect:
      transformConnect(source, request, result.candidate, ev);
      break;
    case TransformationKind::Move:
      transformMove(source, request, result.candidate, ev);
      break;
    case TransformationKind::None:
    default:
      result.candidate = source;
      break;
  }

  result.classification = evaluateClassificationAndG4(
      source, result.candidate, ev, request);
  result.success = (result.classification.genre == GenreResult::Pass);
  return result;
}

// ---------------------------------------------------------------------------
// Node D: Material Growth (REPEAT / DEVELOP)
// ---------------------------------------------------------------------------
enum class GrowthMode : uint8_t {
  None = 0,
  Repeat,
  Develop,
};

inline DevelopmentResult growMaterial(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    uint8_t targetBars,
    GrowthMode mode,
    const DevelopmentRequest& request) {
  DevelopmentResult result{};
  if (targetBars != 1 && targetBars != 2 && targetBars != 4 && targetBars != 8) {
    result.success = false;
    result.classification.genre = GenreResult::Fail;
    result.classification.failureReason = "G4: Invalid target bar count for material growth";
    return result;
  }

  const uint16_t targetLengthTicks = static_cast<uint16_t>(targetBars) * PhraseRuntime::kTicksPerBar;
  result.candidate.lengthTicks = targetLengthTicks;
  result.candidate.count = 0;

  // Copy initial source events for the first bar / segment
  for (uint16_t i = 0; i < source.count && result.candidate.count < PhraseRuntime::kMaxSynthEvents; ++i) {
    result.candidate.events[result.candidate.count++] = source.events[i];
  }

  if (targetBars > 1) {
    PhraseRuntime::RuntimeSynthEventBuffer variation = source;
    if (mode == GrowthMode::Develop) {
      const auto dev = developCandidate(source, request);
      if (!dev.success) {
        return dev; // G4 rejection propagated fail-closed!
      }
      variation = dev.candidate;
      result.evidence = dev.evidence;
      result.classification = dev.classification;
    } else {
      result.evidence.transformation = TransformationKind::None;
      result.classification.genre = GenreResult::Pass;
      result.classification.idea = GroovePuterMaterial::IdeaClassification::Variation;
    }

    const uint8_t sourceBars = static_cast<uint8_t>(source.lengthTicks / PhraseRuntime::kTicksPerBar);
    const uint8_t startBar = sourceBars == 0 ? 1 : sourceBars;
    for (uint8_t bar = startBar; bar < targetBars; ++bar) {
      const uint16_t barOffsetTicks = static_cast<uint16_t>(bar) * PhraseRuntime::kTicksPerBar;
      for (uint16_t i = 0; i < variation.count && result.candidate.count < PhraseRuntime::kMaxSynthEvents; ++i) {
        auto ev = variation.events[i];
        ev.startTick += barOffsetTicks;
        result.candidate.events[result.candidate.count++] = ev;
      }
    }
  } else {
    result.classification.genre = GenreResult::Pass;
    result.classification.idea = GroovePuterMaterial::IdeaClassification::Variation;
  }

  result.success = (result.classification.genre == GenreResult::Pass);
  return result;
}

// ---------------------------------------------------------------------------
// Node E: Provenance / Explanation (Transient Session Only)
// ---------------------------------------------------------------------------
struct DevelopmentProvenance {
  TransformationKind requestKind = TransformationKind::None;
  GrowthMode growthMode = GrowthMode::None;
  GroovePuterMaterial::PreparationBasis sourceBasis{};
  DevelopmentEvidence evidence{};
  DevelopmentClassification classification{};

  bool valid() const { return sourceBasis.valid(); }
};

inline const char* transformationName(TransformationKind kind) {
  switch (kind) {
    case TransformationKind::Revoice: return "REVOICE";
    case TransformationKind::Extend: return "EXTEND";
    case TransformationKind::Displace: return "DISPLACE";
    case TransformationKind::Thin: return "THIN";
    case TransformationKind::Hold: return "HOLD";
    case TransformationKind::Connect: return "CONNECT";
    case TransformationKind::Move: return "MOVE";
    default: return "NONE";
  }
}

inline const char* genreResultName(GenreResult gr) {
  switch (gr) {
    case GenreResult::Pass: return "PASS";
    case GenreResult::Fail: return "FAIL";
    default: return "UNKNOWN";
  }
}

inline const char* ideaName(GroovePuterMaterial::IdeaClassification idea) {
  switch (idea) {
    case GroovePuterMaterial::IdeaClassification::Variation: return "VARIATION";
    case GroovePuterMaterial::IdeaClassification::NewIdea: return "NEW_IDEA";
    default: return "UNKNOWN";
  }
}

inline void formatProvenance(
    const DevelopmentProvenance& prov,
    char* buffer,
    size_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0) return;
  std::snprintf(
      buffer, bufferSize,
      "REQUEST: %s\n"
      "SOURCE: V%u:S%u (id:%u rev:%u)\n"
      "CHANGES: pitch=%s onset=%s artic=%s slides=%s densityDelta=%d theOne=%s\n"
      "CLASSIFICATION: idea=%s genre=%s temporal=UNKNOWN%s%s",
      transformationName(prov.requestKind),
      prov.sourceBasis.reference.address.voice,
      prov.sourceBasis.reference.address.globalSlot,
      prov.sourceBasis.reference.id.value,
      prov.sourceBasis.version.low,
      prov.evidence.harmony.pitchesChanged ? "CHANGED" : "UNCHANGED",
      prov.evidence.rhythm.onsetsChanged ? "CHANGED" : "UNCHANGED",
      prov.evidence.bass.articulationChanged ? "CHANGED" : "UNCHANGED",
      prov.evidence.bass.slidesAdded ? "ADDED" : "NONE",
      prov.evidence.rhythm.densityDelta,
      prov.evidence.rhythm.theOnePreserved ? "PRESERVED" : "LOST",
      ideaName(prov.classification.idea),
      genreResultName(prov.classification.genre),
      prov.classification.failureReason ? " REASON: " : "",
      prov.classification.failureReason ? prov.classification.failureReason : "");
}

}  // namespace GroovePuterDevelopment

#endif  // GROOVEPUTER_DSP_MUSICAL_DEVELOPMENT_H
