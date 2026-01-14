#include "project_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "../help_dialog_frames.h"

namespace {
std::string generateMemorableName() {
  static const char* adjectives[] = {
    "bright", "calm", "clear", "cosmic", "crisp", "deep", "dusty", "electric",
    "faded", "gentle", "golden", "hollow", "icy", "lunar", "neon", "noisy",
    "punchy", "quiet", "rusty", "shiny", "soft", "spicy", "sticky", "sunny",
    "sweet", "velvet", "warm", "wild", "windy", "zippy"
  };
  static const char* nouns[] = {
    "amber", "aster", "bloom", "cactus", "canyon", "cloud", "comet", "desert",
    "echo", "ember", "feather", "forest", "glow", "groove", "harbor", "horizon",
    "meadow", "meteor", "mirror", "mono", "oasis", "orchid", "polaris", "ripple",
    "river", "shadow", "signal", "sky", "spark", "voyage"
  };
  constexpr int adjCount = sizeof(adjectives) / sizeof(adjectives[0]);
  constexpr int nounCount = sizeof(nouns) / sizeof(nouns[0]);
  int adjIdx = rand() % adjCount;
  int nounIdx = rand() % nounCount;
  std::string name = adjectives[adjIdx];
  name.push_back('-');
  name += nouns[nounIdx];
  return name;
}
} // namespace

ProjectPage::ProjectPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    audio_guard_(audio_guard),
    main_focus_(MainFocus::Load),
    dialog_type_(DialogType::None),
    dialog_focus_(DialogFocus::List),
    save_dialog_focus_(SaveDialogFocus::Input),
    selection_index_(0),
    scroll_offset_(0),
    save_name_(generateMemorableName()) {
  refreshScenes();
}

void ProjectPage::refreshScenes() {
  scenes_ = mini_acid_.availableSceneNames();
  if (scenes_.empty()) {
    selection_index_ = 0;
    scroll_offset_ = 0;
    return;
  }
  if (selection_index_ < 0) selection_index_ = 0;
  int maxIdx = static_cast<int>(scenes_.size()) - 1;
  if (selection_index_ > maxIdx) selection_index_ = maxIdx;
  if (scroll_offset_ < 0) scroll_offset_ = 0;
  if (scroll_offset_ > maxIdx) scroll_offset_ = maxIdx;
}

void ProjectPage::openLoadDialog() {
  dialog_type_ = DialogType::Load;
  dialog_focus_ = DialogFocus::List;
  save_dialog_focus_ = SaveDialogFocus::Input;
  refreshScenes();
  std::string current = mini_acid_.currentSceneName();
  for (size_t i = 0; i < scenes_.size(); ++i) {
    if (scenes_[i] == current) {
      selection_index_ = static_cast<int>(i);
      break;
    }
  }
  scroll_offset_ = selection_index_;
}

void ProjectPage::openSaveDialog() {
  dialog_type_ = DialogType::SaveAs;
  save_dialog_focus_ = SaveDialogFocus::Input;
  save_name_ = mini_acid_.currentSceneName();
  if (save_name_.empty()) save_name_ = generateMemorableName();
}

void ProjectPage::closeDialog() {
  dialog_type_ = DialogType::None;
  dialog_focus_ = DialogFocus::List;
  save_dialog_focus_ = SaveDialogFocus::Input;
}

void ProjectPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) {
    audio_guard_(fn);
    return;
  }
  fn();
}

void ProjectPage::moveSelection(int delta) {
  if (scenes_.empty() || delta == 0) return;
  selection_index_ += delta;
  if (selection_index_ < 0) selection_index_ = 0;
  int maxIdx = static_cast<int>(scenes_.size()) - 1;
  if (selection_index_ > maxIdx) selection_index_ = maxIdx;
}

void ProjectPage::ensureSelectionVisible(int visibleRows) {
  if (visibleRows < 1) visibleRows = 1;
  if (scenes_.empty()) {
    scroll_offset_ = 0;
    selection_index_ = 0;
    return;
  }
  int maxIdx = static_cast<int>(scenes_.size()) - 1;
  if (selection_index_ < 0) selection_index_ = 0;
  if (selection_index_ > maxIdx) selection_index_ = maxIdx;
  if (scroll_offset_ < 0) scroll_offset_ = 0;
  if (scroll_offset_ > selection_index_) scroll_offset_ = selection_index_;
  int maxScroll = maxIdx - visibleRows + 1;
  if (maxScroll < 0) maxScroll = 0;
  if (selection_index_ >= scroll_offset_ + visibleRows) {
    scroll_offset_ = selection_index_ - visibleRows + 1;
  }
  if (scroll_offset_ > maxScroll) scroll_offset_ = maxScroll;
}

bool ProjectPage::loadSceneAtSelection() {
  if (scenes_.empty()) return true;
  if (selection_index_ < 0 || selection_index_ >= static_cast<int>(scenes_.size())) return true;
  bool loaded = false;
  std::string name = scenes_[selection_index_];
  withAudioGuard([&]() {
    loaded = mini_acid_.loadSceneByName(name);
  });
  if (loaded) closeDialog();
  return true;
}

void ProjectPage::randomizeSaveName() {
  save_name_ = generateMemorableName();
}

bool ProjectPage::saveCurrentScene() {
  if (save_name_.empty()) randomizeSaveName();
  bool saved = false;
  std::string name = save_name_;
  withAudioGuard([&]() {
    saved = mini_acid_.saveSceneAs(name);
  });
  if (saved) {
    closeDialog();
    refreshScenes();
  }
  return true;
}

bool ProjectPage::createNewScene() {
  randomizeSaveName();
  bool created = false;
  std::string name = save_name_;
  withAudioGuard([&]() {
    created = mini_acid_.createNewSceneWithName(name);
  });
  if (created) {
    refreshScenes();
  }
  return true;
}

bool ProjectPage::handleSaveDialogInput(char key) {
  if (key == '\b') {
    if (!save_name_.empty()) save_name_.pop_back();
    return true;
  }
  if (key >= 32 && key < 127) {
    char c = key;
    bool allowed = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
    if (allowed) {
      if (save_name_.size() < 32) save_name_.push_back(c);
      return true;
    }
  }
  return false;
}

bool ProjectPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;

  if (dialog_type_ == DialogType::Load) {
    switch (ui_event.scancode) {
      case MINIACID_LEFT:
        if (dialog_focus_ == DialogFocus::Cancel) {
          dialog_focus_ = DialogFocus::List;
          return true;
        }
        break;
      case MINIACID_RIGHT:
        if (dialog_focus_ == DialogFocus::List) {
          dialog_focus_ = DialogFocus::Cancel;
          return true;
        }
        break;
      case MINIACID_UP:
        if (dialog_focus_ == DialogFocus::List) {
          moveSelection(-1);
          return true;
        }
        break;
      case MINIACID_DOWN:
        if (dialog_focus_ == DialogFocus::List) {
          moveSelection(1);
          return true;
        }
        break;
      default:
        break;
    }
    char key = ui_event.key;
    if (key == '\n' || key == '\r') {
      if (dialog_focus_ == DialogFocus::Cancel) {
        closeDialog();
        return true;
      }
      return loadSceneAtSelection();
    }
    if (key == '\b') {
      closeDialog();
      return true;
    }
    return false;
  }

  if (dialog_type_ == DialogType::SaveAs) {
    switch (ui_event.scancode) {
      case MINIACID_LEFT:
        if (save_dialog_focus_ == SaveDialogFocus::Cancel) save_dialog_focus_ = SaveDialogFocus::Save;
        else if (save_dialog_focus_ == SaveDialogFocus::Save) save_dialog_focus_ = SaveDialogFocus::Randomize;
        else if (save_dialog_focus_ == SaveDialogFocus::Randomize) save_dialog_focus_ = SaveDialogFocus::Input;
        return true;
      case MINIACID_RIGHT:
        if (save_dialog_focus_ == SaveDialogFocus::Input) save_dialog_focus_ = SaveDialogFocus::Randomize;
        else if (save_dialog_focus_ == SaveDialogFocus::Randomize) save_dialog_focus_ = SaveDialogFocus::Save;
        else if (save_dialog_focus_ == SaveDialogFocus::Save) save_dialog_focus_ = SaveDialogFocus::Cancel;
        return true;
      case MINIACID_UP:
      case MINIACID_DOWN:
        if (save_dialog_focus_ == SaveDialogFocus::Input) {
          save_dialog_focus_ = SaveDialogFocus::Randomize;
        } else {
          save_dialog_focus_ = SaveDialogFocus::Input;
        }
        return true;
      default:
        break;
    }
    char key = ui_event.key;
    if (save_dialog_focus_ == SaveDialogFocus::Input && handleSaveDialogInput(key)) {
      return true;
    }
    if (key == '\n' || key == '\r') {
      if (save_dialog_focus_ == SaveDialogFocus::Randomize) {
        randomizeSaveName();
        return true;
      }
      if (save_dialog_focus_ == SaveDialogFocus::Save || save_dialog_focus_ == SaveDialogFocus::Input) {
        return saveCurrentScene();
      }
      if (save_dialog_focus_ == SaveDialogFocus::Cancel) {
        closeDialog();
        return true;
      }
    }
    if (key == '\b') {
      if (save_dialog_focus_ == SaveDialogFocus::Input) {
        return handleSaveDialogInput(key);
      }
      closeDialog();
      return true;
    }
    return false;
  }

  switch (ui_event.scancode) {
    case MINIACID_LEFT:
      if (main_focus_ == MainFocus::SaveAs) main_focus_ = MainFocus::Load;
      else if (main_focus_ == MainFocus::New) main_focus_ = MainFocus::SaveAs;
      else if (main_focus_ == MainFocus::Mode) main_focus_ = MainFocus::New;
      return true;
      if (main_focus_ == MainFocus::Load) main_focus_ = MainFocus::SaveAs;
      else if (main_focus_ == MainFocus::SaveAs) main_focus_ = MainFocus::New;
      else if (main_focus_ == MainFocus::New) main_focus_ = MainFocus::Mode;
      return true;
    case MINIACID_UP:
    case MINIACID_DOWN:
      return true;
    default:
      break;
  }

  char key = ui_event.key;
  if (key == '\n' || key == '\r') {
    if (main_focus_ == MainFocus::Load) {
      openLoadDialog();
      return true;
    } else if (main_focus_ == MainFocus::SaveAs) {
      openSaveDialog();
      return true;
    } else if (main_focus_ == MainFocus::New) {
      return createNewScene();
    } else if (main_focus_ == MainFocus::Mode) {
      mini_acid_.toggleGrooveboxMode();
      return true;
    }
  }
  return false;
}

const std::string & ProjectPage::getTitle() const {
  static std::string title = "PROJECT";
  return title;
}

void ProjectPage::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  int x = bounds.x;
  int y = bounds.y;
  int w = bounds.w;
  int h = bounds.h;

  int body_y = y + 3;
  int body_h = h - 3;
  if (body_h <= 0) return;

  int line_h = gfx.fontHeight();
  std::string currentName = mini_acid_.currentSceneName();
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, body_y, "Current Scene");
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x, body_y + line_h + 2, currentName.c_str());

  int btn_w = 70;
  if (btn_w > 90) btn_w = 90;
  if (btn_w < 60) btn_w = 60;
  int btn_h = line_h + 8;
  int btn_y = body_y + line_h * 2 + 8;
  int spacing = 6;
  const char* labels[4] = {"Load", "Save As", "New", "Acid"};
  if (mini_acid_.grooveboxMode() == GrooveboxMode::Minimal) labels[3] = "Minimal";
  
  btn_w = 48;
  spacing = 4;
  int total_w = btn_w * 4 + spacing * 3;
  int start_x = x + (w - total_w) / 2;
  
  for (int i = 0; i < 4; ++i) {
    int btn_x = start_x + i * (btn_w + spacing);
    bool focused = (dialog_type_ == DialogType::None && static_cast<int>(main_focus_) == i);
    gfx.fillRect(btn_x, btn_y, btn_w, btn_h, COLOR_PANEL);
    gfx.drawRect(btn_x, btn_y, btn_w, btn_h, focused ? COLOR_ACCENT : COLOR_LABEL);
    int btn_tw = textWidth(gfx, labels[i]);
    gfx.drawText(btn_x + (btn_w - btn_tw) / 2, btn_y + (btn_h - line_h) / 2, labels[i]);
  }

  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, btn_y + btn_h + 6, "Enter to act, arrows to move focus");
  gfx.setTextColor(COLOR_WHITE);

  if (dialog_type_ == DialogType::None) return;

  refreshScenes();

  int dialog_w = w - 16;
  if (dialog_w < 80) dialog_w = w - 4;
  if (dialog_w < 60) dialog_w = 60;
  int dialog_h = h - 16;
  if (dialog_h < 70) dialog_h = h - 4;
  if (dialog_h < 50) dialog_h = 50;
  int dialog_x = x + (w - dialog_w) / 2;
  int dialog_y = y + (h - dialog_h) / 2;

  gfx.fillRect(dialog_x, dialog_y, dialog_w, dialog_h, COLOR_DARKER);
  gfx.drawRect(dialog_x, dialog_y, dialog_w, dialog_h, COLOR_ACCENT);

  if (dialog_type_ == DialogType::Load) {
    int header_h = line_h + 4;
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(dialog_x + 4, dialog_y + 2, "Load Scene");

    int row_h = line_h + 3;
    int cancel_h = line_h + 8;
    int list_y = dialog_y + header_h + 2;
    int list_h = dialog_h - header_h - cancel_h - 10;
    if (list_h < row_h) list_h = row_h;
    int visible_rows = list_h / row_h;
    if (visible_rows < 1) visible_rows = 1;

    ensureSelectionVisible(visible_rows);

    if (scenes_.empty()) {
      gfx.setTextColor(COLOR_LABEL);
      gfx.drawText(dialog_x + 4, list_y, "No scenes found");
      gfx.setTextColor(COLOR_WHITE);
    } else {
      int rowsToDraw = visible_rows;
      if (rowsToDraw > static_cast<int>(scenes_.size()) - scroll_offset_) {
        rowsToDraw = static_cast<int>(scenes_.size()) - scroll_offset_;
      }
      for (int i = 0; i < rowsToDraw; ++i) {
        int sceneIdx = scroll_offset_ + i;
        int row_y = list_y + i * row_h;
        bool selected = sceneIdx == selection_index_;
        if (selected) {
          gfx.fillRect(dialog_x + 2, row_y, dialog_w - 4, row_h, COLOR_PANEL);
          gfx.drawRect(dialog_x + 2, row_y, dialog_w - 4, row_h, COLOR_ACCENT);
        }
        gfx.drawText(dialog_x + 6, row_y + 1, scenes_[sceneIdx].c_str());
      }
    }

    int cancel_w = 60;
    if (cancel_w > dialog_w - 8) cancel_w = dialog_w - 8;
    int cancel_x = dialog_x + dialog_w - cancel_w - 4;
    int cancel_y = dialog_y + dialog_h - cancel_h - 4;
    bool cancelFocused = dialog_focus_ == DialogFocus::Cancel;
    gfx.fillRect(cancel_x, cancel_y, cancel_w, cancel_h, COLOR_PANEL);
    gfx.drawRect(cancel_x, cancel_y, cancel_w, cancel_h, cancelFocused ? COLOR_ACCENT : COLOR_LABEL);
    const char* cancelLabel = "Cancel";
    int cancel_tw = textWidth(gfx, cancelLabel);
    gfx.drawText(cancel_x + (cancel_w - cancel_tw) / 2, cancel_y + (cancel_h - line_h) / 2, cancelLabel);
    return;
  }

  if (dialog_type_ == DialogType::SaveAs) {
    int header_h = line_h + 4;
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(dialog_x + 4, dialog_y + 2, "Save Scene As");

    int input_h = line_h + 8;
    int input_y = dialog_y + header_h + 4;
    gfx.fillRect(dialog_x + 4, input_y, dialog_w - 8, input_h, COLOR_PANEL);
    bool inputFocused = save_dialog_focus_ == SaveDialogFocus::Input;
    gfx.drawRect(dialog_x + 4, input_y, dialog_w - 8, input_h, inputFocused ? COLOR_ACCENT : COLOR_LABEL);
    gfx.drawText(dialog_x + 8, input_y + (input_h - line_h) / 2, save_name_.c_str());

    const char* btnLabels[] = {"Randomize", "Save", "Cancel"};
    SaveDialogFocus btnFocusOrder[] = {SaveDialogFocus::Randomize, SaveDialogFocus::Save, SaveDialogFocus::Cancel};
    int btnCount = 3;
    int btnAreaY = input_y + input_h + 8;
    int btnAreaH = line_h + 8;
    int btnSpacing = 6;
    int btnAreaW = dialog_w - 12;
    int btnStartX = dialog_x + 6;
    int btnWidth = (btnAreaW - btnSpacing * (btnCount - 1)) / btnCount;
    if (btnWidth < 50) btnWidth = 50;
    for (int i = 0; i < btnCount; ++i) {
      int bx = btnStartX + i * (btnWidth + btnSpacing);
      bool focused = save_dialog_focus_ == btnFocusOrder[i];
      gfx.fillRect(bx, btnAreaY, btnWidth, btnAreaH, COLOR_PANEL);
      gfx.drawRect(bx, btnAreaY, btnWidth, btnAreaH, focused ? COLOR_ACCENT : COLOR_LABEL);
      int tw = textWidth(gfx, btnLabels[i]);
      gfx.drawText(bx + (btnWidth - tw) / 2, btnAreaY + (btnAreaH - line_h) / 2, btnLabels[i]);
    }
  }
}
