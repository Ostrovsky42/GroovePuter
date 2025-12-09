#include "miniacid_display.h"
#include <algorithm>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <chrono>
#endif

static constexpr IGfxColor COLOR_WHITE = IGfxColor(0xFFFFFF);
static constexpr IGfxColor COLOR_BLACK = IGfxColor(0x000000);
static constexpr IGfxColor COLOR_GRAY = IGfxColor(0x202020);
static constexpr IGfxColor COLOR_DARKER = IGfxColor(0x101010);
static constexpr IGfxColor COLOR_WAVE = IGfxColor(0x00FF90);
static constexpr IGfxColor COLOR_PANEL = IGfxColor(0x181818);
static constexpr IGfxColor COLOR_ACCENT = IGfxColor(0xFFB000);
static constexpr IGfxColor COLOR_SLIDE = IGfxColor(0x0090FF);
static constexpr IGfxColor COLOR_303_NOTE = IGfxColor(0x00606F);
static constexpr IGfxColor COLOR_STEP_HILIGHT = IGfxColor(0xFFFFFF);
static constexpr IGfxColor COLOR_DRUM_KICK = IGfxColor(0xB03030);
static constexpr IGfxColor COLOR_DRUM_SNARE = IGfxColor(0x7090FF);
static constexpr IGfxColor COLOR_DRUM_HAT = IGfxColor(0xB0B0B0);
static constexpr IGfxColor COLOR_DRUM_OPEN_HAT = IGfxColor(0xE3C14B);
static constexpr IGfxColor COLOR_DRUM_MID_TOM = IGfxColor(0x7DC7FF);
static constexpr IGfxColor COLOR_DRUM_HIGH_TOM = IGfxColor(0x9AE3FF);
static constexpr IGfxColor COLOR_DRUM_RIM = IGfxColor(0xFF7D8D);
static constexpr IGfxColor COLOR_DRUM_CLAP = IGfxColor(0xFFC1E0);
static constexpr IGfxColor COLOR_LABEL = IGfxColor(0xCCCCCC);

static constexpr IGfxColor COLOR_MUTE_BACKGROUND = IGfxColor::Purple();

static constexpr IGfxColor COLOR_GRAY_DARKER = IGfxColor(0x202020);

static constexpr IGfxColor COLOR_KNOB_1 = IGfxColor::Orange();
static constexpr IGfxColor COLOR_KNOB_2 = IGfxColor::Cyan();
static constexpr IGfxColor COLOR_KNOB_3 = IGfxColor::Magenta();
static constexpr IGfxColor COLOR_KNOB_4 = IGfxColor::Green();

static constexpr IGfxColor COLOR_KNOB_CONTROL = IGfxColor::Yellow();
static constexpr IGfxColor COLOR_STEP_SELECTED = IGfxColor::Orange();
static constexpr IGfxColor COLOR_PATTERN_SELECTED_FILL = IGfxColor::Blue();


namespace {

unsigned long nowMillis() {
#if defined(ARDUINO)
  return millis();
#else
  using clock = std::chrono::steady_clock;
  static const auto start = clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start);
  return static_cast<unsigned long>(elapsed.count());
#endif
}

int textWidth(IGfx& gfx, const char* s) {
  if (!s) return 0;
  return gfx.textWidth(s);
}

void drawLineColored(IGfx& gfx, int x0, int y0, int x1, int y1, IGfxColor color) {
  int dx = std::abs(x1 - x0);
  int sx = x0 < x1 ? 1 : -1;
  int dy = -std::abs(y1 - y0);
  int sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    gfx.drawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void formatNoteName(int note, char* buf, size_t bufSize) {
  if (!buf || bufSize == 0) return;
  if (note < 0) {
    snprintf(buf, bufSize, "--");
    return;
  }
  static const char* names[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                "F#", "G",  "G#", "A",  "A#", "B"};
  int octave = note / 12 - 1;
  const char* name = names[note % 12];
  snprintf(buf, bufSize, "%s%d", name, octave);
}

struct Knob {
  const char* label;
  float value;
  float minValue;
  float maxValue;
  const char* unit;

  void draw(IGfx& gfx, int cx, int cy, int radius, IGfxColor ringColor,
            IGfxColor indicatorColor) const {
    float norm = 0.0f;
    if (maxValue > minValue)
      norm = (value - minValue) / (maxValue - minValue);
    norm = std::clamp(norm, 0.0f, 1.0f);

    gfx.drawKnobFace(cx, cy, radius, ringColor, COLOR_BLACK);

    constexpr float kDegToRad = 3.14159265f / 180.0f;

    float degAngle = (135.0f + norm * 270.0f);
    if (degAngle >= 360.0f)
      degAngle -= 360.0f;
    float angle = (degAngle) * kDegToRad;

    int ix = cx + static_cast<int>(roundf(cosf(angle) * (radius - 2)));
    int iy = cy + static_cast<int>(roundf(sinf(angle) * (radius - 2)));

    drawLineColored(gfx, cx, cy, ix, iy, indicatorColor);

    gfx.setTextColor(COLOR_LABEL);
    int label_x = cx - textWidth(gfx, label) / 2;
    gfx.drawText(label_x, cy + radius + 6, label);

    char buf[48];
    if (unit && unit[0]) {
      snprintf(buf, sizeof(buf), "%.0f %s", value, unit);
    } else {
      snprintf(buf, sizeof(buf), "%.2f", value);
    }
    int val_x = cx - textWidth(gfx, buf) / 2;
    gfx.drawText(val_x, cy - radius - 14, buf);
  }
};

} // namespace

MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid)
    : gfx_(gfx), mini_acid_(mini_acid) {
  splash_start_ms_ = nowMillis();
  gfx_.setFont(GfxFont::kFont5x7);
  for (int i = 0; i < NUM_303_VOICES; ++i) {
    int idx = mini_acid_.current303PatternIndex(i);
    if (idx < 0 || idx >= Bank<SynthPattern>::kPatterns) idx = 0;
    pattern_row_cursor_[i] = idx;
  }
  int drumIdx = mini_acid_.currentDrumPatternIndex();
  if (drumIdx < 0 || drumIdx >= Bank<DrumPatternSet>::kPatterns) drumIdx = 0;
  drum_pattern_cursor_ = drumIdx;
}

MiniAcidDisplay::~MiniAcidDisplay() = default;

void MiniAcidDisplay::dismissSplash() {
  splash_active_ = false;
}

void MiniAcidDisplay::nextPage() {
  page_index_ = (page_index_ + 1) % kPageCount;
}

void MiniAcidDisplay::previousPage() {
  page_index_ = (page_index_ - 1 + kPageCount) % kPageCount;
}

int MiniAcidDisplay::active303Voice() const {
  if (page_index_ == kKnobs2 || page_index_ == kPatternEditB) return 1;
  return 0;
}

bool MiniAcidDisplay::is303ControlPage() const {
  return page_index_ == kKnobs || page_index_ == kKnobs2 ||
         page_index_ == kPatternEditA || page_index_ == kPatternEditB;
}

bool MiniAcidDisplay::isPatternEditPage() const {
  return page_index_ == kPatternEditA || page_index_ == kPatternEditB;
}

int MiniAcidDisplay::activePatternVoice() const {
  return isPatternEditPage() ? active303Voice() : 0;
}

int MiniAcidDisplay::activePatternCursor() const {
  int voice = activePatternVoice();
  if (voice < 0 || voice >= NUM_303_VOICES) return 0;
  int idx = pattern_row_cursor_[voice];
  if (idx < 0) idx = 0;
  if (idx >= Bank<SynthPattern>::kPatterns)
    idx = Bank<SynthPattern>::kPatterns - 1;
  return idx;
}

void MiniAcidDisplay::setPatternCursor(int voiceIndex, int cursorIndex) {
  int idx = voiceIndex;
  if (idx < 0 || idx >= NUM_303_VOICES) return;
  int cursor = cursorIndex;
  if (cursor < 0) cursor = 0;
  if (cursor >= Bank<SynthPattern>::kPatterns)
    cursor = Bank<SynthPattern>::kPatterns - 1;
  pattern_row_cursor_[idx] = cursor;
}

int MiniAcidDisplay::activeDrumPatternCursor() const {
  int idx = drum_pattern_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= Bank<DrumPatternSet>::kPatterns)
    idx = Bank<DrumPatternSet>::kPatterns - 1;
  return idx;
}

int MiniAcidDisplay::activeDrumStep() const {
  int idx = drum_step_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= SEQ_STEPS) idx = SEQ_STEPS - 1;
  return idx;
}

int MiniAcidDisplay::activeDrumVoice() const {
  int idx = drum_voice_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= NUM_DRUM_VOICES) idx = NUM_DRUM_VOICES - 1;
  return idx;
}

void MiniAcidDisplay::setDrumPatternCursor(int cursorIndex) {
  int cursor = cursorIndex;
  if (cursor < 0) cursor = 0;
  if (cursor >= Bank<DrumPatternSet>::kPatterns)
    cursor = Bank<DrumPatternSet>::kPatterns - 1;
  drum_pattern_cursor_ = cursor;
}

void MiniAcidDisplay::moveDrumCursor(int delta) {
  if (drum_pattern_focus_) {
    int cursor = activeDrumPatternCursor();
    cursor = (cursor + delta) % Bank<DrumPatternSet>::kPatterns;
    if (cursor < 0) cursor += Bank<DrumPatternSet>::kPatterns;
    drum_pattern_cursor_ = cursor;
    return;
  }
  int step = activeDrumStep();
  step = (step + delta) % SEQ_STEPS;
  if (step < 0) step += SEQ_STEPS;
  drum_step_cursor_ = step;
}

void MiniAcidDisplay::moveDrumCursorVertical(int delta) {
  if (delta == 0) return;
  if (drum_pattern_focus_) {
    if (delta > 0) {
      drum_pattern_focus_ = false;
    }
    return;
  }

  int voice = activeDrumVoice();
  int newVoice = voice + delta;
  if (newVoice < 0 || newVoice >= NUM_DRUM_VOICES) {
    drum_pattern_focus_ = true;
    drum_pattern_cursor_ = activeDrumStep() % Bank<DrumPatternSet>::kPatterns;
    return;
  }
  drum_voice_cursor_ = newVoice;
}

int MiniAcidDisplay::activePatternStep() const {
  int voice = activePatternVoice();
  if (voice < 0 || voice >= NUM_303_VOICES) return 0;
  int idx = pattern_edit_cursor_[voice];
  if (idx < 0) idx = 0;
  if (idx >= SEQ_STEPS) idx = SEQ_STEPS - 1;
  return idx;
}

void MiniAcidDisplay::movePatternCursor(int delta) {
  int voice = activePatternVoice();
  if (voice < 0 || voice >= NUM_303_VOICES) return;
  if (pattern_focus_[voice] == PatternEditFocus::PatternRow) {
    int cursor = activePatternCursor();
    cursor = (cursor + delta) % Bank<SynthPattern>::kPatterns;
    if (cursor < 0) cursor += Bank<SynthPattern>::kPatterns;
    pattern_row_cursor_[voice] = cursor;
    return;
  }
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  col = (col + delta) % 8;
  if (col < 0) col += 8;
  pattern_edit_cursor_[voice] = row * 8 + col;
}

void MiniAcidDisplay::movePatternCursorVertical(int delta) {
  int voice = activePatternVoice();
  if (voice < 0 || voice >= NUM_303_VOICES) return;
  if (delta == 0) return;
  if (pattern_focus_[voice] == PatternEditFocus::PatternRow) {
    int col = activePatternCursor();
    int targetRow = delta > 0 ? 0 : 1;
    pattern_edit_cursor_[voice] = targetRow * 8 + col;
    pattern_focus_[voice] = PatternEditFocus::Steps;
    return;
  }
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  int newRow = row + delta;
  if (newRow < 0) {
    pattern_focus_[voice] = PatternEditFocus::PatternRow;
    setPatternCursor(voice, col);
    return;
  }
  if (newRow > 1) {
    pattern_focus_[voice] = PatternEditFocus::PatternRow;
    setPatternCursor(voice, col);
    return;
  }
  pattern_edit_cursor_[voice] = newRow * 8 + col;
}

void MiniAcidDisplay::focusPatternRow(int voiceIndex) {
  int idx = voiceIndex;
  if (idx < 0 || idx >= NUM_303_VOICES) return;
  setPatternCursor(idx, pattern_row_cursor_[idx]);
  pattern_focus_[idx] = PatternEditFocus::PatternRow;
}

void MiniAcidDisplay::focusPatternSteps(int voiceIndex) {
  int idx = voiceIndex;
  if (idx < 0 || idx >= NUM_303_VOICES) return;
  setPatternCursor(idx, pattern_row_cursor_[idx]);
  int row = pattern_edit_cursor_[idx] / 8;
  if (row < 0 || row > 1) row = 0;
  pattern_edit_cursor_[idx] = row * 8 + pattern_row_cursor_[idx];
  pattern_focus_[idx] = PatternEditFocus::Steps;
}

bool MiniAcidDisplay::patternRowFocused(int voiceIndex) const {
  int idx = voiceIndex;
  if (idx < 0 || idx >= NUM_303_VOICES) return false;
  return pattern_focus_[idx] == PatternEditFocus::PatternRow;
}

void MiniAcidDisplay::focusDrumPatternRow() {
  setDrumPatternCursor(drum_pattern_cursor_);
  drum_pattern_focus_ = true;
}

void MiniAcidDisplay::focusDrumGrid() {
  drum_pattern_focus_ = false;
  drum_step_cursor_ = activeDrumStep();
  drum_voice_cursor_ = activeDrumVoice();
}

bool MiniAcidDisplay::drumPatternRowFocused() const { return drum_pattern_focus_; }

void MiniAcidDisplay::update() {
  if (splash_active_) {
    unsigned long now = nowMillis();
    if (now - splash_start_ms_ >= 5000UL) splash_active_ = false;
  }
  if (splash_active_) {
    drawSplashScreen();
    return;
  }

  gfx_.setFont(GfxFont::kFont5x7);
  gfx_.startWrite();
  gfx_.clear(COLOR_BLACK);
  gfx_.setTextColor(COLOR_WHITE);
  char buf[128];

  auto print = [&](int x, int y, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gfx_.drawText(x, y, buf);
  };

  int margin = 4;

  int content_x = margin;
  int content_w = gfx_.width() - margin * 2;
  int content_y = margin;
  int content_h = 110;
  if (content_h < 0) content_h = 0;

  if (page_index_ == kKnobs) {
    drawKnobPage(content_x, content_y, content_w, content_h, 0);
  } else if (page_index_ == kPatternEditA) {
    drawPatternEditPage(content_x, content_y, content_w, content_h, 0);
  } else if (page_index_ == kKnobs2) {
    drawKnobPage(content_x, content_y, content_w, content_h, 1);
  } else if (page_index_ == kPatternEditB) {
    drawPatternEditPage(content_x, content_y, content_w, content_h, 1);
  } else if (page_index_ == kDrumSequencer) {
    drawDrumSequencerPage(content_x, content_y, content_w, content_h);
  } else if (page_index_ == kWaveform) {
    drawWaveform(content_x, content_y, content_w, content_h);
  } else if (page_index_ == kHelp1) {
    drawHelpPage(content_x, content_y, content_w, content_h, 1);
  } else if (page_index_ == kHelp2) {
    drawHelpPage(content_x, content_y, content_w, content_h, 2);
  } else if (page_index_ == kHelpPattern) {
    drawHelpPage(content_x, content_y, content_w, content_h, 3);
  } else if (page_index_ == kHelpDrum) {
    drawHelpPage(content_x, content_y, content_w, content_h, 4);
  }

  drawMutesSection(margin, content_h + margin, gfx_.width() - margin * 2, gfx_.height() - content_h - margin );

  int hint_w = textWidth(gfx_, "[< 0/0 >]");
  drawPageHint(gfx_.width() - hint_w - margin, margin + 2);

  gfx_.flush();
  gfx_.endWrite();
}

void MiniAcidDisplay::drawSplashScreen() {
  gfx_.startWrite();
  gfx_.clear(COLOR_BLACK);

  auto centerText = [&](int y, const char* text, IGfxColor color) {
    if (!text) return;
    int x = (gfx_.width() - textWidth(gfx_, text)) / 2;
    if (x < 0) x = 0;
    gfx_.setTextColor(color);
    gfx_.drawText(x, y, text);
  };

  gfx_.setFont(GfxFont::kFreeSerif18pt);
  int title_h = gfx_.fontHeight();
  gfx_.setFont(GfxFont::kFont5x7);
  int small_h = gfx_.fontHeight();

  int gap = 6;
  int total_h = title_h + gap + small_h * 2;
  int start_y = (gfx_.height() - total_h) / 2;
  if (start_y < 6) start_y = 6;

  gfx_.setFont(GfxFont::kFreeMono24pt);
  centerText(start_y, "MiniAcid", COLOR_ACCENT);

  gfx_.setFont(GfxFont::kFont5x7);
  int info_y = start_y + title_h + gap;
  centerText(info_y, "Use keys [ ] to move around", COLOR_WHITE);
  centerText(info_y + small_h, "Space - to start/stop sound", COLOR_WHITE);

  gfx_.flush();
  gfx_.endWrite();
}
void MiniAcidDisplay::drawMutesSection(int x, int y, int w, int h) {
  int lh = 16;
  int yy = y;

  int rect_w = w / 10;

  gfx_.setTextColor(COLOR_WHITE);

  if (!mini_acid_.is303Muted(0)) {
    gfx_.fillRect(x + rect_w * 0 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 0 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 0 + 6, yy + 6, "S1");

  if (!mini_acid_.is303Muted(1)) {
    gfx_.fillRect(x + rect_w * 1 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 1 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 1 + 6, yy + 6, "S2");

  if (!mini_acid_.isKickMuted()) {
    gfx_.fillRect(x + rect_w * 2 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 2 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 2 + 6, yy + 6, "BD");

  if (!mini_acid_.isSnareMuted()) {
    gfx_.fillRect(x + rect_w * 3 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 3 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 3 + 6, yy + 6, "SD");

  if (!mini_acid_.isHatMuted()) {
    gfx_.fillRect(x + rect_w * 4 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 4 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 4 + 6, yy + 6, "CH");

  if (!mini_acid_.isOpenHatMuted()) {
    gfx_.fillRect(x + rect_w * 5 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 5 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 5 + 6, yy + 6, "OH");

  if (!mini_acid_.isMidTomMuted()) {
    gfx_.fillRect(x + rect_w * 6 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 6 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 6 + 6, yy + 6, "MT");

  if (!mini_acid_.isHighTomMuted()) {
    gfx_.fillRect(x + rect_w * 7 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 7 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 7 + 6, yy + 6, "HT");

  if (!mini_acid_.isRimMuted()) {
    gfx_.fillRect(x + rect_w * 8 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 8 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 8 + 6, yy + 6, "RS");

  if (!mini_acid_.isClapMuted()) {
    gfx_.fillRect(x + rect_w * 9 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_MUTE_BACKGROUND);
  }
  gfx_.drawRect(x + rect_w * 9 + 1, y + 1, rect_w - 2 - 1, h - 2, COLOR_WHITE);
  gfx_.drawText(x + rect_w * 9 + 6, yy + 6, "CP");
}

int MiniAcidDisplay::drawPageTitle(int x, int y, int w, const char* text) {
  if (w <= 0 || !text) return 0;

  int transport_info_w = 60;

  w = w - transport_info_w; 

  constexpr int kTitleHeight = 11;
  constexpr int kReservedRight = 60; 


  int title_w = w;
  if (title_w > kReservedRight)
    title_w -= kReservedRight;

  gfx_.fillRect(x, y, title_w, kTitleHeight, COLOR_WHITE);

  int text_x = x + (title_w - textWidth(gfx_, text)) / 2;
  if (text_x < x) text_x = x;
  gfx_.setTextColor(COLOR_BLACK);
  gfx_.drawText(text_x, y + 1, text);
  gfx_.setTextColor(COLOR_WHITE);

  // transport info
  {
    int info_x = x + title_w + 2;
    int info_y = y + 1;
    char buf[32];
    if (mini_acid_.isPlaying()) {
      gfx_.fillRect(info_x, info_y - 1, transport_info_w - 4, kTitleHeight, IGfxColor::Blue());
    } else {
      gfx_.fillRect(info_x, info_y - 1, transport_info_w - 4, kTitleHeight, IGfxColor::Gray());
    }
    snprintf(buf, sizeof(buf), "  %0.0fbpm", mini_acid_.bpm());
    gfx_.drawText(info_x, info_y, buf);
  }
  return kTitleHeight;
}

void MiniAcidDisplay::drawWaveform(int x, int y, int w, int h) {
  int title_h = drawPageTitle(x, y, w, "WAVEFORM");

  int wave_y = y + title_h + 2;
  int wave_h = h - title_h - 2;
  if (w < 4 || wave_h < 4) return;

  int16_t samples[AUDIO_BUFFER_SAMPLES/2];
  size_t sampleCount = mini_acid_.copyLastAudio(samples, AUDIO_BUFFER_SAMPLES/2);
  int mid_y = wave_y + wave_h / 2;

  gfx_.setTextColor(IGfxColor::Orange());
  gfx_.drawLine(x, mid_y, x + w - 1, mid_y);

  if (sampleCount > 1) {
    int amplitude = wave_h / 2 - 2;
    if (amplitude < 1) amplitude = 1;
    for (int px = 0; px < w - 1; ++px) {
      size_t idx0 = (size_t)((uint64_t)px * sampleCount / w);
      size_t idx1 = (size_t)((uint64_t)(px + 1) * sampleCount / w);
      if (idx0 >= sampleCount) idx0 = sampleCount - 1;
      if (idx1 >= sampleCount) idx1 = sampleCount - 1;
      float s0 = samples[idx0] / 32768.0f;
      float s1 = samples[idx1] / 32768.0f;
      int y0 = mid_y - static_cast<int>(s0 * amplitude);
      int y1 = mid_y - static_cast<int>(s1 * amplitude);
      drawLineColored(gfx_, x + px, y0, x + px + 1, y1, COLOR_WAVE);
    }
  } 
}

void MiniAcidDisplay::drawHelpPage(int x, int y, int w, int h, int helpPage) {
  char title[16];
  snprintf(title, sizeof(title), "HELP %d", helpPage);
  int title_h = drawPageTitle(x, y, w, title);
  int body_y = y + title_h;
  int body_h = h - title_h;
  if (body_h <= 0) return;

  int col_w = w / 2 - 6;
  int left_x = x + 4;
  int right_x = x + col_w + 10;
  int left_y = body_y + 4;
  int lh = 10;
  int right_y = body_y + 4 + lh;

  auto heading = [&](int px, int py, const char* text) {
    gfx_.setTextColor(COLOR_ACCENT);
    gfx_.drawText(px, py, text);
    gfx_.setTextColor(COLOR_WHITE);
  };

  auto item = [&](int px, int py, const char* key, const char* desc, IGfxColor keyColor) {
    gfx_.setTextColor(keyColor);
    gfx_.drawText(px, py, key);
    gfx_.setTextColor(COLOR_WHITE);
    int key_w = textWidth(gfx_, key);
    gfx_.drawText(px + key_w + 6, py, desc);
  };

  if (helpPage == 1) {
    heading(left_x, left_y, "Transport");
    left_y += lh;
    item(left_x, left_y, "SPACE", "play/stop", IGfxColor::Green());
    left_y += lh;
    item(left_x, left_y, "K / L", "BPM -/+", IGfxColor::Cyan());
    left_y += lh;

    heading(left_x, left_y, "Pages");
    left_y += lh;
    item(left_x, left_y, "[ / ]", "prev/next page", COLOR_LABEL);
    left_y += lh;

    heading(left_x, left_y, "Playback");
    left_y += lh;
    item(left_x, left_y, "I / O", "303A/303B randomize", IGfxColor::Yellow());
    left_y += lh;
    item(left_x, left_y, "P", "drum randomize", IGfxColor::Yellow());
    left_y += lh;

  } else if (helpPage == 2) {
    heading(left_x, left_y, "303 (active page voice)");
    left_y += lh;
    item(left_x, left_y, "A / Z", "cutoff + / -", COLOR_KNOB_1);
    left_y += lh;
    item(left_x, left_y, "S / X", "res + / -", COLOR_KNOB_2);
    left_y += lh;
    item(left_x, left_y, "D / C", "env amt + / -", COLOR_KNOB_3);
    left_y += lh;
    item(left_x, left_y, "F / V", "decay + / -", COLOR_KNOB_4);
    left_y += lh;
    item(left_x, left_y, "M", "toggle delay", IGfxColor::Magenta());

    heading(right_x, right_y, "Mutes");
    right_y += lh;
    item(right_x, right_y, "1", "303A", IGfxColor::Orange());
    right_y += lh;
    item(right_x, right_y, "2", "303B", IGfxColor::Orange());
    right_y += lh;
    item(right_x, right_y, "3-0", "Drum Parts", IGfxColor::Orange());
    right_y += lh;

    heading(right_x, right_y, "Tips");
    right_y += lh;
    item(right_x, right_y, "K/L", "flip pages", COLOR_LABEL);
    right_y += lh;
  } else if (helpPage == 3) {
    heading(left_x, left_y, "303 Pattern Edit");
    left_y += lh;
    heading(left_x, left_y, "Navigation");
    left_y += lh;
    item(left_x, left_y, "LEFT/RIGHT", "", COLOR_LABEL);
    left_y += lh;
    item(left_x, left_y, "UP/DOWN", "move", COLOR_LABEL);
    left_y += lh;
    item(left_x, left_y, "ENTER", "Load pattern", IGfxColor::Green());
    left_y += lh;

    heading(left_x, left_y, "Pattern slots");
    left_y += lh;
    item(left_x, left_y, "Q..I", "Pick pattern", COLOR_PATTERN_SELECTED_FILL);
    left_y += lh;

    int ry = body_y + 4 + lh;
    heading(right_x, ry, "Step edits");
    ry += lh;
    item(right_x, ry, "Q", "Toggle slide", COLOR_SLIDE);
    ry += lh;
    item(right_x, ry, "W", "Toggle accent", COLOR_ACCENT);
    ry += lh;
    item(right_x, ry, "A / Z", "Note +1 / -1", COLOR_303_NOTE);
    ry += lh;
    item(right_x, ry, "S / X", "Octave + / -", COLOR_LABEL);
    ry += lh;
    item(right_x, ry, "BACK", "Clear step", IGfxColor::Red());
    ry += lh;
  } else {
    heading(left_x, left_y, "Drums Pattern Edit");
    left_y += lh;
    heading(left_x, left_y, "Navigation");
    left_y += lh;
    item(left_x, left_y, "LEFT / RIGHT", "", COLOR_LABEL);
    left_y += lh;
    item(left_x, left_y, "UP / DOWN", "move", COLOR_LABEL);
    left_y += lh;
    item(left_x, left_y, "ENTER", "Load/toggle ", IGfxColor::Green());
    left_y += lh;

    heading(left_x, left_y, "Patterns");
    left_y += lh;
    item(left_x, left_y, "Q..I", "Select drum pattern 1-8", COLOR_PATTERN_SELECTED_FILL);
    left_y += lh;
  }
}

void MiniAcidDisplay::drawKnobPage(int x, int y, int w, int h, int voiceIndex) {
  char buf[128];
  auto print = [&](int px, int py, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gfx_.drawText(px, py, buf);
  };

  const char* voiceLabel = voiceIndex == 0 ? "A" : "B";
  snprintf(buf, sizeof(buf), "ACID SYNTH %s", voiceLabel);
  drawPageTitle(x, y, w, buf);

  int center_y = y + h / 2 + 2;

  int x_margin = -10;
  int usable_w = w - x_margin * 2;


  int radius = 18;
  int spacing = usable_w / 5;
  
  gfx_.drawLine(x + x_margin, y, x + x_margin , h);
  gfx_.drawLine(x + x_margin + usable_w, y, x + x_margin + usable_w, h);
  
  int cx1 = x + x_margin + spacing * 1;
  int cx2 = x + x_margin + spacing * 2;
  int cx3 = x + x_margin + spacing * 3;
  int cx4 = x + x_margin + spacing * 4;

  const Parameter& pCut = mini_acid_.parameter303(TB303ParamId::Cutoff, voiceIndex);
  const Parameter& pRes = mini_acid_.parameter303(TB303ParamId::Resonance, voiceIndex);
  const Parameter& pEnv = mini_acid_.parameter303(TB303ParamId::EnvAmount, voiceIndex);
  const Parameter& pDec = mini_acid_.parameter303(TB303ParamId::EnvDecay, voiceIndex);

  Knob cutoff{pCut.label(), pCut.value(), pCut.min(), pCut.max(), pCut.unit()};
  Knob res{pRes.label(), pRes.value(), pRes.min(), pRes.max(), pRes.unit()};
  Knob env{pEnv.label(), pEnv.value(), pEnv.min(), pEnv.max(), pEnv.unit()};
  Knob dec{pDec.label(), pDec.value(), pDec.min(), pDec.max(), pDec.unit()};

  cutoff.draw(gfx_, cx1, center_y, radius, COLOR_KNOB_1, COLOR_KNOB_1);
  res.draw(gfx_, cx2, center_y, radius, COLOR_KNOB_2, COLOR_KNOB_2);
  env.draw(gfx_, cx3, center_y, radius, COLOR_KNOB_3, COLOR_KNOB_3);
  dec.draw(gfx_, cx4, center_y, radius, COLOR_KNOB_4, COLOR_KNOB_4);

  int delta_y_for_controls = 35;
  int delta_x_for_controls = -9;

  gfx_.setTextColor(COLOR_KNOB_CONTROL);
  print(cx1 + delta_x_for_controls, center_y + delta_y_for_controls, "A/Z");
  print(cx2 + delta_x_for_controls, center_y + delta_y_for_controls, "S/X");
  print(cx3 + delta_x_for_controls, center_y + delta_y_for_controls, "D/C");
  print(cx4 + delta_x_for_controls, center_y + delta_y_for_controls, "F/V");


  gfx_.setTextColor(COLOR_WHITE);
}

void MiniAcidDisplay::drawPatternEditPage(int x, int y, int w, int h, int voiceIndex) {
  const char* title = voiceIndex == 0 ? "303A EDIT" : "303B EDIT";
  int title_h = drawPageTitle(x, y, w, title);
  int body_y = y + title_h + 2;
  int body_h = h - title_h - 2;
  if (body_h <= 0) return;

  const int8_t* notes = mini_acid_.pattern303Steps(voiceIndex);
  const bool* accent = mini_acid_.pattern303AccentSteps(voiceIndex);
  const bool* slide = mini_acid_.pattern303SlideSteps(voiceIndex);
  int stepCursor = pattern_edit_cursor_[voiceIndex];
  int playing = mini_acid_.currentStep();
  int selectedPattern = mini_acid_.current303PatternIndex(voiceIndex);
  bool patternFocus = patternRowFocused(voiceIndex);
  bool stepFocus = !patternFocus;
  int patternCursor = activePatternCursor();

  int spacing = 4;
  int pattern_size = (w - spacing * 7 - 2) / 8;
  int pattern_size_height = pattern_size / 2;
  if (pattern_size < 12) pattern_size = 12;
  int pattern_label_h = gfx_.fontHeight();
  int pattern_row_y = body_y + pattern_label_h + 1;

  gfx_.setTextColor(COLOR_LABEL);
  gfx_.drawText(x, body_y, "PATTERNS");
  gfx_.setTextColor(COLOR_WHITE);

  for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
    int col = i % 8;
    int cell_x = x + col * (pattern_size + spacing);
    bool isCursor = patternFocus && patternCursor == i;
    gfx_.fillRect(cell_x, pattern_row_y, pattern_size, pattern_size_height, COLOR_PANEL);
    if (selectedPattern == i) {
      gfx_.fillRect(cell_x - 1, pattern_row_y - 1, pattern_size + 2, pattern_size_height + 2, COLOR_PATTERN_SELECTED_FILL);
      gfx_.drawRect(cell_x - 1, pattern_row_y - 1, pattern_size + 2, pattern_size_height + 2, COLOR_LABEL);
    }
    gfx_.drawRect(cell_x, pattern_row_y, pattern_size, pattern_size_height, COLOR_WHITE);
    if (isCursor) {
      gfx_.drawRect(cell_x - 2, pattern_row_y - 2, pattern_size + 4, pattern_size_height + 4, COLOR_STEP_SELECTED);
    }
    char label[4];
    snprintf(label, sizeof(label), "%d", i + 1);
    int tw = textWidth(gfx_, label);
    int tx = cell_x + (pattern_size - tw) / 2;
    int ty = pattern_row_y + pattern_size_height / 2 - gfx_.fontHeight() / 2;
    gfx_.drawText(tx, ty, label);
  }

  int grid_top = pattern_row_y + pattern_size_height + 6;
  int cell_size = (w - spacing * 7 - 2) / 8;
  if (cell_size < 12) cell_size = 12;
  int indicator_h = 5;
  int indicator_gap = 1;
  int row_height = indicator_h + indicator_gap + cell_size + 4;

  for (int i = 0; i < SEQ_STEPS; ++i) {
    int row = i / 8;
    int col = i % 8;
    int cell_x = x + col * (cell_size + spacing);
    int cell_y = grid_top + row * row_height;

    int indicator_w = (cell_size - 2) / 2;
    if (indicator_w < 4) indicator_w = 4;
    int slide_x = cell_x + cell_size - indicator_w;
    int indicator_y = cell_y;

    gfx_.fillRect(cell_x, indicator_y, indicator_w, indicator_h, slide[i] ? COLOR_SLIDE : COLOR_GRAY_DARKER);
    gfx_.drawRect(cell_x, indicator_y, indicator_w, indicator_h, COLOR_WHITE);
    gfx_.fillRect(slide_x, indicator_y, indicator_w, indicator_h, accent[i] ? COLOR_ACCENT : COLOR_GRAY_DARKER);
    gfx_.drawRect(slide_x, indicator_y, indicator_w, indicator_h, COLOR_WHITE);

    int note_box_y = indicator_y + indicator_h + indicator_gap;
    IGfxColor fill = notes[i] >= 0 ? COLOR_303_NOTE : COLOR_GRAY;
    gfx_.fillRect(cell_x, note_box_y, cell_size, cell_size, fill);
    gfx_.drawRect(cell_x, note_box_y, cell_size, cell_size, COLOR_WHITE);

    if (playing == i) {
      gfx_.drawRect(cell_x - 1, note_box_y - 1, cell_size + 2, cell_size + 2, COLOR_STEP_HILIGHT);
    }
    if (stepFocus && stepCursor == i) {
      gfx_.drawRect(cell_x - 2, note_box_y - 2, cell_size + 4, cell_size + 4, COLOR_STEP_SELECTED);
    }

    char note_label[8];
    formatNoteName(notes[i], note_label, sizeof(note_label));
    int tw = textWidth(gfx_, note_label);
    int tx = cell_x + (cell_size - tw) / 2;
    int ty = note_box_y + cell_size / 2 - gfx_.fontHeight() / 2;
    gfx_.drawText(tx, ty, note_label);
  }
}

void MiniAcidDisplay::drawDrumSequencerPage(int x, int y, int w, int h) {
  int title_h = drawPageTitle(x, y, w, "DRUM SEQUENCER");
  int body_y = y + title_h + 2;
  int body_h = h - title_h - 2;
  if (body_h <= 0) return;

  int pattern_label_h = gfx_.fontHeight();
  gfx_.setTextColor(COLOR_LABEL);
  gfx_.drawText(x, body_y, "PATTERN");
  gfx_.setTextColor(COLOR_WHITE);

  int spacing = 4;
  int pattern_size = (w - spacing * 7 - 2) / 8;
  if (pattern_size < 12) pattern_size = 12;
  int pattern_height = pattern_size / 2;
  int pattern_row_y = body_y + pattern_label_h + 1;

  int selectedPattern = mini_acid_.currentDrumPatternIndex();
  int patternCursor = activeDrumPatternCursor();
  bool patternFocus = drumPatternRowFocused();

  for (int i = 0; i < Bank<DrumPatternSet>::kPatterns; ++i) {
    int col = i % 8;
    int cell_x = x + col * (pattern_size + spacing);
    bool isCursor = patternFocus && patternCursor == i;
    gfx_.fillRect(cell_x, pattern_row_y, pattern_size, pattern_height, COLOR_PANEL);
    if (selectedPattern == i) {
      gfx_.fillRect(cell_x - 1, pattern_row_y - 1, pattern_size + 2, pattern_height + 2, COLOR_PATTERN_SELECTED_FILL);
      gfx_.drawRect(cell_x - 1, pattern_row_y - 1, pattern_size + 2, pattern_height + 2, COLOR_LABEL);
    }
    gfx_.drawRect(cell_x, pattern_row_y, pattern_size, pattern_height, COLOR_WHITE);
    if (isCursor) {
      gfx_.drawRect(cell_x - 2, pattern_row_y - 2, pattern_size + 4, pattern_height + 4, COLOR_STEP_SELECTED);
    }
    char label[4];
    snprintf(label, sizeof(label), "%d", i + 1);
    int tw = textWidth(gfx_, label);
    int tx = cell_x + (pattern_size - tw) / 2;
    int ty = pattern_row_y + pattern_height / 2 - gfx_.fontHeight() / 2;
    gfx_.drawText(tx, ty, label);
  }

  int grid_top = pattern_row_y + pattern_height + 6;
  int grid_h = body_h - (grid_top - body_y);
  if (grid_h <= 0) return;

  int label_w = 18;
  int grid_x = x + label_w;
  int grid_w = w - label_w;
  if (grid_w < 8) grid_w = 8;

  const char* voiceLabels[NUM_DRUM_VOICES] = {"BD", "SD", "CH", "OH", "MT", "HT", "RS", "CP"};
  int labelStripeH = grid_h / NUM_DRUM_VOICES;
  if (labelStripeH < 3) labelStripeH = 3;
  for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
    int ly = grid_top + v * labelStripeH + (labelStripeH - gfx_.fontHeight()) / 2;
    gfx_.setTextColor(COLOR_LABEL);
    gfx_.drawText(x, ly, voiceLabels[v]);
  }
  gfx_.setTextColor(COLOR_WHITE);

  int cursorStep = activeDrumStep();
  int cursorVoice = activeDrumVoice();
  bool gridFocus = !patternFocus;
  drawDrumLane(grid_x, grid_top, grid_w, grid_h, cursorStep, cursorVoice, gridFocus);
}

void MiniAcidDisplay::drawDrumLane(int x, int y, int w, int h, int cursorStep, int cursorVoice, bool gridFocus) {
  const int cell_w = w / SEQ_STEPS;
  if (cell_w < 2) return;

  const bool* kick = mini_acid_.patternKickSteps();
  const bool* snare = mini_acid_.patternSnareSteps();
  const bool* hat = mini_acid_.patternHatSteps();
  const bool* openHat = mini_acid_.patternOpenHatSteps();
  const bool* midTom = mini_acid_.patternMidTomSteps();
  const bool* highTom = mini_acid_.patternHighTomSteps();
  const bool* rim = mini_acid_.patternRimSteps();
  const bool* clap = mini_acid_.patternClapSteps();
  int highlight = mini_acid_.currentStep();

  const bool* hits[NUM_DRUM_VOICES] = {kick, snare, hat, openHat, midTom, highTom, rim, clap};
  const IGfxColor colors[NUM_DRUM_VOICES] = {COLOR_DRUM_KICK, COLOR_DRUM_SNARE, COLOR_DRUM_HAT, COLOR_DRUM_OPEN_HAT,
                                             COLOR_DRUM_MID_TOM, COLOR_DRUM_HIGH_TOM, COLOR_DRUM_RIM, COLOR_DRUM_CLAP};

  int stripe_h = h / NUM_DRUM_VOICES;
  if (stripe_h < 3) stripe_h = 3;

  for (int i = 0; i < SEQ_STEPS; ++i) {
    int cw = cell_w;
    if (cw < 2) cw = 2;
    int cx = x + i * cell_w;

    gfx_.fillRect(cx, y, cw - 1, h - 1, COLOR_DARKER);

    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
      if (!hits[v] || !hits[v][i]) continue;
      int stripe_y = y + v * stripe_h;
      int stripe_w = cw - 3;
      if (stripe_w < 1) stripe_w = 1;
      int stripe_h_clamped = stripe_h - 2;
      if (stripe_h_clamped < 1) stripe_h_clamped = 1;
      gfx_.fillRect(cx + 1, stripe_y + 1, stripe_w, stripe_h_clamped, colors[v]);
    }

    if (highlight == i)
      gfx_.drawRect(cx, y, cw - 1, h - 1, COLOR_STEP_HILIGHT);
    if (gridFocus && cursorStep == i) {
      int stripe_y = y + cursorVoice * stripe_h;
      gfx_.drawRect(cx, stripe_y, cw - 1, stripe_h - 1, COLOR_STEP_SELECTED);
    }
  }
}

void MiniAcidDisplay::drawPageHint(int x, int y) {
  char buf[32];
  snprintf(buf, sizeof(buf), "[< %d/%d >]", page_index_ + 1, kPageCount);
  gfx_.setTextColor(COLOR_LABEL);
  gfx_.drawText(x, y, buf);
  gfx_.setTextColor(COLOR_WHITE);
}
