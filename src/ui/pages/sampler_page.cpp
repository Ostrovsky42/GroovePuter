#include "sampler_page.h"
#include "../../dsp/miniacid_engine.h"
#include "../screen_geometry.h"
#include "../ui_input.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "src/state/scene_revision.h"

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
constexpr char kSequencedPadKeys[] = "qwertyui";

void logSampleSelectionFailure(const char* reason, const std::string& path,
                               uint32_t runtimeId) {
#ifdef ARDUINO
  Serial.printf("[SAMPLER] sample assignment rejected: %s path=%s id=%u\n",
                reason ? reason : "unknown", path.c_str(),
                static_cast<unsigned>(runtimeId));
#else
  std::fprintf(stderr,
               "[SAMPLER] sample assignment rejected: %s path=%s id=%u\n",
               reason ? reason : "unknown", path.c_str(),
               static_cast<unsigned>(runtimeId));
#endif
}
}  // namespace

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
    const int label_w = textWidth(gfx, label_.c_str());
    gfx.setTextColor(value_color_);
    gfx.drawText(bounds.x + label_w + 5, bounds.y, value_.c_str());

    if (isFocused()) {
      constexpr int pad = 2;
      gfx.drawRect(bounds.x - pad, bounds.y - pad,
                   bounds.w + pad * 2, bounds.h + pad * 2, kFocusColor);
    }
  }

 private:
  std::string label_;
  std::string value_;
  IGfxColor label_color_;
  IGfxColor value_color_;
};

SamplerPage::SamplerPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard) {}

void SamplerPage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) initComponents();
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

  const int x = dx() + 4;
  // MiniAcidDisplay composites the global 16 px status header after the page.
  // Keep the first sampler control inside CONTENT so PAD and its number remain
  // visible even though SamplerPage itself receives full-screen boundaries.
  int y = Layout::CONTENT.y + Layout::CONTENT_PAD_Y;
  constexpr int h = 12;
  const int w_full = width() - 8;
  const int w1 = (width() - 8) / 2;

  pad_ctrl_->setBoundaries(Rect(x, y, w_full, h));
  y += h;
  file_ctrl_->setBoundaries(Rect(x, y, w_full, h));
  y += h + 2;

  const int mid_x = x + w1 + 4;
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

  const auto& p = mini_acid_.samplerTrack->pad(current_pad_);

  char buf[64];
  pad_ctrl_->setValue(std::to_string(current_pad_ + 1));

  std::string filename = "(empty)";
  if (const SampleFileInfo* file = mini_acid_.sampleIndex.resolveRuntimeFile(p.id)) {
    filename = file->filename;
  }
  file_ctrl_->setValue(filename);

  std::snprintf(buf, sizeof(buf), "%.2f", p.volume);
  volume_ctrl_->setValue(buf);
  std::snprintf(buf, sizeof(buf), "%.2f", p.pitch);
  pitch_ctrl_->setValue(buf);
  start_ctrl_->setValue(std::to_string(p.startFrame));
  end_ctrl_->setValue(p.endFrame == 0 ? "END" : std::to_string(p.endFrame));
  loop_ctrl_->setValue(p.loop ? "ON" : "OFF");
  reverse_ctrl_->setValue(p.reverse ? "ON" : "OFF");
  choke_ctrl_->setValue(p.chokeGroup == 0 ? "NONE" : std::to_string(p.chokeGroup));

  Container::draw(gfx);
}

bool SamplerPage::selectIndexedSample(int direction) {
  const auto& files = mini_acid_.sampleIndex.getFiles();
  if (files.empty() || mini_acid_.sampleStore == nullptr) {
    logSampleSelectionFailure(files.empty() ? "no indexed WAV files" : "sample store unavailable",
                              "", 0);
    return false;
  }

  const int padIndex = current_pad_;
  const SampleId previousId = mini_acid_.samplerTrack->pad(padIndex).id;
  const SampleFileInfo* currentFile =
      mini_acid_.sampleIndex.resolveRuntimeFile(previousId);

  int currentIndex = -1;
  if (currentFile != nullptr) {
    for (size_t i = 0; i < files.size(); ++i) {
      if (files[i].fullPath == currentFile->fullPath) {
        currentIndex = static_cast<int>(i);
        break;
      }
    }
  }

  int nextIndex = 0;
  if (currentIndex < 0) {
    nextIndex = direction < 0 ? static_cast<int>(files.size()) - 1 : 0;
  } else {
    nextIndex = (currentIndex + direction + static_cast<int>(files.size())) %
                static_cast<int>(files.size());
  }

  const SampleFileInfo& candidate = files[static_cast<size_t>(nextIndex)];
  const auto candidateRef = SampleIndex::calculateStableRef(candidate.fullPath);
  const SampleId candidateId = mini_acid_.sampleIndex.runtimeIdForRef(candidateRef);
  if (candidateId.value == 0) {
    logSampleSelectionFailure("stable identity did not resolve",
                              candidate.fullPath, candidateId.value);
    return false;
  }

  // E invariant: path lookup, WAV I/O, allocation, conversion, LRU work and
  // sample-store publication happen on the control/UI side, never while the
  // audio mutation boundary is held.
  if (!mini_acid_.sampleStore->preload(candidateId)) {
    logSampleSelectionFailure("preload failed; previous pad assignment kept",
                              candidate.fullPath, candidateId.value);
    return false;
  }

  if (candidateId == previousId) return false;

  // Only the pad identity publication is protected by the short audio guard.
  withAudioGuard([&]() {
    mini_acid_.samplerTrack->pad(padIndex).id = candidateId;
  });
  return true;
}

void SamplerPage::adjustFocusedElement(int direction) {
  if (pad_ctrl_->isFocused()) {
    withAudioGuard([&]() {
      current_pad_ =
          (current_pad_ + direction + kRecoveredPadCount) % kRecoveredPadCount;
    });
    return;
  }

  if (file_ctrl_->isFocused()) {
    if (selectIndexedSample(direction)) GroovePuterState::markSceneMutated();
    return;
  }

  auto& p = mini_acid_.samplerTrack->pad(current_pad_);
  const float beforeVolume = p.volume;
  const float beforePitch = p.pitch;
  const uint32_t beforeStart = p.startFrame;
  const uint32_t beforeEnd = p.endFrame;
  const bool beforeLoop = p.loop;
  const bool beforeReverse = p.reverse;
  const uint8_t beforeChoke = p.chokeGroup;

  withAudioGuard([&]() {
    if (volume_ctrl_->isFocused()) {
      p.volume = std::clamp(p.volume + direction * 0.05f, 0.0f, 2.0f);
    } else if (pitch_ctrl_->isFocused()) {
      p.pitch = std::clamp(p.pitch + direction * 0.05f, 0.1f, 4.0f);
    } else if (start_ctrl_->isFocused()) {
      const int64_t next = static_cast<int64_t>(p.startFrame) + direction * 500LL;
      p.startFrame = static_cast<uint32_t>(std::max<int64_t>(0, next));
    } else if (end_ctrl_->isFocused()) {
      const int64_t next = static_cast<int64_t>(p.endFrame) + direction * 500LL;
      p.endFrame = static_cast<uint32_t>(std::max<int64_t>(0, next));
    } else if (loop_ctrl_->isFocused()) {
      p.loop = !p.loop;
    } else if (reverse_ctrl_->isFocused()) {
      p.reverse = !p.reverse;
    } else if (choke_ctrl_->isFocused()) {
      p.chokeGroup = static_cast<uint8_t>((p.chokeGroup + direction + 16) % 16);
    }
  });

  const bool changed =
      p.volume != beforeVolume || p.pitch != beforePitch ||
      p.startFrame != beforeStart || p.endFrame != beforeEnd ||
      p.loop != beforeLoop || p.reverse != beforeReverse ||
      p.chokeGroup != beforeChoke;
  if (changed) GroovePuterState::markSceneMutated();
}

void SamplerPage::prelisten() {
  if (mini_acid_.sampleStore == nullptr) return;
  withAudioGuard([&]() {
    mini_acid_.samplerTrack->triggerPad(current_pad_, 1.0f,
                                        *mini_acid_.sampleStore);
  });
}

bool SamplerPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) {
    return Container::handleEvent(ui_event);
  }

  const int nav = UIInput::navCode(ui_event);
  switch (nav) {
    case GROOVEPUTER_UP:
      focusPrev();
      return true;
    case GROOVEPUTER_DOWN:
      focusNext();
      return true;
    case GROOVEPUTER_LEFT:
      adjustFocusedElement(-1);
      return true;
    case GROOVEPUTER_RIGHT:
      adjustFocusedElement(1);
      return true;
    default:
      break;
  }

  const char lowerKey = static_cast<char>(
      std::tolower(static_cast<unsigned char>(ui_event.key)));
  const char* found = std::strchr(kSequencedPadKeys, lowerKey);
  if (found != nullptr && mini_acid_.sampleStore != nullptr) {
    const int padIdx = static_cast<int>(found - kSequencedPadKeys);
    withAudioGuard([&]() {
      mini_acid_.samplerTrack->triggerPad(padIdx, 1.0f,
                                          *mini_acid_.sampleStore);
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
