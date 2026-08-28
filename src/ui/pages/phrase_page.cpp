#include "phrase_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

#include "../../debug_log.h"
#include "../layout_manager.h"
#include "../ui_colors.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "src/dsp/generated_phrase_song.h"
#include "src/state/generated_phrase_product_state.h"
#include "src/state/generation_request_state.h"
#include "src/state/phrase_generation_request_state.h"
#include "src/state/scene_revision.h"

namespace {

struct PhrasePalette {
  IGfxColor background;
  IGfxColor panel;
  IGfxColor panelSelected;
  IGfxColor border;
  IGfxColor dim;
  IGfxColor text;
  IGfxColor gridOff;
  IGfxColor beatOff;
  IGfxColor synthA;
  IGfxColor synthB;
  IGfxColor drums;
  IGfxColor accent;
};

PhrasePalette paletteForStyle(VisualStyle style) {
  switch (style) {
    case VisualStyle::RETRO_CLASSIC:
      return {
          IGfxColor(0x02040A), IGfxColor(0x0A1020), IGfxColor(0x12233A),
          IGfxColor(0x35506C), IGfxColor(0x61798E), IGfxColor(0xE7F6FF),
          IGfxColor(0x172331), IGfxColor(0x294258), IGfxColor(0x00E5FF),
          IGfxColor(0xFF4FD8), IGfxColor(0xFFD166), IGfxColor(0x73E2A7)};
    case VisualStyle::AMBER:
      return {
          IGfxColor(0x090603), IGfxColor(0x171006), IGfxColor(0x2A1B08),
          IGfxColor(0x6E4A18), IGfxColor(0x9E7136), IGfxColor(0xFFE7B0),
          IGfxColor(0x2C1E0C), IGfxColor(0x493114), IGfxColor(0xFFCA58),
          IGfxColor(0xE6A93E), IGfxColor(0xFF8A3D), IGfxColor(0xFFD166)};
    case VisualStyle::MINIMAL_DARK:
    case VisualStyle::MINIMAL:
    default:
      return {
          COLOR_BG, COLOR_PANEL, IGfxColor(0x101A23), COLOR_LIGHT_GRAY,
          COLOR_MUTED, COLOR_TEXT, IGfxColor(0x18222B), IGfxColor(0x2B3945),
          COLOR_SYNTH_A, COLOR_SYNTH_B, COLOR_WARN, COLOR_ACCENT};
  }
}

IGfxColor slotColor(PhraseCore::SlotId slot, const PhrasePalette& palette) {
  switch (slot) {
    case PhraseCore::SlotId::A: return palette.synthA;
    case PhraseCore::SlotId::B: return palette.synthB;
    case PhraseCore::SlotId::C: return palette.drums;
    case PhraseCore::SlotId::D: return palette.accent;
  }
  return palette.text;
}

const char* roleShort(PhraseCore::Role role) {
  switch (role) {
    case PhraseCore::Role::Main: return "MAIN";
    case PhraseCore::Role::Variation: return "VAR";
    case PhraseCore::Role::Break: return "BREAK";
    case PhraseCore::Role::Ending: return "END";
  }
  return "?";
}

const char* sourceShort(PhraseCore::Source source) {
  switch (source) {
    case PhraseCore::Source::None: return "NONE";
    case PhraseCore::Source::InternalPattern: return "PATTERN";
    case PhraseCore::Source::Generated: return "GENERATED";
    case PhraseCore::Source::Derived: return "DERIVED";
    case PhraseCore::Source::SmfRegion: return "SMF";
    case PhraseCore::Source::LiveCapture: return "LIVE";
  }
  return "?";
}

const char* storageBadge(const PhraseCore::SlotSummary& summary) {
  if (!summary.valid) return "EMPTY";
  switch (summary.storage) {
    case PhraseCore::StorageMode::ReferenceView:
      return summary.mutableBacking ? "REF MUT" : "REF";
    case PhraseCore::StorageMode::OwnedEvents:
      return "OWNED";
    case PhraseCore::StorageMode::Empty:
      return "EMPTY";
  }
  return "?";
}

void formatTrackMask(uint8_t mask, char* output, size_t outputSize) {
  if (!output || outputSize == 0) return;
  output[0] = '\0';
  const char* separator = "";
  if ((mask & PhraseCore::kTrackSynthA) != 0) {
    std::snprintf(output, outputSize, "SA");
    separator = "+";
  }
  if ((mask & PhraseCore::kTrackSynthB) != 0) {
    const size_t used = std::char_traits<char>::length(output);
    std::snprintf(output + used, outputSize - used, "%sSB", separator);
    separator = "+";
  }
  if ((mask & PhraseCore::kTrackDrums) != 0) {
    const size_t used = std::char_traits<char>::length(output);
    std::snprintf(output + used, outputSize - used, "%sDR", separator);
  }
  if (output[0] == '\0') std::snprintf(output, outputSize, "---");
}

void formatPatternRef(int16_t reference, char* output, size_t outputSize) {
  if (!output || outputSize == 0) return;
  if (reference < 0) {
    std::snprintf(output, outputSize, "---");
    return;
  }
  const int page = songPatternPage(reference) + 1;
  const int bank = songPatternBank(reference);
  const int slot = songPatternIndexInBank(reference) + 1;
  if (bank < 0 || slot <= 0) {
    std::snprintf(output, outputSize, "---");
    return;
  }
  std::snprintf(output, outputSize, "%d%c%d", page,
                static_cast<char>('A' + bank), slot);
}

int generatedPatternForBar(
    const GroovePuterState::GeneratedPhraseAcceptedSnapshot& accepted,
    uint8_t bar) {
  if (!accepted.valid || bar >= accepted.bars ||
      accepted.pageIndex < 0 || accepted.pageIndex >= kMaxPages ||
      accepted.firstLocalSlot < 0) {
    return -1;
  }
  const int localSlot = static_cast<int>(accepted.firstLocalSlot) + bar;
  if (localSlot < 0 || localSlot >= kPatternsPerPage) return -1;
  const int bank = localSlot / Bank<SynthPattern>::kPatterns;
  const int index = localSlot % Bank<SynthPattern>::kPatterns;
  return songPatternFromPageBankIndex(accepted.pageIndex, bank, index);
}

int currentGeneratedBar(
    const MiniAcid& engine,
    const GroovePuterState::GeneratedPhraseAcceptedSnapshot& accepted) {
  if (!accepted.valid || !engine.songModeEnabled() ||
      engine.songPlaybackSlot() != accepted.songSlot) {
    return -1;
  }
  const int row = engine.currentSongPosition();
  if (row < accepted.songStart || row >= accepted.songStart + accepted.bars) {
    return -1;
  }
  return row - accepted.songStart;
}

void drawGeneratedBarStrip(
    IGfx& gfx,
    int x,
    int y,
    int width,
    uint8_t bars,
    uint8_t selectedBar,
    int currentBar,
    const PhrasePalette& palette) {
  bars = std::max<uint8_t>(1, std::min<uint8_t>(8, bars));
  constexpr int kGap = 3;
  const int available = width - 34 - kGap * 7;
  const int cellW = std::max(12, available / 8);
  const int cellH = 10;
  gfx.setTextColor(palette.dim);
  gfx.drawText(x, y + 1, "BARS");
  const int startX = x + 34;
  for (int bar = 0; bar < 8; ++bar) {
    const int bx = startX + bar * (cellW + kGap);
    const bool active = bar < bars;
    const bool current = bar == currentBar;
    const bool selected = bar == selectedBar && active;
    if (current) {
      gfx.fillRect(bx, y, cellW, cellH, palette.accent);
      gfx.setTextColor(palette.background);
    } else {
      gfx.setTextColor(active ? palette.text : palette.dim);
    }
    gfx.drawRect(bx, y, cellW, cellH,
                 selected ? palette.text :
                 (active ? palette.border : palette.gridOff));
    if (active) {
      char label[4];
      std::snprintf(label, sizeof(label), "%d", bar + 1);
      gfx.drawText(bx + (cellW - gfx.textWidth(label)) / 2, y + 1, label);
    }
  }
}

void drawMask(IGfx& gfx,
              int x,
              int y,
              uint16_t mask,
              bool resolved,
              IGfxColor color,
              const PhrasePalette& palette) {
  constexpr int kCellW = 5;
  constexpr int kCellH = 6;
  constexpr int kGap = 1;
  for (int step = 0; step < 16; ++step) {
    const int cellX = x + step * (kCellW + kGap);
    const bool active = (mask & static_cast<uint16_t>(1u << step)) != 0;
    const IGfxColor off = (step % 4 == 0) ? palette.beatOff : palette.gridOff;
    if (resolved && active) {
      gfx.fillRect(cellX, y, kCellW, kCellH, color);
    } else {
      gfx.drawRect(cellX, y, kCellW, kCellH, off);
    }
  }
}

void drawTrackRow(IGfx& gfx,
                  int x,
                  int y,
                  const char* label,
                  uint16_t mask,
                  bool resolved,
                  const char* reference,
                  IGfxColor color,
                  const PhrasePalette& palette) {
  gfx.setTextColor(color);
  gfx.drawText(x, y, label);
  drawMask(gfx, x + 22, y + 1, mask, resolved, color, palette);
  gfx.setTextColor(resolved ? palette.text : palette.dim);
  gfx.drawText(x + 124, y, reference);
}

void drawPhraseShape(
    IGfx& gfx,
    int x,
    int y,
    int width,
    const PhraseCore::SlotSummary& current,
    uint8_t captureLength,
    uint8_t previewBar,
    const std::array<PhraseCore::BarPreview, PhraseCore::kMaxBars>& previews,
    const std::array<bool, PhraseCore::kMaxBars>& previewValid,
    IGfxColor activeColor,
    const PhrasePalette& palette) {
  constexpr int kBarW = 18;
  constexpr int kBarH = 9;
  constexpr int kGap = 3;
  constexpr int kBarsX = 33;

  const int bars = current.valid ? current.lengthBars : captureLength;
  gfx.setTextColor(palette.dim);
  gfx.drawText(x, y, "BARS");

  for (int bar = 0; bar < PhraseCore::kMaxBars; ++bar) {
    const int barX = x + kBarsX + bar * (kBarW + kGap);
    const bool inRange = bar < bars;
    const IGfxColor border = inRange ? palette.border : palette.gridOff;
    gfx.drawRect(barX, y, kBarW, kBarH, border);

    if (current.valid && inRange && previewValid[bar]) {
      int fillHeight =
          (static_cast<int>(previews[bar].energy) * (kBarH - 2)) / 255;
      if (previews[bar].energy > 0 && fillHeight == 0) fillHeight = 1;
      if (fillHeight > 0) {
        gfx.fillRect(barX + 1, y + kBarH - 1 - fillHeight,
                     kBarW - 2, fillHeight, activeColor);
      }
    }

    if (inRange && bar == previewBar) {
      gfx.drawRect(barX - 1, y - 1, kBarW + 2, kBarH + 2, activeColor);
    }
  }

  char counter[12];
  std::snprintf(counter, sizeof(counter), "%u/%u",
                static_cast<unsigned>(previewBar + 1),
                static_cast<unsigned>(bars));
  gfx.setTextColor(activeColor);
  const int counterX = x + width - gfx.textWidth(counter);
  gfx.drawText(counterX, y, counter);
}

}  // namespace

PhrasePage::PhrasePage(IGfx& gfx,
                       MiniAcid& mini_acid,
                       AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
  destination_row_ = static_cast<uint8_t>(
      std::clamp(mini_acid_.currentSongPosition(), 0,
                 Song::kMaxPositions - 1));
}

PhraseCore::Role PhrasePage::defaultRoleForSlot(PhraseCore::SlotId slot) {
  switch (slot) {
    case PhraseCore::SlotId::A: return PhraseCore::Role::Main;
    case PhraseCore::SlotId::B: return PhraseCore::Role::Variation;
    case PhraseCore::SlotId::C: return PhraseCore::Role::Break;
    case PhraseCore::SlotId::D: return PhraseCore::Role::Ending;
  }
  return PhraseCore::Role::Main;
}

PhraseCore::SlotId PhrasePage::slotFromIndex(int index) {
  while (index < 0) index += PhraseCore::kSlotCount;
  while (index >= PhraseCore::kSlotCount) index -= PhraseCore::kSlotCount;
  return static_cast<PhraseCore::SlotId>(index);
}

int PhrasePage::indexFromSlot(PhraseCore::SlotId slot) {
  return PhraseCore::slotIndex(slot);
}

void PhrasePage::cycleRequestedLength(int delta) {
  (void)GroovePuterState::cycleRequestedPhraseBars(delta);
  const auto& product = GroovePuterState::generatedPhraseProductState();
  if (!product.accepted.valid &&
      product_bar_cursor_ >= GroovePuterState::requestedPhraseBars()) {
    product_bar_cursor_ = 0;
  }
}

void PhrasePage::cycleProductBar(int delta) {
  const auto& product = GroovePuterState::generatedPhraseProductState();
  const int bars = std::max(
      1, static_cast<int>(product.accepted.valid
          ? product.accepted.bars
          : GroovePuterState::requestedPhraseBars()));
  int bar = static_cast<int>(product_bar_cursor_) + delta;
  while (bar < 0) bar += bars;
  while (bar >= bars) bar -= bars;
  product_bar_cursor_ = static_cast<uint8_t>(bar);
}

bool PhrasePage::focusProductBar() {
  const auto& accepted =
      GroovePuterState::generatedPhraseProductState().accepted;
  if (!accepted.valid || product_bar_cursor_ >= accepted.bars) {
    UI::showToast("NO GENERATED PHRASE", 1000);
    return true;
  }
  if (mini_acid_.isPlaying()) {
    UI::showToast("STOP TO FOCUS BAR", 1000);
    return true;
  }

  const int row = accepted.songStart + product_bar_cursor_;
  const int pattern = generatedPatternForBar(accepted, product_bar_cursor_);
  if (accepted.songSlot < 0 || accepted.songSlot > 1 ||
      row < 0 || row >= Song::kMaxPositions || pattern < 0) {
    UI::showToast("PHRASE TARGET UNAVAILABLE", 1200);
    return true;
  }

  mini_acid_.setActiveSongSlot(accepted.songSlot);
  mini_acid_.setSongMode(true);
  mini_acid_.setSongPlaybackSlot(accepted.songSlot);
  mini_acid_.setSongPosition(row);

  char ref[12];
  formatPatternRef(static_cast<int16_t>(pattern), ref, sizeof(ref));
  char toast[48];
  std::snprintf(toast, sizeof(toast), "BAR %u/%u -> %s",
                static_cast<unsigned>(product_bar_cursor_ + 1),
                static_cast<unsigned>(accepted.bars), ref);
  UI::showToast(toast, 1000);
  return true;
}

void PhrasePage::drawProductView(IGfx& gfx) {
  const PhrasePalette palette = paletteForStyle(UI::currentStyle);
  const auto& product = GroovePuterState::generatedPhraseProductState();
  const auto& accepted = product.accepted;
  const uint8_t requestedBars = GroovePuterState::requestedPhraseBars();
  const auto depth = GroovePuterState::currentGenerationLevel();
  const Scene& scene = mini_acid_.sceneManager().currentScene();

  UI::drawStandardHeader(gfx, mini_acid_, "PHRASE");

  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  char line[96];

  std::snprintf(line, sizeof(line), "LENGTH %uB   DEPTH %s   FEEL %uB",
                static_cast<unsigned>(requestedBars),
                GroovePuterState::generationLevelCode(depth),
                static_cast<unsigned>(scene.feel.patternBars));
  gfx.setTextColor(palette.text);
  gfx.drawText(x, LayoutManager::lineY(0), line);

  const uint8_t displayBars = accepted.valid ? accepted.bars : requestedBars;
  if (product_bar_cursor_ >= displayBars) product_bar_cursor_ = 0;
  const int playingBar = currentGeneratedBar(mini_acid_, accepted);
  drawGeneratedBarStrip(
      gfx, x, LayoutManager::lineY(1), width, displayBars,
      product_bar_cursor_, playingBar, palette);

  const char* outcome = GroovePuterState::generatedPhraseOutcomeName(
      product.lastOutcome);
  const IGfxColor outcomeColor =
      product.lastOutcome == GroovePuterState::GeneratedPhraseOutcome::Accepted
          ? palette.accent
          : (product.lastOutcome == GroovePuterState::GeneratedPhraseOutcome::TypedRejection
                 ? palette.drums
                 : palette.dim);
  std::snprintf(line, sizeof(line), "LAST %-12s  REQ:%uB",
                outcome,
                static_cast<unsigned>(product.lastRequestedBars == 0
                    ? requestedBars : product.lastRequestedBars));
  gfx.setTextColor(outcomeColor);
  gfx.drawText(x, LayoutManager::lineY(2), line);

  if (accepted.valid) {
    if (accepted.usedP1r) {
      std::snprintf(line, sizeof(line), "ID:%u  PROG:%u  HARM:%u EVENTS",
                    static_cast<unsigned>(accepted.phraseGenerationIdentity),
                    static_cast<unsigned>(accepted.progression),
                    static_cast<unsigned>(accepted.harmonicEventPositions));
    } else {
      std::snprintf(line, sizeof(line), "ID:LEGACY  PROG:--  HARM:--");
    }
  } else {
    std::snprintf(line, sizeof(line), "ID:--  PROG:--  HARM:--");
  }
  gfx.setTextColor(accepted.valid ? palette.text : palette.dim);
  gfx.drawText(x, LayoutManager::lineY(3), line);

  char ref[12] = "---";
  int selectedPattern = -1;
  if (accepted.valid && product_bar_cursor_ < accepted.bars) {
    selectedPattern = generatedPatternForBar(accepted, product_bar_cursor_);
    formatPatternRef(static_cast<int16_t>(selectedPattern), ref, sizeof(ref));
  }
  if (accepted.valid) {
    std::snprintf(line, sizeof(line), "BAR %u/%u -> %s   SONG %c%d",
                  static_cast<unsigned>(product_bar_cursor_ + 1),
                  static_cast<unsigned>(accepted.bars), ref,
                  static_cast<char>('A' + accepted.songSlot),
                  static_cast<int>(accepted.songStart) +
                      static_cast<int>(product_bar_cursor_) + 1);
  } else {
    std::snprintf(line, sizeof(line), "BAR --  -> ---   SONG TO:%c%d",
                  static_cast<char>('A' + std::clamp(scene.activeSongSlot, 0, 1)),
                  static_cast<int>(destination_row_) + 1);
  }
  gfx.setTextColor(palette.text);
  gfx.drawText(x, LayoutManager::lineY(4), line);

  if (playingBar >= 0) {
    std::snprintf(line, sizeof(line), "PLAYING BAR %d/%u  FOLLOW=PHYSICAL",
                  playingBar + 1, static_cast<unsigned>(accepted.bars));
    gfx.setTextColor(palette.accent);
  } else if (accepted.valid) {
    std::snprintf(line, sizeof(line), "STOP: BAR %u READY TO FOCUS/EDIT",
                  static_cast<unsigned>(product_bar_cursor_ + 1));
    gfx.setTextColor(palette.text);
  } else {
    std::snprintf(line, sizeof(line), "G GENERATES INTO EXISTING SONG/PATTERNS");
    gfx.setTextColor(palette.dim);
  }
  gfx.drawText(x, LayoutManager::lineY(5), line);

  std::snprintf(line, sizeof(line), "FEEL CYCLE %uB != PHRASE LENGTH %uB",
                static_cast<unsigned>(scene.feel.patternBars),
                static_cast<unsigned>(requestedBars));
  gfx.setTextColor(palette.dim);
  gfx.drawText(x, LayoutManager::lineY(6), line);

  std::snprintf(line, sizeof(line), "TO:%c%d  CORE CAP:%uB   V:CORE",
                static_cast<char>('A' + std::clamp(scene.activeSongSlot, 0, 1)),
                static_cast<int>(destination_row_) + 1,
                static_cast<unsigned>(capture_length_));
  gfx.setTextColor(palette.dim);
  gfx.drawText(x, LayoutManager::lineY(7), line);

  UI::drawStandardFooter(gfx,
                         "U/D:LENGTH L/R:BAR P:DEPTH",
                         "G:GENERATE ENT:FOCUS C+ARROW:TO");
}

bool PhrasePage::handleProductEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  const int nav = UIInput::navCode(ui_event);

  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    if (nav == GROOVEPUTER_LEFT) {
      cycleDestinationRow(-1);
      return true;
    }
    if (nav == GROOVEPUTER_RIGHT) {
      cycleDestinationRow(1);
      return true;
    }
    if (nav == GROOVEPUTER_UP) {
      cycleDestinationRow(-8);
      return true;
    }
    if (nav == GROOVEPUTER_DOWN) {
      cycleDestinationRow(8);
      return true;
    }
  }

  if (nav == GROOVEPUTER_UP) {
    cycleRequestedLength(1);
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    cycleRequestedLength(-1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT) {
    cycleProductBar(-1);
    return true;
  }
  if (nav == GROOVEPUTER_RIGHT) {
    cycleProductBar(1);
    return true;
  }

  char key = ui_event.key;
  if (!key) return false;
  const char lower = static_cast<char>(
      std::tolower(static_cast<unsigned char>(key)));
  if (key == '\n' || key == '\r') return focusProductBar();
  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lower == 'g') {
    return generatePhraseToSong();
  }
  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lower == 'p') {
    const auto level = GroovePuterState::cycleGenerationLevel();
    UI::showToast(GroovePuterState::generationLevelShortName(level), 1100);
    return true;
  }
  return false;
}

void PhrasePage::selectSlot(int index) {
  selected_slot_ = slotFromIndex(index);
  capture_role_ = defaultRoleForSlot(selected_slot_);
  if (parent_slot_ == selected_slot_) {
    parent_slot_ = slotFromIndex(index + 1);
  }
  preview_bar_ = 0;
  invalidatePreview();
}

void PhrasePage::cycleLength(int delta) {
  static constexpr uint8_t kLengths[] = {1, 2, 4, 8};
  int index = 0;
  for (int i = 0; i < 4; ++i) {
    if (capture_length_ == kLengths[i]) {
      index = i;
      break;
    }
  }
  index = (index + delta + 4) % 4;
  capture_length_ = kLengths[index];

  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::SlotSummary current =
      PhraseWorkspace::summary(scene, selected_slot_);
  if (!current.valid && preview_bar_ >= capture_length_) preview_bar_ = 0;
}

void PhrasePage::cycleRole(int delta) {
  int value = static_cast<int>(capture_role_) + delta;
  while (value < 0) value += 4;
  while (value >= 4) value -= 4;
  capture_role_ = static_cast<PhraseCore::Role>(value);
}

void PhrasePage::cyclePreviewBar(int delta) {
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::SlotSummary current =
      PhraseWorkspace::summary(scene, selected_slot_);
  const int bars = std::max(
      1, static_cast<int>(current.valid ? current.lengthBars : capture_length_));
  int value = static_cast<int>(preview_bar_) + delta;
  while (value < 0) value += bars;
  while (value >= bars) value -= bars;
  preview_bar_ = static_cast<uint8_t>(value);

  if (current.valid && preview_bar_ < cached_preview_bars_) {
    preview_valid_ = bar_preview_valid_[preview_bar_];
    preview_ = bar_previews_[preview_bar_];
  }
}

void PhrasePage::cycleParent(int delta) {
  int value = indexFromSlot(parent_slot_);
  do {
    value += delta;
    parent_slot_ = slotFromIndex(value);
  } while (parent_slot_ == selected_slot_);
}

void PhrasePage::cycleDestinationRow(int delta) {
  int row = static_cast<int>(destination_row_) + delta;
  row = std::clamp(row, 0, Song::kMaxPositions - 1);
  destination_row_ = static_cast<uint8_t>(row);
}

void PhrasePage::invalidatePreview() {
  preview_valid_ = false;
  preview_page_ = -1;
  preview_phrase_id_ = PhraseCore::kNoPhraseId;
  cached_preview_bars_ = 0;
  bar_preview_valid_.fill(false);
}

void PhrasePage::refreshPreview() {
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::SlotSummary current =
      PhraseWorkspace::summary(scene, selected_slot_);
  const uint32_t revision =
      GroovePuterState::sceneRevisionSnapshot().currentRevision;
  const int page = mini_acid_.currentPageIndex();

  if (!current.valid) {
    preview_valid_ = false;
    preview_phrase_id_ = PhraseCore::kNoPhraseId;
    preview_revision_ = revision;
    preview_page_ = page;
    cached_preview_bars_ = 0;
    bar_preview_valid_.fill(false);
    return;
  }

  if (preview_bar_ >= current.lengthBars) preview_bar_ = 0;
  const bool cacheMatches =
      preview_phrase_id_ == current.phraseId &&
      preview_revision_ == revision &&
      preview_page_ == page &&
      cached_preview_bars_ == current.lengthBars;

  if (!cacheMatches) {
    bar_preview_valid_.fill(false);
    cached_preview_bars_ =
        std::min<uint8_t>(current.lengthBars, PhraseCore::kMaxBars);
    for (uint8_t bar = 0; bar < cached_preview_bars_; ++bar) {
      bar_preview_valid_[bar] = PhraseWorkspace::barPreview(
          scene, page, selected_slot_, bar, bar_previews_[bar]);
    }
    preview_phrase_id_ = current.phraseId;
    preview_revision_ = revision;
    preview_page_ = page;
  }

  preview_valid_ =
      preview_bar_ < cached_preview_bars_ &&
      bar_preview_valid_[preview_bar_];
  preview_ = preview_valid_ ? bar_previews_[preview_bar_]
                            : PhraseCore::BarPreview{};
}

void PhrasePage::showResult(const char* action,
                            const PhraseCore::Result& result) {
  char message[64];
  if (result) {
    std::snprintf(message, sizeof(message), "%s %s #%u",
                  action,
                  PhraseCore::slotName(result.slot),
                  static_cast<unsigned>(result.phraseId));
  } else {
    std::snprintf(message, sizeof(message), "%s: %s",
                  action, PhraseWorkspace::errorName(result.error));
  }
  UI::showToast(message, result ? 1100 : 1500);
}

bool PhrasePage::captureCurrentRegion() {
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  PhraseWorkspace::CaptureRequest request{};
  request.targetSlot = selected_slot_;
  request.sourceSongSlot =
      static_cast<uint8_t>(std::clamp(scene.activeSongSlot, 0, 1));
  request.startRow = static_cast<uint8_t>(
      std::clamp(mini_acid_.currentSongPosition(), 0,
                 Song::kMaxPositions - 1));
  request.lengthBars = capture_length_;
  request.role = capture_role_;
  request.source = PhraseCore::Source::InternalPattern;
  request.trackMask = PhraseCore::kAllTracks;

  const PhraseCore::Result result = commitPhraseMutation(
      [&](PhraseCore::PhraseBank& preparedBank) {
        return PhraseWorkspace::capturePrepared(scene, request, preparedBank);
      });
  if (result) {
    preview_bar_ = 0;
    const int nextRow = std::min(
        Song::kMaxPositions - 1,
        static_cast<int>(request.startRow) + static_cast<int>(request.lengthBars));
    destination_row_ = static_cast<uint8_t>(nextRow);
    invalidatePreview();
  }
  showResult("CAPTURED", result);
  return true;
}

bool PhrasePage::generatePhraseToSong() {
  const int songStart = static_cast<int>(destination_row_);
  const uint8_t requestedBars = GroovePuterState::requestedPhraseBars();
  const GeneratedPhraseSong::Result result = GeneratedPhraseSong::generate(
      mini_acid_, requestedBars, songStart,
      [&](auto&& operation) {
        if (audio_guard_) {
          audio_guard_(std::forward<decltype(operation)>(operation));
        } else {
          operation();
        }
      });

  if (!result) {
    const bool typedRejection =
        result.status == GeneratedPhraseSong::LifecycleStatus::Failed &&
        result.p1r.usedP1r &&
        result.p1r.executionStatus ==
            GroovePuterRhythm::PhraseExecutionStatus::Rejected;
    if (typedRejection) {
      GroovePuterState::publishGeneratedPhraseTypedRejection(requestedBars);
    } else {
      GroovePuterState::publishGeneratedPhraseExecutionFailure(requestedBars);
    }
    LOG_WARN_UI("Generated Phrase -> Song failed at TO=%d: %s",
                songStart + 1, GeneratedPhraseSong::statusText(result));
    UI::showToast(GeneratedPhraseSong::statusText(result), 1600);
    return true;
  }

  const PhraseGenerator::PhraseResult& phraseResult = result.phrase;
  const int firstGlobal = phraseResult.firstGlobalPattern;
  const int pageIndex = firstGlobal >= 0 ? songPatternPage(firstGlobal) : -1;
  const int firstLocalSlot = firstGlobal >= 0
      ? songPatternBank(firstGlobal) * Bank<SynthPattern>::kPatterns +
            songPatternIndexInBank(firstGlobal)
      : -1;
  const int songSlot = std::clamp(
      mini_acid_.sceneManager().currentScene().activeSongSlot, 0, 1);
  GroovePuterState::publishGeneratedPhraseAccepted(
      requestedBars,
      static_cast<uint8_t>(phraseResult.bars),
      songSlot,
      pageIndex,
      firstLocalSlot,
      phraseResult.songStart,
      result.p1r.usedP1r,
      result.status == GeneratedPhraseSong::LifecycleStatus::PendingNextBar,
      result.p1r.phraseGenerationIdentity,
      static_cast<uint8_t>(result.p1r.progression),
      result.p1r.harmonicEventPositions);

  product_bar_cursor_ = 0;
  preview_bar_ = 0;
  const int nextRow = std::min(
      Song::kMaxPositions - 1,
      static_cast<int>(songStart) + phraseResult.bars);
  destination_row_ = static_cast<uint8_t>(nextRow);
  invalidatePreview();

  char message[64];
  if (result.status == GeneratedPhraseSong::LifecycleStatus::PendingNextBar) {
    std::snprintf(message, sizeof(message), "%dB GEN -> NEXT BAR %d-%d",
                  phraseResult.bars,
                  phraseResult.songStart + 1,
                  phraseResult.songStart + phraseResult.bars);
  } else {
    std::snprintf(message, sizeof(message), "%dB GEN -> SONG %d-%d",
                  phraseResult.bars,
                  phraseResult.songStart + 1,
                  phraseResult.songStart + phraseResult.bars);
  }
  UI::showToast(message, 1600);
  LOG_INFO_UI("Generated %dB phrase -> Song rows %d..%d page=%d firstPattern=%d status=%s",
              phraseResult.bars,
              phraseResult.songStart + 1,
              phraseResult.songStart + phraseResult.bars,
              mini_acid_.currentPageIndex() + 1,
              phraseResult.firstGlobalPattern,
              GeneratedPhraseSong::statusText(result));
  return true;
}

bool PhrasePage::deriveFromParent() {
  PhraseWorkspace::DeriveRequest request{};
  request.targetSlot = selected_slot_;
  request.parentSlot = parent_slot_;
  request.role = capture_role_;
  const PhraseCore::Result result = commitPhraseMutation(
      [&](PhraseCore::PhraseBank& preparedBank) {
        return PhraseWorkspace::derivePrepared(request, preparedBank);
      });
  if (result) {
    preview_bar_ = 0;
    invalidatePreview();
  }
  showResult("DERIVED", result);
  return true;
}

bool PhrasePage::writeToCurrentRow(bool overwrite) {
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::SlotSummary source =
      PhraseWorkspace::summary(scene, selected_slot_);
  PhraseWorkspace::WriteRequest request{};
  request.sourceSlot = selected_slot_;
  request.destinationSongSlot =
      static_cast<uint8_t>(std::clamp(scene.activeSongSlot, 0, 1));
  request.startRow = destination_row_;
  request.overwrite = overwrite;

  const PhraseCore::Result result = commitSongMutation(
      [&](Song& preparedSong) {
        return PhraseWorkspace::writeToSongPrepared(
            scene.phraseBank, request, preparedSong);
      });
  if (result) {
    if (!overwrite && source.valid) {
      const int nextRow = std::min(
          Song::kMaxPositions - 1,
          static_cast<int>(request.startRow) + static_cast<int>(source.lengthBars));
      destination_row_ = static_cast<uint8_t>(nextRow);
    }
    invalidatePreview();
  }
  showResult(overwrite ? "REPLACED" : "INSERTED", result);
  return true;
}

bool PhrasePage::clearCurrentSlot() {
  const PhraseCore::Result result = commitPhraseMutation(
      [&](PhraseCore::PhraseBank& preparedBank) {
        return PhraseWorkspace::clearPrepared(selected_slot_, preparedBank);
      });
  if (result) {
    preview_bar_ = 0;
    invalidatePreview();
  }
  showResult("CLEAR", result);
  return true;
}

bool PhrasePage::undoPreparedOwnedState() {
  using GroovePuterUndo::PhraseUndoPayload;
  using GroovePuterUndo::SongUndoPayload;
  using GroovePuterUndo::UndoKind;
  using GroovePuterUndo::UndoResult;

  auto& owner = GroovePuterUndo::undoOwner();
  if (!owner.hasUndo()) return false;

  if (owner.kind() == UndoKind::Generation &&
      GeneratedPhraseSong::ownsCurrentUndoReceipt()) {
    const UndoResult result = GeneratedPhraseSong::undoLastGeneratedPhrase(
        mini_acid_,
        [&](auto&& operation) {
          if (audio_guard_) {
            audio_guard_(std::forward<decltype(operation)>(operation));
          } else {
            operation();
          }
        });
    if (result == UndoResult::Restored) {
      invalidatePreview();
      UI::showToast("UNDO: GENERATED PHRASE", 1000);
      return true;
    }
    if (result == UndoResult::TargetUnavailable) {
      UI::showToast(mini_acid_.isPlaying()
                        ? "UNDO: STOP OR WAIT"
                        : "UNDO: RETURN PAGE",
                    1200);
      return true;
    }
    return result == UndoResult::Expired;
  }

  if (owner.kind() == UndoKind::Phrase) {
    const bool redo = owner.nextIsRedo();
    const UndoResult result = owner.togglePrepared<PhraseUndoPayload>(
        UndoKind::Phrase,
        [&](const PhraseUndoPayload& receipt) {
          return GroovePuterUndo::phraseUndoTargetAvailable(
              mini_acid_.sceneManager(), receipt);
        },
        [&](PhraseUndoPayload& receipt) {
          const auto exchange = [&]() {
            GroovePuterUndo::exchangePhraseUndo(
                mini_acid_.sceneManager(), receipt);
          };
          if (audio_guard_) audio_guard_(exchange);
          else exchange();
        });
    if (result == UndoResult::Restored) {
      invalidatePreview();
      UI::showToast(redo ? "REDO: PHRASE" : "UNDO: PHRASE", 900);
      return true;
    }
    if (result == UndoResult::ContextUnavailable ||
        result == UndoResult::TargetUnavailable) {
      return false;
    }
    return result == UndoResult::Expired;
  }

  if (owner.kind() == UndoKind::Song) {
    const bool redo = owner.nextIsRedo();
    const UndoResult result = owner.togglePrepared<SongUndoPayload>(
        UndoKind::Song,
        [&](const SongUndoPayload& receipt) {
          return GroovePuterUndo::songUndoTargetAvailable(
              mini_acid_.sceneManager(), receipt);
        },
        [&](SongUndoPayload& receipt) {
          const auto exchange = [&]() {
            GroovePuterUndo::exchangeSongUndo(
                mini_acid_.sceneManager(), receipt);
          };
          if (audio_guard_) audio_guard_(exchange);
          else exchange();
        });
    if (result == UndoResult::Restored) {
      invalidatePreview();
      UI::showToast(redo ? "REDO: SONG" : "UNDO: SONG", 900);
      return true;
    }
    if (result == UndoResult::ContextUnavailable ||
        result == UndoResult::TargetUnavailable) {
      return false;
    }
    return result == UndoResult::Expired;
  }

  return false;
}

void PhrasePage::draw(IGfx& gfx) {
  if (product_view_) {
    drawProductView(gfx);
    return;
  }

  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::SlotSummary current =
      PhraseWorkspace::summary(scene, selected_slot_);
  refreshPreview();

  const PhrasePalette palette = paletteForStyle(UI::currentStyle);
  const IGfxColor activeColor = slotColor(selected_slot_, palette);

  UI::drawStandardHeader(gfx, mini_acid_, "PHRASE CORE");

  // MiniAcidDisplay paints the skin before each page draw. Do not perform a
  // second full-content clear here; render the eight compact content bands once.
  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  const int slotY = LayoutManager::lineY(0);
  constexpr int kGap = 3;
  const int slotWidth = (width - kGap * 3) / 4;

  for (int i = 0; i < PhraseCore::kSlotCount; ++i) {
    const PhraseCore::SlotId slot = slotFromIndex(i);
    const PhraseCore::SlotSummary summary =
        PhraseWorkspace::summary(scene, slot);
    const int slotX = x + i * (slotWidth + kGap);
    const bool selected = slot == selected_slot_;
    const IGfxColor color = slotColor(slot, palette);

    gfx.fillRect(slotX, slotY, slotWidth, 11,
                 selected ? palette.panelSelected : palette.panel);
    gfx.drawRect(slotX, slotY, slotWidth, 11,
                 selected ? color : palette.border);

    char label[16];
    std::snprintf(label, sizeof(label), "%s %s",
                  PhraseCore::slotName(slot),
                  summary.valid ? roleShort(summary.role) : "---");
    gfx.setTextColor(selected ? color :
                     (summary.valid ? palette.text : palette.dim));
    gfx.drawText(slotX + 3, slotY + 2, label);
    if (summary.valid) {
      gfx.fillRect(slotX + slotWidth - 5, slotY + 4, 2, 2, color);
    }
  }

  char line[96];
  const int summaryY = LayoutManager::lineY(1);
  if (current.valid) {
    std::snprintf(line, sizeof(line), "%s  %uB  %s  %s",
                  roleShort(current.role),
                  static_cast<unsigned>(current.lengthBars),
                  sourceShort(current.source),
                  storageBadge(current));
  } else {
    std::snprintf(line, sizeof(line), "EMPTY SLOT  NEXT CAP %s %uB",
                  roleShort(capture_role_),
                  static_cast<unsigned>(capture_length_));
  }
  gfx.setTextColor(activeColor);
  gfx.drawText(x, summaryY, line);

  const int contextY = LayoutManager::lineY(2);
  const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
  if (current.valid) {
    char tracks[20];
    formatTrackMask(current.trackMask, tracks, sizeof(tracks));
    std::snprintf(line, sizeof(line), "ID:%u P:%u %s  TO:%c%d",
                  static_cast<unsigned>(current.phraseId),
                  static_cast<unsigned>(current.parentId),
                  tracks,
                  static_cast<char>('A' + songSlot),
                  static_cast<int>(destination_row_) + 1);
  } else {
    std::snprintf(line, sizeof(line), "FROM:%c%d DERIVE:%s  TO:%c%d",
                  static_cast<char>('A' + songSlot),
                  mini_acid_.currentSongPosition() + 1,
                  PhraseCore::slotName(parent_slot_),
                  static_cast<char>('A' + songSlot),
                  static_cast<int>(destination_row_) + 1);
  }
  gfx.setTextColor(palette.dim);
  gfx.drawText(x, contextY, line);

  const int shapeY = LayoutManager::lineY(3) + 1;
  drawPhraseShape(gfx, x, shapeY, width, current, capture_length_,
                  preview_bar_, bar_previews_, bar_preview_valid_,
                  activeColor, palette);

  char refA[12];
  char refB[12];
  char refD[12];
  if (preview_valid_) {
    formatPatternRef(preview_.patternRefs[0], refA, sizeof(refA));
    formatPatternRef(preview_.patternRefs[1], refB, sizeof(refB));
    formatPatternRef(preview_.patternRefs[2], refD, sizeof(refD));
  } else {
    std::snprintf(refA, sizeof(refA), "---");
    std::snprintf(refB, sizeof(refB), "---");
    std::snprintf(refD, sizeof(refD), "---");
  }

  drawTrackRow(
      gfx, x, LayoutManager::lineY(4), "SA",
      preview_valid_ ? preview_.synthAMask : 0,
      preview_valid_ &&
          (preview_.resolvedMask & PhraseCore::kTrackSynthA) != 0,
      refA, palette.synthA, palette);
  drawTrackRow(
      gfx, x, LayoutManager::lineY(5), "SB",
      preview_valid_ ? preview_.synthBMask : 0,
      preview_valid_ &&
          (preview_.resolvedMask & PhraseCore::kTrackSynthB) != 0,
      refB, palette.synthB, palette);
  drawTrackRow(
      gfx, x, LayoutManager::lineY(6), "DR",
      preview_valid_ ? preview_.drumMask : 0,
      preview_valid_ &&
          (preview_.resolvedMask & PhraseCore::kTrackDrums) != 0,
      refD, palette.drums, palette);

  const int actionY = LayoutManager::lineY(7);
  std::snprintf(line, sizeof(line), "CAP %uB %s  GEN %uB  P:%s",
                static_cast<unsigned>(capture_length_),
                roleShort(capture_role_),
                static_cast<unsigned>(GroovePuterState::requestedPhraseBars()),
                PhraseCore::slotName(parent_slot_));
  gfx.setTextColor(palette.dim);
  gfx.drawText(x, actionY, line);

  const char* ownership =
      current.valid ? (current.mutableBacking ? "REF LINKED" : "REF")
                    : "ENTER CAP";
  gfx.setTextColor(current.valid ? activeColor : palette.text);
  gfx.drawText(x + width - gfx.textWidth(ownership), actionY, ownership);

  UI::drawStandardFooter(gfx,
                         "1-4:SLOT L/R:BAR U/D:CAPLEN",
                         "G:GEN V:PRODUCT ENT/D/W");
}

bool PhrasePage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == GROOVEPUTER_APPLICATION_EVENT &&
      ui_event.app_event_type == GROOVEPUTER_APP_EVENT_UNDO) {
    return undoPreparedOwnedState();
  }
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  char toggleKey = ui_event.key;
  const char toggleLower = toggleKey
      ? static_cast<char>(std::tolower(static_cast<unsigned char>(toggleKey)))
      : 0;
  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta && toggleLower == 'v') {
    product_view_ = !product_view_;
    UI::showToast(product_view_ ? "PHRASE PRODUCT" : "PHRASE CORE", 700);
    return true;
  }

  if (product_view_) return handleProductEvent(ui_event);

  const int nav = UIInput::navCode(ui_event);

  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta) {
    if (nav == GROOVEPUTER_LEFT) {
      cycleDestinationRow(-1);
      return true;
    }
    if (nav == GROOVEPUTER_RIGHT) {
      cycleDestinationRow(1);
      return true;
    }
    if (nav == GROOVEPUTER_UP) {
      cycleDestinationRow(-8);
      return true;
    }
    if (nav == GROOVEPUTER_DOWN) {
      cycleDestinationRow(8);
      return true;
    }
  }

  if (nav == GROOVEPUTER_UP) {
    cycleLength(1);
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    cycleLength(-1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT) {
    cyclePreviewBar(-1);
    return true;
  }
  if (nav == GROOVEPUTER_RIGHT) {
    cyclePreviewBar(1);
    return true;
  }

  char key = ui_event.key;
  if (!key) return false;
  const char lower = static_cast<char>(
      std::tolower(static_cast<unsigned char>(key)));

  if (!ui_event.ctrl && key >= '1' && key <= '4') {
    selectSlot(key - '1');
    return true;
  }
  if (key == '\n' || key == '\r') return captureCurrentRegion();
  if (key == '\b' || key == 0x7F) return clearCurrentSlot();
  if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lower == 'g') {
    return generatePhraseToSong();
  }

  switch (lower) {
    case 'r': cycleRole(ui_event.shift ? -1 : 1); return true;
    case 'p': cycleParent(ui_event.shift ? -1 : 1); return true;
    case 'd': return deriveFromParent();
    case 'w': return writeToCurrentRow(ui_event.alt);
    default: break;
  }
  return false;
}

const std::string& PhrasePage::getTitle() const {
  return title_;
}
