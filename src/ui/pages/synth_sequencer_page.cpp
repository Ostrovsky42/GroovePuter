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
      PhraseNotesSelection::derive(phrase, cursorTick);
  const IGfxColor voiceColor = synthTabColor(voice_index_);

  gfx.setTextColor(voiceColor);
  gfx.drawText(bounds.x + 4, bounds.y, "MELODY");
  // Left of the tab strip, not right-aligned: the NOTES/KNOBS/MORE strip owns
  // the top-right corner from x=190 and silently overdrew this line.
  char where[20];
  std::snprintf(where, sizeof(where), "BAR %u OF %u",
                static_cast<unsigned>(viewport.focusBar) + 1u,
                static_cast<unsigned>(viewport.totalBars));
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4 + textWidth(gfx, "MELODY") + 10, bounds.y, where);

  const int overviewX = bounds.x + 4;
  const int overviewW = std::max(16, bounds.w - 8);
  constexpr int kOverviewH = 3;
  const int overviewY = bounds.y + 10;
  for (uint8_t bar = 0; bar < viewport.totalBars; ++bar) {
    const int x0 = overviewX + static_cast<int>(
        (static_cast<uint32_t>(bar) * overviewW) / viewport.totalBars);
    const int x1 = overviewX + static_cast<int>(
        (static_cast<uint32_t>(bar + 1u) * overviewW) / viewport.totalBars);
    const int cellW = std::max(1, x1 - x0 - 1);
    gfx.drawRect(x0, overviewY, cellW, kOverviewH, COLOR_LABEL);
    if (bar == viewport.focusBar) {
      gfx.fillRect(x0 + 1, overviewY + 1, std::max(1, cellW - 2),
                   kOverviewH - 2, voiceColor);
    }
  }

  // One horizontal lane. Vertical position carries no meaning at all, which is
  // deliberate: the previous packing made a note's row depend on its
  // neighbours, so the picture rearranged itself on every edit. Here the only
  // spatial claim is the true one -- horizontal is time, width is duration.
  const int laneX = bounds.x + 4;
  const int laneW = std::max(32, bounds.w - 8);
  // The shell's feel chip reports the Pattern grid, which this screen is not
  // editing. Declining it removes a wrong label, not merely a busy one.
  UI::publishShellFeelOverlay(false);

  const int laneTop = bounds.y + 24;
  constexpr int kLaneH = 24;
  const int blockY = laneTop + 4;
  constexpr int kBlockH = 16;
  const uint32_t barStart =
      static_cast<uint32_t>(viewport.focusBar) * PhraseRuntime::kTicksPerBar;
  const uint32_t barEnd = barStart + PhraseRuntime::kTicksPerBar;

  const auto tickToX = [&](uint32_t tick) -> int {
    if (tick <= barStart) return laneX;
    if (tick >= barEnd) return laneX + laneW;
    return laneX + static_cast<int>(((tick - barStart) *
        static_cast<uint32_t>(laneW)) / PhraseRuntime::kTicksPerBar);
  };

  for (int beat = 0; beat < 4; ++beat) {
    const int beatX =
        tickToX(barStart + static_cast<uint32_t>(beat) * kBeatTicks);
    const char label[2] = {static_cast<char>('1' + beat), '\0'};
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(beatX + 2, bounds.y + 15, label);
    gfx.fillRect(beatX, laneTop, 1, kLaneH, COLOR_LABEL);
  }

  const uint16_t stepTicks = RuntimePhraseEdit::gridTicks(phrase_cursor_.grid);
  if (stepTicks > 0) {
    for (uint32_t tick = barStart; tick < barEnd; tick += stepTicks) {
      gfx.fillRect(tickToX(tick), blockY + kBlockH / 2, 1, 1, COLOR_LABEL);
    }
  }

  // The window the detail strip magnifies. One beat wide, so even a 1/32 note
  // is about 29 px there and can carry its name.
  constexpr uint32_t kDetailTicks = kBeatTicks;
  uint32_t detailStart = cursorTick > barStart + kDetailTicks / 2u
      ? cursorTick - kDetailTicks / 2u
      : barStart;
  if (detailStart + kDetailTicks > barEnd) detailStart = barEnd - kDetailTicks;
  const uint32_t detailEnd = detailStart + kDetailTicks;

  const int detailTop = bounds.y + 56;
  constexpr int kDetailH = 15;
  const int detailBlockY = detailTop + 2;
  constexpr int kDetailBlockH = 11;
  const auto detailToX = [&](uint32_t tick) -> int {
    if (tick <= detailStart) return laneX;
    if (tick >= detailEnd) return laneX + laneW;
    return laneX + static_cast<int>(((tick - detailStart) *
        static_cast<uint32_t>(laneW)) / kDetailTicks);
  };

  // Draws one note in either strip. Selection is the loudest thing on screen:
  // everything unselected is dim, the selected block is the only bright fill
  // and the only one carrying a name, so "which sound is E5" cannot be
  // ambiguous. Accent survives as a cap rather than competing for the fill.
  const auto drawNote = [&](uint16_t index,
                            int x0, int audibleX, int endX,
                            int y, int h,
                            bool selected, bool carriedIn, bool withName) {
    const IGfxColor fill = selected ? COLOR_WHITE : COLOR_LABEL;
    if (audibleX > x0) gfx.fillRect(x0, y, audibleX - x0, h, fill);
    drawMutedTail(gfx, audibleX, endX, y, h, fill);
    if ((phrase.events[index].flags & PhraseRuntime::kEventAccent) != 0) {
      gfx.fillRect(x0, y, std::max(1, endX - x0), 2, voiceColor);
    }
    // The attack edge. Without it two adjacent notes merge into one shape and
    // become uncountable. A carried-in note has no attack here, so its left
    // edge is marked dim instead of cut.
    gfx.fillRect(x0, y, 1, h, carriedIn ? voiceColor : IGfxColor::Black());

    if (withName) {
      char name[8];
      formatNoteName(phrase.events[index].note, name, sizeof(name));
      const int nameW = textWidth(gfx, name);
      if (audibleX - x0 >= nameW + 4) {
        gfx.setTextColor(IGfxColor::Black());
        gfx.drawText(x0 + ((audibleX - x0) - nameW) / 2, y + (h - 7) / 2, name);
      }
    }
    if (selected) {
      gfx.drawRect(x0, y - 3, std::max(2, endX - x0), h + 6, COLOR_WHITE);
    }
  };

  bool selectionTruncated = false;
  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseNotesProjection::NoteSpan span{};
    if (!PhraseNotesProjection::project(phrase, i, span)) continue;

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

    if (span.endTick > barStart && span.startTick < barEnd) {
      drawNote(i, tickToX(span.startTick), tickToX(audibleEnd),
               tickToX(span.endTick), blockY, kBlockH, selected,
               span.startTick < barStart, selected);
    }
    if (span.endTick > detailStart && span.startTick < detailEnd) {
      drawNote(i, detailToX(span.startTick), detailToX(audibleEnd),
               detailToX(span.endTick), detailBlockY, kDetailBlockH, selected,
               span.startTick < detailStart, true);
    }
  }

  // The cursor marks time only in the margins above and below the block band.
  // Drawing it across the band is what previously buried the content it was
  // supposed to point at.
  const int cursorX = tickToX(cursorTick);
  gfx.fillRect(cursorX, laneTop, 1, 3, COLOR_WHITE);
  gfx.fillRect(cursorX, blockY + kBlockH + 3, 1,
               (laneTop + kLaneH) - (blockY + kBlockH + 3), COLOR_WHITE);

  // Tying the two strips together is the whole job here. On its own a bracket
  // reads as decoration, and two lanes stacked without an explanation read as
  // two instruments. The link is therefore stated three ways at once: a bracket
  // over the slice, guides fanning out to the box corners, and a caption.
  const int bracketY = laneTop + kLaneH;
  const int bracketFrom = tickToX(detailStart);
  const int bracketTo = tickToX(detailEnd);
  gfx.fillRect(bracketFrom, bracketY, std::max(1, bracketTo - bracketFrom), 1,
               voiceColor);
  gfx.fillRect(bracketFrom, bracketY - 2, 1, 3, voiceColor);
  gfx.fillRect(bracketTo - 1, bracketY - 2, 1, 3, voiceColor);

  const int guideTop = bracketY + 1;
  const int guideSteps = std::max(1, (detailTop - 1) - guideTop);
  for (int step = 0; step <= guideSteps; ++step) {
    const int left = bracketFrom + ((laneX - bracketFrom) * step) / guideSteps;
    const int right = (bracketTo - 1) +
        (((laneX + laneW - 1) - (bracketTo - 1)) * step) / guideSteps;
    gfx.fillRect(left, guideTop + step, 1, 1, voiceColor);
    gfx.fillRect(right, guideTop + step, 1, 1, voiceColor);
  }

  gfx.drawRect(laneX, detailTop, laneW, kDetailH, voiceColor);

  const char* caption = "ZOOMED IN ON THIS PART";
  const int captionW = textWidth(gfx, caption);
  const int captionX = laneX + (laneW - captionW) / 2;
  gfx.fillRect(captionX - 3, guideTop, captionW + 6, 9, IGfxColor::Black());
  gfx.setTextColor(voiceColor);
  gfx.drawText(captionX, guideTop + 1, caption);

  gfx.fillRect(detailToX(cursorTick), detailTop + 1, 1, kDetailH - 2,
               COLOR_WHITE);

  // Where the music is, kept distinct from where the cursor is: a solid bar in
  // the voice colour riding the bottom of the lane, never crossing the block
  // band it would otherwise hide.
  if (mini_acid_.isPlaying()) {
    const uint16_t playTick = mini_acid_.currentPhrasePlayTick(voice_index_);
    if (playTick >= barStart && playTick < barEnd) {
      const int playX = tickToX(playTick);
      gfx.fillRect(playX - 2, laneTop + kLaneH - 3, 5, 3, voiceColor);
      gfx.fillRect(playX, blockY + kBlockH + 1, 1,
                   (laneTop + kLaneH - 3) - (blockY + kBlockH + 1), voiceColor);
    }
    if (playTick >= detailStart && playTick < detailEnd) {
      gfx.fillRect(detailToX(playTick), detailTop + 1, 1, kDetailH - 2,
                   voiceColor);
    }
  }

  // What is selected, said in words, in one place.
  const int statusY = bounds.y + 73;
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


  // Kept dim and off the two primary hint rows: delete and undo must stay
  // discoverable without competing with the four keys a beginner needs first.
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(bounds.x + 4, bounds.y + 82, "BS DELETE  CTRL+Z UNDO");

  // "CUT" sat next to the length and read as a command. It is a consequence, so
  // it is spelled as one -- and it gets its own column, because sharing a line
  // with the length ran the two strings together.
  if (selectionTruncated) {
    const char* stopped = "STOPPED BY NEXT";
    gfx.setTextColor(voiceColor);
    gfx.drawText(bounds.x + bounds.w - 4 - textWidth(gfx, stopped),
                 bounds.y + 82, stopped);
  }

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
    if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
      phrase_cursor_ = PhraseNotesCursor::changeGrid(
          phrase_cursor_, nav == GROOVEPUTER_UP ? 1 : -1, phrase.lengthTicks);
      // The grid is observable when it is changed rather than permanently
      // printed: it is an expert control, and on the main screen it competed
      // for attention with the four keys a first-time user actually needs.
      char toast[24];
      std::snprintf(toast, sizeof(toast), "STEP %s",
                    PhraseNotesCursor::gridLabel(phrase_cursor_.grid));
      UI::showToast(toast, 900);
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
    phrase_cursor_ = PhraseNotesCursor::move(
        phrase_cursor_, nav == GROOVEPUTER_RIGHT ? 1 : -1, phrase.lengthTicks);
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
