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

std::string compactFilename(std::string filename) {
  constexpr size_t kMaxChars = 26;
  if (filename.size() <= kMaxChars) return filename;
  return std::string("...") + filename.substr(filename.size() - (kMaxChars - 3));
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
      constexpr int pad = 1;
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

SamplerPage::SamplerPage(MiniAcid& mini_acid, AudioGuard audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {}

SamplerPage::SamplerPage(IGfx& gfx, MiniAcid& mini_acid,
                         AudioGuard audio_guard)
    : SamplerPage(mini_acid, audio_guard) {
  (void)gfx;
}

void SamplerPage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
  if (!initialized_) initComponents();
}

void SamplerPage::initComponents() {
  layer_ctrl_ = std::make_shared<LabelValueComponent>("LAYER:", COLOR_LABEL, COLOR_WHITE);
  pad_ctrl_ = std::make_shared<LabelValueComponent>("PAD:", COLOR_LABEL, COLOR_KNOB_1);
  file_ctrl_ = std::make_shared<LabelValueComponent>("SAMPLE:", COLOR_LABEL, COLOR_KNOB_2);
  volume_ctrl_ = std::make_shared<LabelValueComponent>("VOL:", COLOR_LABEL, COLOR_KNOB_3);
  pitch_ctrl_ = std::make_shared<LabelValueComponent>("PITCH:", COLOR_LABEL, COLOR_KNOB_3);
  start_ctrl_ = std::make_shared<LabelValueComponent>("START:", COLOR_LABEL, COLOR_KNOB_4);
  end_ctrl_ = std::make_shared<LabelValueComponent>("END:", COLOR_LABEL, COLOR_KNOB_4);
  loop_ctrl_ = std::make_shared<LabelValueComponent>("LOOP:", COLOR_LABEL, COLOR_KNOB_1);
  reverse_ctrl_ = std::make_shared<LabelValueComponent>("REV:", COLOR_LABEL, COLOR_KNOB_1);
  choke_ctrl_ = std::make_shared<LabelValueComponent>("CHOKE:", COLOR_LABEL, COLOR_KNOB_1);

  addChild(layer_ctrl_);
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
  int y = Layout::CONTENT.y + Layout::CONTENT_PAD_Y;
  constexpr int h = 10;
  const int w_full = width() - 8;
  const int w1 = (width() - 12) / 2;
  const int mid_x = x + w1 + 4;

  layer_ctrl_->setBoundaries(Rect(x, y, w_full, h));
  y += h;
  pad_ctrl_->setBoundaries(Rect(x, y, w_full, h));
  y += h;
  file_ctrl_->setBoundaries(Rect(x, y, w_full, h));
  y += h + 2;

  volume_ctrl_->setBoundaries(Rect(x, y, w1, h));
  pitch_ctrl_->setBoundaries(Rect(mid_x, y, w1, h));
  y += h;

  start_ctrl_->setBoundaries(Rect(x, y, w1, h));
  end_ctrl_->setBoundaries(Rect(mid_x, y, w1, h));
  y += h;

  loop_ctrl_->setBoundaries(Rect(x, y, w1, h));
  reverse_ctrl_->setBoundaries(Rect(mid_x, y, w1, h));
  y += h;

  choke_ctrl_->setBoundaries(Rect(x, y, w_full, h));

  initialized_ = true;
}

int SamplerPage::assignedPadCount() const {
  int count = 0;
  for (int i = 0; i < kRecoveredPadCount; ++i) {
    if (mini_acid_.samplerTrack->pad(i).id.value != 0) ++count;
  }
  return count;
}

void SamplerPage::draw(IGfx& gfx) {
  if (!initialized_) initComponents();

  const auto& p = mini_acid_.samplerTrack->pad(current_pad_);

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%s  %d/%d",
                mini_acid_.samplerTrack->isEnabled() ? "ON" : "OFF",
                assignedPadCount(), kRecoveredPadCount);
  layer_ctrl_->setValue(buf);
  pad_ctrl_->setValue(std::to_string(current_pad_ + 1));

  std::string filename = "OFF";
  if (const SampleFileInfo* file = mini_acid_.sampleIndex.resolveRuntimeFile(p.id)) {
    filename = compactFilename(file->filename);
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

  const int fileCount = static_cast<int>(files.size());
  const int step = direction < 0 ? -1 : 1;

  for (int attempt = 0; attempt < fileCount; ++attempt) {
    const SampleFileInfo& candidate = files[static_cast<size_t>(nextIndex)];
    const auto candidateRef = SampleIndex::calculateStableRef(candidate.fullPath);
    const SampleId candidateId = mini_acid_.sampleIndex.runtimeIdForRef(candidateRef);
    if (candidateId.value == 0) {
      logSampleSelectionFailure("stable identity did not resolve",
                                candidate.fullPath, candidateId.value);
    } else if (candidateId == previousId) {
      return false;
    } else {
      // WAV I/O, allocation, conversion and LRU work remain outside AudioGuard.
      if (mini_acid_.sampleStore->preload(candidateId)) {
        withAudioGuard([&]() {
          mini_acid_.samplerTrack->pad(padIndex).id = candidateId;
        });
        return true;
      }
      logSampleSelectionFailure(
          "preload failed; trying next candidate; previous pad assignment kept",
          candidate.fullPath, candidateId.value);
    }

    nextIndex = (nextIndex + step + fileCount) % fileCount;
  }

  return false;
}

bool SamplerPage::clearCurrentPad() {
  auto& p = mini_acid_.samplerTrack->pad(current_pad_);
  if (p.id.value == 0) return false;
  withAudioGuard([&]() {
    mini_acid_.samplerTrack->stopPad(current_pad_);
    p.id = SampleId{0};
  });
  GroovePuterState::markSceneMutated();
  return true;
}

void SamplerPage::toggleSampleLayer() {
  withAudioGuard([&]() {
    mini_acid_.samplerTrack->toggleEnabled();
  });
  GroovePuterState::markSceneMutated();
}

void SamplerPage::adjustFocusedElement(int direction) {
  if (layer_ctrl_->isFocused()) {
    toggleSampleLayer();
    return;
  }

  if (pad_ctrl_->isFocused()) {
    current_pad_ =
        (current_pad_ + direction + kRecoveredPadCount) % kRecoveredPadCount;
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
  if (mini_acid_.sampleStore == nullptr ||
      !mini_acid_.samplerTrack->isEnabled()) return;
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

  if (!ui_event.alt && !ui_event.ctrl && !ui_event.meta && lowerKey == 'm') {
    toggleSampleLayer();
    return true;
  }

  if ((ui_event.key == '\b' || ui_event.key == 0x7F) &&
      file_ctrl_->isFocused()) {
    clearCurrentPad();
    return true;
  }

  const char* found = std::strchr(kSequencedPadKeys, lowerKey);
  if (found != nullptr && mini_acid_.sampleStore != nullptr) {
    const int padIdx = static_cast<int>(found - kSequencedPadKeys);
    withAudioGuard([&]() {
      mini_acid_.samplerTrack->triggerPad(padIdx, 1.0f,
                                          *mini_acid_.sampleStore);
    });
    return true;
  }

  if (ui_event.key == '\n' || ui_event.key == '\r') {
    prelisten();
    return true;
  }

  // Space deliberately falls through. Once SAMPLES lives inside DRUMS,
  // transport keeps owning Space instead of this tab hijacking it.
  return Container::handleEvent(ui_event);
}

const std::string& SamplerPage::getTitle() const { return title_; }
