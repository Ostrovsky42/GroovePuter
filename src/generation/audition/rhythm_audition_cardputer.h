#pragma once

#include "rhythm_audition_session.h"
#include "../../dsp/miniacid_engine.h"
#include "../../ui/ui_common.h"

namespace GroovePuterRhythm {
namespace Audition {

inline Session& cardputerSession() {
  static Session session;
  return session;
}

inline void showCardputerStatus(const Session& session) {
  char status[72]{};
  session.formatStatus(status, sizeof(status));
  Serial.printf("[RHYTHM-AUDITION] %s bpm~%u\n",
                status,
                static_cast<unsigned>(
                    session.controller().currentDefinition().suggestedBpm));
  UI::showToast(status, 1100);
}

inline bool handleCardputerAuditionEvent(const UIEvent& evt,
                                         MiniAcid& engine) {
  if (!evt.alt) return false;

  Session& session = cardputerSession();
  const char key = evt.key;
  SceneManager& scenes = engine.sceneManager();
  DrumPatternSet& drums = scenes.editCurrentDrumPattern();
  SynthPattern& synthA = scenes.editCurrentSynthPattern(0);
  SynthPattern& synthB = scenes.editCurrentSynthPattern(1);

  if (key == 'a' || key == 'A') {
    if (session.active()) {
      session.deactivate(drums, synthA, synthB);
      Serial.println("[RHYTHM-AUDITION] OFF restored original current patterns");
      UI::showToast("AUDITION OFF / RESTORED", 1100);
      return true;
    }
    if (engine.songModeEnabled()) {
      Serial.println("[RHYTHM-AUDITION] refused: Song mode is active");
      UI::showToast("AUD: EXIT SONG MODE", 1100);
      return true;
    }
    if (!session.activate(drums, synthA, synthB)) {
      Serial.println("[RHYTHM-AUDITION] activation failed");
      UI::showToast("AUDITION INVALID", 1100);
      return true;
    }
    showCardputerStatus(session);
    return true;
  }

  if (!session.active()) return false;

  bool handled = false;
  bool ok = false;
  if (key >= '1' && key <= '5') {
    handled = true;
    ok = session.selectDefinition(
        static_cast<uint8_t>(key - '1'), drums, synthA, synthB);
  } else if (key == 'p' || key == 'P') {
    handled = true;
    ok = session.cycleLevel(drums, synthA, synthB);
  } else if (key == '[') {
    handled = true;
    ok = session.shiftSeed(-1, drums, synthA, synthB);
  } else if (key == ']') {
    handled = true;
    ok = session.shiftSeed(1, drums, synthA, synthB);
  } else if (key == 'b' || key == 'B') {
    handled = true;
    ok = session.toggleBass(drums, synthA, synthB);
  } else if (key == 'r' || key == 'R') {
    handled = true;
    ok = session.rerender(drums, synthA, synthB);
  }

  if (!handled) return false;
  if (!ok) {
    Serial.println("[RHYTHM-AUDITION] command rejected; previous audition pattern preserved");
    UI::showToast("AUDITION INVALID", 900);
  } else {
    showCardputerStatus(session);
  }
  return true;
}

}  // namespace Audition
}  // namespace GroovePuterRhythm
