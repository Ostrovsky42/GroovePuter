#pragma once
#ifndef GROOVEPUTER_UI_MATERIAL_DEVELOPMENT_UX_H
#define GROOVEPUTER_UI_MATERIAL_DEVELOPMENT_UX_H

#include <cctype>
#include "ui_core.h"
#include "ui_widgets.h"
#include "src/dsp/miniacid_engine.h"
#include "src/dsp/musical_development.h"

namespace GroovePuterMaterialDevelopmentUx {

inline bool isDevelopEvent(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  if (event.ctrl || event.meta) return false;
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  return key == 'd';
}

inline bool isVaryEvent(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  if (event.ctrl || event.meta) return false;
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  return key == 'v' && event.alt;
}

inline bool isGoEvent(const UIEvent& event, const MiniAcid& engine, int voiceIndex) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  if (!engine.hasPendingMaterial(voiceIndex)) return false;
  // Enter without modifiers is the only GO gesture. Modified Enter belongs to
  // ACCEPT and must never activate NEXT as a side effect.
  if (event.alt || event.ctrl || event.meta) return false;
  return (event.key == '\n' || event.key == '\r' || event.key == 0x0A || event.key == 0x0D);
}

inline bool isCancelEvent(const UIEvent& event, const MiniAcid& engine, int voiceIndex) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  if (!engine.hasPendingMaterial(voiceIndex)) return false;
  // Plain Escape is intentionally narrow: Alt+Backspace/Delete is DISCARD,
  // and destructive actions must not be inferred from editing keys.
  if (event.alt || event.ctrl || event.meta) return false;
  return event.key == 0x1B || event.scancode == GROOVEPUTER_ESCAPE;
}

inline bool isDiscardEvent(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  return (event.alt && (event.key == '\b' || event.key == 0x7F ||
                        event.key == 'x' || event.key == 'X'));
}

inline bool handleDevelop(
    MiniAcid& engine,
    int voiceIndex,
    GroovePuterDevelopment::TransformationKind transformation =
        GroovePuterDevelopment::TransformationKind::Revoice) {
  GroovePuterDevelopment::DevelopmentRequest req{};
  req.transformation = transformation;
  req.genreId = static_cast<uint8_t>(engine.genreManager().generativeMode());
  req.rootKey = 0; // Default C root reference

  GroovePuterDevelopment::DevelopmentResult result{};
  const auto prepareResult =
      engine.developWorkingMaterial(voiceIndex, req, &result);

  if (prepareResult == MiniAcid::NextPrepareResult::Prepared ||
      prepareResult == MiniAcid::NextPrepareResult::Replaced) {
    if (result.classification.idea ==
        GroovePuterMaterial::IdeaClassification::NewIdea) {
      UI::showToast("NEXT READY: NEW MATERIAL", 1400);
    } else if (result.classification.idea ==
               GroovePuterMaterial::IdeaClassification::Variation) {
      UI::showToast("NEXT READY: VARIATION", 1400);
    } else if (result.classification.idea ==
               GroovePuterMaterial::IdeaClassification::Preserved) {
      UI::showToast("NEXT READY: REFINED", 1400);
    } else {
      UI::showToast("NEXT READY", 1400);
    }
    return true;
  }

  // G4/internal classifier diagnostics remain available in logs and tests;
  // the instrument tells the player the actionable outcome instead.
  if (prepareResult == MiniAcid::NextPrepareResult::UnsupportedCurrentState &&
      engine.songNeedsNextBuffer(voiceIndex)) {
    UI::showToast("NEXT BUSY: SONG", 1200);
  } else {
    UI::showToast("NEXT NOT READY", 1200);
  }
  return true;
}

inline bool handleGo(MiniAcid& engine, int voiceIndex) {
  if (!engine.hasPendingMaterial(voiceIndex)) {
    UI::showToast("NO NEXT MATERIAL", 1000);
    return true;
  }
  const auto req = engine.requestGoNextMaterial(voiceIndex);
  if (req == MiniAcid::GoRequestResult::Queued) {
    UI::showToast("GO: QUEUED", 1200);
    return true;
  }
  if (req == MiniAcid::GoRequestResult::ActivatedImmediately) {
    UI::showToast("GO: ACTIVATED", 1200);
    return true;
  }
  UI::showToast("GO: FAILED", 1200);
  return true;
}

inline bool handleCancel(MiniAcid& engine, int voiceIndex) {
  if (!engine.hasPendingMaterial(voiceIndex)) return false;
  if (engine.isGoQueued(voiceIndex)) {
    engine.cancelGoQueue(voiceIndex);
    UI::showToast("GO DISARMED: NEXT KEPT", 1200);
    return true;
  }
  const bool cancelled = engine.cancelNextMaterial(voiceIndex);
  UI::showToast(cancelled ? "NEXT DISCARDED" : "NEXT CANCEL FAILED", 1000);
  return true;
}

inline bool handleDiscard(MiniAcid& engine, int voiceIndex) {
  const bool hadQueuedGo = engine.isGoQueued(voiceIndex);
  const auto result = engine.discardCurrentMaterial(voiceIndex);
  switch (result) {
    case MiniAcid::DiscardResult::Discarded:
      UI::showToast(hadQueuedGo ? "DISCARD: RESTORED; GO DISARMED"
                                : "DISCARD: RESTORED",
                    hadQueuedGo ? 1500 : 1200);
      return true;
    case MiniAcid::DiscardResult::AlreadyClean:
      UI::showToast("DISCARD: ALREADY CLEAN", 1000);
      return true;
    default:
      UI::showToast("DISCARD: FAILED", 1200);
      return true;
  }
}

}  // namespace GroovePuterMaterialDevelopmentUx

#endif  // GROOVEPUTER_UI_MATERIAL_DEVELOPMENT_UX_H
