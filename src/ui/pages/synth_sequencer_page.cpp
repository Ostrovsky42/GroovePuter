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
#include "../phrase_notes_join_edit.h"
#include "../phrase_selection_state.h"
#include "../phrase_source_toggle.h"
#include "../phrase_notes_pitch_edit.h"
#include "../phrase_notes_viewport.h"
#include "../phrase_instrument_controls.h"
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
// How much of a note actually sounds: the stored end, or the next attack if
// that comes first. Growing the stored length past that point changes nothing
// anyone can hear, and saying "NOTE LONGER" there would be a false report.
uint32_t audibleEndTick(
    const PhraseRuntime::RuntimeSynthEventBuffer& phrase, uint16_t index) {
  if (index >= phrase.count) return 0;
  const auto& event = phrase.events[index];
  uint32_t end = event.startTick +
      event.durationSubticks / PhraseRuntime::kSubticksPerTick;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    const uint16_t other = phrase.events[i].startTick;
    if (other > event.startTick && other < end) end = other;
  }
  return end;
}

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
  // Two views of one melody. They share the selection and every operation;
  // only presentation and scrolling are their own.
  if (phrase_view_ == PhraseView::List) {
    drawPhraseList(gfx);
    return;
  }
  drawPhraseRoll(gfx);
}

void SynthSequencerPage::drawPhraseRoll(IGfx& gfx) {
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
  phrase_selection_ = PhraseSelectionState::resolve(phrase, phrase_selection_);
  if (!phrase_selection_.active) {
    phrase_selection_ = PhraseSelectionState::first(phrase);
  }
  const PhraseNotesSelection::Selection selection =
      phrase_selection_.active
          ? PhraseNotesSelection::Selection{
                true, phrase_selection_.eventIndex, {}}
          : PhraseNotesSelection::Selection{};
  const IGfxColor voiceColor = synthTabColor(voice_index_);

  gfx.setTextColor(voiceColor);
  gfx.drawText(bounds.x + 4, bounds.y, "PHRASE");
  char where[20];
  std::snprintf(where, sizeof(where), "BAR %u/%u",
                static_cast<unsigned>(viewport.focusBar) + 1u,
                static_cast<unsigned>(viewport.totalBars));
  gfx.setTextColor(COLOR_LABEL);
  const int whereX = bounds.x + 4 + textWidth(gfx, "PHRASE") + 10;
  gfx.drawText(whereX, bounds.y, where);
  gfx.drawText(whereX + textWidth(gfx, where) + 8, bounds.y, "PLAY:PHR");
  char grid[16];
  std::snprintf(grid, sizeof(grid), "GRID %s",
                PhraseNotesCursor::gridLabel(phrase_cursor_.grid));
  gfx.drawText(bounds.x + 4, bounds.y + 9, grid);

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
  const int halfWindow = kVisibleNotes / 2;
  if (phrase_pitch_lowest_ == 0) {
    phrase_pitch_lowest_ = static_cast<int>(anchorNote) - halfWindow;
  }
  // Scroll only when the selection leaves an edge, and only far enough to
  // bring it back. Everything else stays where the eye left it.
  if (static_cast<int>(anchorNote) < phrase_pitch_lowest_) {
    phrase_pitch_lowest_ = static_cast<int>(anchorNote);
  } else if (static_cast<int>(anchorNote) >=
             phrase_pitch_lowest_ + kVisibleNotes) {
    phrase_pitch_lowest_ =
        static_cast<int>(anchorNote) - (kVisibleNotes - 1);
  }
  const int lowestNote = phrase_pitch_lowest_;
  const int centreNote = lowestNote + halfWindow;

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

  // Where a new sound would go. Drawn as a full-height caret so it is findable
  // on an empty melody too, dim through the plane so it never hides a block,
  // and bright in the margin where nothing else lives.
  const int cursorX = tickToX(cursorTick);
  for (int y = planeTop; y < planeTop + kPlaneH; y += 2) {
    gfx.fillRect(cursorX, y, 1, 1, COLOR_LABEL);
  }
  gfx.fillRect(cursorX - 1, planeTop + kPlaneH + 1, 3, 3, COLOR_WHITE);

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
    std::snprintf(status, sizeof(status), "EMPTY HERE   ENTER ADDS A SOUND");
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
               "ENTER ADD  J JOIN  BS DEL  ^Z UNDO");

  UI::drawStandardFooter(gfx,
                         "SPACE LISTEN/STOP  U/D HIGHER LOWER",
                         "L/R PICK SOUND  ALT+L/R SHORTER LONGER");
}

void SynthSequencerPage::drawPhraseList(IGfx& gfx) {
  const auto& bounds = Layout::CONTENT;
  gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, IGfxColor::Black());
  UI::publishShellFeelOverlay(false);

  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);
  if (!PhraseNotesProjection::validate(phrase)) {
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(bounds.x + 4, bounds.y + 20, "MELODY UNREADABLE");
    UI::drawStandardFooter(gfx, "CTRL+Z UNDO", "");
    return;
  }

  phrase_selection_ = PhraseSelectionState::resolve(phrase, phrase_selection_);
  if (!phrase_selection_.active) {
    phrase_selection_ = PhraseSelectionState::first(phrase);
  }
  const IGfxColor voiceColor = synthTabColor(voice_index_);

  gfx.setTextColor(voiceColor);
  gfx.drawText(bounds.x + 4, bounds.y, "SOUNDS");
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4 + textWidth(gfx, "SOUNDS") + 10, bounds.y,
               "V ROLL   ALT+R SRC");

  // Row order: start time, ties by buffer index. Stable, so changing a length
  // or a pitch never makes the rows jump around under the hand.
  uint16_t order[PhraseRuntime::kMaxSynthEvents];
  for (uint16_t i = 0; i < phrase.count; ++i) order[i] = i;
  for (uint16_t i = 1; i < phrase.count; ++i) {
    const uint16_t value = order[i];
    uint16_t j = i;
    while (j > 0 &&
           PhraseSelectionState::precedes(phrase, value, order[j - 1])) {
      order[j] = order[j - 1];
      --j;
    }
    order[j] = value;
  }

  uint16_t selectedRow = 0;
  for (uint16_t row = 0; row < phrase.count; ++row) {
    if (order[row] == phrase_selection_.eventIndex) selectedRow = row;
  }

  constexpr int kRows = 5;
  constexpr int kRowH = 11;
  if (selectedRow < phrase_list_top_) phrase_list_top_ = selectedRow;
  if (selectedRow >= phrase_list_top_ + kRows) {
    phrase_list_top_ = static_cast<uint16_t>(selectedRow - (kRows - 1));
  }
  if (phrase.count <= kRows) phrase_list_top_ = 0;

  // Left column: which sound and what it is. Right: the same shared timeline
  // for every row, so start and length stay comparable between rows -- that is
  // what a plain list of numbers cannot show.
  const int laneX = bounds.x + 62;
  const int laneW = bounds.w - 62 - 4;
  const int listTop = bounds.y + 12;

  for (int beat = 0; beat < 4; ++beat) {
    const int beatX = laneX + (laneW * beat) / 4;
    gfx.fillRect(beatX, listTop - 2, 1, kRows * kRowH + 2, COLOR_LABEL);
  }

  for (int row = 0; row < kRows; ++row) {
    const uint16_t index = static_cast<uint16_t>(phrase_list_top_ + row);
    if (index >= phrase.count) break;
    const uint16_t eventIndex = order[index];
    const auto& event = phrase.events[eventIndex];
    const bool selected = eventIndex == phrase_selection_.eventIndex;
    const int y = listTop + row * kRowH;

    if (selected) {
      gfx.fillRect(bounds.x + 2, y - 1, bounds.w - 4, kRowH - 1,
                   IGfxColor(0x12233A));
      gfx.drawRect(bounds.x + 2, y - 1, bounds.w - 4, kRowH - 1, COLOR_WHITE);
    }

    char label[16];
    char name[8];
    formatNoteName(event.note, name, sizeof(name));
    std::snprintf(label, sizeof(label), "%02u %s",
                  static_cast<unsigned>(index) + 1u, name);
    gfx.setTextColor(selected ? COLOR_WHITE : COLOR_LABEL);
    gfx.drawText(bounds.x + 5, y + 1, label);

    // Position is the start, width is the stored length; the striped part is
    // the length the next attack will silence, exactly as in the roll.
    const uint32_t barTicks = PhraseRuntime::kTicksPerBar;
    const uint32_t start = event.startTick % barTicks;
    uint32_t audibleEnd = event.startTick +
        event.durationSubticks / PhraseRuntime::kSubticksPerTick;
    for (uint16_t i = 0; i < phrase.count; ++i) {
      const uint16_t other = phrase.events[i].startTick;
      if (other > event.startTick && other < audibleEnd) audibleEnd = other;
    }
    const uint32_t storedEnd = event.startTick +
        event.durationSubticks / PhraseRuntime::kSubticksPerTick;

    const auto toX = [&](uint32_t tick) -> int {
      const uint32_t clamped = tick > barTicks ? barTicks : tick;
      return laneX + static_cast<int>(
          (clamped * static_cast<uint32_t>(laneW)) / barTicks);
    };
    const int x0 = toX(start);
    const int xAudible = toX(audibleEnd % barTicks == 0 && audibleEnd >= barTicks
                                 ? barTicks
                                 : audibleEnd - (event.startTick - start));
    const int xEnd = toX(storedEnd - (event.startTick - start));
    const IGfxColor fill = selected ? COLOR_WHITE : COLOR_LABEL;
    if (xAudible > x0) gfx.fillRect(x0, y + 2, xAudible - x0, 5, fill);
    drawMutedTail(gfx, xAudible, xEnd, y + 2, 5, fill);
    if ((event.flags & PhraseRuntime::kEventAccent) != 0) {
      gfx.fillRect(x0, y + 1, std::max(1, xEnd - x0), 1, voiceColor);
    }
  }

  char status[48];
  if (phrase_selection_.active) {
    const auto& event = phrase.events[phrase_selection_.eventIndex];
    char name[8];
    formatNoteName(event.note, name, sizeof(name));
    char length[24];
    formatPhraseLength(
        static_cast<uint16_t>(event.durationSubticks /
                              PhraseRuntime::kSubticksPerTick),
        length, sizeof(length));
    std::snprintf(status, sizeof(status), "SOUND %u OF %u   %s   %s",
                  static_cast<unsigned>(selectedRow) + 1u,
                  static_cast<unsigned>(phrase.count), name, length);
    gfx.setTextColor(COLOR_WHITE);
  } else {
    std::snprintf(status, sizeof(status), "NO SOUNDS YET");
    gfx.setTextColor(COLOR_LABEL);
  }
  gfx.drawText(bounds.x + 4, bounds.y + 72, status);

  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4, bounds.y + 82, "ENTER ADD  J JOIN  BS DEL");

  UI::drawStandardFooter(gfx,
                         "SPACE LISTEN/STOP  L/R HIGHER LOWER",
                         "U/D PICK SOUND  ALT+L/R SHORTER LONGER");
}

bool SynthSequencerPage::handlePhraseNotesEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN ||
      ui_event.ctrl || ui_event.meta) {
    return false;
  }

  const int nav = UIInput::navCode(ui_event);
  const auto& phrase = mini_acid_.currentPhraseBuffer(voice_index_);
  const char lower = ui_event.key
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(ui_event.key)))
      : 0;

  if (!ui_event.alt && lower == 'l') {
    const auto outcome = PhraseInstrumentControls::applyLengthChangeDetailed(
        phrase, +1, [&](uint8_t bars) {
          bool committed = false;
          const auto apply = [&]() {
            committed = mini_acid_.setPhraseLength(voice_index_, bars);
          };
          if (audio_guard_) audio_guard_(apply);
          else apply();
          return committed;
        });
    const auto& after = mini_acid_.currentPhraseBuffer(voice_index_);
    phrase_cursor_ = PhraseNotesCursor::clamp(phrase_cursor_, after.lengthTicks);
    char toast[32];
    if (outcome.result == PhraseInstrumentControls::LengthChangeResult::Changed) {
      std::snprintf(toast, sizeof(toast), "PHRASE LENGTH %uB",
                    static_cast<unsigned>(
                        PhraseInstrumentControls::lengthBars(after.lengthTicks)));
      UI::showToast(toast, 1000);
    } else if (outcome.result ==
               PhraseInstrumentControls::LengthChangeResult::WouldTruncateEvent) {
      std::snprintf(toast, sizeof(toast), "NOTES BEYOND %uB",
                    static_cast<unsigned>(outcome.targetBars));
      UI::showToast(toast, 1400);
    } else if (outcome.result ==
               PhraseInstrumentControls::LengthChangeResult::Unchanged) {
      std::snprintf(toast, sizeof(toast), "LENGTH ALREADY %uB",
                    static_cast<unsigned>(
                        PhraseInstrumentControls::lengthBars(after.lengthTicks)));
      UI::showToast(toast, 1000);
    } else {
      UI::showToast("LENGTH FAILED", 1000);
    }
    return true;
  }

  if (!ui_event.alt && (ui_event.key == '[' || ui_event.key == ']')) {
    phrase_cursor_ = PhraseInstrumentControls::jumpBar(
        phrase_cursor_, ui_event.key == ']' ? +1 : -1, phrase.lengthTicks);
    char toast[32];
    std::snprintf(toast, sizeof(toast), "PHRASE BAR %u/%u",
                  static_cast<unsigned>(PhraseNotesCursor::focusBar(phrase_cursor_) + 1),
                  static_cast<unsigned>(
                      PhraseInstrumentControls::lengthBars(phrase.lengthTicks)));
    UI::showToast(toast, 900);
    return true;
  }
  const bool isBackspace = ui_event.key == '\b' || ui_event.key == 0x7F;

  // Enter adds a sound where the cursor stands. Until now the editor could
  // only change and remove, never create, so an emptied melody was a dead end.
  if (ui_event.key == '\n' && !ui_event.alt) {
    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);

    // Where the new sound goes. Not "wherever the cursor happens to be": the
    // Cardputer has no Shift, LEFT/RIGHT hop between sounds, and an emptied
    // melody has nothing to hop between -- so requiring the user to navigate
    // to a free position made adding unreachable in exactly the case where it
    // matters most. It goes after the selected sound instead, on the first
    // free step, which needs no positioning at all.
    const uint16_t step = RuntimePhraseEdit::gridTicks(phrase_cursor_.grid);
    uint16_t target = PhraseNotesCursor::tick(phrase_cursor_);
    const PhraseNotesSelection::Selection anchor =
        PhraseNotesSelection::deriveInCell(phrase, target, step);
    if (anchor.active && step > 0) {
      target = static_cast<uint16_t>(
          phrase.events[anchor.eventIndex].startTick + step);
      while (target < phrase.lengthTicks) {
        bool occupied = false;
        for (uint16_t i = 0; i < phrase.count; ++i) {
          if (phrase.events[i].startTick / step == target / step) {
            occupied = true;
            break;
          }
        }
        if (!occupied) break;
        target = static_cast<uint16_t>(target + step);
      }
      if (target >= phrase.lengthTicks) {
        target = PhraseNotesCursor::tick(phrase_cursor_);
      }
    }

    PhraseNotesInsertEdit::Prepared prepared{};
    const auto result = PhraseNotesInsertEdit::prepare(
        phrase, target, phrase_cursor_.grid, prepared);
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

    if (committed && step > 0) {
      phrase_cursor_.cell = static_cast<uint8_t>(target / step);
      phrase_cursor_ = PhraseNotesCursor::clamp(
          phrase_cursor_, phrase.lengthTicks);
    }
    UI::showToast(committed ? "SOUND ADDED" : "EDIT STALE", 900);
    return true;
  }

  if (isBackspace && !ui_event.alt) {
    phrase_cursor_ = PhraseNotesCursor::clamp(phrase_cursor_, phrase.lengthTicks);
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
      phrase_pitch_lowest_ += nav == GROOVEPUTER_UP ? 1 : -1;
      if (phrase_pitch_lowest_ < 1) phrase_pitch_lowest_ = 1;
      if (phrase_pitch_lowest_ > 116) phrase_pitch_lowest_ = 116;
      return true;
    }
    if (nav != GROOVEPUTER_LEFT && nav != GROOVEPUTER_RIGHT) {
      return false;
    }

    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);
    PhraseNotesDurationEdit::Prepared prepared{};
    const int direction = nav == GROOVEPUTER_RIGHT ? 1 : -1;
    const PhraseNotesSelection::Selection before =
        PhraseNotesSelection::deriveInCell(
            phrase, PhraseNotesCursor::tick(phrase_cursor_),
            PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
    const uint32_t audibleBefore =
        before.active ? audibleEndTick(phrase, before.eventIndex) : 0;
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

    if (!committed) {
      UI::showToast("EDIT STALE", 900);
      return true;
    }

    // If the growth landed entirely in the part the next attack silences, the
    // note did not get longer -- only its stored value did. Say so, and name
    // the way out.
    const auto& after = mini_acid_.currentPhraseBuffer(voice_index_);
    const PhraseNotesSelection::Selection nowSelected =
        PhraseNotesSelection::deriveInCell(
            after, PhraseNotesCursor::tick(phrase_cursor_),
            PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
    const bool audibleChanged =
        !nowSelected.active ||
        audibleEndTick(after, nowSelected.eventIndex) != audibleBefore;
    if (direction > 0 && !audibleChanged) {
      UI::showToast("NEXT SOUND BLOCKS LENGTH  J JOIN", 1600);
    } else {
      UI::showToast(direction > 0 ? "NOTE LONGER" : "NOTE SHORTER", 900);
    }
    return true;
  }

  if (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT) {
    // One grid cell per press. Jumping straight to the next onset read better
    // in dense material but made a gap in the middle of a melody unreachable,
    // so there was nowhere to stand to add a sound -- and an emptied melody
    // trapped the cursor entirely. Every cell is reachable this way, and no
    // sound is skipped because a cell holding one selects it.
    //
    // The pitch window is deliberately left alone: it moves only when the new
    // selection falls outside it, so moving does not rearrange the picture.
    phrase_cursor_ = PhraseNotesCursor::move(
        phrase_cursor_, nav == GROOVEPUTER_RIGHT ? 1 : -1, phrase.lengthTicks);
    const uint16_t insertTick = PhraseNotesCursor::tick(phrase_cursor_);
    phrase_selection_ = PhraseSelectionState::withInsertTick(
        phrase_selection_, insertTick);
    const PhraseNotesSelection::Selection underCursor =
        PhraseNotesSelection::deriveInCell(
            phrase, insertTick,
            PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
    if (underCursor.active) {
      const uint16_t retainedInsertTick = phrase_selection_.insertTick;
      phrase_selection_ = PhraseSelectionState::at(phrase, underCursor.eventIndex);
      phrase_selection_.insertTick = retainedInsertTick;
    }
    return true;
  }

  // Switch view. Musical data is untouched and the selected sound comes along:
  // that is the whole point of two views over one melody.
  if (!ui_event.alt && !ui_event.ctrl && !ui_event.meta &&
      (ui_event.key == 'v' || ui_event.key == 'V')) {
    phrase_view_ = phrase_view_ == PhraseView::Roll ? PhraseView::List
                                                    : PhraseView::Roll;
    UI::showToast(phrase_view_ == PhraseView::List ? "VIEW: LIST"
                                                   : "VIEW: PIANO ROLL",
                  1000);
    return true;
  }

  // Each view selects along the axis its presentation leaves free: rows in the
  // list, time in the roll. The operation underneath is the same one.
  if (phrase_view_ == PhraseView::List && !ui_event.alt &&
      (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN)) {
    phrase_selection_ = PhraseSelectionState::step(
        phrase, phrase_selection_, nav == GROOVEPUTER_DOWN ? 1 : -1);
    return true;
  }

  // Continue this sound instead of the next one. Explicit, because removing a
  // neighbouring sound is a musical decision -- not something "longer" should
  // do behind the user's back.
  if (!ui_event.alt && !ui_event.ctrl && !ui_event.meta &&
      (ui_event.key == 'j' || ui_event.key == 'J')) {
    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);
    PhraseNotesJoinEdit::Prepared prepared{};
    const auto result = PhraseNotesJoinEdit::prepare(
        phrase, PhraseNotesCursor::tick(phrase_cursor_),
        phrase_cursor_.grid, prepared);
    if (result != PhraseNotesJoinEdit::Result::Ready) {
      const char* why = "CANNOT JOIN";
      if (result == PhraseNotesJoinEdit::Result::NoTarget) why = "NO SOUND HERE";
      else if (result == PhraseNotesJoinEdit::Result::NoNext) why = "NOTHING AFTER IT";
      else if (result == PhraseNotesJoinEdit::Result::Ambiguous) {
        why = "TWO SOUNDS START THERE";
      }
      UI::showToast(why, 1200);
      return true;
    }

    const bool committed = commitRuntimePhraseEditWithUndo(
        mini_acid_, audio_guard_, voice_index_, prepared.before, prepared.after);
    if (!committed) {
      UI::showToast("EDIT STALE", 1000);
      return true;
    }
    const auto& joined = mini_acid_.currentPhraseBuffer(voice_index_);
    const PhraseNotesSelection::Selection stillSelected =
        PhraseNotesSelection::deriveInCell(
            joined, PhraseNotesCursor::tick(phrase_cursor_),
            PhraseNotesCursor::quantumTicks(phrase_cursor_.grid));
    bool stillCut = false;
    if (stillSelected.active) {
      const auto& event = joined.events[stillSelected.eventIndex];
      const uint32_t stored = event.startTick +
          event.durationSubticks / PhraseRuntime::kSubticksPerTick;
      stillCut = audibleEndTick(joined, stillSelected.eventIndex) < stored;
    }
    UI::showToast(stillCut ? "JOINED  ONE MORE SOUND AFTER" : "JOINED WITH NEXT",
                  stillCut ? 1600 : 1000);
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
  // Pitch: UP/DOWN in the roll, LEFT/RIGHT in the list, because each view has
  // already spent the other axis on selection.
  const bool pitchUp = phrase_view_ == PhraseView::List
      ? nav == GROOVEPUTER_RIGHT
      : nav == GROOVEPUTER_UP;
  const bool pitchDown = phrase_view_ == PhraseView::List
      ? nav == GROOVEPUTER_LEFT
      : nav == GROOVEPUTER_DOWN;

  if (pitchUp || pitchDown) {
    phrase_cursor_ = PhraseNotesCursor::clamp(
        phrase_cursor_, phrase.lengthTicks);
    PhraseNotesPitchEdit::Prepared prepared{};
    const int direction = pitchUp ? 1 : -1;
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
                  const auto currentSource = mini_acid_.currentSequencedSource(voice_index_);
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
    const auto result = PhraseSourceToggle::toggle(mini_acid_, audio_guard_, voice_index_);
    if (result == PhraseSourceToggle::Result::MadePhrase) {
      UI::showToast("MAKE PHRASE", 1000);
    } else {
      UI::showToast(mini_acid_.currentSequencedSource(voice_index_) ==
                            MiniAcid::SequencedSource::Phrase
                        ? "SOURCE: MELODY"
                        : "SOURCE: PATTERN",
                    1000);
    }
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
    if (!GroovePuterUndo::captureCurrentSynthPatternUndo(manager, voice_index_, before)) {
      return true;
    }

    SynthPattern generated = before.before;
    const GenerativeParams& genreParams = mini_acid_.genreManager().getCompiledGenerativeParams();
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
      changed = GroovePuterOutput::applyModeWithLocalCleanup(mini_acid_, track, next);
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
      mini_acid_.currentSequencedSource(voice_index_) == MiniAcid::SequencedSource::Phrase) {
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
      mini_acid_.currentSequencedSource(voice_index_) == MiniAcid::SequencedSource::Pattern) {
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
