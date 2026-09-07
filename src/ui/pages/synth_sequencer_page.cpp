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
#include "../phrase_notes_delete_edit.h"
#include "../phrase_notes_duration_edit.h"
#include "../phrase_notes_insert_edit.h"
#include "../phrase_source_toggle.h"
#include "../phrase_notes_pitch_edit.h"
#include "../phrase_notes_viewport.h"
#include "../screen_geometry.h"
#include "../ui_common.h"
#include "../ui_utils.h"
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

// ALT+R switches a voice between its Pattern and its Phrase. R is arbitrary --
// ALT+P is the global jump to the MIDI player and cannot be taken on one page
// without making the shortcut mean two things -- so the label carries the
// meaning instead of the letter.
bool isSourceToggleKey(const UIEvent& event) {
  if (event.event_type != GROOVEPUTER_KEY_DOWN ||
      !event.alt || event.ctrl || event.meta) {
    return false;
  }
  const char key = event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(event.key)))
      : 0;
  return key == 'r' || event.scancode == GROOVEPUTER_R;
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

bool commitRuntimePhraseEditWithUndo(
    MiniAcid& miniAcid,
    const AudioGuard& audioGuard,
    int voiceIndex,
    const PhraseRuntime::RuntimeSynthEventBuffer& beforeBuffer,
    const PhraseRuntime::RuntimeSynthEventBuffer& afterBuffer) {
  if (voiceIndex < 0 || voiceIndex >= 2 ||
      !RuntimePhraseEdit::validate(beforeBuffer) ||
      !RuntimePhraseEdit::validate(afterBuffer)) {
    return false;
  }

  bool committed = false;
  const auto apply = [&]() {
    auto& live = miniAcid.currentPhraseBuffer(voiceIndex);
    if (!RuntimePhraseEdit::same(live, beforeBuffer)) return;

    GroovePuterUndo::RuntimePhraseUndoPayload receipt{};
    receipt.voiceIndex = static_cast<uint8_t>(voiceIndex);
    receipt.source = static_cast<uint8_t>(
        miniAcid.currentSequencedSource(voiceIndex));
    receipt.before = beforeBuffer;

    committed = GroovePuterUndo::undoOwner().commitRuntimePrepared(
        GroovePuterUndo::UndoKind::RuntimePhrase, receipt, [&]() {
          (void)RuntimePhraseEdit::commit(live, afterBuffer);
        });
  };
  if (audioGuard) audioGuard(apply);
  else apply();
  return committed;
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

namespace {

constexpr uint16_t kBeatTicks = PhraseRuntime::kTicksPerBar / 4u;

// Length spoken in beats, the only unit this screen teaches: the ruler above
// the lane numbers the beats, so "2 BEATS" needs no theory and "1/16" does.
//
// Measured in eighths of a beat, because a duration is not required to land on
// the grid at all -- a gate projected from a Pattern is typically a fraction of
// a step -- and an exact-fractions table would fall through to a placeholder
// for almost every note. Inexact values are marked with "~" rather than
// rounded silently.
void formatPhraseLength(uint16_t ticks, char* buf, size_t bufSize) {
  constexpr uint16_t kEighthOfBeatTicks = kBeatTicks / 8u;
  const char* approx = (ticks % kEighthOfBeatTicks) != 0u ? "~" : "";
  const unsigned eighths =
      (ticks + kEighthOfBeatTicks / 2u) / kEighthOfBeatTicks;
  if (eighths == 0u) {
    std::snprintf(buf, bufSize, "VERY SHORT");
    return;
  }

  static const char* const kFractions[8] = {
      "", "1/8", "1/4", "3/8", "1/2", "5/8", "3/4", "7/8"};
  const unsigned beats = eighths / 8u;
  const char* fraction = kFractions[eighths % 8u];

  if (beats > 0u && fraction[0] != '\0') {
    std::snprintf(buf, bufSize, "%s%u %s BEATS", approx, beats, fraction);
  } else if (beats > 0u) {
    std::snprintf(buf, bufSize, "%s%u BEAT%s", approx, beats,
                  beats == 1u ? "" : "S");
  } else {
    std::snprintf(buf, bufSize, "%s%s BEAT", approx, fraction);
  }
}

// The stored duration that will not be heard, because the next attack releases
// this note first. Striped rather than filled: the block keeps its honest
// width, and the silent part is visibly not the same thing as the sounding one.
void drawMutedTail(IGfx& gfx, int fromX, int toX, int y, int h, IGfxColor color) {
  for (int x = fromX; x < toX; x += 2) {
    gfx.fillRect(x, y, 1, h, color);
  }
}

}  // namespace

void SynthSequencerPage::drawPhraseNotes(IGfx& gfx) {
  const auto& bounds = Layout::CONTENT;
  gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, IGfxColor::Black());

  // The shell's feel chip reports the Pattern grid, which this screen is not
  // editing. Declining it removes a wrong label, not merely a busy one.
  UI::publishShellFeelOverlay(false);

  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);
  if (!PhraseNotesProjection::validate(phrase)) {
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(bounds.x + 4, bounds.y + 20, "MELODY UNREADABLE");
    UI::drawStandardFooter(gfx, "CTRL+Z UNDO", "");
    return;
  }

  phrase_cursor_ = PhraseNotesCursor::clamp(phrase_cursor_, phrase.lengthTicks);
  const uint16_t cursorTick = PhraseNotesCursor::tick(phrase_cursor_);
  const uint8_t focusBar = PhraseNotesCursor::focusBar(phrase_cursor_);
  const PhraseNotesViewport::Window viewport =
      PhraseNotesViewport::resolve(phrase.lengthTicks, focusBar);
  const PhraseNotesSelection::Selection selection =
      PhraseNotesSelection::deriveInCell(
          phrase, cursorTick,
          PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
  const IGfxColor voiceColor = synthTabColor(voice_index_);

  gfx.setTextColor(voiceColor);
  gfx.drawText(bounds.x + 4, bounds.y, "MELODY");
  char where[20];
  std::snprintf(where, sizeof(where), "BAR %u OF %u",
                static_cast<unsigned>(viewport.focusBar) + 1u,
                static_cast<unsigned>(viewport.totalBars));
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4 + textWidth(gfx, "MELODY") + 10, bounds.y, where);

  // One editor. Horizontal is time, vertical is pitch -- the only two claims
  // the material actually makes. The overview strip and the magnified lane are
  // gone: a melody drawn on two axes has a shape, and a shape needs no second
  // view to explain it.
  const int planeX = bounds.x + 4;
  const int planeW = std::max(32, bounds.w - 8);
  const int planeTop = bounds.y + 17;
  constexpr int kRowH = 5;
  constexpr int kVisibleNotes = 11;
  constexpr int kPlaneH = kRowH * kVisibleNotes;

  const uint32_t barStart =
      static_cast<uint32_t>(viewport.focusBar) * PhraseRuntime::kTicksPerBar;
  const uint32_t barEnd = barStart + PhraseRuntime::kTicksPerBar;

  const auto tickToX = [&](uint32_t tick) -> int {
    if (tick <= barStart) return planeX;
    if (tick >= barEnd) return planeX + planeW;
    return planeX + static_cast<int>(((tick - barStart) *
        static_cast<uint32_t>(planeW)) / PhraseRuntime::kTicksPerBar);
  };

  // The pitch window follows the selection and is centred on it, so the sound
  // being edited is never off screen. ALT+UP/DOWN nudges it for looking around,
  // clamped so browsing can never hide what you are working on -- moving the
  // view must not be a way to lose your place.
  const uint8_t anchorNote = selection.active
      ? phrase.events[selection.eventIndex].note
      : 60;
  int centreNote = static_cast<int>(anchorNote) + phrase_pitch_offset_;
  const int halfWindow = kVisibleNotes / 2;
  if (centreNote - halfWindow > static_cast<int>(anchorNote)) {
    centreNote = static_cast<int>(anchorNote) + halfWindow;
  }
  if (centreNote + halfWindow < static_cast<int>(anchorNote)) {
    centreNote = static_cast<int>(anchorNote) - halfWindow;
  }
  const int lowestNote = centreNote - halfWindow;

  const auto noteToY = [&](uint8_t note) -> int {
    const int row = (lowestNote + kVisibleNotes - 1) - static_cast<int>(note);
    return planeTop + row * kRowH;
  };
  const auto noteVisible = [&](uint8_t note) -> bool {
    const int value = static_cast<int>(note);
    return value >= lowestNote && value < lowestNote + kVisibleNotes;
  };

  // Beats, numbered above the plane and ruled through it. Without them a shape
  // has no scale and "two beats long" cannot be read off the picture.
  for (int beat = 0; beat < 4; ++beat) {
    const int beatX =
        tickToX(barStart + static_cast<uint32_t>(beat) * kBeatTicks);
    const char label[2] = {static_cast<char>('1' + beat), '\0'};
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(beatX + 2, bounds.y + 9, label);
    gfx.fillRect(beatX, planeTop, 1, kPlaneH, COLOR_LABEL);
  }

  bool selectionTruncated = false;
  int aboveWindow = 0;
  int belowWindow = 0;

  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseNotesProjection::NoteSpan span{};
    if (!PhraseNotesProjection::project(phrase, i, span)) continue;
    if (span.endTick <= barStart || span.startTick >= barEnd) continue;

    // The voice is monophonic at playback: the next attack anywhere ahead
    // releases this note. The block keeps the stored width -- otherwise
    // ALT+LEFT/RIGHT would edit a value the screen never shows -- and the part
    // that will not sound is drawn striped instead of filled.
    uint32_t audibleEnd = span.endTick;
    for (uint16_t j = 0; j < phrase.count; ++j) {
      const uint16_t otherStart = phrase.events[j].startTick;
      if (otherStart > span.startTick && otherStart < audibleEnd) {
        audibleEnd = otherStart;
      }
    }
    const bool selected = selection.active && selection.eventIndex == i;
    if (selected) selectionTruncated = audibleEnd < span.endTick;

    const uint8_t note = phrase.events[i].note;
    if (!noteVisible(note)) {
      if (static_cast<int>(note) >= lowestNote + kVisibleNotes) ++aboveWindow;
      else ++belowWindow;
      continue;
    }

    const int x0 = tickToX(span.startTick);
    const int audibleX = tickToX(audibleEnd);
    const int endX = tickToX(span.endTick);
    const int y = noteToY(note);
    const int h = kRowH - 1;
    const IGfxColor fill = selected ? COLOR_WHITE : COLOR_LABEL;

    if (audibleX > x0) gfx.fillRect(x0, y, audibleX - x0, h, fill);
    drawMutedTail(gfx, audibleX, endX, y, h, fill);
    if ((phrase.events[i].flags & PhraseRuntime::kEventAccent) != 0) {
      gfx.fillRect(x0, y, std::max(1, endX - x0), 1, voiceColor);
    }
    // The attack edge. Without it two adjacent notes on the same pitch merge
    // into one shape and become uncountable.
    gfx.fillRect(x0, y, 1, h, span.startTick < barStart ? voiceColor
                                                        : IGfxColor::Black());
    if (selected) {
      gfx.drawRect(x0 - 1, y - 2, std::max(3, endX - x0 + 2), h + 4,
                   COLOR_WHITE);
    }
  }

  // Sounds outside the pitch window still exist. Saying so costs two glyphs
  // and prevents reading a partial picture as the whole melody.
  if (aboveWindow > 0) {
    gfx.setTextColor(voiceColor);
    gfx.drawText(planeX + planeW - 8, planeTop - 1, "^");
  }
  if (belowWindow > 0) {
    gfx.setTextColor(voiceColor);
    gfx.drawText(planeX + planeW - 8, planeTop + kPlaneH - 6, "v");
  }

  // Time position, marked only in the margin so it never covers a block.
  const int cursorX = tickToX(cursorTick);
  gfx.fillRect(cursorX, planeTop + kPlaneH + 1, 1, 3, COLOR_WHITE);

  // Where the music is, distinct from where the cursor is.
  if (mini_acid_.isPlaying()) {
    const uint16_t playTick = mini_acid_.currentPhrasePlayTick(voice_index_);
    if (playTick >= barStart && playTick < barEnd) {
      const int playX = tickToX(playTick);
      gfx.fillRect(playX - 2, planeTop + kPlaneH + 5, 5, 3, voiceColor);
    }
  }

  const int statusY = bounds.y + 75;
  char status[48];
  if (selection.active) {
    char name[8];
    formatNoteName(phrase.events[selection.eventIndex].note, name, sizeof(name));
    char length[24];
    formatPhraseLength(
        static_cast<uint16_t>(
            phrase.events[selection.eventIndex].durationSubticks /
            PhraseRuntime::kSubticksPerTick),
        length, sizeof(length));
    std::snprintf(status, sizeof(status), "SELECTED %s   %s", name, length);
    gfx.setTextColor(COLOR_WHITE);
  } else {
    std::snprintf(status, sizeof(status), "NO SOUND HERE");
    gfx.setTextColor(COLOR_LABEL);
  }
  gfx.drawText(bounds.x + 4, statusY, status);

  if (selectionTruncated) {
    const char* stopped = "STOPPED BY NEXT";
    gfx.setTextColor(voiceColor);
    gfx.drawText(bounds.x + bounds.w - 4 - textWidth(gfx, stopped), statusY,
                 stopped);
  }

  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4, bounds.y + 84,
               "ENTER ADD  BS DEL  ^Z UNDO  ALT+R SRC");

  UI::drawStandardFooter(gfx,
                         "SPACE LISTEN/STOP  U/D HIGHER LOWER",
                         "L/R PICK SOUND  ALT+L/R SHORTER LONGER");
}

bool SynthSequencerPage::handlePhraseNotesEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN ||
      ui_event.ctrl || ui_event.meta) {
    return false;
  }

  const int nav = UIInput::navCode(ui_event);
  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);
  const bool isBackspace = ui_event.key == '\b' || ui_event.key == 0x7F;

  // Enter adds a sound where the cursor stands. Until now the editor could
  // only change and remove, never create, so an emptied melody was a dead end.
  if (ui_event.key == '\n' && !ui_event.alt) {
    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);
    PhraseNotesInsertEdit::Prepared prepared{};
    const auto result = PhraseNotesInsertEdit::prepare(
        phrase, PhraseNotesCursor::tick(phrase_cursor_),
        phrase_cursor_.grid, prepared);
    if (result != PhraseNotesInsertEdit::Result::Ready) {
      const char* why = "ADD FAILED";
      if (result == PhraseNotesInsertEdit::Result::Occupied) {
        why = "SOUND ALREADY HERE";
      } else if (result == PhraseNotesInsertEdit::Result::Full) {
        why = "MELODY FULL";
      }
      UI::showToast(why, 900);
      return true;
    }

    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);

    UI::showToast(committed ? "SOUND ADDED" : "EDIT STALE", 900);
    return true;
  }

  if (isBackspace && !ui_event.alt) {
    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);
    PhraseNotesDeleteEdit::Prepared prepared{};
    const auto result = PhraseNotesDeleteEdit::prepare(
        phrase, PhraseNotesCursor::tick(phrase_cursor_), prepared);
    if (result != PhraseNotesDeleteEdit::Result::Ready) {
      UI::showToast(
          result == PhraseNotesDeleteEdit::Result::NoTarget
              ? "NO NOTE"
              : "DELETE FAILED",
          900);
      return true;
    }

    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);

    UI::showToast(committed ? "NOTE DELETED" : "EDIT STALE", 900);
    return true;
  }

  if (ui_event.alt) {
    // Grid resolution is a second-level control now. A beginner never needs it,
    // and plain Up/Down is worth far more spent on pitch.
    // Browsing the pitch range is deliberately a separate gesture from
    // changing a pitch: moving the view must never alter the music.
    if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
      phrase_pitch_offset_ += nav == GROOVEPUTER_UP ? 1 : -1;
      if (phrase_pitch_offset_ > 24) phrase_pitch_offset_ = 24;
      if (phrase_pitch_offset_ < -24) phrase_pitch_offset_ = -24;
      return true;
    }
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

    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);

    UI::showToast(
        committed
            ? (direction > 0 ? "NOTE LONGER" : "NOTE SHORTER")
            : "EDIT STALE",
        900);
    return true;
  }

  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    // Selecting sounds, not grid steps. On a 1/32 grid the old behaviour cost
    // four presses to reach the next note, landing on empty ticks in between.
    // Browsing the pitch window resets here: the window belongs to whatever is
    // selected now.
    phrase_cursor_ = PhraseNotesCursor::moveToOnset(
        phrase_cursor_, phrase, nav == GROOVEPUTER_RIGHT ? 1 : -1);
    phrase_pitch_offset_ = 0;
    return true;
  }

  // The grid still decides how far ALT+LEFT/RIGHT moves a length and where
  // ENTER puts a new sound, so it stays reachable -- just not on the arrows,
  // which now carry pitch and selection.
  if (!ui_event.alt && !ui_event.ctrl && !ui_event.meta &&
      (ui_event.key == 'g' || ui_event.key == 'G')) {
    phrase_cursor_ = PhraseNotesCursor::changeGrid(
        phrase_cursor_, 1, phrase.lengthTicks);
    char toast[24];
    std::snprintf(toast, sizeof(toast), "STEP %s",
                  PhraseNotesCursor::gridLabel(phrase_cursor_.grid));
    UI::showToast(toast, 900);
    return true;
  }
  if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);
    PhraseNotesPitchEdit::Prepared prepared{};
    const int direction = nav == GROOVEPUTER_UP ? 1 : -1;
    const auto result = PhraseNotesPitchEdit::prepare(
        phrase, PhraseNotesCursor::tick(phrase_cursor_), direction, prepared);
    if (result != PhraseNotesPitchEdit::Result::Ready) {
      UI::showToast(
          result == PhraseNotesPitchEdit::Result::NoTarget
              ? "NO NOTE"
              : "PITCH LIMIT",
          900);
      return true;
    }

    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);

    UI::showToast(
        committed
            ? (direction > 0 ? "NOTE HIGHER" : "NOTE LOWER")
            : "EDIT STALE",
        900);
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

  if (phraseNotes && GroovePuterUndoUx::isUndoEvent(ui_event)) {
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

  // The source switch belongs to both views, so it sits above the PHRASE-only
  // handler: from PATTERN there is otherwise no way back except three
  // keypresses on the MORE tab.
  if (synth_tab_ == SynthTab::Notes && isSourceToggleKey(ui_event)) {
    PhraseSourceToggle::toggle(mini_acid_, audio_guard_, voice_index_);
    UI::showToast(mini_acid_.currentSequencedSource(voice_index_) ==
                          MiniAcid::SequencedSource::Phrase
                      ? "SOURCE: MELODY"
                      : "SOURCE: PATTERN",
                  1000);
    return true;
  }

  if (phraseNotes && handlePhraseNotesEvent(ui_event)) return true;

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
