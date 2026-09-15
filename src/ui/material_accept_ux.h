#pragma once
#ifndef GROOVEPUTER_UI_MATERIAL_ACCEPT_UX_H
#define GROOVEPUTER_UI_MATERIAL_ACCEPT_UX_H

#include <cctype>
#include "ui_core.h"
#include "ui_widgets.h"
#include "src/dsp/miniacid_engine.h"

namespace GroovePuterMaterialAcceptUx {

inline bool isAcceptEvent(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;

  // Alt+Enter, Ctrl+Enter or Meta+Enter (Enter = Accept)
  if ((event.alt || event.ctrl || event.meta) &&
      (event.key == '\n' || event.key == '\r' || event.key == 0x0A || event.key == 0x0D)) {
    return true;
  }

  return false;
}

inline bool handleAccept(MiniAcid& engine, int voiceIndex) {
  const auto result = engine.acceptMaterialWorking(voiceIndex);
  switch (result) {
    case MiniAcid::AcceptResult::Accepted:
      UI::showToast("ACCEPT SUCCESS", 1500);
      return true;
    case MiniAcid::AcceptResult::AlreadyClean:
      UI::showToast("ACCEPT: CLEAN", 1000);
      return true;
    case MiniAcid::AcceptResult::NoWorkingMaterial:
      UI::showToast("ACCEPT: NO WORKING", 1000);
      return true;
    case MiniAcid::AcceptResult::InvalidVoice:
    case MiniAcid::AcceptResult::InvalidCandidate:
      UI::showToast("ACCEPT: INVALID", 1200);
      return true;
    case MiniAcid::AcceptResult::CommitFailed:
      UI::showToast("ACCEPT: COMMIT FAILED", 1500);
      return true;
    case MiniAcid::AcceptResult::UnsupportedCurrentState:
    default:
      UI::showToast("ACCEPT FAILED", 1200);
      return true;
  }
}

}  // namespace GroovePuterMaterialAcceptUx

#endif  // GROOVEPUTER_UI_MATERIAL_ACCEPT_UX_H
