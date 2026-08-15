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

std::string compactBrowserText(std::string value, size_t maxChars = 30) {
  if (value.size() <= maxChars) return value;
  if (maxChars <= 3) return value.substr(value.size() - maxChars);
  return std::string("...") + value.substr(value.size() - (maxChars - 3));
}

std::string normalizeDirectoryPath(std::string path) {
  while (path.size() > 1 && path.back() == '/') path.pop_back();
  return path;
}

std::string parentDirectoryPath(const std::string& path) {
  const std::string normalized = normalizeDirectoryPath(path);
  const size_t slash = normalized.find_last_of('/');
  if (slash == std::string::npos) return {};
  if (slash == 0) return "/";
  return normalized.substr(0, slash);
}

std::string pathBaseName(const std::string& path) {
  const std::string normalized = normalizeDirectoryPath(path);
  const size_t slash = normalized.find_last_of('/');
  return slash == std::string::npos ? normalized : normalized.substr(slash + 1);
}

bool pathWithinRoot(const std::string& path, const std::string& root) {
  const std::string normalizedPath = normalizeDirectoryPath(path);
  const std::string normalizedRoot = normalizeDirectoryPath(root);
  if (normalizedRoot.empty()) return false;
  if (normalizedPath == normalizedRoot) return true;
  const std::string prefix = normalizedRoot + "/";
  return normalizedPath.rfind(prefix, 0) == 0;
}

std::string relativeToRoot(const std::string& path, const std::string& root) {
  const std::string normalizedPath = normalizeDirectoryPath(path);
  const std::string normalizedRoot = normalizeDirectoryPath(root);
  if (normalizedPath == normalizedRoot) return "/";
  const std::string prefix = normalizedRoot + "/";
  if (normalizedPath.rfind(prefix, 0) == 0) {
    return normalizedPath.substr(prefix.size());
  }
  return normalizedPath;
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
    filename = compactFilename(
        relativeToRoot(file->fullPath, mini_acid_.sampleIndex.rootDirectory()));
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
  if (sample_browser_open_) drawSampleBrowser(gfx);
}

bool SamplerPage::assignIndexedSample(const SampleFileInfo& candidate) {
  if (mini_acid_.sampleStore == nullptr) {
    logSampleSelectionFailure("sample store unavailable", candidate.fullPath, 0);
    return false;
  }

  const int padIndex = current_pad_;
  const SampleId previousId = mini_acid_.samplerTrack->pad(padIndex).id;
  const auto candidateRef = SampleIndex::calculateStableRef(candidate.fullPath);
  const SampleId candidateId = mini_acid_.sampleIndex.runtimeIdForRef(candidateRef);

  if (candidateId.value == 0) {
    logSampleSelectionFailure("stable identity did not resolve",
                              candidate.fullPath, candidateId.value);
    return false;
  }
  if (candidateId == previousId) return false;

  // WAV I/O, allocation, conversion and LRU work remain outside AudioGuard.
  if (!mini_acid_.sampleStore->preload(candidateId)) {
    logSampleSelectionFailure("preload failed; previous pad assignment kept",
                              candidate.fullPath, candidateId.value);
    return false;
  }

  withAudioGuard([&]() {
    mini_acid_.samplerTrack->pad(padIndex).id = candidateId;
  });
  return true;
}

bool SamplerPage::selectIndexedSample(int direction) {
  if (mini_acid_.sampleStore == nullptr) {
    logSampleSelectionFailure("sample store unavailable", "", 0);
    return false;
  }

  const SampleId previousId = mini_acid_.samplerTrack->pad(current_pad_).id;
  const SampleFileInfo* currentFile =
      mini_acid_.sampleIndex.resolveRuntimeFile(previousId);

  const std::string& root = mini_acid_.sampleIndex.rootDirectory();
  if (currentFile != nullptr) {
    browser_dir_ = parentDirectoryPath(currentFile->fullPath);
  } else if (!pathWithinRoot(browser_dir_, root)) {
    browser_dir_ = root;
  }

  const auto files = mini_acid_.sampleIndex.filesInDirectory(browser_dir_);
  if (files.empty()) {
    logSampleSelectionFailure("no WAV files in current folder", browser_dir_, 0);
    return false;
  }

  int currentIndex = -1;
  if (currentFile != nullptr) {
    for (size_t i = 0; i < files.size(); ++i) {
      if (files[i]->fullPath == currentFile->fullPath) {
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
    const SampleFileInfo& candidate = *files[static_cast<size_t>(nextIndex)];
    if (assignIndexedSample(candidate)) return true;

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

void SamplerPage::openSampleBrowser() {
  const std::string& root = mini_acid_.sampleIndex.rootDirectory();
  if (root.empty()) {
    logSampleSelectionFailure("sample index root unavailable", "", 0);
    return;
  }

  const SampleId currentId = mini_acid_.samplerTrack->pad(current_pad_).id;
  if (const SampleFileInfo* currentFile =
          mini_acid_.sampleIndex.resolveRuntimeFile(currentId)) {
    const std::string currentDir = parentDirectoryPath(currentFile->fullPath);
    browser_dir_ = pathWithinRoot(currentDir, root) ? currentDir : root;
  } else if (!pathWithinRoot(browser_dir_, root)) {
    browser_dir_ = root;
  }

  browser_selection_ = 0;
  browser_scroll_offset_ = 0;
  refreshSampleBrowser();
  sample_browser_open_ = true;
}

void SamplerPage::closeSampleBrowser() {
  sample_browser_open_ = false;
  browser_subdirs_.clear();
  browser_files_.clear();
  browser_selection_ = 0;
  browser_scroll_offset_ = 0;
}

void SamplerPage::refreshSampleBrowser() {
  const std::string& root = mini_acid_.sampleIndex.rootDirectory();
  if (!pathWithinRoot(browser_dir_, root)) browser_dir_ = root;

  browser_subdirs_ = mini_acid_.sampleIndex.indexedSubdirectories(browser_dir_);
  browser_files_ = mini_acid_.sampleIndex.filesInDirectory(browser_dir_);

  const bool hasParent = !root.empty() && browser_dir_ != root;
  const int total = static_cast<int>(browser_subdirs_.size() + browser_files_.size()) +
                    (hasParent ? 1 : 0);
  if (total <= 0) {
    browser_selection_ = 0;
    browser_scroll_offset_ = 0;
    return;
  }

  browser_selection_ = std::clamp(browser_selection_, 0, total - 1);
  browser_scroll_offset_ = std::clamp(browser_scroll_offset_, 0, total - 1);
}

bool SamplerPage::browserGoParent() {
  const std::string& root = mini_acid_.sampleIndex.rootDirectory();
  if (root.empty() || browser_dir_ == root) return false;

  std::string parent = parentDirectoryPath(browser_dir_);
  if (!pathWithinRoot(parent, root)) parent = root;
  browser_dir_ = parent;
  browser_selection_ = 0;
  browser_scroll_offset_ = 0;
  refreshSampleBrowser();
  return true;
}

bool SamplerPage::activateSampleBrowserSelection() {
  const std::string& root = mini_acid_.sampleIndex.rootDirectory();
  const bool hasParent = !root.empty() && browser_dir_ != root;
  int index = browser_selection_;

  if (hasParent) {
    if (index == 0) return browserGoParent();
    --index;
  }

  if (index >= 0 && index < static_cast<int>(browser_subdirs_.size())) {
    browser_dir_ = browser_subdirs_[static_cast<size_t>(index)];
    browser_selection_ = 0;
    browser_scroll_offset_ = 0;
    refreshSampleBrowser();
    return true;
  }

  index -= static_cast<int>(browser_subdirs_.size());
  if (index < 0 || index >= static_cast<int>(browser_files_.size())) return false;

  const SampleFileInfo* selected = browser_files_[static_cast<size_t>(index)];
  if (selected == nullptr) return false;

  const SampleId currentId = mini_acid_.samplerTrack->pad(current_pad_).id;
  if (const SampleFileInfo* current = mini_acid_.sampleIndex.resolveRuntimeFile(currentId)) {
    if (current->fullPath == selected->fullPath) {
      closeSampleBrowser();
      return true;
    }
  }

  if (!assignIndexedSample(*selected)) return true;

  GroovePuterState::markSceneMutated();
  closeSampleBrowser();
  return true;
}

void SamplerPage::drawSampleBrowser(IGfx& gfx) {
  const int x = dx() + 4;
  const int y = dy() + 4;
  const int w = std::max(20, width() - 8);
  const int h = std::max(40, height() - 8);

  gfx.fillRect(x, y, w, h, COLOR_DARKER);
  gfx.drawRect(x, y, w, h, COLOR_ACCENT);
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x + 4, y + 3, "SAMPLE BROWSER");

  const std::string relative = relativeToRoot(
      browser_dir_, mini_acid_.sampleIndex.rootDirectory());
  const std::string pathText = compactBrowserText(
      relative == "/" ? std::string("/") : std::string("/") + relative, 30);
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x + 4, y + 13, pathText.c_str());

  const bool hasParent =
      !mini_acid_.sampleIndex.rootDirectory().empty() &&
      browser_dir_ != mini_acid_.sampleIndex.rootDirectory();
  const int total = static_cast<int>(browser_subdirs_.size() + browser_files_.size()) +
                    (hasParent ? 1 : 0);

  constexpr int rowHeight = 11;
  const int listY = y + 25;
  const int visibleRows = std::max(1, (h - 29) / rowHeight);

  if (total <= 0) {
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 4, listY, "(NO WAV FILES)");
    return;
  }

  const int start = std::clamp(browser_scroll_offset_, 0, total - 1);
  const int end = std::min(total, start + visibleRows);
  for (int displayIndex = start; displayIndex < end; ++displayIndex) {
    int logical = displayIndex;
    std::string label;

    if (hasParent) {
      if (logical == 0) {
        label = "< ..";
      } else {
        --logical;
      }
    }

    if (label.empty() && logical < static_cast<int>(browser_subdirs_.size())) {
      label = "> " + pathBaseName(browser_subdirs_[static_cast<size_t>(logical)]) + "/";
    } else if (label.empty()) {
      logical -= static_cast<int>(browser_subdirs_.size());
      if (logical >= 0 && logical < static_cast<int>(browser_files_.size())) {
        const SampleFileInfo* file = browser_files_[static_cast<size_t>(logical)];
        if (file != nullptr) label = "  " + file->filename;
      }
    }

    label = compactBrowserText(label, 30);
    const int rowY = listY + (displayIndex - start) * rowHeight;
    if (displayIndex == browser_selection_) {
      gfx.fillRect(x + 2, rowY - 1, w - 4, rowHeight, COLOR_PANEL);
      gfx.drawRect(x + 2, rowY - 1, w - 4, rowHeight, COLOR_ACCENT);
    }
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 5, rowY, label.c_str());
  }
}

bool SamplerPage::handleSampleBrowserEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return true;

  const std::string& root = mini_acid_.sampleIndex.rootDirectory();
  const bool hasParent = !root.empty() && browser_dir_ != root;
  const int total = static_cast<int>(browser_subdirs_.size() + browser_files_.size()) +
                    (hasParent ? 1 : 0);
  const int nav = UIInput::navCode(ui_event);

  if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
    if (total > 0) {
      const int delta = nav == GROOVEPUTER_UP ? -1 : 1;
      browser_selection_ = std::clamp(browser_selection_ + delta, 0, total - 1);

      constexpr int rowHeight = 11;
      const int visibleRows = std::max(1, (std::max(40, height() - 8) - 29) / rowHeight);
      if (browser_selection_ < browser_scroll_offset_) {
        browser_scroll_offset_ = browser_selection_;
      } else if (browser_selection_ >= browser_scroll_offset_ + visibleRows) {
        browser_scroll_offset_ = browser_selection_ - visibleRows + 1;
      }
    }
    return true;
  }

  if (nav == GROOVEPUTER_LEFT) {
    browserGoParent();
    return true;
  }
  if (nav == GROOVEPUTER_RIGHT) {
    activateSampleBrowserSelection();
    return true;
  }

  if (ui_event.scancode == GROOVEPUTER_ESCAPE || ui_event.key == 27) {
    closeSampleBrowser();
    return true;
  }

  if (ui_event.key == '\b' || ui_event.key == 0x7F) {
    if (!browserGoParent()) closeSampleBrowser();
    return true;
  }

  if (ui_event.key == '\n' || ui_event.key == '\r') {
    activateSampleBrowserSelection();
    return true;
  }

  return true;
}

bool SamplerPage::handleEvent(UIEvent& ui_event) {
  if (sample_browser_open_) return handleSampleBrowserEvent(ui_event);

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
    if (file_ctrl_->isFocused()) {
      openSampleBrowser();
    } else {
      prelisten();
    }
    return true;
  }

  // Space deliberately falls through. Once SAMPLES lives inside DRUMS,
  // transport keeps owning Space instead of this tab hijacking it.
  return Container::handleEvent(ui_event);
}

const std::string& SamplerPage::getTitle() const { return title_; }
