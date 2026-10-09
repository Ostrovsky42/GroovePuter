// SONG -> FORM (F): an energy curve that inserts ordinary Song rows after the
// source block. Plan: docs/superpowers/plans/2026-10-08-0918-energy-curve.md.
// Included at the end of song_page.cpp.

namespace {
const char* formEnergyMeaning(uint8_t energy) {
  switch (energy) {
    case 0: return "E0 SILENCE";
    case 1: return "E1 DRUMS";
    case 2: return "E2 DRUMS+BASS";
    case 3: return "E3 +LEAD WITH SPACE";
    case 4: return "E4 FULL";
    default: return "";
  }
}

bool formRowHasMaterial(const Song& song, int row) {
  for (int t = 0; t < SongPosition::kTrackCount; ++t) {
    if (song.positions[row].patterns[t] >= 0) return true;
  }
  return false;
}
}  // namespace

// Source = the selected rows (up to 8), else the run of rows with material
// at the cursor (up to 8): normally the TAKE the cursor is on. On an empty row
// (typically just below the TAKE, or after a previous insert) the nearest TAKE
// above the cursor is used, so the curve lands after what the user sees.
void SongPage::formSourceBlock(int& first, int& count) const {
  const Song& song = mini_acid_.sceneManager().currentScene().songs[
      mini_acid_.activeSongSlot()];
  const int length = song.length < 1 ? 1 : song.length;
  if (has_selection_) {
    int minRow, maxRow, minTrack, maxTrack;
    getSelectionBounds(minRow, maxRow, minTrack, maxTrack);
    first = std::max(0, std::min(minRow, length - 1));
    count = std::max(1, std::min<int>(maxRow - first + 1, EnergyCurve::kMaxSourceRows));
    count = std::min(count, length - first);
    return;
  }
  first = std::max(0, std::min(cursorRow(), length - 1));
  count = 1;
  if (!formRowHasMaterial(song, first)) {
    int last = first;
    while (last > 0 && !formRowHasMaterial(song, last)) --last;
    if (!formRowHasMaterial(song, last)) return;  // nothing above: NoSource
    first = last;
    while (count < EnergyCurve::kMaxSourceRows && first > 0 &&
           formRowHasMaterial(song, first - 1)) {
      --first;
      ++count;
    }
    return;
  }
  while (count < EnergyCurve::kMaxSourceRows && first + count < length &&
         formRowHasMaterial(song, first + count)) {
    ++count;
  }
}

bool SongPage::applyForm() {
  int first = 0, count = 1;
  formSourceBlock(first, count);
  const int insertAt = first + count;
  EnergyCurve::Status status = EnergyCurve::Status::BadRange;
  const bool committed = commitSongMutation([&](Song& song) {
    status = EnergyCurve::apply(song, form_curve_, first, count, insertAt);
  });
  if (!committed) {
    if (status != EnergyCurve::Status::Ok) {
      showToast(EnergyCurve::statusText(status), 1200);
    }
    return true;
  }
  char toast[40];
  std::snprintf(toast, sizeof(toast), "FORM: %u BARS AT ROW %d  CTRL+Z",
                static_cast<unsigned>(EnergyCurve::totalBars(form_curve_)),
                insertAt + 1);
  showToast(toast, 1600);
  // Show the result where it now lives: the GRID, cursor on the first new row.
  form_view_ = false;
  clearSelection();
  moveCursorToRow(insertAt);
  return true;
}

bool SongPage::handleFormEvent(UIEvent& ui_event) {
  EnergyCurve::Curve& curve = form_curve_;
  if (curve.count == 0) curve = EnergyCurve::presetCurve(EnergyCurve::Preset::Wave);
  if (form_section_ >= curve.count) form_section_ = curve.count - 1;
  EnergyCurve::Section& section = curve.sections[form_section_];

  if (ui_event.scancode == GROOVEPUTER_ESCAPE || ui_event.key == 0x1B ||
      ui_event.key == '`') {
    form_view_ = false;
    return true;
  }
  if (UIInput::isLeft(ui_event)) {
    if (form_section_ > 0) --form_section_;
    return true;
  }
  if (UIInput::isRight(ui_event)) {
    if (form_section_ + 1 < curve.count) ++form_section_;
    return true;
  }
  if (UIInput::isUp(ui_event)) {
    if (section.energy < EnergyCurve::kMaxEnergy) ++section.energy;
    return true;
  }
  if (UIInput::isDown(ui_event)) {
    if (section.energy > 0) --section.energy;
    return true;
  }
  const char key = static_cast<char>(
      std::tolower(static_cast<unsigned char>(ui_event.key)));
  if (key == '\n' || key == '\r') {
    section.bars = section.bars == 4 ? 8 : 4;
    return true;
  }
  if (key == 'n') {  // a new section after this one, same energy and length
    if (curve.count >= EnergyCurve::kMaxSections) {
      showToast("8 SECTIONS MAX", 800);
      return true;
    }
    for (int i = curve.count; i > form_section_ + 1; --i) curve.sections[i] = curve.sections[i - 1];
    curve.sections[form_section_ + 1] = section;
    ++curve.count;
    ++form_section_;
    return true;
  }
  if (key == '\b' || key == 0x7F) {
    if (curve.count <= 1) return true;
    for (int i = form_section_; i + 1 < curve.count; ++i) curve.sections[i] = curve.sections[i + 1];
    --curve.count;
    if (form_section_ >= curve.count) form_section_ = curve.count - 1;
    return true;
  }
  if (key >= '1' && key <= '3') {
    const auto preset = static_cast<EnergyCurve::Preset>(key - '1');
    curve = EnergyCurve::presetCurve(preset);
    form_section_ = 0;
    showToast(EnergyCurve::presetName(preset), 600);
    return true;
  }
  if (key == 'g') return applyForm();
  // Space, page keys and the rest stay global.
  return false;
}

void SongPage::drawForm(IGfx& gfx) {
  UI::drawStandardHeader(gfx, mini_acid_, "SONG FORM");
  LayoutManager::clearContent(gfx);
  const auto palette = UI::themePalette();
  const EnergyCurve::Curve& curve = form_curve_;

  int first = 0, count = 1;
  formSourceBlock(first, count);
  char line[48];
  const Song& song = mini_acid_.sceneManager().currentScene().songs[
      mini_acid_.activeSongSlot()];
  bool anySource = false;
  for (int r = first; r < first + count; ++r) anySource = anySource || formRowHasMaterial(song, r);
  if (anySource) {
    std::snprintf(line, sizeof(line), "SRC %d-%d  INSERT AT %d  %u BARS",
                  first + 1, first + count, first + count + 1,
                  static_cast<unsigned>(EnergyCurve::totalBars(curve)));
  } else {
    std::snprintf(line, sizeof(line), "SRC --  PUT CURSOR ON A TAKE");
  }
  gfx.setTextColor(anySource ? palette.secondary : palette.warning);
  gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

  // One column per section: height = energy, width ~ bars.
  const int top = LayoutManager::lineY(1);
  const int bottom = LayoutManager::lineY(5) + Layout::LINE_HEIGHT - 2;
  const int height = std::max(8, bottom - top);
  const int left = Layout::COL_1;
  const int width = Layout::CONTENT.w - Layout::CONTENT_PAD_X * 2;
  const int unitBars = std::max<int>(1, EnergyCurve::totalBars(curve));
  int x = left;
  for (uint8_t i = 0; i < curve.count; ++i) {
    const EnergyCurve::Section& s = curve.sections[i];
    const int w = std::max(6, (width * s.bars) / unitBars);
    const bool selected = i == form_section_;
    const int barH = s.energy == 0 ? 2 : (height * s.energy) / EnergyCurve::kMaxEnergy;
    gfx.drawRect(x, top, w - 2, height, selected ? palette.focus : palette.dim);
    gfx.fillRect(x + 1, top + height - barH, w - 4, barH,
                 selected ? palette.accent : palette.secondary);
    char label[6];
    std::snprintf(label, sizeof(label), "%u", static_cast<unsigned>(s.bars));
    gfx.setTextColor(selected ? palette.text : palette.dim);
    gfx.drawText(x + 2, top + 2, label);
    x += w;
  }

  if (form_section_ < curve.count) {
    gfx.setTextColor(palette.text);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6),
                 formEnergyMeaning(curve.sections[form_section_].energy));
  }
  UI::drawStandardFooter(gfx, "[UD]ENERGY [LR]SECT ENT:4/8 G:INSERT",
                         "1-3:SHAPE N:ADD BS:DEL F:GRID");
}
