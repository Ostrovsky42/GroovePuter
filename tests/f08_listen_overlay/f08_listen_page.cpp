#include "f08_listen_page.h"

#include <cstdio>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "../../dsp/miniacid_engine.h"
#include "../layout_manager.h"
#include "../ui_colors.h"
#include "../ui_common.h"
#include "../ui_input.h"

namespace {

const char* variantName(GroovePuterRhythm::F08ListenVariant variant) {
  return variant == GroovePuterRhythm::F08ListenVariant::Old ? "OLD" : "NEW";
}

}  // namespace

F08ListenPage::F08ListenPage(IGfx& gfx,
                             MiniAcid& mini_acid,
                             AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
}

void F08ListenPage::setTrackMuted(int track, bool muted) {
  if (track < 0 || track >= 10) return;
  const bool currentMuted = !mini_acid_.isTrackActive(track);
  if (currentMuted == muted) return;

  switch (track) {
    case 0: mini_acid_.setMute303(0, muted); break;
    case 1: mini_acid_.setMute303(1, muted); break;
    case 2: mini_acid_.toggleMuteKick(); break;
    case 3: mini_acid_.toggleMuteSnare(); break;
    case 4: mini_acid_.toggleMuteHat(); break;
    case 5: mini_acid_.toggleMuteOpenHat(); break;
    case 6: mini_acid_.toggleMuteMidTom(); break;
    case 7: mini_acid_.toggleMuteHighTom(); break;
    case 8: mini_acid_.toggleMuteRim(); break;
    case 9: mini_acid_.toggleMuteClap(); break;
    default: break;
  }
}

void F08ListenPage::setAllTracksMuted(bool muted) {
  for (int track = 0; track < 10; ++track) setTrackMuted(track, muted);
}

void F08ListenPage::captureRuntime() {
  if (saved_.captured) return;
  saved_.captured = true;
  saved_.playing = mini_acid_.isPlaying();
  saved_.bpm = mini_acid_.bpm();
  saved_.songMode = mini_acid_.songModeEnabled();
  saved_.loopMode = mini_acid_.loopModeEnabled();
  saved_.loopStart = mini_acid_.loopStartRow();
  saved_.loopEnd = mini_acid_.loopEndRow();
  saved_.activeSongSlot = mini_acid_.activeSongSlot();
  saved_.playbackSongSlot = mini_acid_.songPlaybackSlot();
  saved_.songPosition = mini_acid_.currentSongPosition();
  saved_.drumBank = mini_acid_.currentDrumBankIndex();
  saved_.drumPattern = mini_acid_.currentDrumPatternIndex();
  for (int synth = 0; synth < 2; ++synth) {
    saved_.synthBank[synth] = mini_acid_.current303BankIndex(synth);
    saved_.synthPattern[synth] = mini_acid_.current303PatternIndex(synth);
  }
  for (int track = 0; track < 10; ++track) {
    saved_.muted[track] = !mini_acid_.isTrackActive(track);
  }
}

void F08ListenPage::restoreRuntime() {
  if (!saved_.captured) return;

  const auto restore = [&]() {
    mini_acid_.stop();
    mini_acid_.setSongMode(false);
    mini_acid_.setDrumBankIndex(saved_.drumBank);
    mini_acid_.setDrumPatternIndex(saved_.drumPattern);
    for (int synth = 0; synth < 2; ++synth) {
      mini_acid_.set303BankIndex(synth, saved_.synthBank[synth]);
      mini_acid_.set303PatternIndex(synth, saved_.synthPattern[synth]);
    }
    mini_acid_.setActiveSongSlot(saved_.activeSongSlot);
    mini_acid_.setSongPlaybackSlot(saved_.playbackSongSlot);
    mini_acid_.setSongPosition(saved_.songPosition);
    mini_acid_.setLoopRange(saved_.loopStart, saved_.loopEnd);
    mini_acid_.setLoopMode(saved_.loopMode);
    mini_acid_.setBpm(saved_.bpm);
    for (int track = 0; track < 10; ++track) {
      setTrackMuted(track, saved_.muted[track]);
    }
    mini_acid_.setSongMode(saved_.songMode);
    if (saved_.playing) mini_acid_.start();
  };

  if (audio_guard_) audio_guard_(restore);
  else restore();
  saved_ = RuntimeState{};
}

bool F08ListenPage::applySelection() {
  bool applied = false;
  const auto apply = [&]() {
    setAllTracksMuted(false);
    applied = GroovePuterRhythm::applyF08ListenCase(
        mini_acid_, case_index_, variant_);
  };
  if (audio_guard_) audio_guard_(apply);
  else apply();

  last_apply_ok_ = applied;
  const auto info = GroovePuterRhythm::f08ListenCaseInfo(case_index_);
#ifdef ARDUINO
  Serial.printf(
      "[F08-LISTEN] case=%u/%u variant=%s mode=%s ordinal=%u voice=%c "
      "focus=%s progression=%s clock=%s bpm=%u output=%s applied=%d\n",
      static_cast<unsigned>(case_index_ + 1),
      static_cast<unsigned>(GroovePuterRhythm::f08ListenCaseCount()),
      variantName(variant_),
      info.mode,
      static_cast<unsigned>(info.ordinal),
      info.voice,
      info.focus,
      info.progression,
      variant_ == GroovePuterRhythm::F08ListenVariant::Old
          ? info.oldClock
          : info.newClock,
      static_cast<unsigned>(info.bpm),
      info.fingerprintChanged ? "CHANGED" : "SAME",
      applied ? 1 : 0);
#endif
  if (!applied) UI::showToast("F08 FIXTURE FAILED", 1600);
  return applied;
}

void F08ListenPage::selectCase(int delta) {
  const int count = GroovePuterRhythm::f08ListenCaseCount();
  if (count <= 0) return;
  int next = static_cast<int>(case_index_) + delta;
  while (next < 0) next += count;
  while (next >= count) next -= count;
  case_index_ = static_cast<uint8_t>(next);
  variant_ = GroovePuterRhythm::F08ListenVariant::Old;
  applySelection();
}

void F08ListenPage::setVariant(
    GroovePuterRhythm::F08ListenVariant variant) {
  variant_ = variant;
  applySelection();
}

void F08ListenPage::replay() {
  applySelection();
}

void F08ListenPage::onEnter(int context) {
  (void)context;
  captureRuntime();
  case_index_ = 0;
  variant_ = GroovePuterRhythm::F08ListenVariant::Old;
  applySelection();
}

void F08ListenPage::onExit() {
  restoreRuntime();
}

void F08ListenPage::draw(IGfx& gfx) {
  const auto info = GroovePuterRhythm::f08ListenCaseInfo(case_index_);
  const uint8_t count = GroovePuterRhythm::f08ListenCaseCount();

  UI::drawStandardHeader(gfx, mini_acid_, "F08 LISTEN");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  char line[96];

  gfx.setTextColor(COLOR_DANGER);
  std::snprintf(line, sizeof(line), "TEST ONLY                         %02u/%02u",
                static_cast<unsigned>(case_index_ + 1),
                static_cast<unsigned>(count));
  gfx.drawText(x, LayoutManager::lineY(0), line);

  gfx.setTextColor(COLOR_TEXT);
  std::snprintf(line, sizeof(line), "%s %u %c  %s",
                info.mode,
                static_cast<unsigned>(info.ordinal),
                info.voice,
                info.focus);
  gfx.drawText(x, LayoutManager::lineY(1), line);

  gfx.setTextColor(COLOR_ACCENT);
  gfx.drawText(x, LayoutManager::lineY(2), info.progression);

  gfx.setTextColor(COLOR_TEXT);
  std::snprintf(line, sizeof(line), "OLD %s    NEW %s",
                info.oldClock, info.newClock);
  gfx.drawText(x, LayoutManager::lineY(3), line);

  gfx.setTextColor(last_apply_ok_ ? COLOR_INFO : COLOR_DANGER);
  std::snprintf(line, sizeof(line), "> %s   BPM %u   %s",
                variantName(variant_),
                static_cast<unsigned>(info.bpm),
                info.fingerprintChanged ? "CHANGED" : "SAME");
  gfx.drawText(x, LayoutManager::lineY(4), line);

  gfx.setTextColor(COLOR_WARN);
  gfx.drawText(x, LayoutManager::lineY(5), info.group);

  gfx.setTextColor(COLOR_MUTED);
  gfx.drawText(x, LayoutManager::lineY(6), "A progression   B movement natural");
  gfx.drawText(x, LayoutManager::lineY(7), "C no-step8      D roles coherent");

  UI::drawStandardFooter(
      gfx,
      "L/R:CASE A:OLD B:NEW",
      "G:REPLAY C+A+F:EXIT");
}

bool F08ListenPage::handleEvent(UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  const int nav = UIInput::navCode(event);
  if (!event.alt && !event.ctrl && !event.meta) {
    if (nav == GROOVEPUTER_LEFT) {
      selectCase(-1);
      return true;
    }
    if (nav == GROOVEPUTER_RIGHT) {
      selectCase(1);
      return true;
    }

    if (event.key == 'a' || event.key == 'A') {
      setVariant(GroovePuterRhythm::F08ListenVariant::Old);
      return true;
    }
    if (event.key == 'b' || event.key == 'B') {
      setVariant(GroovePuterRhythm::F08ListenVariant::New);
      return true;
    }
    if (event.key == 'g' || event.key == 'G') {
      replay();
      return true;
    }
  }
  return false;
}
