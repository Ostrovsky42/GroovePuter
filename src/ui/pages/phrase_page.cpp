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

constexpr IGfxColor kPhraseAColor(0x00D7FF);
constexpr IGfxColor kPhraseBColor(0xFF4FD8);
constexpr IGfxColor kPhraseCColor(0xFFD166);
constexpr IGfxColor kPhraseDColor(0x73E2A7);
constexpr IGfxColor kDimColor(0x526474);
constexpr IGfxColor kPanelColor(0x121922);
constexpr IGfxColor kGridOffColor(0x25303A);

IGfxColor slotColor(PhraseCore::SlotId slot) {
  switch (slot) {
    case PhraseCore::SlotId::A: return kPhraseAColor;
    case PhraseCore::SlotId::B: return kPhraseBColor;
    case PhraseCore::SlotId::C: return kPhraseCColor;
    case PhraseCore::SlotId::D: return kPhraseDColor;
  }
  return COLOR_WHITE;
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
              IGfxColor color) {
  constexpr int kCellW = 6;
  constexpr int kCellH = 5;
  for (int step = 0; step < 16; ++step) {
    const int cellX = x + step * (kCellW + 1);
    const bool active = (mask & static_cast<uint16_t>(1u << step)) != 0;
    const IGfxColor off = (step % 4 == 0) ? IGfxColor(0x36434F) : kGridOffColor;
    if (!resolved) {
      gfx.drawRect(cellX, y, kCellW, kCellH, off);
    } else if (active) {
      gfx.fillRect(cellX, y, kCellW, kCellH, color);
    } else {
      gfx.drawRect(cellX, y, kCellW, kCellH, off);
    }
  }
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
  const int bars = current.valid ? current.lengthBars : capture_length_;
  int value = static_cast<int>(preview_bar_) + delta;
  while (value < 0) value += bars;
  while (value >= bars) value -= bars;
  preview_bar_ = static_cast<uint8_t>(value);
  invalidatePreview();
}

void PhrasePage::cycleParent(int delta) {
  int value = indexFromSlot(parent_slot_);
  do {
    value += delta;
    parent_slot_ = slotFromIndex(value);
  } while (parent_slot_ == selected_slot_);
}

void PhrasePage::invalidatePreview() {
  preview_valid_ = false;
  preview_page_ = -1;
  preview_phrase_id_ = PhraseCore::kNoPhraseId;
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
    return;
  }
  if (preview_bar_ >= current.lengthBars) preview_bar_ = 0;
  if (preview_valid_ && preview_phrase_id_ == current.phraseId &&
      preview_revision_ == revision && preview_page_ == page) {
    return;
  }

  preview_valid_ = PhraseWorkspace::barPreview(
      scene, page, selected_slot_, preview_bar_, preview_);
  preview_phrase_id_ = current.phraseId;
  preview_revision_ = revision;
  preview_page_ = page;
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
  }
  showResult("CLEAR", result);
  return true;
}

void PhrasePage::draw(IGfx& gfx) {
  const Scene& scene = mini_acid_.sceneManager().currentScene();
  const PhraseCore::SlotSummary current =
      PhraseWorkspace::summary(scene, selected_slot_);
  refreshPreview();

  UI::drawStandardHeader(gfx, mini_acid_, "PHRASE CORE");
  LayoutManager::clearContent(gfx);

  const int x = Layout::CONTENT.x;
  const int y = LayoutManager::lineY(0);
  const int width = Layout::CONTENT.w;
  const int gap = 3;
  const int slotWidth = (width - gap * 3) / 4;

  for (int i = 0; i < PhraseCore::kSlotCount; ++i) {
    const PhraseCore::SlotId slot = slotFromIndex(i);
    const PhraseCore::SlotSummary summary =
        PhraseWorkspace::summary(scene, slot);
    const int slotX = x + i * (slotWidth + gap);
    const bool selected = slot == selected_slot_;
    const IGfxColor color = slotColor(slot);
    gfx.fillRect(slotX, y, slotWidth, 11, selected ? kPanelColor : IGfxColor(0x0E1319));
    gfx.drawRect(slotX, y, slotWidth, 11, selected ? color : kDimColor);
    char label[16];
    std::snprintf(label, sizeof(label), "%s %s",
                  PhraseCore::slotName(slot), summary.valid ? "REF" : "---");
    gfx.setTextColor(selected ? color : (summary.valid ? COLOR_WHITE : kDimColor));
    gfx.drawText(slotX + 3, y + 2, label);
  }

  char line[96];
  if (current.valid) {
    std::snprintf(line, sizeof(line), "%s  %uB  %s  ID:%u P:%u",
                  PhraseCore::roleName(current.role),
                  static_cast<unsigned>(current.lengthBars),
                  PhraseCore::sourceName(current.source),
                  static_cast<unsigned>(current.phraseId),
                  static_cast<unsigned>(current.parentId));
  } else {
    std::snprintf(line, sizeof(line), "NEW %s  %uB  FROM SONG ROW %d",
                  PhraseCore::roleName(capture_role_),
                  static_cast<unsigned>(capture_length_),
                  mini_acid_.currentSongPosition() + 1);
  }
  gfx.setTextColor(slotColor(selected_slot_));
  gfx.drawText(x, y + 14, line);

  std::snprintf(line, sizeof(line), "CAP %uB %s  DERIVE:%s  BAR:%u/%u",
                static_cast<unsigned>(capture_length_),
                PhraseCore::roleName(capture_role_),
                PhraseCore::slotName(parent_slot_),
                static_cast<unsigned>(preview_bar_ + 1),
                static_cast<unsigned>(current.valid ? current.lengthBars
                                                    : capture_length_));
  gfx.setTextColor(IGfxColor(0x8AA4BA));
  gfx.drawText(x, y + 25, line);

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

  const int gridX = x + 25;
  const int gridY = y + 38;
  gfx.setTextColor(kPhraseAColor);
  gfx.drawText(x, gridY - 1, "A");
  drawMask(gfx, gridX, gridY,
           preview_valid_ ? preview_.synthAMask : 0,
           preview_valid_ && (preview_.resolvedMask & PhraseCore::kTrackSynthA),
           kPhraseAColor);
  gfx.setTextColor(kDimColor);
  gfx.drawText(gridX + 116, gridY - 1, refA);

  gfx.setTextColor(kPhraseBColor);
  gfx.drawText(x, gridY + 9, "B");
  drawMask(gfx, gridX, gridY + 10,
           preview_valid_ ? preview_.synthBMask : 0,
           preview_valid_ && (preview_.resolvedMask & PhraseCore::kTrackSynthB),
           kPhraseBColor);
  gfx.setTextColor(kDimColor);
  gfx.drawText(gridX + 116, gridY + 9, refB);

  gfx.setTextColor(kPhraseCColor);
  gfx.drawText(x, gridY + 19, "D");
  drawMask(gfx, gridX, gridY + 20,
           preview_valid_ ? preview_.drumMask : 0,
           preview_valid_ && (preview_.resolvedMask & PhraseCore::kTrackDrums),
           kPhraseCColor);
  gfx.setTextColor(kDimColor);
  gfx.drawText(gridX + 116, gridY + 19, refD);

  const int energyY = gridY + 31;
  gfx.setTextColor(IGfxColor(0x8AA4BA));
  gfx.drawText(x, energyY, "ENERGY");
  gfx.drawRect(x + 43, energyY, 82, 6, kGridOffColor);
  const int energyWidth = preview_valid_ ?
      (static_cast<int>(preview_.energy) * 80) / 255 : 0;
  if (energyWidth > 0) {
    gfx.fillRect(x + 44, energyY + 1, energyWidth, 4,
                 slotColor(selected_slot_));
  }
  gfx.setTextColor(current.valid ? COLOR_WHITE : kDimColor);
  gfx.drawText(x + 133, energyY,
               current.valid ? "REF MUTABLE" : "EMPTY SLOT");

  UI::drawStandardFooter(gfx,
                         "1-4 SLOT  UP/DN LEN  L/R BAR",
                         "ENT CAP  R ROLE  P PARENT  D/W/DEL");
}

bool PhrasePage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  const int nav = UIInput::navCode(ui_event);
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
