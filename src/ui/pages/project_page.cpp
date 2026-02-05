#include "project_page.h"
#include "../ui_common.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "../layout_manager.h"
#include "../screen_geometry.h"
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

struct TapeColor {
    const char* name;
    Rgb8 rgb;
};

static const TapeColor TAPE_PALETTE[] = {
    {"Amber",    {255, 128, 0}},
    {"WarmTape", {255, 100, 50}},
    {"Violet",   {180, 100, 255}},
    {"Mint",     {100, 255, 180}},
    {"Ice",      {100, 200, 255}},
    {"Rose",     {255, 100, 150}}
};

static const char* LED_MODE_NAMES[] = {"Off", "StepTrig", "Beat", "MuteState"};
static const char* VOICE_ID_NAMES[] = {"303A", "303B", "Kick", "Snare", "HatC", "HatO", "TomM", "TomH", "Rim", "Clap"};
static const uint8_t BRI_STEPS[] = {10, 25, 40, 60, 90};
static const uint16_t FLASH_STEPS[] = {20, 40, 60, 90};

} // namespace

ProjectPage::ProjectPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
  : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard),
    main_focus_(MainFocus::Load),
    dialog_type_(DialogType::None),
    dialog_focus_(DialogFocus::List),
    save_dialog_focus_(SaveDialogFocus::Input),
    selection_index_(0),
    scroll_offset_(0),
    loadError_(false),
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
  selection_index_ = 0;
  scroll_offset_ = 0;
  loadError_ = false;
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


void ProjectPage::moveSelection(int delta) {
  loadError_ = false; // Clear error on move
  selection_index_ += delta;
  if (selection_index_ < 0) selection_index_ = 0;
  if (!scenes_.empty() && selection_index_ >= static_cast<int>(scenes_.size())) {
    selection_index_ = static_cast<int>(scenes_.size()) - 1;
  }
  ensureSelectionVisible(10); // Assuming 10 rows visible
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
  if (loaded) {
    closeDialog();
  } else {
    loadError_ = true;
  }
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

void ProjectPage::renderProject() {
    int x = gfx_.width() / 2 - 80;
    int y = gfx_.height() / 2 - 30;
    int w = 160;
    int h = 60;
    
    // Draw dialog
    gfx_.fillRect(x, y, w, h, COLOR_DARKER);
    gfx_.drawRect(x, y, w, h, COLOR_ACCENT);
    gfx_.setTextColor(COLOR_WHITE);
    const char* label = "Rendering...";
    int tw = textWidth(gfx_, label);
    gfx_.drawText(x + (w-tw)/2, y + 15, label);
    
    // Progress loop setup
    int bx = x + 10;
    int by = y + 35;
    int bw = w - 20;
    int bh = 10;
    gfx_.drawRect(bx, by, bw, bh, COLOR_LABEL);
    
    std::string filename = "/" + mini_acid_.currentSceneName() + ".wav";
    if (mini_acid_.currentSceneName().empty()) filename = "/render.wav";
    
    auto cb = [&](float progress) {
         int fillW = (int)(bw * progress);
         if (fillW > bw - 2) fillW = bw - 2;
         if (fillW > 0) {
             gfx_.fillRect(bx + 1, by + 1, fillW, bh - 2, COLOR_ACCENT);
         }
         // Note: Assuming direct draw or that UI thread isn't needed to pump display
    };
    
    bool success = false;
    withAudioGuard([&]() {
        success = mini_acid_.renderProjectToWav(filename, cb);
    });
    
    // Show result
    gfx_.fillRect(x, y, w, h, COLOR_DARKER);
    gfx_.drawRect(x, y, w, h, success ? COLOR_ACCENT : COLOR_RED);
    const char* result = success ? "Done!" : "Failed!";
    tw = textWidth(gfx_, result);
    gfx_.drawText(x + (w-tw)/2, y + 25, result);
    
    // Delay for user visibility
    unsigned long start = millis();
    while (millis() - start < 1000) { ; }
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
                if (dialog_focus_ == DialogFocus::Cancel) { dialog_focus_ = DialogFocus::List; return true; }
                break;
            case MINIACID_RIGHT:
                if (dialog_focus_ == DialogFocus::List) { dialog_focus_ = DialogFocus::Cancel; return true; }
                break;
            case MINIACID_UP:
                if (dialog_focus_ == DialogFocus::List) { moveSelection(-1); return true; }
                break;
            case MINIACID_DOWN:
                if (dialog_focus_ == DialogFocus::List) { moveSelection(1); return true; }
                break;
            default: break;
        }
        char key = ui_event.key;
        if (key == '\n' || key == '\r') {
            if (dialog_focus_ == DialogFocus::Cancel) { closeDialog(); return true; }
            return loadSceneAtSelection();
        }
        if (key == '\b') { closeDialog(); return true; }
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
                if (save_dialog_focus_ == SaveDialogFocus::Input) save_dialog_focus_ = SaveDialogFocus::Randomize;
                else save_dialog_focus_ = SaveDialogFocus::Input;
                return true;
            default: break;
        }
        char key = ui_event.key;
        if (save_dialog_focus_ == SaveDialogFocus::Input && handleSaveDialogInput(key)) return true;
        if (key == '\n' || key == '\r') {
            if (save_dialog_focus_ == SaveDialogFocus::Randomize) { randomizeSaveName(); return true; }
            if (save_dialog_focus_ == SaveDialogFocus::Save || save_dialog_focus_ == SaveDialogFocus::Input) return saveCurrentScene();
            if (save_dialog_focus_ == SaveDialogFocus::Cancel) { closeDialog(); return true; }
        }
        if (key == '\b') {
            if (save_dialog_focus_ == SaveDialogFocus::Input) return handleSaveDialogInput(key);
            closeDialog(); return true;
        }
        return false;
    }

    switch (ui_event.scancode) {
        case MINIACID_LEFT:
            if (main_focus_ == MainFocus::Volume) { mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, -1); return true; }
            if (main_focus_ == MainFocus::SaveAs) main_focus_ = MainFocus::Load;
            else if (main_focus_ == MainFocus::New) main_focus_ = MainFocus::SaveAs;
            else if (main_focus_ == MainFocus::Render) main_focus_ = MainFocus::New;
            else if (main_focus_ == MainFocus::Mode) main_focus_ = MainFocus::Render;
            else if (main_focus_ == MainFocus::VisualStyle) main_focus_ = MainFocus::Mode;
            else if (main_focus_ >= MainFocus::LedMode && main_focus_ <= MainFocus::LedFlash) {
                if (main_focus_ == MainFocus::LedMode) main_focus_ = MainFocus::VisualStyle;
                else main_focus_ = static_cast<MainFocus>(static_cast<int>(main_focus_) - 1);
            }
            return true;
        case MINIACID_RIGHT:
            if (main_focus_ == MainFocus::Volume) { mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, 1); return true; }
            if (main_focus_ == MainFocus::Load) main_focus_ = MainFocus::SaveAs;
            else if (main_focus_ == MainFocus::SaveAs) main_focus_ = MainFocus::New;
            else if (main_focus_ == MainFocus::New) main_focus_ = MainFocus::Render;
            else if (main_focus_ == MainFocus::Render) main_focus_ = MainFocus::Mode;
            else if (main_focus_ == MainFocus::Mode) main_focus_ = MainFocus::VisualStyle;
            else if (main_focus_ == MainFocus::VisualStyle) main_focus_ = MainFocus::LedMode;
            else if (main_focus_ >= MainFocus::LedMode && main_focus_ < MainFocus::LedFlash) {
                main_focus_ = static_cast<MainFocus>(static_cast<int>(main_focus_) + 1);
            }
            return true;
        case MINIACID_UP:
            if (main_focus_ == MainFocus::Volume) { main_focus_ = MainFocus::LedMode; return true; }
            if (main_focus_ >= MainFocus::LedMode && main_focus_ <= MainFocus::LedFlash) { main_focus_ = MainFocus::Load; return true; }
            break;
        case MINIACID_DOWN:
            if (main_focus_ <= MainFocus::VisualStyle) { main_focus_ = MainFocus::LedMode; return true; }
            if (main_focus_ >= MainFocus::LedMode && main_focus_ <= MainFocus::LedFlash) { main_focus_ = MainFocus::Volume; return true; }
            break;
        default: break;
    }

    char key = ui_event.key;
    if (key == '\n' || key == '\r') {
        if (main_focus_ == MainFocus::Load) { openLoadDialog(); return true; }
        if (main_focus_ == MainFocus::SaveAs) { openSaveDialog(); return true; }
        if (main_focus_ == MainFocus::New) return createNewScene();
        if (main_focus_ == MainFocus::Render) { renderProject(); return true; }
        if (main_focus_ == MainFocus::Mode) { mini_acid_.toggleGrooveboxMode(); return true; }
        if (main_focus_ == MainFocus::VisualStyle) {
             switch (UI::currentStyle) {
                 case VisualStyle::MINIMAL: UI::currentStyle = VisualStyle::RETRO_CLASSIC; break;
                 case VisualStyle::RETRO_CLASSIC: UI::currentStyle = VisualStyle::AMBER; break;
                 case VisualStyle::AMBER: UI::currentStyle = VisualStyle::MINIMAL; break;
                 default: UI::currentStyle = VisualStyle::MINIMAL; break;
             }
             return true;
        }
        
        auto& led = mini_acid_.sceneManager().currentScene().led;
        if (main_focus_ == MainFocus::LedMode) { led.mode = static_cast<LedMode>((static_cast<int>(led.mode) + 1) % 4); return true; }
        if (main_focus_ == MainFocus::LedSource) {
            led.source = static_cast<LedSource>((static_cast<int>(led.source) + 1) % static_cast<int>(VoiceId::Count));
            switch (led.source) {
                case LedSource::SynthA: led.color = TAPE_PALETTE[1].rgb; break;
                case LedSource::SynthB: led.color = TAPE_PALETTE[2].rgb; break;
                case LedSource::DrumKick: led.color = TAPE_PALETTE[0].rgb; break;
                case LedSource::DrumSnare: led.color = TAPE_PALETTE[3].rgb; break;
                case LedSource::DrumClap: led.color = TAPE_PALETTE[5].rgb; break;
                default: led.color = TAPE_PALETTE[4].rgb; break;
            }
            return true;
        }
        if (main_focus_ == MainFocus::LedColor) {
            int currentIdx = 0;
            for (int i=0; i<6; ++i) if (TAPE_PALETTE[i].rgb.r == led.color.r && TAPE_PALETTE[i].rgb.g == led.color.g) currentIdx = i;
            led.color = TAPE_PALETTE[(currentIdx + 1) % 6].rgb;
            return true;
        }
        if (main_focus_ == MainFocus::LedBri) {
            int currentIdx = 0;
            for (int i=0; i<5; ++i) if (BRI_STEPS[i] == led.brightness) currentIdx = i;
            led.brightness = BRI_STEPS[(currentIdx + 1) % 5];
            return true;
        }
        if (main_focus_ == MainFocus::LedFlash) {
            int currentIdx = 0;
            for (int i=0; i<4; ++i) if (FLASH_STEPS[i] == led.flashMs) currentIdx = i;
            led.flashMs = FLASH_STEPS[(currentIdx + 1) % 4];
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
  UI::drawStandardHeader(gfx, mini_acid_, "PROJECT");
  LayoutManager::clearContent(gfx);
  
  // Use Layout::CONTENT for proper positioning
  int x = Layout::CONTENT.x + Layout::CONTENT_PAD_X;
  int y = Layout::CONTENT.y + Layout::CONTENT_PAD_Y;
  int w = Layout::CONTENT.w - 2 * Layout::CONTENT_PAD_X;
  int h = Layout::CONTENT.h - 2 * Layout::CONTENT_PAD_Y;

  int body_y = y;
  int body_h = h;
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
  int btn_h = line_h + 6;  // Reduced from +8 to +6
  int btn_y = body_y + line_h * 2 + 8;
  int spacing = 3;
  const char* labels[6] = {"Load", "Save As", "New", "Render", "Acid", "Theme"};
  if (mini_acid_.grooveboxMode() == GrooveboxMode::Minimal) labels[4] = "Minimal";
  if (UI::currentStyle == VisualStyle::MINIMAL) labels[5] = "Carb";
  else if (UI::currentStyle == VisualStyle::RETRO_CLASSIC) labels[5] = "Cyb";
  else labels[5] = "Amb";
  
  btn_w = 42;
  // Increase spacing if needed, or reduce width? 6 buttons * 42 = 252 + spacing. Screens are usually 320 or 240. Cardputer is 240x135.
  // 6 * 38 = 228. Fit tight.
  btn_w = 36; 
  spacing = 2;
  int total_w = btn_w * 6 + spacing * 5;
  int start_x = x + (w - total_w) / 2;
  
  for (int i = 0; i < 6; ++i) {
    int btn_x = start_x + i * (btn_w + spacing);
    bool focused = (dialog_type_ == DialogType::None && static_cast<int>(main_focus_) == i);
    gfx.fillRect(btn_x, btn_y, btn_w, btn_h, COLOR_PANEL);
    gfx.drawRect(btn_x, btn_y, btn_w, btn_h, focused ? COLOR_ACCENT : COLOR_LABEL);
    int btn_tw = textWidth(gfx, labels[i]);
    gfx.drawText(btn_x + (btn_w - btn_tw) / 2, btn_y + (btn_h - line_h) / 2, labels[i]);
  }

  // LED Section
  int led_y = btn_y + btn_h + 10;
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, led_y, "LED SETTINGS");
  
  int led_ctrl_y = led_y + line_h + 4;
  int led_btn_w = 38;
  int led_spacing = 2;
  
  auto& led = mini_acid_.sceneManager().currentScene().led;
  char briBuf[8]; snprintf(briBuf, sizeof(briBuf), "%d%%", led.brightness);
  char flsBuf[8]; snprintf(flsBuf, sizeof(flsBuf), "%dm", led.flashMs);
  
  const char* ledLabels[5] = { LED_MODE_NAMES[static_cast<int>(led.mode)], 
                               VOICE_ID_NAMES[static_cast<int>(led.source)],
                               "Color", briBuf, flsBuf };
                               
  for (int i=0; i<5; ++i) {
      int bx = start_x + (i * (led_btn_w + led_spacing));
      MainFocus focus = static_cast<MainFocus>(static_cast<int>(MainFocus::LedMode) + i);
      bool focused = (dialog_type_ == DialogType::None && main_focus_ == focus);
      
      gfx.fillRect(bx, led_ctrl_y, led_btn_w, btn_h, COLOR_PANEL);
      
      IGfxColor btnColor = COLOR_LABEL;
      if (focused) btnColor = COLOR_ACCENT;
      else if (i == 2) {
          btnColor = IGfxColor((led.color.r << 16) | (led.color.g << 8) | led.color.b);
      }
      gfx.drawRect(bx, led_ctrl_y, led_btn_w, btn_h, btnColor);
      
      int tw = textWidth(gfx, ledLabels[i]);
      gfx.drawText(bx + (led_btn_w - tw) / 2, led_ctrl_y + (btn_h - line_h) / 2, ledLabels[i]);
  }

  // Volume Slider
  int vol_y = led_ctrl_y + btn_h + 8;
  int vol_h = 10;
  int vol_label_w = 30;
  int track_x = start_x + vol_label_w + 5;
  int track_w = total_w - vol_label_w - 5;
  
  bool volFocused = (dialog_type_ == DialogType::None && static_cast<int>(main_focus_) == static_cast<int>(MainFocus::Volume));
  
  gfx.setTextColor(volFocused ? COLOR_ACCENT : COLOR_LABEL);
  gfx.drawText(start_x, vol_y + (vol_h - line_h)/2, "Vol:");
  
  gfx.drawRect(track_x, vol_y, track_w, vol_h, volFocused ? COLOR_ACCENT : COLOR_DARKER);
  float volVal = mini_acid_.miniParameter(MiniAcidParamId::MainVolume).value();
  int fill_w = (int)(track_w * volVal);
  if (fill_w > track_w - 2) fill_w = track_w - 2;
  if (fill_w > 0) {
      gfx.fillRect(track_x + 1, vol_y + 1, fill_w, vol_h - 2, volFocused ? COLOR_ACCENT : COLOR_GRAY);
  }

  // Warning
  //gfx.setTextColor(COLOR_ACCENT);
 //gfx.drawText(x, vol_y + vol_h + 4, "NOTE: RGB needs Screen BL=100%");

  // v1.1  Footer
  UI::drawStandardFooter(gfx, "[ARROWS]NAV [ENT]SELECT", "[M]MODE");

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
    if (loadError_) {
      gfx.setTextColor(COLOR_ACCENT);
      gfx.drawText(dialog_x + dialog_w - 70, dialog_y + 2, "LOAD FAILED");
    }

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
