#pragma once

#include "stage7a_session.h"
#include "../../dsp/miniacid_engine.h"
#include "../../ui/ui_common.h"

namespace GroovePuterRhythm {
namespace Stage7AAudition {

inline Session& cardputerSession() {
  static Session session;
  return session;
}

inline void showCardputerStatus(const Session& session) {
  char status[96]{};
  session.formatStatus(status, sizeof(status));
  Serial.printf("[STAGE7A-AUDITION] %s bpm~%u atlas=%s\n",
                status,
                static_cast<unsigned>(session.currentDefinition().suggestedBpm),
                session.currentDefinition().atlasCandidate);
  UI::showToast(status, 1200);
}

// Returns true when Stage 7A owns the event. The caller already holds the
// normal control-plane audio mutation guard used by MiniAcidDisplay events.
inline bool handleCardputerEvent(const UIEvent& event, MiniAcid& engine) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  Session& session = cardputerSession();
  const char key = event.key;
  const bool toggleChord =
      event.alt && event.ctrl && (key == 'a' || key == 'A');

  if (!session.active() && !toggleChord) return false;

  // Preserve the existing global transport and emergency project reset owners.
  const bool panicChord =
      event.alt && event.ctrl && (key == '\b' || key == static_cast<char>(127));
  if (session.active() && (panicChord || key == ' ')) return false;

  SceneManager& scenes = engine.sceneManager();
  DrumPatternSet& drums = scenes.editCurrentDrumPattern();
  SynthPattern& synthA = scenes.editCurrentSynthPattern(0);
  SynthPattern& synthB = scenes.editCurrentSynthPattern(1);

  if (toggleChord) {
    if (session.active()) {
      session.deactivate(drums, synthA, synthB);
      Serial.println("[STAGE7A-AUDITION] OFF / original patterns restored");
      UI::showToast("S7A OFF / RESTORED", 1200);
      return true;
    }
    if (engine.songModeEnabled()) {
      Serial.println("[STAGE7A-AUDITION] refused: Song mode active");
      UI::showToast("S7A: EXIT SONG MODE", 1200);
      return true;
    }
    if (!session.activate(drums, synthA, synthB)) {
      Serial.println("[STAGE7A-AUDITION] activation failed");
      UI::showToast("S7A INVALID", 1000);
      return true;
    }
    showCardputerStatus(session);
    return true;
  }

  // Audition is intentionally modal: temporary patterns cannot be navigated
  // into a Save/project workflow. Only the explicit command set below is live.
  if (!event.alt || event.ctrl || event.meta) return true;

  bool handled = true;
  bool ok = false;
  if (key >= '1' && key <= '5') {
    ok = session.selectCandidate(
        static_cast<uint8_t>(key - '1'), drums, synthA, synthB);
  } else if (key == 'p' || key == 'P') {
    ok = session.cycleLevel(drums, synthA, synthB);
  } else if (key == '[') {
    ok = session.shiftSeed(-1, drums, synthA, synthB);
  } else if (key == ']') {
    ok = session.shiftSeed(1, drums, synthA, synthB);
  } else if (key == 'r' || key == 'R') {
    ok = session.rerender(drums, synthA, synthB);
  } else {
    handled = false;
  }

  if (!handled) return true;
  if (!ok) {
    Serial.println("[STAGE7A-AUDITION] command rejected / previous pattern preserved");
    UI::showToast("S7A INVALID", 900);
  } else {
    showCardputerStatus(session);
  }
  return true;
}

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
