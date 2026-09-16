#pragma once
#ifndef GROOVEPUTER_DSP_MUSICAL_DEVELOPMENT_H
#define GROOVEPUTER_DSP_MUSICAL_DEVELOPMENT_H

#include <cstdint>
#include <cstring>

#include "src/phrase/runtime_synth_events.h"
#include "src/phrase/runtime_phrase_edit.h"
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

enum class DevelopmentDisposition : uint8_t {
  Reject = 0,
  Hold,
  Publish,
};

struct DevelopmentResult {
  bool success = false;
  PhraseRuntime::RuntimeSynthEventBuffer candidate{};
  DevelopmentEvidence evidence{};
  DevelopmentClassification classification{};
  DevelopmentDisposition disposition = DevelopmentDisposition::Reject;
};

inline DevelopmentDisposition evaluateDisposition(
    const DevelopmentClassification& classification) {
  if (classification.genre == GenreResult::Fail) {
    return DevelopmentDisposition::Reject;
  }
  if (classification.genre == GenreResult::Unknown) {
    return DevelopmentDisposition::Reject;
  }
  if (classification.genre == GenreResult::Pass) {
    return DevelopmentDisposition::Publish;
  }
  return DevelopmentDisposition::Reject;
}

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
    }
  }

  // Derive evidence from observed musical facts
  bool allPitchClassesPreserved = true;
  bool anyPitchChanged = false;
  for (uint16_t i = 0; i < candidate.count; ++i) {
    if ((candidate.events[i].note % 12) != (source.events[i].note % 12)) {
      allPitchClassesPreserved = false;
    }
    if (candidate.events[i].note != source.events[i].note) {
      anyPitchChanged = true;
    }
  }
  evidence.harmony.pitchesChanged = anyPitchChanged;
  evidence.harmony.pitchClassesPreserved = allPitchClassesPreserved;
  evidence.harmony.rootPreserved = allPitchClassesPreserved;
  evidence.harmony.scaleDegreesValid = allPitchClassesPreserved;
  evidence.harmony.harmonicSupport = allPitchClassesPreserved ? TriState::Pass : TriState::Fail;
  evidence.harmony.cadence = TriState::Unknown;
  evidence.harmony.harmonicFunction = TriState::Unknown;
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformExtend(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& /*request*/,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Extend;
  evidence.harmony.cadence = TriState::Unknown;
  evidence.harmony.harmonicFunction = TriState::Unknown;
  // EXTEND is deferred in 0.9.13: repository lacks authoritative production tonal root context.
}

inline void transformDisplace(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Displace;

  const bool genreGuardsTheOne =
      request.requireTheOne ||
      request.genreId == static_cast<uint8_t>(GenerativeMode::FunkSoul);

  bool anyOnsetChanged = false;
  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& ev = candidate.events[i];
    if (ev.startTick == 0 && genreGuardsTheOne && !request.forceDisplaceTheOne) {
      // Funk / The One: downbeat anchor is preserved
      continue;
    }
    const uint16_t newStart = (ev.startTick + request.displaceTicks) % candidate.lengthTicks;
    if (newStart != ev.startTick) {
      ev.startTick = newStart;
      anyOnsetChanged = true;
    }
  }
  evidence.rhythm.onsetsChanged = anyOnsetChanged;
  evidence.rhythm.metricAlignment = TriState::Pass;
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformThin(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& /*request*/,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  evidence.transformation = TransformationKind::Thin;
  evidence.rhythm.metricAlignment = TriState::Pass;
  candidate.lengthTicks = source.lengthTicks;
  candidate.count = 0;

  for (uint16_t i = 0; i < source.count; ++i) {
    const auto& ev = source.events[i];
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
    const DevelopmentRequest& /*request*/,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Hold;

  int16_t totalDurationDelta = 0;
  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& ev = candidate.events[i];
    const uint16_t beforeDur = ev.durationSubticks;
    const uint16_t extended = beforeDur * 2;
    if (extended > beforeDur) {
      ev.durationSubticks = extended;
      evidence.bass.durationsExtended = true;
      evidence.bass.articulationChanged = true;
      totalDurationDelta += static_cast<int16_t>(extended - beforeDur);
    }
  }
  evidence.bass.durationDeltaSubticks = totalDurationDelta;
  evidence.bass.contourPreserved = TriState::Pass;
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformConnect(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& /*request*/,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Connect;

  for (uint16_t i = 0; i < candidate.count; ++i) {
    auto& ev = candidate.events[i];
    if ((ev.flags & PhraseRuntime::kEventSlide) == 0) {
      ev.flags |= PhraseRuntime::kEventSlide;
      evidence.bass.slidesAdded = true;
      evidence.bass.articulationChanged = true;
    }
  }
  evidence.bass.contourPreserved = TriState::Pass;
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline void transformMove(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    DevelopmentEvidence& evidence) {
  candidate = source;
  evidence.transformation = TransformationKind::Move;

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
  evidence.bass.contourPreserved = TriState::Pass;
  evidence.rhythm.theOnePreserved = hasEventOnTheOne(candidate);
}

inline DevelopmentClassification evaluateClassificationAndG4(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const PhraseRuntime::RuntimeSynthEventBuffer& candidate,
    const DevelopmentEvidence& evidence,
    const DevelopmentRequest& request,
    const PhraseRuntime::RuntimeSynthEventBuffer* sourceAnchor = nullptr) {
  DevelopmentClassification classification{};
  classification.temporalRole = TemporalRoleResult::Unknown;

  if (request.transformation == TransformationKind::Extend) {
    classification.genre = GenreResult::Fail;
    classification.failureReason = "EXTEND DEFERRED: NO TONAL ROOT AUTHORITY";
    classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    return classification;
  }

  // 1. G4 Structural Constraints
  // Bounded metric gravity witness for Funk/Soul:
  // If the source motif established metric gravity with a downbeat anchor (The One at step 0),
  // candidate must not destroy that protected anchor.
  const bool genreRequiresTheOne =
      request.requireTheOne ||
      request.genreId == static_cast<uint8_t>(GenerativeMode::FunkSoul);

  const bool sourceHadTheOne = hasEventOnTheOne(source);
  if (genreRequiresTheOne && sourceHadTheOne && !hasEventOnTheOne(candidate)) {
    classification.genre = GenreResult::Fail;
    classification.failureReason = "G4: Funk downbeat metric anchor destroyed";
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

  // 2. Idea Continuity Evaluation: compares against predecessor AND source anchor
  if (sourceAnchor != nullptr && sourceAnchor->count > 0) {
    bool anchorPitchDiff = false;
    bool anchorOnsetDiff = false;
    if (candidate.count != sourceAnchor->count) {
      anchorOnsetDiff = true;
    }
    for (uint16_t i = 0; i < candidate.count && i < sourceAnchor->count; ++i) {
      if ((candidate.events[i].note % 12) != (sourceAnchor->events[i].note % 12)) {
        anchorPitchDiff = true;
      }
      if (candidate.events[i].startTick != sourceAnchor->events[i].startTick) {
        anchorOnsetDiff = true;
      }
    }
    if (anchorPitchDiff && anchorOnsetDiff) {
      classification.idea = GroovePuterMaterial::IdeaClassification::NewIdea;
    } else if (evidence.harmony.pitchesChanged ||
               evidence.rhythm.onsetsChanged ||
               evidence.bass.articulationChanged) {
      classification.idea = GroovePuterMaterial::IdeaClassification::Variation;
    } else {
      classification.idea = GroovePuterMaterial::IdeaClassification::Preserved;
    }
  } else {
    if (evidence.harmony.pitchesChanged && evidence.rhythm.onsetsChanged) {
      classification.idea = GroovePuterMaterial::IdeaClassification::NewIdea;
    } else if (evidence.harmony.pitchesChanged ||
               evidence.rhythm.onsetsChanged ||
               evidence.bass.articulationChanged) {
      classification.idea = GroovePuterMaterial::IdeaClassification::Variation;
    } else {
      classification.idea = GroovePuterMaterial::IdeaClassification::Preserved;
    }
  }

  return classification;
}

inline DevelopmentResult developCandidate(
    const PhraseRuntime::RuntimeSynthEventBuffer& source,
    const DevelopmentRequest& request,
    const PhraseRuntime::RuntimeSynthEventBuffer* sourceAnchor = nullptr) {
  DevelopmentResult result{};
  if (!RuntimePhraseEdit::validate(source)) {
    result.classification.genre = GenreResult::Fail;
    result.classification.failureReason = "Source validation failed";
    result.classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    result.disposition = DevelopmentDisposition::Reject;
    result.success = false;
    return result;
  }

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

  if (!RuntimePhraseEdit::validate(result.candidate)) {
    result.classification.genre = GenreResult::Fail;
    result.classification.failureReason = "Candidate validation failed";
    result.classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    result.disposition = DevelopmentDisposition::Reject;
    result.success = false;
    return result;
  }

  result.classification = evaluateClassificationAndG4(
      source, result.candidate, ev, request, sourceAnchor);
  result.disposition = evaluateDisposition(result.classification);
  result.success = (result.disposition == DevelopmentDisposition::Publish);
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
  if (!RuntimePhraseEdit::validate(source)) {
    result.classification.genre = GenreResult::Fail;
    result.classification.failureReason = "Source validation failed";
    result.classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    result.disposition = DevelopmentDisposition::Reject;
    result.success = false;
    return result;
  }

  if (targetBars != 1 && targetBars != 2 && targetBars != 4 && targetBars != 8) {
    result.success = false;
    result.classification.genre = GenreResult::Fail;
    result.classification.failureReason = "G4: Invalid target bar count for material growth";
    result.classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    result.disposition = DevelopmentDisposition::Reject;
    return result;
  }

  if (mode == GrowthMode::Develop) {
    // Truthful growth: multi-bar development is explicitly deferred in 0.9.13
    result.success = false;
    result.classification.genre = GenreResult::Fail;
    result.classification.failureReason = "DEVELOP GROWTH DEFERRED: REQUIRES MULTI-BAR PHRASE ENGINE";
    result.classification.idea = GroovePuterMaterial::IdeaClassification::Unknown;
    result.classification.temporalRole = TemporalRoleResult::Unknown;
    result.disposition = DevelopmentDisposition::Reject;
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
    const PhraseRuntime::RuntimeSynthEventBuffer& variation = source;
    result.evidence.transformation = TransformationKind::None;
    result.classification.genre = GenreResult::Pass;
    result.classification.idea = GroovePuterMaterial::IdeaClassification::Preserved;
    result.classification.temporalRole = TemporalRoleResult::Pass;

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
    result.classification.idea = GroovePuterMaterial::IdeaClassification::Preserved;
    result.classification.temporalRole = TemporalRoleResult::Pass;
  }

  result.disposition = evaluateDisposition(result.classification);
  result.success = (result.disposition == DevelopmentDisposition::Publish);
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
    case TransformationKind::Extend: return "EXTEND (DEFERRED)";
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

inline const char* temporalRoleName(TemporalRoleResult tr) {
  switch (tr) {
    case TemporalRoleResult::Pass: return "PASS";
    case TemporalRoleResult::Fail: return "FAIL";
    default: return "UNKNOWN";
  }
}

inline const char* ideaName(GroovePuterMaterial::IdeaClassification idea) {
  switch (idea) {
    case GroovePuterMaterial::IdeaClassification::Preserved: return "PRESERVED";
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
      "CLASSIFICATION: idea=%s genre=%s temporal=%s%s%s",
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
      temporalRoleName(prov.classification.temporalRole),
      prov.classification.failureReason ? " REASON: " : "",
      prov.classification.failureReason ? prov.classification.failureReason : "");
}

}  // namespace GroovePuterDevelopment

#endif  // GROOVEPUTER_DSP_MUSICAL_DEVELOPMENT_H
