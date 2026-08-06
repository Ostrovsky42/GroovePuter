#include "phrase_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <utility>

#include "../layout_manager.h"
#include "../ui_colors.h"
#include "../ui_common.h"
#include "../ui_input.h"
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

void PhrasePage::moveArrangementCursor(int delta) {
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const uint8_t length = scene.phraseBank.arrangement.length;
  const int maximum = length < PhraseCore::kArrangementCapacity
                          ? length
                          : PhraseCore::kArrangementCapacity - 1;
  int value = static_cast<int>(arrangement_cursor_) + delta;
  if (value < 0) value = 0;
  if (value > maximum) value = maximum;
  arrangement_cursor_ = static_cast<uint8_t>(value);
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

void PhrasePage::showArrangementResult(
    const char* action,
    const PhraseCore::ArrangementResult& result,
    PhraseCore::SlotId slot,
    bool includeSlot) {
  char message[64];
  if (!result) {
    std::snprintf(message, sizeof(message), "%s: %s",
                  action, PhraseWorkspace::errorName(result.error));
  } else if (includeSlot) {
    std::snprintf(message, sizeof(message), "%s %02u %s",
                  action,
                  static_cast<unsigned>(result.position + 1),
                  PhraseCore::slotName(slot));
  } else if (result.totalBars > 0) {
    std::snprintf(message, sizeof(message), "%s %uB",
                  action, static_cast<unsigned>(result.totalBars));
  } else {
    std::snprintf(message, sizeof(message), "%s", action);
  }
  UI::showToast(message, result ? 1100 : 1500);
}

bool PhrasePage::captureCurrentRegion() {
  Scene& scene = mini_acid_.sceneManager().currentScene();
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

  const PhraseCore::Result result = PhraseWorkspace::capture(
      scene, request, [&](auto&& operation) {
        if (audio_guard_) audio_guard_(std::forward<decltype(operation)>(operation));
        else operation();
      });
  if (result) {
    preview_bar_ = 0;
    invalidatePreview();
  }
  showResult("CAPTURED", result);
  return true;
}

bool PhrasePage::deriveFromParent() {
  Scene& scene = mini_acid_.sceneManager().currentScene();
  PhraseWorkspace::DeriveRequest request{};
  request.targetSlot = selected_slot_;
  request.parentSlot = parent_slot_;
  request.role = capture_role_;
  const PhraseCore::Result result = PhraseWorkspace::derive(
      scene, request, [&](auto&& operation) {
        if (audio_guard_) audio_guard_(std::forward<decltype(operation)>(operation));
        else operation();
      });
  if (result) {
    preview_bar_ = 0;
    invalidatePreview();
  }
  showResult("DERIVED", result);
  return true;
}

bool PhrasePage::writeToCurrentRow(bool overwrite) {
  Scene& scene = mini_acid_.sceneManager().currentScene();
  PhraseWorkspace::WriteRequest request{};
  request.sourceSlot = selected_slot_;
  request.destinationSongSlot =
      static_cast<uint8_t>(std::clamp(scene.activeSongSlot, 0, 1));
  request.startRow = static_cast<uint8_t>(
      std::clamp(mini_acid_.currentSongPosition(), 0,
                 Song::kMaxPositions - 1));
  request.overwrite = overwrite;

  const PhraseCore::Result result = PhraseWorkspace::writeToSong(
      scene, request, [&](auto&& operation) {
        if (audio_guard_) audio_guard_(std::forward<decltype(operation)>(operation));
        else operation();
      });
  if (result) invalidatePreview();
  showResult(overwrite ? "WRITE!" : "WRITE", result);
  return true;
}

bool PhrasePage::clearCurrentSlot() {
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::Result result = PhraseWorkspace::clear(
      scene, selected_slot_, [&](auto&& operation) {
        if (audio_guard_) audio_guard_(std::forward<decltype(operation)>(operation));
        else operation();
      });
  if (result) {
    preview_bar_ = 0;
    invalidatePreview();
    const uint8_t length = scene.phraseBank.arrangement.length;
    if (length == 0) arrangement_cursor_ = 0;
    else if (arrangement_cursor_ >= length) arrangement_cursor_ = length - 1;
  }
  showResult("CLEAR", result);
  return true;
}

bool PhrasePage::assignArrangementSlot(PhraseCore::SlotId slot) {
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::ArrangementResult result =
      PhraseWorkspace::assignArrangement(
          scene, arrangement_cursor_, slot, [&](auto&& operation) {
            if (audio_guard_) audio_guard_(std::forward<decltype(operation)>(operation));
            else operation();
          });
  showArrangementResult("ARR SET", result, slot, true);
  if (result && arrangement_cursor_ + 1 < PhraseCore::kArrangementCapacity &&
      arrangement_cursor_ + 1 <= result.length) {
    ++arrangement_cursor_;
  }
  return true;
}

bool PhrasePage::removeArrangementSlot() {
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::ArrangementResult result =
      PhraseWorkspace::removeArrangement(
          scene, arrangement_cursor_, [&](auto&& operation) {
            if (audio_guard_) audio_guard_(std::forward<decltype(operation)>(operation));
            else operation();
          });
  showArrangementResult("ARR REMOVE", result);
  if (result) {
    if (result.length == 0) arrangement_cursor_ = 0;
    else if (arrangement_cursor_ >= result.length) arrangement_cursor_ = result.length - 1;
  }
  return true;
}

bool PhrasePage::clearArrangementChain() {
  Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::ArrangementResult result =
      PhraseWorkspace::clearArrangement(scene, [&](auto&& operation) {
        if (audio_guard_) audio_guard_(std::forward<decltype(operation)>(operation));
        else operation();
      });
  arrangement_cursor_ = 0;
  showArrangementResult("ARR CLEAR", result);
  return true;
}

bool PhrasePage::writeArrangementToCurrentRow(bool overwrite) {
  Scene& scene = mini_acid_.sceneManager().currentScene();
  PhraseWorkspace::ArrangementWriteRequest request{};
  request.destinationSongSlot =
      static_cast<uint8_t>(std::clamp(scene.activeSongSlot, 0, 1));
  request.startRow = static_cast<uint8_t>(
      std::clamp(mini_acid_.currentSongPosition(), 0,
                 Song::kMaxPositions - 1));
  request.overwrite = overwrite;
  const PhraseCore::ArrangementResult result =
      PhraseWorkspace::writeArrangementToSong(
          scene, request, [&](auto&& operation) {
            if (audio_guard_) audio_guard_(std::forward<decltype(operation)>(operation));
            else operation();
          });
  showArrangementResult(overwrite ? "ARR WRITE!" : "ARR WRITE", result);
  return true;
}

void PhrasePage::drawCore(IGfx& gfx) {
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::SlotSummary current =
      PhraseWorkspace::summary(scene, selected_slot_);
  refreshPreview();

  const PhrasePalette palette = paletteForStyle(UI::currentStyle);
  const IGfxColor activeColor = slotColor(selected_slot_, palette);
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
  if (current.valid) {
    char tracks[20];
    formatTrackMask(current.trackMask, tracks, sizeof(tracks));
    std::snprintf(line, sizeof(line), "ID:%u P:%u SRC:%c%02u %s",
                  static_cast<unsigned>(current.phraseId),
                  static_cast<unsigned>(current.parentId),
                  static_cast<char>('A' + current.sourceSongSlot),
                  static_cast<unsigned>(current.sourceStartRow + 1),
                  tracks);
  } else {
    const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
    std::snprintf(line, sizeof(line), "FROM SONG %c ROW %d  DERIVE:%s",
                  static_cast<char>('A' + songSlot),
                  mini_acid_.currentSongPosition() + 1,
                  PhraseCore::slotName(parent_slot_));
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

  drawTrackRow(gfx, x, LayoutManager::lineY(4), "SA",
               preview_valid_ ? preview_.synthAMask : 0,
               preview_valid_ &&
                   (preview_.resolvedMask & PhraseCore::kTrackSynthA) != 0,
               refA, palette.synthA, palette);
  drawTrackRow(gfx, x, LayoutManager::lineY(5), "SB",
               preview_valid_ ? preview_.synthBMask : 0,
               preview_valid_ &&
                   (preview_.resolvedMask & PhraseCore::kTrackSynthB) != 0,
               refB, palette.synthB, palette);
  drawTrackRow(gfx, x, LayoutManager::lineY(6), "DR",
               preview_valid_ ? preview_.drumMask : 0,
               preview_valid_ &&
                   (preview_.resolvedMask & PhraseCore::kTrackDrums) != 0,
               refD, palette.drums, palette);

  const int actionY = LayoutManager::lineY(7);
  const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
  std::snprintf(line, sizeof(line), "D:%c%02d N:%uB %s",
                static_cast<char>('A' + songSlot),
                mini_acid_.currentSongPosition() + 1,
                static_cast<unsigned>(capture_length_),
                roleShort(capture_role_));
  gfx.setTextColor(palette.dim);
  gfx.drawText(x, actionY, line);

  const char* ownership =
      current.valid ? (current.mutableBacking ? "REF LINKED" : "REF")
                    : "ENTER CAP";
  gfx.setTextColor(current.valid ? activeColor : palette.text);
  gfx.drawText(x + width - gfx.textWidth(ownership), actionY, ownership);

  UI::drawStandardFooter(gfx,
                         "TAB:ARR 1-4:SLOT L/R:BAR U/D:LEN",
                         "ENT:CAP D:DERIVE W:WRITE SH+W:OVER");
}

void PhrasePage::drawArrangement(IGfx& gfx) {
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::PhraseArrangement& arrangement =
      scene.phraseBank.arrangement;
  const PhrasePalette palette = paletteForStyle(UI::currentStyle);
  const int x = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  constexpr int kColumns = 8;
  constexpr int kGap = 2;
  const int cellWidth = (width - (kColumns - 1) * kGap) / kColumns;
  constexpr int kCellHeight = 18;

  char line[96];
  std::snprintf(line, sizeof(line), "CHAIN %u/%u  TOTAL %uB  REF LINKED",
                static_cast<unsigned>(arrangement.length),
                static_cast<unsigned>(PhraseCore::kArrangementCapacity),
                static_cast<unsigned>(PhraseWorkspace::arrangementTotalBars(scene)));
  gfx.setTextColor(palette.accent);
  gfx.drawText(x, LayoutManager::lineY(0), line);

  for (int position = 0; position < PhraseCore::kArrangementCapacity; ++position) {
    const int row = position / kColumns;
    const int column = position % kColumns;
    const int cellX = x + column * (cellWidth + kGap);
    const int cellY = LayoutManager::lineY(row == 0 ? 1 : 3);
    const bool selected = position == arrangement_cursor_;
    const bool assigned = position < arrangement.length &&
                          arrangement.slots[position] < PhraseCore::kSlotCount;
    const PhraseCore::SlotId slot = assigned
        ? static_cast<PhraseCore::SlotId>(arrangement.slots[position])
        : PhraseCore::SlotId::A;
    const IGfxColor color = assigned ? slotColor(slot, palette) : palette.dim;

    gfx.fillRect(cellX, cellY, cellWidth, kCellHeight,
                 selected ? palette.panelSelected : palette.panel);
    gfx.drawRect(cellX, cellY, cellWidth, kCellHeight,
                 selected ? (assigned ? color : palette.accent) : palette.border);

    char indexText[4];
    std::snprintf(indexText, sizeof(indexText), "%02d", position + 1);
    gfx.setTextColor(selected ? palette.text : palette.dim);
    gfx.drawText(cellX + 2, cellY + 1, indexText);

    const char* value = assigned ? PhraseCore::slotName(slot)
                                 : (selected && position == arrangement.length ? "+" : ".");
    gfx.setTextColor(assigned ? color : palette.dim);
    gfx.drawText(cellX + cellWidth - 8, cellY + 9, value);
  }

  const int detailY = LayoutManager::lineY(5);
  if (arrangement_cursor_ < arrangement.length &&
      arrangement.slots[arrangement_cursor_] < PhraseCore::kSlotCount) {
    const PhraseCore::SlotId slot = static_cast<PhraseCore::SlotId>(
        arrangement.slots[arrangement_cursor_]);
    const PhraseCore::SlotSummary summary = PhraseWorkspace::summary(scene, slot);
    if (summary.valid) {
      std::snprintf(line, sizeof(line), "POS %02u: %s %s %uB ID:%u",
                    static_cast<unsigned>(arrangement_cursor_ + 1),
                    PhraseCore::slotName(slot), roleShort(summary.role),
                    static_cast<unsigned>(summary.lengthBars),
                    static_cast<unsigned>(summary.phraseId));
    } else {
      std::snprintf(line, sizeof(line), "POS %02u: INVALID",
                    static_cast<unsigned>(arrangement_cursor_ + 1));
    }
  } else {
    std::snprintf(line, sizeof(line), "POS %02u: APPEND WITH 1/2/3/4",
                  static_cast<unsigned>(arrangement_cursor_ + 1));
  }
  gfx.setTextColor(palette.text);
  gfx.drawText(x, detailY, line);

  const int songSlot = std::clamp(scene.activeSongSlot, 0, 1);
  std::snprintf(line, sizeof(line), "DEST SONG %c ROW %d  SAFE WRITE",
                static_cast<char>('A' + songSlot),
                mini_acid_.currentSongPosition() + 1);
  gfx.setTextColor(palette.dim);
  gfx.drawText(x, LayoutManager::lineY(6), line);

  gfx.setTextColor(palette.accent);
  gfx.drawText(x, LayoutManager::lineY(7),
               "A/B/C/D EXPAND BY SAVED 1/2/4/8B LENGTH");

  UI::drawStandardFooter(gfx,
                         "TAB:CORE L/R:POS U/D:+8 1-4:SET",
                         "W:WRITE SH+W:OVER DEL:RM SH+DEL:CLEAR");
}

void PhrasePage::draw(IGfx& gfx) {
  UI::drawStandardHeader(gfx, mini_acid_,
                         view_ == View::Core ? "PHRASE CORE"
                                             : "PHRASE ARRANGE");
  if (view_ == View::Core) drawCore(gfx);
  else drawArrangement(gfx);
}

bool PhrasePage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  char key = ui_event.key;
  if (key == '\t') {
    view_ = view_ == View::Core ? View::Arrange : View::Core;
    return true;
  }

  const int nav = UIInput::navCode(ui_event);
  if (view_ == View::Arrange) {
    if (nav == GROOVEPUTER_LEFT) {
      moveArrangementCursor(-1);
      return true;
    }
    if (nav == GROOVEPUTER_RIGHT) {
      moveArrangementCursor(1);
      return true;
    }
    if (nav == GROOVEPUTER_UP) {
      moveArrangementCursor(-8);
      return true;
    }
    if (nav == GROOVEPUTER_DOWN) {
      moveArrangementCursor(8);
      return true;
    }
    if (!key) return false;
    if (!ui_event.ctrl && !ui_event.alt && !ui_event.meta &&
        key >= '1' && key <= '4') {
      return assignArrangementSlot(slotFromIndex(key - '1'));
    }
    if (key == '\b' || key == 0x7F) {
      return ui_event.shift ? clearArrangementChain()
                            : removeArrangementSlot();
    }
    const char lower = static_cast<char>(
        std::tolower(static_cast<unsigned char>(key)));
    if (lower == 'w') return writeArrangementToCurrentRow(ui_event.shift);
    return false;
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

  if (!key) return false;
  const char lower = static_cast<char>(
      std::tolower(static_cast<unsigned char>(key)));

  if (!ui_event.ctrl && key >= '1' && key <= '4') {
    selectSlot(key - '1');
    return true;
  }
  if (key == '\n' || key == '\r') return captureCurrentRegion();
  if (key == '\b' || key == 0x7F) return clearCurrentSlot();

  switch (lower) {
    case 'r': cycleRole(ui_event.shift ? -1 : 1); return true;
    case 'p': cycleParent(ui_event.shift ? -1 : 1); return true;
    case 'd': return deriveFromParent();
    case 'w': return writeToCurrentRow(ui_event.shift);
    default: break;
  }
  return false;
}

const std::string& PhrasePage::getTitle() const {
  return title_;
}
