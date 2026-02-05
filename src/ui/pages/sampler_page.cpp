#include "sampler_page.h"
#include <cstdio>
#include <algorithm>

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
}

class SamplerPage::LabelValueComponent : public FocusableComponent {
 public:
  LabelValueComponent(const char* label, IGfxColor label_color,
                      IGfxColor value_color)
      : label_(label ? label : ""),
        label_color_(label_color),
        value_color_(value_color) {}

  void setValue(const std::string& value) { value_ = value; }

  void draw(IGfx& gfx) override {
    const Rect& bounds = getBoundaries();
    gfx.setTextColor(label_color_);
    gfx.drawText(bounds.x, bounds.y, label_.c_str());
    int label_w = textWidth(gfx, label_.c_str());
    gfx.setTextColor(value_color_);
    gfx.drawText(bounds.x + label_w + 5, bounds.y, value_.c_str());

    if (isFocused()) {
      int pad = 2;
      gfx.drawRect(bounds.x - pad, bounds.y - pad,
                   bounds.w + pad * 2, bounds.h + pad * 2, kFocusColor);
    }
  }

 private:
  std::string label_ = "";
  std::string value_ = "";
  IGfxColor label_color_;
  IGfxColor value_color_;
};

SamplerPage::SamplerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard) {
}

void SamplerPage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) {
    initComponents();
  }
}

void SamplerPage::initComponents() {
  kit_ctrl_ = std::make_shared<LabelValueComponent>("KIT:", COLOR_WHITE, COLOR_ACCENT);
  kit_ctrl_->setValue("[LOAD]");
  
  pad_ctrl_ = std::make_shared<LabelValueComponent>("PAD:", COLOR_WHITE, COLOR_KNOB_1);
  file_ctrl_ = std::make_shared<LabelValueComponent>("SMP:", COLOR_WHITE, COLOR_KNOB_2);
  volume_ctrl_ = std::make_shared<LabelValueComponent>("VOL:", COLOR_WHITE, COLOR_KNOB_3);
  pitch_ctrl_ = std::make_shared<LabelValueComponent>("PCH:", COLOR_WHITE, COLOR_KNOB_3);
  start_ctrl_ = std::make_shared<LabelValueComponent>("STR:", COLOR_WHITE, COLOR_KNOB_4);
  end_ctrl_ = std::make_shared<LabelValueComponent>("END:", COLOR_WHITE, COLOR_KNOB_4);
  loop_ctrl_ = std::make_shared<LabelValueComponent>("LOP:", COLOR_WHITE, COLOR_KNOB_1);
  reverse_ctrl_ = std::make_shared<LabelValueComponent>("REV:", COLOR_WHITE, COLOR_KNOB_1);
  choke_ctrl_ = std::make_shared<LabelValueComponent>("CHK:", COLOR_WHITE, COLOR_KNOB_1);

  addChild(kit_ctrl_);
  addChild(pad_ctrl_);
  addChild(file_ctrl_);
  addChild(volume_ctrl_);
  addChild(pitch_ctrl_);
  addChild(start_ctrl_);
  addChild(end_ctrl_);
  addChild(loop_ctrl_);
  addChild(reverse_ctrl_);
  addChild(choke_ctrl_);

  int x = dx() + 4;
  int y = dy() + 2;
  int h = 12; 
  int w_full = width() - 8;
  int w1 = (width() - 8) / 2;

  kit_ctrl_->setBoundaries(Rect(x, y, w_full, h)); y += h + 2;
  pad_ctrl_->setBoundaries(Rect(x, y, w_full, h)); y += h;
  file_ctrl_->setBoundaries(Rect(x, y, w_full, h)); y += h + 2;
  
  // Two columns for the rest
  int mid_x = x + w1 + 4;
  volume_ctrl_->setBoundaries(Rect(x, y, w1, h)); 
  pitch_ctrl_->setBoundaries(Rect(mid_x, y, w1, h)); 
  y += h;

  start_ctrl_->setBoundaries(Rect(x, y, w1, h)); 
  end_ctrl_->setBoundaries(Rect(mid_x, y, w1, h)); 
  y += h;

  loop_ctrl_->setBoundaries(Rect(x, y, w1, h)); 
  reverse_ctrl_->setBoundaries(Rect(mid_x, y, w1, h)); 
  y += h;

  choke_ctrl_->setBoundaries(Rect(x, y, w1, h));

  initialized_ = true;
}

void SamplerPage::draw(IGfx& gfx) {
  if (!initialized_) initComponents();

  SamplerPad& p = mini_acid_.samplerTrack->pad(current_pad_);
  
  char buf[64];
  pad_ctrl_->setValue(std::to_string(current_pad_ + 1));
  
  // Find filename in index
  std::string filename = "(empty)";
  for(const auto& f : mini_acid_.sampleIndex.getFiles()) {
      if (f.id.value == p.id.value) {
          filename = f.filename;
          break;
      }
  }
  file_ctrl_->setValue(filename);
  
  snprintf(buf, sizeof(buf), "%.2f", p.volume); volume_ctrl_->setValue(buf);
  snprintf(buf, sizeof(buf), "%.2f", p.pitch); pitch_ctrl_->setValue(buf);
  start_ctrl_->setValue(std::to_string(p.startFrame));
  end_ctrl_->setValue(p.endFrame == 0 ? "END" : std::to_string(p.endFrame));
  loop_ctrl_->setValue(p.loop ? "ON" : "OFF");
  reverse_ctrl_->setValue(p.reverse ? "ON" : "OFF");
  choke_ctrl_->setValue(p.chokeGroup == 0 ? "NONE" : std::to_string(p.chokeGroup));

  Container::draw(gfx);
  
  if (dialog_type_ != DialogType::None) {
      drawDialog(gfx);
  }
}

void SamplerPage::adjustFocusedElement(int direction) {
  SamplerPad& p = mini_acid_.samplerTrack->pad(current_pad_);
  const auto& files = mini_acid_.sampleIndex.getFiles();

  audio_guard_([&]() {
    if (pad_ctrl_->isFocused()) {
      current_pad_ = (current_pad_ + direction + 16) % 16;
    } else if (file_ctrl_->isFocused()) {
      if (files.empty()) return;
      // Find current index
      int idx = -1;
      for (size_t i = 0; i < files.size(); ++i) {
          if (files[i].id.value == p.id.value) { idx = i; break; }
      }
      idx = (idx + direction + files.size()) % files.size();
      p.id = files[idx].id;
      // Auto-preload if on main thread? 
      // Actually store.preload handles it.
      // Auto-preload handled by setStore
      mini_acid_.sampleStore->preload(p.id);
    } else if (kit_ctrl_->isFocused()) {
        openLoadKitDialog();
    } else if (volume_ctrl_->isFocused()) {
      p.volume = std::clamp(p.volume + direction * 0.05f, 0.0f, 2.0f);
    } else if (pitch_ctrl_->isFocused()) {
      p.pitch = std::clamp(p.pitch + direction * 0.05f, 0.1f, 4.0f);
    } else if (start_ctrl_->isFocused()) {
      p.startFrame += direction * 500; // rough adjust
      if ((int)p.startFrame < 0) p.startFrame = 0;
    } else if (end_ctrl_->isFocused()) {
      p.endFrame += direction * 500;
      if ((int)p.endFrame < 0) p.endFrame = 0;
    } else if (loop_ctrl_->isFocused()) {
      p.loop = !p.loop;
    } else if (reverse_ctrl_->isFocused()) {
      p.reverse = !p.reverse;
    } else if (choke_ctrl_->isFocused()) {
      p.chokeGroup = (p.chokeGroup + direction + 16) % 16;
    }
  });
}

void SamplerPage::prelisten() {
    audio_guard_([&]() {
        mini_acid_.samplerTrack->triggerPad(current_pad_, 1.0f, *mini_acid_.sampleStore);
    });
}

bool SamplerPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) {
    return Container::handleEvent(ui_event);
  }

  if (dialog_type_ != DialogType::None) {
      return handleDialogEvent(ui_event);
  }

  switch (ui_event.scancode) {
    case MINIACID_UP:
      focusPrev();
      return true;
    case MINIACID_DOWN:
      focusNext();
      return true;
    case MINIACID_RIGHT:
      adjustFocusedElement(1);
      return true;
    default:
      break;
  }

  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(ui_event.key)));
  
  // Q-I triggered pads 1-8 (Standardized row)
  const char* triggerKeys = "qwertyu";
  const char* found = strchr(triggerKeys, lowerKey);
  if (found) {
    int padIdx = found - triggerKeys;
    audio_guard_([&]() {
        mini_acid_.samplerTrack->triggerPad(padIdx, 1.0f, *mini_acid_.sampleStore);
    });
    return true;
  }

  if (ui_event.key == ' ') {
      prelisten();
      return true;
  }

  return Container::handleEvent(ui_event);
}

const std::string& SamplerPage::getTitle() const { return title_; }

void SamplerPage::refreshKits() {
    kits_ = mini_acid_.sampleIndex.getSubdirectories("/bonnethead/kits");
    if (kits_.empty()) {
        // Fallback for testing or bad path
        kits_ = mini_acid_.sampleIndex.getSubdirectories("/sd/bonnethead/kits"); 
    }
    list_selection_index_ = 0;
    list_scroll_offset_ = 0;
}

void SamplerPage::openLoadKitDialog() {
    refreshKits();
    dialog_type_ = DialogType::LoadKit;
}

void SamplerPage::closeDialog() {
    dialog_type_ = DialogType::None;
}

void SamplerPage::loadKit(const std::string& kitName) {
    if (kitName.empty()) return;
    std::string path = "/bonnethead/kits/" + kitName;
    mini_acid_.sampleIndex.scanDirectory(path);
    
    // Auto map
    const auto& files = mini_acid_.sampleIndex.getFiles();
    audio_guard_([&]() {
        for (int i=0; i<16; ++i) {
            auto& pad = mini_acid_.samplerTrack->pad(i);
            // Heuristic or sequential
            pad.id.value = 0;
            if (i < (int)files.size()) {
                pad.id = files[i].id;
                // Detect specific samples for specific pads?
                // Pad 0=Kick, 1=Snare, 2=Hat, 3=OpenHat
                pad.volume = 1.0f;
                pad.pitch = 1.0f;
                pad.startFrame = 0;
                pad.endFrame = 0;
                pad.loop = false;
                pad.reverse = false;
                pad.chokeGroup = 0;
                
                mini_acid_.sampleStore->preload(pad.id);
            }
        }
    });
    
    kit_ctrl_->setValue(kitName);
    closeDialog();
}

void SamplerPage::drawDialog(IGfx& gfx) {
    int w = width() - 20;
    int h = height() - 20;
    int x = dx() + 10;
    int y = dy() + 10;
    
    gfx.fillRect(x, y, w, h, COLOR_DARKER);
    gfx.drawRect(x, y, w, h, COLOR_ACCENT);
    
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x+5, y+5, "Select Kit:");
    
    int list_y = y + 20;
    int row_h = 14;
    int rows = (h - 30) / row_h;
    
    if (kits_.empty()) {
        gfx.drawText(x+5, list_y, "(No kits found)");
        return;
    }
    
    int startIdx = list_scroll_offset_;
    int endIdx = std::min((int)kits_.size(), startIdx + rows);
    
    for (int i = startIdx; i < endIdx; ++i) {
         int ry = list_y + (i - startIdx) * row_h;
         if (i == list_selection_index_) {
             gfx.fillRect(x+2, ry, w-4, row_h, COLOR_PANEL);
             gfx.drawRect(x+2, ry, w-4, row_h, COLOR_ACCENT);
         }
         gfx.drawText(x+5, ry+2, kits_[i].c_str());
    }
}

bool SamplerPage::handleDialogEvent(UIEvent& ui_event) {
    if (ui_event.event_type == MINIACID_KEY_DOWN) {
        if (ui_event.scancode == MINIACID_UP) {
            if (list_selection_index_ > 0) {
                list_selection_index_--;
                if (list_selection_index_ < list_scroll_offset_) list_scroll_offset_ = list_selection_index_;
            }
            return true;
        }
        if (ui_event.scancode == MINIACID_DOWN) {
            if (list_selection_index_ < (int)kits_.size() - 1) {
                list_selection_index_++;
                int rows = (height() - 50) / 14; 
                if (list_selection_index_ >= list_scroll_offset_ + rows) list_scroll_offset_ = list_selection_index_ - rows + 1;
            }
            return true;
        }
        if (ui_event.key == '\n' || ui_event.key == '\r') {
            if (!kits_.empty() && list_selection_index_ >= 0 && list_selection_index_ < (int)kits_.size()) {
                loadKit(kits_[list_selection_index_]);
            } else {
                closeDialog();
            }
            return true;
        }
        if (ui_event.scancode == MINIACID_ESCAPE || ui_event.key == 'q') {
            closeDialog();
            return true;
        }
    }
    return true;
}
