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

SamplerPage::SamplerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard)
    : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard) {
}

void SamplerPage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) {
    initComponents();
  }
}

void SamplerPage::initComponents() {
  pad_ctrl_ = std::make_shared<LabelValueComponent>("PAD:", COLOR_WHITE, COLOR_KNOB_1);
  file_ctrl_ = std::make_shared<LabelValueComponent>("SMP:", COLOR_WHITE, COLOR_KNOB_2);
  volume_ctrl_ = std::make_shared<LabelValueComponent>("VOL:", COLOR_WHITE, COLOR_KNOB_3);
  pitch_ctrl_ = std::make_shared<LabelValueComponent>("PCH:", COLOR_WHITE, COLOR_KNOB_3);
  start_ctrl_ = std::make_shared<LabelValueComponent>("STR:", COLOR_WHITE, COLOR_KNOB_4);
  end_ctrl_ = std::make_shared<LabelValueComponent>("END:", COLOR_WHITE, COLOR_KNOB_4);
  loop_ctrl_ = std::make_shared<LabelValueComponent>("LOP:", COLOR_WHITE, COLOR_KNOB_1);
  reverse_ctrl_ = std::make_shared<LabelValueComponent>("REV:", COLOR_WHITE, COLOR_KNOB_1);
  choke_ctrl_ = std::make_shared<LabelValueComponent>("CHK:", COLOR_WHITE, COLOR_KNOB_1);

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
  int h = gfx_.fontHeight() + 2; // Compact height
  int w1 = (width() - 8) / 2;
  int w_full = width() - 8;

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

  SamplerPad& p = mini_acid_.samplerTrack.pad(current_pad_);
  
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
}

void SamplerPage::adjustFocusedElement(int direction) {
  SamplerPad& p = mini_acid_.samplerTrack.pad(current_pad_);
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
      mini_acid_.sampleStore->preload(p.id);
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
        mini_acid_.samplerTrack.triggerPad(current_pad_, 1.0f, *mini_acid_.sampleStore);
    });
}

bool SamplerPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) {
    return Container::handleEvent(ui_event);
  }

  switch (ui_event.scancode) {
    case MINIACID_UP:
      focusPrev();
      return true;
    case MINIACID_DOWN:
      focusNext();
      return true;
    case MINIACID_LEFT:
      adjustFocusedElement(-1);
      return true;
    case MINIACID_RIGHT:
      adjustFocusedElement(1);
      return true;
    default:
      break;
  }

  if (ui_event.key == ' ') {
      prelisten();
      return true;
  }

  return Container::handleEvent(ui_event);
}

const std::string& SamplerPage::getTitle() const { return title_; }
