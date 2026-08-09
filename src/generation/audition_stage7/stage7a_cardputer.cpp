#include "stage7a_cardputer.h"

#include "stage7a_session.h"
#include "../../dsp/miniacid_engine.h"
#include "../../midi/transport_clock_runtime.h"
#include "../../ui/ui_common.h"
#include "../../ui/ui_input.h"

namespace GroovePuterRhythm {
namespace Stage7AAudition {
namespace {

Session& auditionSession() {
  static Session session;
  return session;
}

void showCardputerStatus(const Session& session) {
  char status[96]{};
  session.formatStatus(status, sizeof(status));
  Serial.printf("[STAGE7B-AUDITION] %s bpm~%u atlas=%s\n",
                status,
                static_cast<unsigned>(session.currentDefinition().suggestedBpm),
                session.currentDefinition().atlasCandidate);
  UI::showToast(status, 1200);
}

}  // namespace

bool CardputerSessionFacade::active() const {
  return auditionSession().active();
}

const CardputerSessionFacade& cardputerSession() {
  static const CardputerSessionFacade facade{};
  return facade;
}

bool handleCardputerEvent(const UIEvent& event, MiniAcid& engine) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  Session& session = auditionSession();
  const char key = event.key;
  const bool toggleChord =
      event.alt && event.ctrl && (key == 'a' || key == 'A');

  if (!session.active() && !toggleChord) return false;

  SceneManager& scenes = engine.sceneManager();
  DrumPatternSet& drums = scenes.editCurrentDrumPattern();
  SynthPattern& synthA = scenes.editCurrentSynthPattern(0);
  SynthPattern& synthB = scenes.editCurrentSynthPattern(1);

  const bool panicChord =
      event.alt && event.ctrl && (key == '\b' || key == static_cast<char>(127));
  if (session.active() && panicChord) {
    session.deactivate(drums, synthA, synthB);
    Serial.println("[STAGE7B-AUDITION] OFF before project reset");
    return false;
  }

  if (toggleChord) {
    if (session.active()) {
      session.deactivate(drums, synthA, synthB);
      Serial.println("[STAGE7B-AUDITION] OFF / original patterns restored");
      UI::showToast("S7B OFF / RESTORED", 1200);
      return true;
    }
    if (engine.songModeEnabled()) {
      Serial.println("[STAGE7B-AUDITION] refused: Song mode active");
      UI::showToast("S7B: EXIT SONG MODE", 1200);
      return true;
    }
    if (!session.activate(drums, synthA, synthB)) {
      Serial.println("[STAGE7B-AUDITION] activation failed");
      UI::showToast("S7B INVALID", 1000);
      return true;
    }
    showCardputerStatus(session);
    return true;
  }

  if (key == ' ') {
    if (GroovePuterMidi::transportClockRuntime().source() ==
        GroovePuterMidi::TransportClockSource::SeqtrakExternal) {
      UI::showToast("SEQ MASTER: USE SEQTRAK", 900);
    } else if (engine.isPlaying()) {
      engine.stop();
    } else {
      engine.start();
    }
    return true;
  }

  if (!event.ctrl || event.alt || event.meta) return true;

  const int nav = UIInput::navCode(event);
  bool handled = true;
  bool ok = false;
  if (key >= '1' && key <= '4') {
    ok = session.selectCandidate(
        static_cast<uint8_t>(key - '1'), drums, synthA, synthB);
  } else if (key == 'p' || key == 'P') {
    ok = session.cycleLevel(drums, synthA, synthB);
  } else if (nav == GROOVEPUTER_LEFT) {
    ok = session.shiftSeed(-1, drums, synthA, synthB);
  } else if (nav == GROOVEPUTER_RIGHT) {
    ok = session.shiftSeed(1, drums, synthA, synthB);
  } else if (key == 'r' || key == 'R') {
    ok = session.rerender(drums, synthA, synthB);
  } else {
    handled = false;
  }

  if (!handled) return true;
  if (!ok) {
    Serial.println("[STAGE7B-AUDITION] command rejected / previous pattern preserved");
    UI::showToast("S7B INVALID", 900);
  } else {
    showCardputerStatus(session);
  }
  return true;
}

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
