#include "motion_page.h"

#include "../ui_common.h"
#include "../ui_input.h"
#include "../ui_widgets.h"
#include "../layout_manager.h"
#include "../ui_colors.h"

#include <cstdio>
#include <algorithm>

namespace {
const char* modeLabel(uint8_t m) {
  if (m == static_cast<uint8_t>(MotionMode::Shake)) return "SHAKE";
  if (m == static_cast<uint8_t>(MotionMode::ShakeGate)) return "SHAKE G";
  return "TILT";
}

const char* axisLabel(uint8_t a) {
  return (a == static_cast<uint8_t>(MotionAxis::X)) ? "X" : "Y";
}

const char* targetLabel(uint8_t t) {
  switch (t) {
    case static_cast<uint8_t>(MotionTarget::Resonance): return "RESO";
    case static_cast<uint8_t>(MotionTarget::TextureAmount): return "TEXTURE";
    case static_cast<uint8_t>(MotionTarget::TapeWow): return "TAPE WOW";
    case static_cast<uint8_t>(MotionTarget::TapeSat): return "TAPE SAT";
    case static_cast<uint8_t>(MotionTarget::DelayMix): return "DELAY MIX";
    case static_cast<uint8_t>(MotionTarget::Cutoff):
    default: return "CUTOFF";
  }
}

const char* voiceLabel(uint8_t v) {
  switch (v) {
    case static_cast<uint8_t>(MotionVoice::A): return "A";
    case static_cast<uint8_t>(MotionVoice::B): return "B";
    default: return "AB";
  }
}

const char* curveLabel(uint8_t c) {
  switch (c) {
    case static_cast<uint8_t>(MotionCurve::Linear): return "LIN";
    case static_cast<uint8_t>(MotionCurve::Exp): return "EXP";
    default: return "SOFT";
  }
}

const char* quantLabel(uint8_t q) {
  switch (q) {
    case 1: return "1/8";
    case 2: return "1/4";
    default: return "1/16";
  }
}

void drawInlineSlider(IGfx& gfx, int x, int y, int w, int h, int value, int maxValue, bool selected) {
  if (maxValue <= 0) return;
  if (value < 0) value = 0;
  if (value > maxValue) value = maxValue;
  if (w < 8) return;
  if (h < 3) h = 3;

  const int fillW = (w * value) / maxValue;
  const IGfxColor border = selected ? COLOR_KNOB_1 : COLOR_LABEL;
  const IGfxColor fill = selected ? COLOR_KNOB_1 : COLOR_KNOB_2;

  gfx.drawRect(x, y, w, h, border);
  if (fillW > 0) {
    gfx.fillRect(x + 1, y + 1, std::max(1, fillW - 2), h - 2, fill);
  }
}
}

MotionPage::MotionPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard)
    : mini_acid_(mini_acid), audio_guard_(audio_guard) {
  (void)gfx;
}

const std::string& MotionPage::getTitle() const {
  static std::string kTitle = "MOTION";
  return kTitle;
}

void MotionPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) audio_guard_(fn);
  else fn();
}

void MotionPage::ensureRowVisible() {
  if (row_ < first_visible_row_) first_visible_row_ = row_;
  int maxVisibleTop = ROW_COUNT - kVisibleRows;
  if (maxVisibleTop < 0) maxVisibleTop = 0;
  if (row_ >= first_visible_row_ + kVisibleRows) {
    first_visible_row_ = row_ - kVisibleRows + 1;
  }
  if (first_visible_row_ < 0) first_visible_row_ = 0;
  if (first_visible_row_ > maxVisibleTop) first_visible_row_ = maxVisibleTop;
}

void MotionPage::draw(IGfx& gfx) {
  UI::drawStandardHeader(gfx, mini_acid_, "MOTION");
  LayoutManager::clearContent(gfx);

  MotionSettings& m = mini_acid_.sceneManager().currentScene().motion;
  preset_index_ = std::clamp((int)m.preset, 0, 3);

  char line[48];
  auto row = [&](int idx, const char* txt) {
    if (idx < first_visible_row_ || idx >= first_visible_row_ + kVisibleRows) return;
    int yRow = idx - first_visible_row_;
    Widgets::drawListRow(gfx, Layout::COL_1, LayoutManager::lineY(yRow), Layout::CONTENT.w - 8, txt, row_ == idx);
  };

  std::snprintf(line, sizeof(line), "Enabled   %s", m.enabled ? "ON" : "OFF");
  row(0, line);
  std::snprintf(line, sizeof(line), "Master    %s", m.masterEnable ? "ON" : "OFF");
  row(1, line);
  std::snprintf(line, sizeof(line), "Mode      %s", modeLabel(m.mode));
  row(2, line);
  std::snprintf(line, sizeof(line), "Axis      %s", axisLabel(m.axis));
  row(3, line);
  std::snprintf(line, sizeof(line), "Target    %s", targetLabel(m.target));
  row(4, line);
  std::snprintf(line, sizeof(line), "Voice     %s", voiceLabel(m.voice));
  row(5, line);
  std::snprintf(line, sizeof(line), "Depth     %d%%", m.depth);
  row(6, line);
  std::snprintf(line, sizeof(line), "Deadzone  %d%%", m.deadzone);
  row(7, line);
  std::snprintf(line, sizeof(line), "Smooth    %d%%", m.smoothing);
  row(8, line);
  std::snprintf(line, sizeof(line), "Rate      %d", m.rateLimit);
  row(9, line);
  std::snprintf(line, sizeof(line), "Threshold %d%%", m.shakeThreshold);
  row(10, line);
  std::snprintf(line, sizeof(line), "Hold      %d", m.holdSteps);
  row(11, line);
  std::snprintf(line, sizeof(line), "Quantize  %s", quantLabel(m.quantize));
  row(12, line);
  std::snprintf(line, sizeof(line), "Invert    %s", m.invert ? "ON" : "OFF");
  row(13, line);
  std::snprintf(line, sizeof(line), "Preset    %d", preset_index_ + 1);
  row(14, line);

  std::snprintf(line, sizeof(line), "Curve:%s  Preset:%d", curveLabel(m.curve), preset_index_ + 1);
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(Layout::COL_1, LayoutManager::lineY(kVisibleRows), line);

  const int sliderX = Layout::COL_1 + Layout::CONTENT.w - 54;
  const int sliderW = 48;
  const int sliderH = 6;
  if (ROW_DEPTH >= first_visible_row_ && ROW_DEPTH < first_visible_row_ + kVisibleRows) {
    drawInlineSlider(gfx, sliderX, LayoutManager::lineY(ROW_DEPTH - first_visible_row_) + 4, sliderW, sliderH, m.depth, 100, row_ == ROW_DEPTH);
  }
  if (ROW_DEADZONE >= first_visible_row_ && ROW_DEADZONE < first_visible_row_ + kVisibleRows) {
    drawInlineSlider(gfx, sliderX, LayoutManager::lineY(ROW_DEADZONE - first_visible_row_) + 4, sliderW, sliderH, m.deadzone, 50, row_ == ROW_DEADZONE);
  }
  if (ROW_SMOOTHING >= first_visible_row_ && ROW_SMOOTHING < first_visible_row_ + kVisibleRows) {
    drawInlineSlider(gfx, sliderX, LayoutManager::lineY(ROW_SMOOTHING - first_visible_row_) + 4, sliderW, sliderH, m.smoothing, 95, row_ == ROW_SMOOTHING);
  }
  if (ROW_RATE >= first_visible_row_ && ROW_RATE < first_visible_row_ + kVisibleRows) {
    drawInlineSlider(gfx, sliderX, LayoutManager::lineY(ROW_RATE - first_visible_row_) + 4, sliderW, sliderH, m.rateLimit - 1, 19, row_ == ROW_RATE);
  }
  if (ROW_THRESHOLD >= first_visible_row_ && ROW_THRESHOLD < first_visible_row_ + kVisibleRows) {
    drawInlineSlider(gfx, sliderX, LayoutManager::lineY(ROW_THRESHOLD - first_visible_row_) + 4, sliderW, sliderH, m.shakeThreshold, 100, row_ == ROW_THRESHOLD);
  }

  // Scroll marker
  if (ROW_COUNT > kVisibleRows) {
    char sb[16];
    std::snprintf(sb, sizeof(sb), "%d/%d", row_ + 1, ROW_COUNT);
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(Layout::COL_1 + Layout::CONTENT.w - 24, LayoutManager::lineY(kVisibleRows), sb);
  }

  UI::drawStandardFooter(gfx, "[TAB] next row", "[<- ->] adjust [ENT] toggle");
}

void MotionPage::applyPreset(int idx) {
  MotionSettings& m = mini_acid_.sceneManager().currentScene().motion;
  if (idx < 0) idx = 0;
  if (idx > 3) idx = 3;
  m.preset = static_cast<uint8_t>(idx);
  switch (idx) {
    case 0: // WAH TILT
      m.mode = static_cast<uint8_t>(MotionMode::Tilt);
      m.axis = static_cast<uint8_t>(MotionAxis::Y);
      m.target = static_cast<uint8_t>(MotionTarget::Cutoff);
      m.voice = static_cast<uint8_t>(MotionVoice::AB);
      m.depth = 55; m.deadzone = 10; m.smoothing = 65; m.rateLimit = 3;
      m.curve = static_cast<uint8_t>(MotionCurve::Soft);
      m.invert = false;
      break;
    case 1: // DUB HAND
      m.mode = static_cast<uint8_t>(MotionMode::Tilt);
      m.axis = static_cast<uint8_t>(MotionAxis::X);
      m.target = static_cast<uint8_t>(MotionTarget::TextureAmount);
      m.voice = static_cast<uint8_t>(MotionVoice::AB);
      m.depth = 45; m.deadzone = 14; m.smoothing = 75; m.rateLimit = 2;
      m.curve = static_cast<uint8_t>(MotionCurve::Soft);
      m.invert = true;
      break;
    case 2: // TAPE DRIFT
      m.mode = static_cast<uint8_t>(MotionMode::Tilt);
      m.axis = static_cast<uint8_t>(MotionAxis::Y);
      m.target = static_cast<uint8_t>(MotionTarget::TapeWow);
      m.voice = static_cast<uint8_t>(MotionVoice::AB);
      m.depth = 35; m.deadzone = 12; m.smoothing = 85; m.rateLimit = 1;
      m.curve = static_cast<uint8_t>(MotionCurve::Soft);
      m.invert = false;
      break;
    case 3: // SHAKE HIT
    default:
      m.mode = static_cast<uint8_t>(MotionMode::ShakeGate);
      m.axis = static_cast<uint8_t>(MotionAxis::Y);
      m.target = static_cast<uint8_t>(MotionTarget::DelayMix);
      m.voice = static_cast<uint8_t>(MotionVoice::AB);
      m.depth = 30; m.deadzone = 20; m.smoothing = 40; m.rateLimit = 4;
      m.shakeThreshold = 45; m.holdSteps = 2; m.quantize = 1;
      m.curve = static_cast<uint8_t>(MotionCurve::Linear);
      m.invert = false;
      break;
  }
}

void MotionPage::adjustRow(int delta) {
  MotionSettings& m = mini_acid_.sceneManager().currentScene().motion;
  switch (row_) {
    case ROW_ENABLED: m.enabled = !m.enabled; break;
    case ROW_MASTER: m.masterEnable = !m.masterEnable; break;
    case ROW_MODE:
      m.mode = static_cast<uint8_t>((m.mode + 3 + delta) % 3); break;
    case ROW_AXIS:
      m.axis = static_cast<uint8_t>((m.axis + 2 + delta) % 2); break;
    case ROW_TARGET: {
      int t = static_cast<int>(m.target) + delta;
      if (t < 0) t = 5; if (t > 5) t = 0;
      m.target = static_cast<uint8_t>(t);
      break;
    }
    case ROW_VOICE: {
      int v = static_cast<int>(m.voice) + delta;
      if (v < 0) v = 2; if (v > 2) v = 0;
      m.voice = static_cast<uint8_t>(v);
      break;
    }
    case ROW_DEPTH:
      m.depth = static_cast<uint8_t>(std::clamp((int)m.depth + delta * 5, 0, 100)); break;
    case ROW_DEADZONE:
      m.deadzone = static_cast<uint8_t>(std::clamp((int)m.deadzone + delta * 2, 0, 50)); break;
    case ROW_SMOOTHING:
      m.smoothing = static_cast<uint8_t>(std::clamp((int)m.smoothing + delta * 5, 0, 95)); break;
    case ROW_RATE:
      m.rateLimit = static_cast<uint8_t>(std::clamp((int)m.rateLimit + delta, 1, 20)); break;
    case ROW_THRESHOLD:
      m.shakeThreshold = static_cast<uint8_t>(std::clamp((int)m.shakeThreshold + delta * 5, 0, 100)); break;
    case ROW_HOLD:
      m.holdSteps = static_cast<uint8_t>(std::clamp((int)m.holdSteps + delta, 1, 8)); break;
    case ROW_QUANTIZE:
      m.quantize = static_cast<uint8_t>((m.quantize + 3 + delta) % 3); break;
    case ROW_INVERT:
      m.invert = !m.invert; break;
    case ROW_PRESET:
      preset_index_ = std::clamp(preset_index_ + delta, 0, 3);
      applyPreset(preset_index_);
      break;
    default:
      break;
  }
}

bool MotionPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

  int nav = UIInput::navCode(ui_event);
  if (nav == GROOVEPUTER_UP) {
    row_ = (row_ + ROW_COUNT - 1) % ROW_COUNT;
    ensureRowVisible();
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    row_ = (row_ + 1) % ROW_COUNT;
    ensureRowVisible();
    return true;
  }
  if (nav == GROOVEPUTER_LEFT) {
    withAudioGuard([&]() { adjustRow(-1); });
    return true;
  }
  if (nav == GROOVEPUTER_RIGHT) {
    withAudioGuard([&]() { adjustRow(1); });
    return true;
  }
  if (ui_event.key == '\t') {
    row_ = (row_ + 1) % ROW_COUNT;
    ensureRowVisible();
    return true;
  }
  if (ui_event.key == '\n' || ui_event.key == '\r' || ui_event.key == ' ') {
    withAudioGuard([&]() { adjustRow(1); });
    return true;
  }
  if (ui_event.key >= '1' && ui_event.key <= '4') {
    preset_index_ = ui_event.key - '1';
    withAudioGuard([&]() { applyPreset(preset_index_); });
    return true;
  }
  return false;
}
