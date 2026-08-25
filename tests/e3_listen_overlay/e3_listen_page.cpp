#include "e3_listen_page.h"

#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "../../dsp/miniacid_engine.h"
#include "../layout_manager.h"
#include "../ui_colors.h"
#include "../ui_common.h"
#include "../ui_input.h"

namespace {

const char* shortRole(const char* role) {
  if (std::strcmp(role, "BassRhythm") == 0) return "Bass";
  if (std::strcmp(role, "Backbeat") == 0) return "Backbeat";
  if (std::strcmp(role, "ClosedHat") == 0) return "CHat";
  if (std::strcmp(role, "OpenHat") == 0) return "OHat";
  if (std::strcmp(role, "Percussion") == 0) return "Perc";
  return role;
}

int isolationTrackForRole(uint8_t roleIndex) {
  // Production standardDrumPatternBinding():
  // Kick->KICK, Backbeat->SNARE, ClosedHat->CLOSED_HAT,
  // OpenHat->OPEN_HAT, Percussion->RIM. Synth A owns BassRhythm.
  switch (roleIndex) {
    case 0: return 2;
    case 1: return 3;
    case 2: return 4;
    case 3: return 5;
    case 4: return 8;
    case 5: return 0;
    case 6: return 1;
    case 7: return 1;
    default: return -1;
  }
}

}  // namespace

E3ListenPage::E3ListenPage(IGfx& gfx,
                           MiniAcid& mini_acid,
                           AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
}

void E3ListenPage::setTrackMuted(int track, bool muted) {
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

void E3ListenPage::setAllTracksMuted(bool muted) {
  for (int track = 0; track < 10; ++track) setTrackMuted(track, muted);
}

void E3ListenPage::captureRuntime() {
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

void E3ListenPage::restoreRuntime() {
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

  GroovePuterRhythm::disableE3ListenReview();
  saved_ = RuntimeState{};
}

void E3ListenPage::applyIsolation() {
  if (!isolated_) {
    setAllTracksMuted(false);
    return;
  }
  const auto info = GroovePuterRhythm::e3ListenCaseInfo(case_index_);
  const int track = isolationTrackForRole(info.roleIndex);
  setAllTracksMuted(true);
  if (track >= 0) setTrackMuted(track, false);
}

bool E3ListenPage::applySelection() {
  bool applied = false;
  const auto apply = [&]() {
    setAllTracksMuted(false);
    applied = GroovePuterRhythm::applyE3ListenCase(
        mini_acid_, case_index_, variant_);
    if (applied) applyIsolation();
  };
  if (audio_guard_) audio_guard_(apply);
  else apply();

  last_apply_ok_ = applied;
  const auto info = GroovePuterRhythm::e3ListenCaseInfo(case_index_);
#ifdef ARDUINO
  Serial.printf(
      "[E3-LISTEN] case=%u/%u id=%s category=%s op=%s family=%s level=%s "
      "role=%s source=%u target=%u distance=%u class=%s role_exact=%d "
      "side=%s mix=%s bpm=124 feel=STRAIGHT root=C attempt=0 applied=%d\n",
      static_cast<unsigned>(case_index_ + 1),
      static_cast<unsigned>(GroovePuterRhythm::e3ListenCaseCount()),
      info.caseId,
      info.category,
      info.operation,
      info.family,
      info.level,
      info.role,
      static_cast<unsigned>(info.sourceStep),
      static_cast<unsigned>(info.targetStep),
      static_cast<unsigned>(info.distance),
      GroovePuterRhythm::e3ListenAudibilityClassName(info.audibilityClass),
      info.mutatedRoleExact ? 1 : 0,
      GroovePuterRhythm::e3ListenVariantName(variant_),
      isolated_ ? "ISOLATED" : "FULL",
      applied ? 1 : 0);
#endif
  if (!applied) UI::showToast("E3 FIXTURE FAILED", 1600);
  return applied;
}

void E3ListenPage::selectCase(int delta) {
  const int count = GroovePuterRhythm::e3ListenCaseCount();
  if (count <= 0) return;
  int next = static_cast<int>(case_index_) + delta;
  while (next < 0) next += count;
  while (next >= count) next -= count;
  case_index_ = static_cast<uint8_t>(next);
  variant_ = GroovePuterRhythm::E3ListenVariant::Canonical;
  applySelection();
}

void E3ListenPage::jumpGroup(int delta) {
  const int count = GroovePuterRhythm::e3ListenCaseCount();
  if (count <= 0 || delta == 0) return;
  const auto current = GroovePuterRhythm::e3ListenCaseInfo(case_index_);
  int next = case_index_;
  for (int scanned = 0; scanned < count; ++scanned) {
    next += delta > 0 ? 1 : -1;
    while (next < 0) next += count;
    while (next >= count) next -= count;
    const auto candidate =
        GroovePuterRhythm::e3ListenCaseInfo(static_cast<uint8_t>(next));
    if (std::strcmp(candidate.category, current.category) != 0) {
      case_index_ = static_cast<uint8_t>(next);
      variant_ = GroovePuterRhythm::E3ListenVariant::Canonical;
      applySelection();
      return;
    }
  }
}

void E3ListenPage::setVariant(
    GroovePuterRhythm::E3ListenVariant variant) {
  variant_ = variant;
  applySelection();
}

void E3ListenPage::replay() {
  applySelection();
}

void E3ListenPage::togglePlaying() {
  const auto toggle = [&]() {
    if (mini_acid_.isPlaying()) mini_acid_.stop();
    else mini_acid_.start();
  };
  if (audio_guard_) audio_guard_(toggle);
  else toggle();
}

void E3ListenPage::onEnter(int context) {
  (void)context;
  captureRuntime();
  case_index_ = 0;
  variant_ = GroovePuterRhythm::E3ListenVariant::Canonical;
  isolated_ = false;
  applySelection();
}

void E3ListenPage::onExit() {
  restoreRuntime();
}

void E3ListenPage::draw(IGfx& gfx) {
  const auto info = GroovePuterRhythm::e3ListenCaseInfo(case_index_);
  const uint8_t count = GroovePuterRhythm::e3ListenCaseCount();

  UI::drawStandardHeader(gfx, mini_acid_, "E3 LISTEN");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  char line[96];

  gfx.setTextColor(COLOR_DANGER);
  std::snprintf(line, sizeof(line), "TEST ONLY                         %02u/%02u",
                static_cast<unsigned>(case_index_ + 1),
                static_cast<unsigned>(count));
  gfx.drawText(x, LayoutManager::lineY(0), line);

  gfx.setTextColor(COLOR_TEXT);
  std::snprintf(line, sizeof(line), "%s  %s  %s",
                info.category, info.family, info.level);
  gfx.drawText(x, LayoutManager::lineY(1), line);

  gfx.setTextColor(COLOR_ACCENT);
  if (info.sourceStep < 16 && info.targetStep < 16) {
    std::snprintf(line, sizeof(line), "%s %s  %u -> %u  d%u",
                  info.operation, shortRole(info.role),
                  static_cast<unsigned>(info.sourceStep),
                  static_cast<unsigned>(info.targetStep),
                  static_cast<unsigned>(info.distance));
  } else if (info.sourceStep < 16) {
    std::snprintf(line, sizeof(line), "%s %s  %u -> OFF",
                  info.operation, shortRole(info.role),
                  static_cast<unsigned>(info.sourceStep));
  } else if (info.targetStep < 16) {
    std::snprintf(line, sizeof(line), "%s %s  OFF -> %u",
                  info.operation, shortRole(info.role),
                  static_cast<unsigned>(info.targetStep));
  } else {
    std::snprintf(line, sizeof(line), "%s %s  OFF",
                  info.operation, shortRole(info.role));
  }
  gfx.drawText(x, LayoutManager::lineY(2), line);

  gfx.setTextColor(COLOR_TEXT);
  std::snprintf(line, sizeof(line), "src %s/%s  density %u>%u",
                info.sourceClass, info.sourceKind,
                static_cast<unsigned>(info.densityBefore),
                static_cast<unsigned>(info.densityAfter));
  gfx.drawText(x, LayoutManager::lineY(3), line);

  gfx.setTextColor(COLOR_WARN);
  std::snprintf(line, sizeof(line), "%s  role:%s",
                GroovePuterRhythm::e3ListenAudibilityClassName(
                    info.audibilityClass),
                info.mutatedRoleExact ? "EXACT" : "CONTEXT");
  gfx.drawText(x, LayoutManager::lineY(4), line);

  gfx.setTextColor(last_apply_ok_ ? COLOR_INFO : COLOR_DANGER);
  std::snprintf(line, sizeof(line), "[1]C [2]V [3]W   PLAY:%s %s",
                GroovePuterRhythm::e3ListenVariantName(variant_),
                mini_acid_.isPlaying() ? ">" : "||");
  gfx.drawText(x, LayoutManager::lineY(5), line);

  gfx.setTextColor(COLOR_MUTED);
  std::snprintf(line, sizeof(line), "%s  bar0 x4  I:isolate",
                isolated_ ? "ISOLATED" : "FULL MIX");
  gfx.drawText(x, LayoutManager::lineY(6), line);
  gfx.drawText(x, LayoutManager::lineY(7),
               "L/R case  U/D group  G replay");

  UI::drawStandardFooter(
      gfx,
      "1:C 2:V 3:W SPACE",
      "I:ISO CTRL+V:EXIT");
}

bool E3ListenPage::handleEvent(UIEvent& event) {
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
    if (nav == GROOVEPUTER_UP) {
      jumpGroup(-1);
      return true;
    }
    if (nav == GROOVEPUTER_DOWN) {
      jumpGroup(1);
      return true;
    }

    if (event.key == '1') {
      setVariant(GroovePuterRhythm::E3ListenVariant::Canonical);
      return true;
    }
    if (event.key == '2') {
      setVariant(GroovePuterRhythm::E3ListenVariant::Before);
      return true;
    }
    if (event.key == '3') {
      setVariant(GroovePuterRhythm::E3ListenVariant::After);
      return true;
    }
    if (event.key == 'g' || event.key == 'G') {
      replay();
      return true;
    }
    if (event.key == 'i' || event.key == 'I') {
      isolated_ = !isolated_;
      applyIsolation();
      return true;
    }
    if (event.key == ' ') {
      togglePlaying();
      return true;
    }
  }
  return false;
}
