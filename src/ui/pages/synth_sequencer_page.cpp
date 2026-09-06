#include "synth_sequencer_page.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "../../../platform_sdl/arduino_compat.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>

#include "pattern_edit_page.h"
#include "tb303_params_page.h"
#include "../help_dialog_frames.h"
#include "../key_normalize.h"
#include "../phrase_notes_projection.h"
#include "../phrase_notes_selection.h"
#include "../phrase_notes_duration_edit.h"
#include "../phrase_notes_lane_layout.h"
#include "../phrase_notes_viewport.h"
#include "../screen_geometry.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../ui_theme.h"
#include "../undo_ux.h"
#include "src/output/output_mode_runtime.h"
#include "src/state/scene_revision.h"
#include "src/state/synth_pattern_edit.h"
#include "src/state/undo_owner.h"
#include "src/state/undo_receipts.h"

namespace {
constexpr int kNotesTabStripX = 190;
constexpr int kParamsTabStripX = 172;
constexpr int kTabStripW = 32;
constexpr int kTabStripH = 11;
constexpr int kPatternNumbersX = 106;
constexpr int kPatternNumbersEndX = kPatternNumbersX + 8 * 10 - 1;
static_assert(kNotesTabStripX > kPatternNumbersEndX,
              "NOTES tab must stay after the pattern numbers");
static_assert(kNotesTabStripX + kTabStripW <= Layout::SCREEN_W,
              "NOTES tab must stay on screen");

inline IGfxColor synthTabColor(int voiceIndex) {
  return voiceIndex == 0 ? IGfxColor(0x33C8FF) : IGfxColor(0xFF4FCB);
}

bool isOutputCycleKey(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN ||
      !event.alt || event.ctrl || event.meta) {
    return false;
  }
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  return key == 'o' || event.scancode == GROOVEPUTER_O;
}

bool isSynthGenerateKey(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  return key == 'g' || event.scancode == GROOVEPUTER_G;
}
}  // namespace

SynthSequencerPage::SynthSequencerPage(IGfx& gfx,
                                       MiniAcid& mini_acid,
                                       AudioGuard audio_guard,
                                       int voice_index)
    : mini_acid_(mini_acid),
      audio_guard_(audio_guard),
      voice_index_(voice_index) {
  fallback_title_ = (voice_index_ == 0) ? "SYNTH A" : "SYNTH B";
  phrase_title_ = (voice_index_ == 0)
      ? "SYNTH A NOTES PHRASE"
      : "SYNTH B NOTES PHRASE";

  pattern_page_ = std::make_shared<PatternEditPage>(gfx, mini_acid, audio_guard, voice_index_);
  params_page_ = std::make_shared<TB303ParamsPage>(gfx, mini_acid, audio_guard, voice_index_);
  addPage(pattern_page_);
  addPage(params_page_);
  setSynthTab(SynthTab::Notes);
}

void SynthSequencerPage::setSynthTab(SynthTab tab) {
  synth_tab_ = tab;
  switch (synth_tab_) {
    case SynthTab::Notes:
      setActivePageIndex(0);
      if (params_page_) params_page_->showMoreTab(false);
      break;
    case SynthTab::Knobs:
      setActivePageIndex(1);
      if (params_page_) params_page_->showMoreTab(false);
      break;
    case SynthTab::More:
      setActivePageIndex(1);
      if (params_page_) params_page_->showMoreTab(true);
      break;
  }
}

const char* SynthSequencerPage::activeTabName() const {
  switch (synth_tab_) {
    case SynthTab::Notes: return "[N]KM";
    case SynthTab::Knobs: return "N[K]M";
    case SynthTab::More: return "NK[M]";
  }
  return "[N]KM";
}

void SynthSequencerPage::drawTabIndicator(IGfx& gfx) const {
  const char* label = "[N]KM";
  switch (synth_tab_) {
    case SynthTab::Notes: label = "[N]KM"; break;
    case SynthTab::Knobs: label = "N[K]M"; break;
    case SynthTab::More: label = "NK[M]"; break;
  }

  const bool notesTab = synth_tab_ == SynthTab::Notes;
  const int x = notesTab ? kNotesTabStripX : kParamsTabStripX;
  const int y = Layout::CONTENT.y;
  gfx.fillRect(x, y, kTabStripW, kTabStripH, IGfxColor::Black());
  gfx.setTextColor(synthTabColor(voice_index_));
  gfx.drawText(x + (kTabStripW - gfx.textWidth(label)) / 2,
               y + 1,
               label);
}

void SynthSequencerPage::drawPhraseNotes(IGfx& gfx) {
  const auto& bounds = Layout::CONTENT;
  gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, IGfxColor::Black());

  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);
  gfx.setTextColor(synthTabColor(voice_index_));
  gfx.drawText(bounds.x + 4, bounds.y + 4, "SOURCE: PHRASE");

  if (!PhraseNotesProjection::validate(phrase)) {
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(bounds.x + 4, bounds.y + 20, "PHRASE INVALID");
    return;
  }

  phrase_cursor_ = PhraseNotesCursor::clamp(phrase_cursor_, phrase.lengthTicks);
  const uint16_t cursorTick = PhraseNotesCursor::tick(phrase_cursor_);
  const uint8_t focusBar = PhraseNotesCursor::focusBar(phrase_cursor_);
  const PhraseNotesViewport::Window viewport =
      PhraseNotesViewport::resolve(phrase.lengthTicks, focusBar);
  const PhraseNotesSelection::Selection selection =
      PhraseNotesSelection::derive(phrase, cursorTick);

  char status[40];
  const unsigned bars = phrase.lengthTicks / PhraseRuntime::kTicksPerBar;
  std::snprintf(status, sizeof(status), "L:%u G:%s E:%u",
                bars, PhraseNotesCursor::gridLabel(phrase_cursor_.grid),
                static_cast<unsigned>(phrase.count));
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(bounds.x + 4, bounds.y + 18, status);

  const int overviewX = bounds.x + 4;
  const int overviewY = bounds.y + 30;
  const int overviewW = std::max(16, bounds.w - 8);
  const int overviewH = 6;
  const IGfxColor noteColor = synthTabColor(voice_index_);
  for (uint8_t bar = 0; bar < viewport.totalBars; ++bar) {
    const int x0 = overviewX + static_cast<int>(
        (static_cast<uint32_t>(bar) * overviewW) / viewport.totalBars);
    const int x1 = overviewX + static_cast<int>(
        (static_cast<uint32_t>(bar + 1u) * overviewW) / viewport.totalBars);
    const int cellW = std::max(1, x1 - x0 - 1);
    gfx.drawRect(x0, overviewY, cellW, overviewH, COLOR_LABEL);
    if (bar == viewport.focusBar) {
      gfx.fillRect(x0 + 1, overviewY + 1, std::max(1, cellW - 2),
                   overviewH - 2, noteColor);
    }
  }

  const int timelineX = bounds.x + 4;
  const int timelineY = bounds.y + 42;
  const int timelineW = std::max(16, bounds.w - 8);
  const int rowH = 8;
  const int maxRows = std::max(1, (bounds.h - 46) / rowH);
  const uint16_t windowStartTick = static_cast<uint16_t>(
      static_cast<uint16_t>(viewport.startBar) * PhraseRuntime::kTicksPerBar);
  const uint16_t windowTicks = static_cast<uint16_t>(
      static_cast<uint16_t>(viewport.barCount) * PhraseRuntime::kTicksPerBar);
  const uint32_t windowStartSubtick =
      static_cast<uint32_t>(windowStartTick) * PhraseRuntime::kSubticksPerTick;
  const uint32_t windowSubticks =
      static_cast<uint32_t>(windowTicks) * PhraseRuntime::kSubticksPerTick;
  const uint32_t windowEndSubtick = windowStartSubtick + windowSubticks;
  const PhraseNotesLaneLayout::Layout laneLayout = PhraseNotesLaneLayout::build(
      phrase, windowStartSubtick, windowSubticks,
      static_cast<uint8_t>(maxRows));

  gfx.drawRect(timelineX, timelineY, timelineW, maxRows * rowH, COLOR_LABEL);
  const uint32_t cursorSubtick =
      static_cast<uint32_t>(cursorTick) * PhraseRuntime::kSubticksPerTick;
  if (cursorSubtick >= windowStartSubtick && cursorSubtick < windowEndSubtick) {
    const int cursorX = timelineX + static_cast<int>(
        ((cursorSubtick - windowStartSubtick) * static_cast<uint32_t>(timelineW)) /
        windowSubticks);
    gfx.fillRect(cursorX, timelineY, 1, maxRows * rowH, COLOR_WHITE);
  }
  if (viewport.barCount == 2u) {
    gfx.fillRect(timelineX + timelineW / 2, timelineY,
                 1, maxRows * rowH, COLOR_LABEL);
  }

  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseNotesProjection::NoteSpan span{};
    if (!PhraseNotesProjection::project(phrase, i, span)) continue;
    if (span.endSubtick <= windowStartSubtick ||
        span.startSubtick >= windowEndSubtick) continue;

    const uint32_t clippedStart =
        std::max<uint32_t>(span.startSubtick, windowStartSubtick);
    const uint32_t clippedEnd =
        std::min<uint32_t>(span.endSubtick, windowEndSubtick);
    const int startX = timelineX + static_cast<int>(
        ((clippedStart - windowStartSubtick) *
         static_cast<uint32_t>(timelineW)) / windowSubticks);
    int endX = timelineX + static_cast<int>(
        ((clippedEnd - windowStartSubtick) *
         static_cast<uint32_t>(timelineW)) / windowSubticks);
    if (endX <= startX) endX = startX + 1;

    const uint8_t lane = laneLayout.laneByEvent[i];
    if (lane == PhraseNotesLaneLayout::kOverflowLane) continue;
    const int row = static_cast<int>(lane);
    const int y = timelineY + row * rowH + 2;
    const bool selected = selection.active && selection.eventIndex == i;
    const bool onsetVisible = span.startSubtick >= windowStartSubtick;
    if (onsetVisible) {
      gfx.fillRect(startX, y, 2, 5, noteColor);
    }
    const int continuationX = onsetVisible ? startX + 2 : startX;
    if (endX > continuationX) {
      gfx.fillRect(continuationX, y + 1, endX - continuationX, 3, noteColor);
    }
    if (selected) {
      gfx.drawRect(startX, y - 1, std::max(2, endX - startX), 7, COLOR_WHITE);
    }
  }

  UI::drawStandardFooter(gfx, "L/R:CUR U/D:GRID", "A+L/R:LEN");
}

bool SynthSequencerPage::handlePhraseNotesEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN ||
      ui_event.ctrl || ui_event.meta) {
    return false;
  }

  const int nav = UIInput::navCode(ui_event);
  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);

  if (ui_event.alt) {
    if (nav != GROOVEPUTER_LEFT && nav != GROOVEPUTER_RIGHT) {
      return false;
    }

    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);
    PhraseNotesDurationEdit::Prepared prepared{};
    const int direction = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    const auto result = PhraseNotesDurationEdit::prepare(
        phrase,
        PhraseNotesCursor::tick(phrase_cursor_),
        phrase_cursor_.grid,
        direction,
        prepared);
    if (result != PhraseNotesDurationEdit::Result::Ready) {
      UI::showToast(
          result == PhraseNotesDurationEdit::Result::NoTarget
              ? "NO NOTE"
              : "LEN LIMIT",
          900);
      return true;
    }

    bool committed = false;
    const auto apply = [&]() {
      auto& live = mini_acid_.currentPhraseBuffer(voice_index_);
      if (!RuntimePhraseEdit::same(live, prepared.before) ||
          !RuntimePhraseEdit::validate(prepared.after)) {
        return;
      }

      GroovePuterUndo::RuntimePhraseUndoPayload before{};
      before.voiceIndex = static_cast<uint8_t>(voice_index_);
      before.source = static_cast<uint8_t>(
          mini_acid_.currentSequencedSource(voice_index_));
      before.before = prepared.before;

      committed = GroovePuterUndo::undoOwner().commitRuntimePrepared(
          GroovePuterUndo::UndoKind::RuntimePhrase, before, [&]() {
            (void)RuntimePhraseEdit::commit(live, prepared.after);
          });
    };
    if (audio_guard_) audio_guard_(apply);
    else apply();

    UI::showToast(
        committed
            ? (direction > 0 ? "NOTE LONGER" : "NOTE SHORTER")
            : "EDIT STALE",
        900);
    return true;
  }

  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    phrase_cursor_ = PhraseNotesCursor::move(
        phrase_cursor_, nav == GROOVEPUTER_RIGHT ? 1 : -1, phrase.lengthTicks);
    return true;
  }
  if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
    phrase_cursor_ = PhraseNotesCursor::changeGrid(
        phrase_cursor_, nav == GROOVEPUTER_UP ? 1 : -1, phrase.lengthTicks);
    return true;
  }
  return false;
}
void SynthSequencerPage::draw(IGfx& gfx) {
  if (synth_tab_ == SynthTab::Notes &&
      mini_acid_.currentSequencedSource(voice_index_) ==
          MiniAcid::SequencedSource::Phrase) {
    drawPhraseNotes(gfx);
    drawTabIndicator(gfx);
    return;
  }
  MultiPage::draw(gfx);
  drawTabIndicator(gfx);
}

bool SynthSequencerPage::handleEvent(UIEvent& ui_event) {
  const bool phraseNotes =
      synth_tab_ == SynthTab::Notes &&
      mini_acid_.currentSequencedSource(voice_index_) ==
          MiniAcid::SequencedSource::Phrase;
  if (phraseNotes && handlePhraseNotesEvent(ui_event)) return true;

  if (GroovePuterUndoUx::isUndoEvent(ui_event) &&
      synth_tab_ == SynthTab::Notes) {
    auto& owner = GroovePuterUndo::undoOwner();
    if (owner.hasUndo() &&
        owner.kind() == GroovePuterUndo::UndoKind::RuntimePhrase) {
      const bool redo = owner.nextIsRedo();
      const auto result =
          owner.toggleRuntimePrepared<GroovePuterUndo::RuntimePhraseUndoPayload>(
              GroovePuterUndo::UndoKind::RuntimePhrase,
              [&](const GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
                return GroovePuterUndo::validRuntimePhraseUndoPayload(retained) &&
                       retained.voiceIndex == static_cast<uint8_t>(voice_index_);
              },
              [&](GroovePuterUndo::RuntimePhraseUndoPayload& retained) {
                const auto exchange = [&]() {
                  auto& live = mini_acid_.currentPhraseBuffer(voice_index_);
                  GroovePuterUndo::exchangeFixedValue(live, retained.before);
                  const auto currentSource =
                      mini_acid_.currentSequencedSource(voice_index_);
                  mini_acid_.setSequencedSource(
                      voice_index_,
                      static_cast<MiniAcid::SequencedSource>(retained.source));
                  retained.source = static_cast<uint8_t>(currentSource);
                };
                if (audio_guard_) audio_guard_(exchange);
                else exchange();
              });

      if (result == GroovePuterUndo::UndoResult::Restored) {
        UI::showToast(redo ? "REDO: PHRASE" : "UNDO: PHRASE", 900);
      } else if (result == GroovePuterUndo::UndoResult::Expired) {
        UI::showToast(redo ? "REDO: EXPIRED" : "UNDO: EXPIRED", 900);
      } else {
        UI::showToast(GroovePuterUndoUx::fallbackToast(owner.hasUndo()), 900);
      }
      return true;
    }
  }

  if (!phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event) &&
      synth_tab_ == SynthTab::Notes) {
    auto& owner = GroovePuterUndo::undoOwner();
    if (owner.hasUndo() && owner.kind() == GroovePuterUndo::UndoKind::Pattern) {
      const bool redo = owner.nextIsRedo();
      const bool handled = MultiPage::handleEvent(ui_event);
      if (handled && owner.hasUndo() &&
          owner.kind() == GroovePuterUndo::UndoKind::Pattern &&
          owner.nextIsRedo() != redo) {
        UI::showToast(redo ? "REDO: PATTERN" : "UNDO: PATTERN", 900);
      }
      return handled;
    }
  }

  if (!phraseNotes && synth_tab_ == SynthTab::Notes &&
      isSynthGenerateKey(ui_event) && !mini_acid_.isPlaying()) {
    SceneManager& manager = mini_acid_.sceneManager();
    GroovePuterUndo::SynthPatternUndoPayload before{};
    if (!GroovePuterUndo::captureCurrentSynthPatternUndo(
            manager, voice_index_, before)) {
      return true;
    }

    SynthPattern generated = before.before;
    const GenerativeParams& genreParams =
        mini_acid_.genreManager().getCompiledGenerativeParams();
    auto behavior = mini_acid_.genreManager().getBehavior();
    if (mini_acid_.genreManager().generativeMode() == GenerativeMode::Reggae) {
      if (voice_index_ == 0) {
        behavior.stepMask = 0x1111;
        behavior.motifLength = 2;
        behavior.avoidClusters = true;
        behavior.forceOctaveJump = false;
      } else {
        behavior.stepMask = 0xAAAA;
        behavior.motifLength = 4;
        behavior.avoidClusters = false;
        behavior.forceOctaveJump = false;
      }
    }
    mini_acid_.modeManager().generatePattern(
        generated, mini_acid_.bpm(), genreParams, behavior, voice_index_);

    if (GroovePuterUndo::PatternEdit::samePattern(before.before, generated) ||
        !GroovePuterUndo::synthPatternUndoTargetAvailable(manager, before)) {
      return true;
    }

    GroovePuterUndo::SynthPatternUndoPayload prepared = before;
    prepared.before = generated;
    (void)GroovePuterUndo::undoOwner().commitPrepared(
        GroovePuterUndo::UndoKind::Pattern, before, [&]() {
          const auto apply = [&]() {
            GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);
            (void)mini_acid_.refreshPatternRuntimeEvents(
                prepared.synthIndex, prepared.bankIndex, prepared.patternIndex);
          };
          if (audio_guard_) audio_guard_(apply);
          else apply();
        });
    return true;
  }

  if (isOutputCycleKey(ui_event)) {
    const GroovePuterOutput::Track track = voice_index_ == 0
        ? GroovePuterOutput::Track::SynthA
        : GroovePuterOutput::Track::SynthB;
    const GroovePuterOutput::Mode next =
        GroovePuterOutput::hasExplicitMode(track)
            ? GroovePuterOutput::cycleMode(GroovePuterOutput::mode(track))
            : GroovePuterOutput::Mode::Layer;

    bool changed = false;
    auto apply = [&]() {
      changed = GroovePuterOutput::applyModeWithLocalCleanup(
          mini_acid_, track, next);
    };
    if (audio_guard_) audio_guard_(apply);
    else apply();

    if (changed) GroovePuterState::markSceneMutated();
    char toast[48];
    std::snprintf(toast, sizeof(toast), "SYNTH %c OUT:%s",
                  voice_index_ == 0 ? 'A' : 'B',
                  GroovePuterOutput::modeName(next));
    UI::showToast(toast, 1200);
    return true;
  }

  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN && UIInput::isTab(ui_event)) {
    if (ui_event.ctrl || ui_event.alt || ui_event.meta) return false;

    const uint32_t now = millis();
    if (last_tab_switch_ms_ != 0 && (now - last_tab_switch_ms_) < 250u) {
      return true;
    }
    last_tab_switch_ms_ = now;

    const int next = (static_cast<int>(synth_tab_) + 1) % 3;
    setSynthTab(static_cast<SynthTab>(next));

    char toast[40];
    std::snprintf(toast, sizeof(toast), "SYNTH %c: %s",
                  voice_index_ == 0 ? 'A' : 'B', activeTabName());
    UI::showToast(toast, 700);
    return true;
  }

  if (phraseNotes) return false;
  return MultiPage::handleEvent(ui_event);
}

const std::string& SynthSequencerPage::getTitle() const {
  if (synth_tab_ == SynthTab::Notes &&
      mini_acid_.currentSequencedSource(voice_index_) ==
          MiniAcid::SequencedSource::Phrase) {
    return phrase_title_;
  }
  if (synth_tab_ == SynthTab::Notes && pattern_page_) {
    return pattern_page_->getTitle();
  }
  if (params_page_) return params_page_->getTitle();
  return fallback_title_;
}

void SynthSequencerPage::setContext(int context) {
  setSynthTab(SynthTab::Notes);
  if (pattern_page_) pattern_page_->setContext(context);
}

void SynthSequencerPage::setVisualStyle(VisualStyle style) {
  if (pattern_page_) pattern_page_->setVisualStyle(style);
  if (params_page_) params_page_->setVisualStyle(style);
}

void SynthSequencerPage::tick() {
  if (synth_tab_ == SynthTab::Notes && pattern_page_ &&
      mini_acid_.currentSequencedSource(voice_index_) ==
          MiniAcid::SequencedSource::Pattern) {
    pattern_page_->syncSongPatternContext();
    pattern_page_->tick();
  }
}

std::unique_ptr<MultiPageHelpDialog> SynthSequencerPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int SynthSequencerPage::getHelpFrameCount() const {
  return 2;
}

void SynthSequencerPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) {
    case 0:
      drawHelpPage303PatternEdit(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    case 1:
      drawHelpPage303(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}
