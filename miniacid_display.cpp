#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <string>
#include <vector>
#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <chrono>
#endif

#include "miniacid_display.h"

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
static constexpr IGfxColor WAVE_COLORS[] = {
  COLOR_WAVE,
  IGfxColor::Cyan(),
  IGfxColor::Magenta(),
  IGfxColor::Yellow(),
  IGfxColor::White(),
};
static constexpr int NUM_WAVE_COLORS = sizeof(WAVE_COLORS) / sizeof(WAVE_COLORS[0]);


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


HelpPage::HelpPage(PageHelper& page_helper) : 
  page_helper_(page_helper), 
  help_page_index_(0),
  total_help_pages_(6)
{}

void HelpPage::drawHelpPage(IGfx& gfx,int x, int y, int w, int h, int helpPage) {
  int body_y = y;
  int body_h = h;
  if (body_h <= 0) return;

  int col_w = w / 2 - 6;
  int left_x = x + 4;
  int right_x = x + col_w + 10;
  int left_y = body_y + 4;
  int lh = 10;
  int right_y = body_y + 4 + lh;

  auto heading = [&](int px, int py, const char* text) {
    gfx.setTextColor(COLOR_ACCENT);
    gfx.drawText(px, py, text);
    gfx.setTextColor(COLOR_WHITE);
  };

  auto item = [&](int px, int py, const char* key, const char* desc, IGfxColor keyColor) {
    gfx.setTextColor(keyColor);
    gfx.drawText(px, py, key);
    gfx.setTextColor(COLOR_WHITE);
    int key_w = textWidth(gfx, key);
    gfx.drawText(px + key_w + 6, py, desc);
  };

  if (helpPage == 0) {
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

  } else if (helpPage == 1) {
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
  } else if (helpPage == 2) {
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
  } else if (helpPage == 3) {
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
  } else if (helpPage == 4) {
    heading(left_x, left_y, "Song Page");
    left_y += lh;
    heading(left_x, left_y, "Navigation");
    left_y += lh;
    item(left_x, left_y, "LEFT/RIGHT", "col / mode focus", COLOR_LABEL);
    left_y += lh;
    item(left_x, left_y, "UP/DOWN", "rows", COLOR_LABEL);
    left_y += lh;
    item(left_x, left_y, "ALT+UP/DN", "slot +/-", IGfxColor::Yellow());
    left_y += lh;

    heading(left_x, left_y, "Patterns");
    left_y += lh;
    item(left_x, left_y, "Q..I", "set 1-8", COLOR_PATTERN_SELECTED_FILL);
    left_y += lh;
    item(left_x, left_y, "BACK", "clear slot", IGfxColor::Red());
    left_y += lh;
  } else if (helpPage == 5) {
    heading(left_x, left_y, "Song Page (cont.)");
    left_y += lh;

    heading(left_x, left_y, "Playhead");
    left_y += lh;
    item(left_x, left_y, "ALT+UP/DN @PLAY", "nudge playhead", IGfxColor::Yellow());
    left_y += lh;
    left_y += lh;

    heading(left_x, left_y, "Mode");
    left_y += lh;
    item(left_x, left_y, "ENTER @ MODE", "Song/Pat toggle", IGfxColor::Green());
    left_y += lh;
    item(left_x, left_y, "M", "toggle mode", IGfxColor::Magenta());
    left_y += lh;
  }
}

void HelpPage::draw(IGfx& gfx, int x, int y, int w, int h) {
  char title[16];
  snprintf(title, sizeof(title), "HELP");
  int title_h = page_helper_.drawPageTitle(x, y, w, title);
  drawHelpPage(gfx, x, y + title_h, w, h - title_h, help_page_index_);
  // draw a basic scrollbar

  gfx.setTextColor(IGfxColor::Gray());
  gfx.drawLine(w, y + title_h, w, h);
  int total_height = h - title_h;
  int page_size = total_height / total_help_pages_;

  gfx.setTextColor(IGfxColor::White());
  int y1 = y + title_h + page_size * help_page_index_;
  int y2 = y1 + page_size;
  gfx.drawLine(w, y1 , w, y2);
}

bool HelpPage::handleEvent(UIEvent &ui_event) {
  bool handled = false;
  switch(ui_event.scancode){
    case MINIACID_UP:
      help_page_index_--;
      handled = true;
      break;
    case MINIACID_DOWN:
      help_page_index_++;
      handled = true;
      break;
    default:
      break;
  }
  if(help_page_index_ < 0) help_page_index_ = 0;
  if(help_page_index_ >= total_help_pages_) help_page_index_ = total_help_pages_ - 1;
  return handled;
}

Synth303ParamsPage::Synth303ParamsPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard, int voice_index) :
    gfx_(gfx),
    mini_acid_(mini_acid),
    page_helper_(page_helper),
    audio_guard_(audio_guard),
    voice_index_(voice_index)
{

}

void Synth303ParamsPage::draw(IGfx& gfx, int x, int y, int w, int h)
{
  char buf[128];
  auto print = [&](int px, int py, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    gfx_.drawText(px, py, buf);
  };

  char title[16];
  snprintf(title, sizeof(title), "ACID SYNTH %c", "AB"[voice_index_]);
  int title_h = page_helper_.drawPageTitle(x, y, w, title);

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

  const Parameter& pCut = mini_acid_.parameter303(TB303ParamId::Cutoff, voice_index_);
  const Parameter& pRes = mini_acid_.parameter303(TB303ParamId::Resonance, voice_index_);
  const Parameter& pEnv = mini_acid_.parameter303(TB303ParamId::EnvAmount, voice_index_);
  const Parameter& pDec = mini_acid_.parameter303(TB303ParamId::EnvDecay, voice_index_);

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

void Synth303ParamsPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) {
    audio_guard_(fn);
    return;
  }
  fn();
}

bool Synth303ParamsPage::handleEvent(UIEvent& ui_event) 
{
  bool event_handled = false;
  int steps = 5;
  switch(ui_event.key){
    case 'a':
      withAudioGuard([&]() { 
        mini_acid_.adjust303Parameter(TB303ParamId::Cutoff, steps, voice_index_); 
      });
      event_handled = true;
      break;
    case 'z':
      withAudioGuard([&]() { 
        mini_acid_.adjust303Parameter(TB303ParamId::Cutoff, -steps, voice_index_);
      });
      event_handled = true;
      break;
    case 's':
      withAudioGuard([&]() { 
        mini_acid_.adjust303Parameter(TB303ParamId::Resonance, steps, voice_index_);
      });
      event_handled = true;
      break;
    case 'x':
      withAudioGuard([&]() { 
        mini_acid_.adjust303Parameter(TB303ParamId::Resonance, -steps, voice_index_);
      });
      event_handled = true;
      break;
    case 'd':
      withAudioGuard([&]() { 
        mini_acid_.adjust303Parameter(TB303ParamId::EnvAmount, steps, voice_index_);
      });
      event_handled = true;
      break;
    case 'c':
      withAudioGuard([&]() { 
        mini_acid_.adjust303Parameter(TB303ParamId::EnvAmount, -steps, voice_index_);
      });
      event_handled = true;
      break;
    case 'f':
      withAudioGuard([&]() { 
        mini_acid_.adjust303Parameter(TB303ParamId::EnvDecay, steps, voice_index_);
      });
      event_handled = true;
      break;
    case 'v':
      withAudioGuard([&]() { 
        mini_acid_.adjust303Parameter(TB303ParamId::EnvDecay, -1, voice_index_);
      });
      event_handled = true;
      break;
    case 'm':
      withAudioGuard([&]() { 
        mini_acid_.toggleDelay303(voice_index_);
      });
      break;
    default:
      break;
  }
  return event_handled;
}

WaveformPage::WaveformPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    page_helper_(page_helper),
    audio_guard_(audio_guard),
    wave_color_index_(0)
{
}

void WaveformPage::draw(IGfx& gfx, int x, int y, int w, int h) {
  int title_h = page_helper_.drawPageTitle(x, y, w, "WAVEFORM");

  int wave_y = y + title_h + 2;
  int wave_h = h - title_h - 2;
  if (w < 4 || wave_h < 4) return;

  int16_t samples[AUDIO_BUFFER_SAMPLES/2];
  size_t sampleCount = mini_acid_.copyLastAudio(samples, AUDIO_BUFFER_SAMPLES/2);
  int mid_y = wave_y + wave_h / 2;

  gfx_.setTextColor(IGfxColor::Orange());
  gfx_.drawLine(x, mid_y, x + w - 1, mid_y);

  if (sampleCount > 1) {
    IGfxColor waveColor = WAVE_COLORS[wave_color_index_ % NUM_WAVE_COLORS];
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
      drawLineColored(gfx_, x + px, y0, x + px + 1, y1, waveColor);
    }
  } 
}

bool WaveformPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;
  switch (ui_event.scancode) {
    case MINIACID_UP:
    case MINIACID_DOWN:
      wave_color_index_ = (wave_color_index_ + 1) % NUM_WAVE_COLORS;
      return true;
    default:
      break;
  }
  return false;
}

PatternEditPage::PatternEditPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard, int voice_index)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    page_helper_(page_helper),
    audio_guard_(audio_guard),
    voice_index_(voice_index),
    pattern_edit_cursor_(0),
    pattern_row_cursor_(0),
    focus_(Focus::Steps) {
  int idx = mini_acid_.current303PatternIndex(voice_index_);
  if (idx < 0 || idx >= Bank<SynthPattern>::kPatterns) idx = 0;
  pattern_row_cursor_ = idx;
}

int PatternEditPage::clampCursor(int cursorIndex) const {
  int cursor = cursorIndex;
  if (cursor < 0) cursor = 0;
  if (cursor >= Bank<SynthPattern>::kPatterns)
    cursor = Bank<SynthPattern>::kPatterns - 1;
  return cursor;
}

int PatternEditPage::patternIndexFromKey(char key) const {
  switch (std::tolower(static_cast<unsigned char>(key))) {
    case 'q': return 0;
    case 'w': return 1;
    case 'e': return 2;
    case 'r': return 3;
    case 't': return 4;
    case 'y': return 5;
    case 'u': return 6;
    case 'i': return 7;
    default: return -1;
  }
}

void PatternEditPage::ensureStepFocus() {
  if (patternRowFocused()) focus_ = Focus::Steps;
}

void PatternEditPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) {
    audio_guard_(fn);
    return;
  }
  fn();
}

int PatternEditPage::activePatternCursor() const {
  return clampCursor(pattern_row_cursor_);
}

int PatternEditPage::activePatternStep() const {
  int idx = pattern_edit_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= SEQ_STEPS) idx = SEQ_STEPS - 1;
  return idx;
}

void PatternEditPage::setPatternCursor(int cursorIndex) {
  pattern_row_cursor_ = clampCursor(cursorIndex);
}

void PatternEditPage::focusPatternRow() {
  if (mini_acid_.songModeEnabled()) return;
  setPatternCursor(pattern_row_cursor_);
  focus_ = Focus::PatternRow;
}

void PatternEditPage::focusPatternSteps() {
  int row = pattern_edit_cursor_ / 8;
  if (row < 0 || row > 1) row = 0;
  pattern_edit_cursor_ = row * 8 + activePatternCursor();
  focus_ = Focus::Steps;
}

bool PatternEditPage::patternRowFocused() const {
  if (mini_acid_.songModeEnabled()) return false;
  return focus_ == Focus::PatternRow;
}

void PatternEditPage::movePatternCursor(int delta) {
  if (mini_acid_.songModeEnabled() && focus_ == Focus::PatternRow) {
    focus_ = Focus::Steps;
  }
  if (focus_ == Focus::PatternRow) {
    int cursor = activePatternCursor();
    cursor = (cursor + delta) % Bank<SynthPattern>::kPatterns;
    if (cursor < 0) cursor += Bank<SynthPattern>::kPatterns;
    pattern_row_cursor_ = cursor;
    return;
  }
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  col = (col + delta) % 8;
  if (col < 0) col += 8;
  pattern_edit_cursor_ = row * 8 + col;
}

void PatternEditPage::movePatternCursorVertical(int delta) {
  if (delta == 0) return;
  if (mini_acid_.songModeEnabled() && focus_ == Focus::PatternRow) {
    focus_ = Focus::Steps;
  }
  if (focus_ == Focus::PatternRow) {
    int col = activePatternCursor();
    int targetRow = delta > 0 ? 0 : 1;
    pattern_edit_cursor_ = targetRow * 8 + col;
    focus_ = Focus::Steps;
    return;
  }
  int idx = activePatternStep();
  int row = idx / 8;
  int col = idx % 8;
  int newRow = row + delta;
  if (newRow < 0) {
    if (mini_acid_.songModeEnabled()) newRow = 0;
    else {
      focus_ = Focus::PatternRow;
      setPatternCursor(col);
      return;
    }
  }
  if (newRow > 1) {
    if (mini_acid_.songModeEnabled()) newRow = 1;
    else {
      focus_ = Focus::PatternRow;
      setPatternCursor(col);
      return;
    }
  }
  pattern_edit_cursor_ = newRow * 8 + col;
}

bool PatternEditPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;
  bool handled = false;
  switch (ui_event.scancode) {
    case MINIACID_LEFT:
      movePatternCursor(-1);
      handled = true;
      break;
    case MINIACID_RIGHT:
      movePatternCursor(1);
      handled = true;
      break;
    case MINIACID_UP:
      movePatternCursorVertical(-1);
      handled = true;
      break;
    case MINIACID_DOWN:
      movePatternCursorVertical(1);
      handled = true;
      break;
    default:
      break;
  }
  if (handled) return true;

  char key = ui_event.key;
  if (!key) return false;

  if ((key == '\n' || key == '\r') && patternRowFocused()) {
    if (mini_acid_.songModeEnabled()) return true;
    int cursor = activePatternCursor();
    setPatternCursor(cursor);
    withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, cursor); });
    return true;
  }

  int patternIdx = patternIndexFromKey(key);
  bool patternKeyReserved = false;
  if (patternIdx >= 0) {
    char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
    patternKeyReserved = (lowerKey == 'q' || lowerKey == 'w');
    if (!patternKeyReserved || patternRowFocused()) {
      if (mini_acid_.songModeEnabled()) return true;
      focusPatternRow();
      setPatternCursor(patternIdx);
      withAudioGuard([&]() { mini_acid_.set303PatternIndex(voice_index_, patternIdx); });
      return true;
    }
  }

  auto ensureStepFocusAndCursor = [&]() {
    if (patternRowFocused()) {
      focusPatternSteps();
    } else {
      ensureStepFocus();
    }
  };

  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
  switch (lowerKey) {
    case 'q': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.toggle303SlideStep(voice_index_, step); });
      return true;
    }
    case 'w': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.toggle303AccentStep(voice_index_, step); });
      return true;
    }
    case 'a': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.adjust303StepNote(voice_index_, step, 1); });
      return true;
    }
    case 'z': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.adjust303StepNote(voice_index_, step, -1); });
      return true;
    }
    case 's': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.adjust303StepOctave(voice_index_, step, 1); });
      return true;
    }
    case 'x': {
      ensureStepFocusAndCursor();
      int step = activePatternStep();
      withAudioGuard([&]() { mini_acid_.adjust303StepOctave(voice_index_, step, -1); });
      return true;
    }
    default:
      break;
  }

  if (key == '\b') {
    ensureStepFocusAndCursor();
    int step = activePatternStep();
    withAudioGuard([&]() { mini_acid_.clear303StepNote(voice_index_, step); });
    return true;
  }

  return false;
}

void PatternEditPage::draw(IGfx& gfx, int x, int y, int w, int h) {
  const char* title = voice_index_ == 0 ? "303A EDIT" : "303B EDIT";
  int title_h = page_helper_.drawPageTitle(x, y, w, title);
  int body_y = y + title_h + 2;
  int body_h = h - title_h - 2;
  if (body_h <= 0) return;

  const int8_t* notes = mini_acid_.pattern303Steps(voice_index_);
  const bool* accent = mini_acid_.pattern303AccentSteps(voice_index_);
  const bool* slide = mini_acid_.pattern303SlideSteps(voice_index_);
  int stepCursor = pattern_edit_cursor_;
  int playing = mini_acid_.currentStep();
  int selectedPattern = mini_acid_.display303PatternIndex(voice_index_);
  bool songMode = mini_acid_.songModeEnabled();
  bool patternFocus = !songMode && patternRowFocused();
  bool stepFocus = !patternFocus;
  int patternCursor = songMode && selectedPattern >= 0 ? selectedPattern : activePatternCursor();

  int spacing = 4;
  int pattern_size = (w - spacing * 7 - 2) / 8;
  int pattern_size_height = pattern_size / 2;
  if (pattern_size < 12) pattern_size = 12;
  int pattern_label_h = gfx.fontHeight();
  int pattern_row_y = body_y + pattern_label_h + 1;

  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, body_y, "PATTERNS");
  gfx.setTextColor(COLOR_WHITE);

  for (int i = 0; i < Bank<SynthPattern>::kPatterns; ++i) {
    int col = i % 8;
    int cell_x = x + col * (pattern_size + spacing);
    bool isCursor = patternFocus && patternCursor == i;
    IGfxColor bg = songMode ? COLOR_GRAY_DARKER : COLOR_PANEL;
    gfx.fillRect(cell_x, pattern_row_y, pattern_size, pattern_size_height, bg);
    if (selectedPattern == i) {
      IGfxColor sel = songMode ? IGfxColor::Yellow() : COLOR_PATTERN_SELECTED_FILL;
      IGfxColor border = songMode ? IGfxColor::Yellow() : COLOR_LABEL;
      gfx.fillRect(cell_x - 1, pattern_row_y - 1, pattern_size + 2, pattern_size_height + 2, sel);
      gfx.drawRect(cell_x - 1, pattern_row_y - 1, pattern_size + 2, pattern_size_height + 2, border);
    }
    gfx.drawRect(cell_x, pattern_row_y, pattern_size, pattern_size_height, songMode ? COLOR_LABEL : COLOR_WHITE);
    if (isCursor) {
      gfx.drawRect(cell_x - 2, pattern_row_y - 2, pattern_size + 4, pattern_size_height + 4, COLOR_STEP_SELECTED);
    }
    char label[4];
    snprintf(label, sizeof(label), "%d", i + 1);
    int tw = textWidth(gfx, label);
    int tx = cell_x + (pattern_size - tw) / 2;
    int ty = pattern_row_y + pattern_size_height / 2 - gfx.fontHeight() / 2;
    gfx.setTextColor(songMode ? COLOR_LABEL : COLOR_WHITE);
    gfx.drawText(tx, ty, label);
    gfx.setTextColor(COLOR_WHITE);
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

    gfx.fillRect(cell_x, indicator_y, indicator_w, indicator_h, slide[i] ? COLOR_SLIDE : COLOR_GRAY_DARKER);
    gfx.drawRect(cell_x, indicator_y, indicator_w, indicator_h, COLOR_WHITE);
    gfx.fillRect(slide_x, indicator_y, indicator_w, indicator_h, accent[i] ? COLOR_ACCENT : COLOR_GRAY_DARKER);
    gfx.drawRect(slide_x, indicator_y, indicator_w, indicator_h, COLOR_WHITE);

    int note_box_y = indicator_y + indicator_h + indicator_gap;
    IGfxColor fill = notes[i] >= 0 ? COLOR_303_NOTE : COLOR_GRAY;
    gfx.fillRect(cell_x, note_box_y, cell_size, cell_size, fill);
    gfx.drawRect(cell_x, note_box_y, cell_size, cell_size, COLOR_WHITE);

    if (playing == i) {
      gfx.drawRect(cell_x - 1, note_box_y - 1, cell_size + 2, cell_size + 2, COLOR_STEP_HILIGHT);
    }
    if (stepFocus && stepCursor == i) {
      gfx.drawRect(cell_x - 2, note_box_y - 2, cell_size + 4, cell_size + 4, COLOR_STEP_SELECTED);
    }

    char note_label[8];
    formatNoteName(notes[i], note_label, sizeof(note_label));
    int tw = textWidth(gfx, note_label);
    int tx = cell_x + (cell_size - tw) / 2;
    int ty = note_box_y + cell_size / 2 - gfx.fontHeight() / 2;
    gfx.drawText(tx, ty, note_label);
  }
}

DrumSequencerPage::DrumSequencerPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    page_helper_(page_helper),
    audio_guard_(audio_guard),
    drum_step_cursor_(0),
    drum_voice_cursor_(0),
    drum_pattern_cursor_(0),
    drum_pattern_focus_(true) {
  int drumIdx = mini_acid_.currentDrumPatternIndex();
  if (drumIdx < 0 || drumIdx >= Bank<DrumPatternSet>::kPatterns) drumIdx = 0;
  drum_pattern_cursor_ = drumIdx;
}

int DrumSequencerPage::activeDrumPatternCursor() const {
  int idx = drum_pattern_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= Bank<DrumPatternSet>::kPatterns)
    idx = Bank<DrumPatternSet>::kPatterns - 1;
  return idx;
}

int DrumSequencerPage::activeDrumStep() const {
  int idx = drum_step_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= SEQ_STEPS) idx = SEQ_STEPS - 1;
  return idx;
}

int DrumSequencerPage::activeDrumVoice() const {
  int idx = drum_voice_cursor_;
  if (idx < 0) idx = 0;
  if (idx >= NUM_DRUM_VOICES) idx = NUM_DRUM_VOICES - 1;
  return idx;
}

void DrumSequencerPage::setDrumPatternCursor(int cursorIndex) {
  int cursor = cursorIndex;
  if (cursor < 0) cursor = 0;
  if (cursor >= Bank<DrumPatternSet>::kPatterns)
    cursor = Bank<DrumPatternSet>::kPatterns - 1;
  drum_pattern_cursor_ = cursor;
}

void DrumSequencerPage::moveDrumCursor(int delta) {
  if (mini_acid_.songModeEnabled()) {
    drum_pattern_focus_ = false;
  }
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

void DrumSequencerPage::moveDrumCursorVertical(int delta) {
  if (delta == 0) return;
  if (mini_acid_.songModeEnabled()) {
    drum_pattern_focus_ = false;
  }
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

void DrumSequencerPage::focusPatternRow() {
  setDrumPatternCursor(drum_pattern_cursor_);
  drum_pattern_focus_ = true;
}

void DrumSequencerPage::focusGrid() {
  drum_pattern_focus_ = false;
  drum_step_cursor_ = activeDrumStep();
  drum_voice_cursor_ = activeDrumVoice();
}

bool DrumSequencerPage::patternRowFocused() const {
  if (mini_acid_.songModeEnabled()) return false;
  return drum_pattern_focus_;
}

int DrumSequencerPage::patternIndexFromKey(char key) const {
  switch (std::tolower(static_cast<unsigned char>(key))) {
    case 'q': return 0;
    case 'w': return 1;
    case 'e': return 2;
    case 'r': return 3;
    case 't': return 4;
    case 'y': return 5;
    case 'u': return 6;
    case 'i': return 7;
    default: return -1;
  }
}

void DrumSequencerPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) {
    audio_guard_(fn);
    return;
  }
  fn();
}

bool DrumSequencerPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;
  bool handled = false;
  switch (ui_event.scancode) {
    case MINIACID_LEFT:
      moveDrumCursor(-1);
      handled = true;
      break;
    case MINIACID_RIGHT:
      moveDrumCursor(1);
      handled = true;
      break;
    case MINIACID_UP:
      moveDrumCursorVertical(-1);
      handled = true;
      break;
    case MINIACID_DOWN:
      moveDrumCursorVertical(1);
      handled = true;
      break;
    default:
      break;
  }

  if (handled) return true;

  // key-based actions
  char key = ui_event.key;
  if (!key) return false;

  if (key == '\n' || key == '\r') {
    if (patternRowFocused()) {
      int cursor = activeDrumPatternCursor();
      withAudioGuard([&]() { mini_acid_.setDrumPatternIndex(cursor); });
    } else {
      int step = activeDrumStep();
      int voice = activeDrumVoice();
      withAudioGuard([&]() { mini_acid_.toggleDrumStep(voice, step); });
    }
    return true;
  }

  int patternIdx = patternIndexFromKey(key);
  if (patternIdx >= 0) {
    if (mini_acid_.songModeEnabled()) return true; // ignore in song mode
    focusPatternRow();
    setDrumPatternCursor(patternIdx);
    withAudioGuard([&]() { mini_acid_.setDrumPatternIndex(patternIdx); });
    return true;
  }

  return false;
}

void DrumSequencerPage::draw(IGfx& gfx, int x, int y, int w, int h) {
  int title_h = page_helper_.drawPageTitle(x, y, w, "DRUM SEQUENCER");
  int body_y = y + title_h + 2;
  int body_h = h - title_h - 2;
  if (body_h <= 0) return;

  int pattern_label_h = gfx.fontHeight();
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, body_y, "PATTERN");
  gfx.setTextColor(COLOR_WHITE);

  int spacing = 4;
  int pattern_size = (w - spacing * 7 - 2) / 8;
  if (pattern_size < 12) pattern_size = 12;
  int pattern_height = pattern_size / 2;
  int pattern_row_y = body_y + pattern_label_h + 1;

  int selectedPattern = mini_acid_.displayDrumPatternIndex();
  int patternCursor = activeDrumPatternCursor();
  bool songMode = mini_acid_.songModeEnabled();
  bool patternFocus = !songMode && patternRowFocused();
  if (songMode && selectedPattern >= 0) patternCursor = selectedPattern;

  for (int i = 0; i < Bank<DrumPatternSet>::kPatterns; ++i) {
    int col = i % 8;
    int cell_x = x + col * (pattern_size + spacing);
    bool isCursor = patternFocus && patternCursor == i;
    IGfxColor bg = songMode ? COLOR_GRAY_DARKER : COLOR_PANEL;
    gfx.fillRect(cell_x, pattern_row_y, pattern_size, pattern_height, bg);
    if (selectedPattern == i) {
      IGfxColor sel = songMode ? IGfxColor::Yellow() : COLOR_PATTERN_SELECTED_FILL;
      IGfxColor border = songMode ? IGfxColor::Yellow() : COLOR_LABEL;
      gfx.fillRect(cell_x - 1, pattern_row_y - 1, pattern_size + 2, pattern_height + 2, sel);
      gfx.drawRect(cell_x - 1, pattern_row_y - 1, pattern_size + 2, pattern_height + 2, border);
    }
    gfx.drawRect(cell_x, pattern_row_y, pattern_size, pattern_height, songMode ? COLOR_LABEL : COLOR_WHITE);
    if (isCursor) {
      gfx.drawRect(cell_x - 2, pattern_row_y - 2, pattern_size + 4, pattern_height + 4, COLOR_STEP_SELECTED);
    }
    char label[4];
    snprintf(label, sizeof(label), "%d", i + 1);
    int tw = textWidth(gfx, label);
    int tx = cell_x + (pattern_size - tw) / 2;
    int ty = pattern_row_y + pattern_height / 2 - gfx.fontHeight() / 2;
    gfx.setTextColor(songMode ? COLOR_LABEL : COLOR_WHITE);
    gfx.drawText(tx, ty, label);
    gfx.setTextColor(COLOR_WHITE);
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
    int ly = grid_top + v * labelStripeH + (labelStripeH - gfx.fontHeight()) / 2;
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(x, ly, voiceLabels[v]);
  }
  gfx.setTextColor(COLOR_WHITE);

  int cursorStep = activeDrumStep();
  int cursorVoice = activeDrumVoice();
  bool gridFocus = !patternFocus;

  const int cell_w = grid_w / SEQ_STEPS;
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

  int stripe_h = grid_h / NUM_DRUM_VOICES;
  if (stripe_h < 3) stripe_h = 3;

  for (int i = 0; i < SEQ_STEPS; ++i) {
    int cw = cell_w;
    if (cw < 2) cw = 2;
    int cx = grid_x + i * cell_w;

    gfx.fillRect(cx, grid_top, cw - 1, grid_h - 1, COLOR_DARKER);

    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
      if (!hits[v] || !hits[v][i]) continue;
      int stripe_y = grid_top + v * stripe_h;
      int stripe_w = cw - 3;
      if (stripe_w < 1) stripe_w = 1;
      int stripe_h_clamped = stripe_h - 2;
      if (stripe_h_clamped < 1) stripe_h_clamped = 1;
      gfx.fillRect(cx + 1, stripe_y + 1, stripe_w, stripe_h_clamped, colors[v]);
    }

    if (highlight == i)
      gfx.drawRect(cx, grid_top, cw - 1, grid_h - 1, COLOR_STEP_HILIGHT);
    if (gridFocus && cursorStep == i) {
      int stripe_y = grid_top + cursorVoice * stripe_h;
      gfx.drawRect(cx, stripe_y, cw - 1, stripe_h - 1, COLOR_STEP_SELECTED);
    }
  }
}

SongPage::SongPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    page_helper_(page_helper),
    audio_guard_(audio_guard),
    cursor_row_(0),
    cursor_track_(0),
    scroll_row_(0) {
  cursor_row_ = mini_acid_.currentSongPosition();
  if (cursor_row_ < 0) cursor_row_ = 0;
  int maxSongRow = mini_acid_.songLength() - 1;
  if (maxSongRow < 0) maxSongRow = 0;
  if (cursor_row_ > maxSongRow) cursor_row_ = maxSongRow;
  if (cursor_row_ >= Song::kMaxPositions) cursor_row_ = Song::kMaxPositions - 1;
}

int SongPage::clampCursorRow(int row) const {
  int maxRow = Song::kMaxPositions - 1;
  if (maxRow < 0) maxRow = 0;
  if (row < 0) row = 0;
  if (row > maxRow) row = maxRow;
  return row;
}

int SongPage::cursorRow() const {
  return clampCursorRow(cursor_row_);
}

int SongPage::cursorTrack() const {
  int track = cursor_track_;
  if (track < 0) track = 0;
  if (track > 4) track = 4;
  return track;
}

bool SongPage::cursorOnModeButton() const { return cursorTrack() == 4; }
bool SongPage::cursorOnPlayheadLabel() const { return cursorTrack() == 3; }

void SongPage::moveCursorHorizontal(int delta) {
  int track = cursorTrack();
  track += delta;
  if (track < 0) track = 0;
  if (track > 4) track = 4;
  cursor_track_ = track;
  syncSongPositionToCursor();
}

void SongPage::moveCursorVertical(int delta) {
  if (delta == 0) return;
  if (cursorOnPlayheadLabel() || cursorOnModeButton()) {
    moveCursorHorizontal(delta);
    return;
  }
  int row = cursorRow();
  row += delta;
  row = clampCursorRow(row);
  cursor_row_ = row;
  syncSongPositionToCursor();
}

void SongPage::syncSongPositionToCursor() {
  if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
    withAudioGuard([&]() { mini_acid_.setSongPosition(cursorRow()); });
  }
}

void SongPage::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) {
    audio_guard_(fn);
    return;
  }
  fn();
}

SongTrack SongPage::trackForColumn(int col, bool& valid) const {
  valid = true;
  if (col == 0) return SongTrack::SynthA;
  if (col == 1) return SongTrack::SynthB;
  if (col == 2) return SongTrack::Drums;
  valid = false;
  return SongTrack::Drums;
}

int SongPage::patternIndexFromKey(char key) const {
  switch (std::tolower(static_cast<unsigned char>(key))) {
    case 'q': return 0;
    case 'w': return 1;
    case 'e': return 2;
    case 'r': return 3;
    case 't': return 4;
    case 'y': return 5;
    case 'u': return 6;
    case 'i': return 7;
    default: return -1;
  }
}

bool SongPage::adjustSongPatternAtCursor(int delta) {
  bool trackValid = false;
  SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid) return false;
  int row = cursorRow();
  int current = mini_acid_.songPatternAt(row, track);
  int maxPattern = track == SongTrack::Drums
                     ? Bank<DrumPatternSet>::kPatterns - 1
                     : Bank<SynthPattern>::kPatterns - 1;
  int next = current;
  if (delta > 0) next = current < 0 ? 0 : current + 1;
  else if (delta < 0) next = current < 0 ? -1 : current - 1;
  if (next > maxPattern) next = maxPattern;
  if (next < -1) next = -1;
  if (next == current) return false;
  withAudioGuard([&]() {
    if (next < 0) mini_acid_.clearSongPattern(row, track);
    else mini_acid_.setSongPattern(row, track, next);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  return true;
}

bool SongPage::adjustSongPlayhead(int delta) {
  int len = mini_acid_.songLength();
  if (len < 1) len = 1;
  int maxPos = len - 1;
  if (maxPos < 0) maxPos = 0;
  if (maxPos >= Song::kMaxPositions) maxPos = Song::kMaxPositions - 1;
  int current = mini_acid_.songPlayheadPosition();
  int next = current + delta;
  if (next < 0) next = 0;
  if (next > maxPos) next = maxPos;
  if (next == current) return false;
  withAudioGuard([&]() { mini_acid_.setSongPosition(next); });
  setScrollToPlayhead(next);
  return true;
}

bool SongPage::assignPattern(int patternIdx) {
  bool trackValid = false;
  SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid || cursorOnModeButton()) return false;
  int row = cursorRow();
  withAudioGuard([&]() {
    mini_acid_.setSongPattern(row, track, patternIdx);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  return true;
}

bool SongPage::clearPattern() {
  bool trackValid = false;
  SongTrack track = trackForColumn(cursorTrack(), trackValid);
  if (!trackValid) return false;
  int row = cursorRow();
  withAudioGuard([&]() {
    mini_acid_.clearSongPattern(row, track);
    if (mini_acid_.songModeEnabled() && !mini_acid_.isPlaying()) {
      mini_acid_.setSongPosition(row);
    }
  });
  return true;
}

bool SongPage::toggleSongMode() {
  withAudioGuard([&]() { mini_acid_.toggleSongMode(); });
  return true;
}

void SongPage::setScrollToPlayhead(int playhead) {
  if (playhead < 0) playhead = 0;
  int rowHeight = gfx_.fontHeight() + 6;
  if (rowHeight < 8) rowHeight = 8;
  int visibleRows = (gfx_.height() - 20) / rowHeight;
  if (visibleRows < 1) visibleRows = 1;
  if (scroll_row_ > playhead) scroll_row_ = playhead;
  if (scroll_row_ + visibleRows - 1 < playhead) {
    scroll_row_ = playhead - visibleRows + 1;
    if (scroll_row_ < 0) scroll_row_ = 0;
  }
}

bool SongPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;

  if (ui_event.alt && (ui_event.scancode == MINIACID_UP || ui_event.scancode == MINIACID_DOWN)) {
    int delta = ui_event.scancode == MINIACID_UP ? 1 : -1;
    if (cursorOnPlayheadLabel()) return adjustSongPlayhead(delta);
    return adjustSongPatternAtCursor(delta);
  }

  bool handled = false;
  switch (ui_event.scancode) {
    case MINIACID_LEFT:
      moveCursorHorizontal(-1);
      handled = true;
      break;
    case MINIACID_RIGHT:
      moveCursorHorizontal(1);
      handled = true;
      break;
    case MINIACID_UP:
      moveCursorVertical(-1);
      handled = true;
      break;
    case MINIACID_DOWN:
      moveCursorVertical(1);
      handled = true;
      break;
    default:
      break;
  }
  if (handled) return true;

  char key = ui_event.key;
  if (!key) return false;

  if (cursorOnModeButton() && (key == '\n' || key == '\r')) {
    return toggleSongMode();
  }

  if (key == 'm' || key == 'M') {
    return toggleSongMode();
  }

  int patternIdx = patternIndexFromKey(key);
  if (cursorOnModeButton() && patternIdx >= 0) return false;
  if (patternIdx >= 0) return assignPattern(patternIdx);

  if (key == '\b') {
    return clearPattern();
  }

  return false;
}

void SongPage::draw(IGfx& gfx, int x, int y, int w, int h) {
  int title_h = page_helper_.drawPageTitle(x, y, w, "SONG");
  int body_y = y + title_h + 2;
  int body_h = h - title_h - 2;
  if (body_h <= 0) return;

  int label_h = gfx.fontHeight();
  int header_h = label_h + 4;
  int row_h = label_h + 6;
  if (row_h < 10) row_h = 10;
  int usable_h = body_h - header_h;
  if (usable_h < row_h) usable_h = row_h;
  int visible_rows = usable_h / row_h;
  if (visible_rows < 1) visible_rows = 1;

  int song_len = mini_acid_.songLength();
  int cursor_row = cursorRow();
  int playhead = mini_acid_.songPlayheadPosition();
  bool playingSong = mini_acid_.isPlaying() && mini_acid_.songModeEnabled();

  if (playingSong) {
    int minTarget = std::min(cursor_row, playhead);
    int maxTarget = std::max(cursor_row, playhead);
    if (minTarget < scroll_row_) scroll_row_ = minTarget;
    if (maxTarget >= scroll_row_ + visible_rows) scroll_row_ = maxTarget - visible_rows + 1;
  } else {
    if (cursor_row < scroll_row_) scroll_row_ = cursor_row;
    if (cursor_row >= scroll_row_ + visible_rows) scroll_row_ = cursor_row - visible_rows + 1;
  }
  if (scroll_row_ < 0) scroll_row_ = 0;
  int maxStart = Song::kMaxPositions - visible_rows;
  if (maxStart < 0) maxStart = 0;
  if (scroll_row_ > maxStart) scroll_row_ = maxStart;

  int pos_col_w = 20;
  int spacing = 3;
  int modeBtnW = 70;
  int track_col_w = (w - pos_col_w - spacing * 5 - modeBtnW) / 3;
  if (track_col_w < 20) track_col_w = 20;

  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, body_y, "POS");
  gfx.drawText(x + pos_col_w + spacing, body_y, "303A");
  gfx.drawText(x + pos_col_w + spacing + track_col_w, body_y, "303B");
  gfx.drawText(x + pos_col_w + spacing + track_col_w * 2, body_y, "Drums");
  char lenBuf[24];
  snprintf(lenBuf, sizeof(lenBuf), "PLAYHD %d:%d", playhead + 1, song_len);
  int lenX = x + pos_col_w + spacing + track_col_w * 3 + spacing + 10;
  int lenW = textWidth(gfx, lenBuf);
  bool playheadSelected = cursorOnPlayheadLabel();
  if (playheadSelected) {
    gfx.drawRect(lenX - 2, body_y - 1, lenW + 4, label_h + 2, COLOR_STEP_SELECTED);
  }
  gfx.drawText(lenX, body_y, lenBuf);

  // Mode button
  bool songMode = mini_acid_.songModeEnabled();
  IGfxColor modeColor = songMode ? IGfxColor::Green() : IGfxColor::Blue();
  int modeX = x + w - modeBtnW;
  int modeY = body_y - 2 + 30;
  int modeH = header_h + row_h;
  gfx.fillRect(modeX, modeY, modeBtnW - 2, modeH, COLOR_PANEL);
  gfx.drawRect(modeX, modeY, modeBtnW - 2, modeH, modeColor);
  char modeLabel[32];
  snprintf(modeLabel, sizeof(modeLabel), "MODE:%s", songMode ? "SONG" : "PAT");
  int twMode = textWidth(gfx, modeLabel);
  gfx.setTextColor(modeColor);
  gfx.drawText(modeX + (modeBtnW - twMode) / 2, modeY + modeH / 2 - label_h / 2, modeLabel);
  gfx.setTextColor(COLOR_WHITE);
  bool modeSelected = cursorOnModeButton();
  if (modeSelected) {
    gfx.drawRect(modeX - 2, modeY - 2, modeBtnW + 2, modeH + 4, COLOR_STEP_SELECTED);
  }

  int row_y = body_y + header_h;
  for (int i = 0; i < visible_rows; ++i) {
    int row_idx = scroll_row_ + i;
    if (row_idx >= Song::kMaxPositions) break;
    bool isCursorRow = row_idx == cursor_row;
    bool isPlayhead = playingSong && row_idx == playhead;
    IGfxColor rowHighlight = isPlayhead ? IGfxColor::Magenta() : COLOR_DARKER;
    if (isPlayhead) {
      gfx.fillRect(x, row_y - 1, w - modeBtnW - 2, row_h, rowHighlight);
    } else if (isCursorRow) {
      gfx.fillRect(x, row_y - 1, w - modeBtnW - 2, row_h, COLOR_PANEL);
    } else {
      gfx.fillRect(x, row_y - 1, w - modeBtnW - 2, row_h, COLOR_DARKER);
    }

    char posLabel[8];
    snprintf(posLabel, sizeof(posLabel), "%d", row_idx + 1);
    gfx.setTextColor(row_idx < song_len ? COLOR_WHITE : COLOR_LABEL);
    gfx.drawText(x, row_y + 2, posLabel);
    gfx.setTextColor(COLOR_WHITE);

    for (int t = 0; t < SongPosition::kTrackCount; ++t) {
      int col_x = x + pos_col_w + spacing + t * (track_col_w + spacing);
      int patternIdx = mini_acid_.songPatternAt(row_idx,
                        t == 0 ? SongTrack::SynthA : (t == 1 ? SongTrack::SynthB : SongTrack::Drums));
      bool isSelected = isCursorRow && cursorTrack() == t;
      if (isSelected) {
        gfx.drawRect(col_x - 1, row_y - 2, track_col_w + 2, row_h + 2 - 1, COLOR_STEP_SELECTED);
      }
      char label[6];
      if (patternIdx < 0) {
        snprintf(label, sizeof(label), "--");
        gfx.setTextColor(COLOR_LABEL);
      } else {
        snprintf(label, sizeof(label), "%d", patternIdx + 1);
        gfx.setTextColor(COLOR_WHITE);
      }
      int tw = textWidth(gfx, label);
      int tx = col_x + (track_col_w - tw) / 2;
      gfx.drawText(tx, row_y + (row_h - label_h) / 2 - 1, label);
      gfx.setTextColor(COLOR_WHITE);
    }
    row_y += row_h;
  }
}

ProjectPage::ProjectPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard)
  : gfx_(gfx),
    mini_acid_(mini_acid),
    page_helper_(page_helper),
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
      return true;
    case MINIACID_RIGHT:
      if (main_focus_ == MainFocus::Load) main_focus_ = MainFocus::SaveAs;
      else if (main_focus_ == MainFocus::SaveAs) main_focus_ = MainFocus::New;
      return true;
    case MINIACID_UP:
    case MINIACID_DOWN:
      return true;
    default:
      break;
  }

  if (ui_event.key == '\n' || ui_event.key == '\r') {
    if (main_focus_ == MainFocus::Load) {
      openLoadDialog();
      return true;
    } else if (main_focus_ == MainFocus::SaveAs) {
      openSaveDialog();
      return true;
    } else if (main_focus_ == MainFocus::New) {
      return createNewScene();
    }
  }
  return false;
}

void ProjectPage::draw(IGfx& gfx, int x, int y, int w, int h) {
  int title_h = page_helper_.drawPageTitle(x, y, w, "PROJECT");
  int body_y = y + title_h + 3;
  int body_h = h - title_h - 3;
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
  int total_w = btn_w * 3 + spacing * 2;
  int start_x = x + (w - total_w) / 2;
  const char* labels[3] = {"Load", "Save As", "New"};
  for (int i = 0; i < 3; ++i) {
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

MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid)
    : gfx_(gfx), mini_acid_(mini_acid) {
  splash_start_ms_ = nowMillis();
  gfx_.setFont(GfxFont::kFont5x7);

  // Initialize Pages here.
  // doing this refactor incrementally
  pages_[k303AParameters] = std::make_unique<Synth303ParamsPage>(gfx_, mini_acid_, *this, audio_guard_, 0);
  pages_[k303BPatternEdit] = std::make_unique<PatternEditPage>(gfx_, mini_acid_, *this, audio_guard_, 1);
  pages_[k303APatternEdit] = std::make_unique<PatternEditPage>(gfx_, mini_acid_, *this, audio_guard_, 0);
  pages_[k303BParameters] = std::make_unique<Synth303ParamsPage>(gfx_, mini_acid_, *this, audio_guard_, 1);
  pages_[k303BPatternEdit] = std::make_unique<PatternEditPage>(gfx_, mini_acid_, *this, audio_guard_, 1);
  pages_[kDrumSequencer] = std::make_unique<DrumSequencerPage>(gfx_, mini_acid_, *this, audio_guard_);
  pages_[kSong] = std::make_unique<SongPage>(gfx_, mini_acid_, *this, audio_guard_);
  pages_[kProject] = std::make_unique<ProjectPage>(gfx_, mini_acid_, *this, audio_guard_);
  pages_[kWaveform] = std::make_unique<WaveformPage>(gfx_, mini_acid_, *this, audio_guard_);
  pages_[kHelp] = std::make_unique<HelpPage>(*this);
}

MiniAcidDisplay::~MiniAcidDisplay() = default;

void MiniAcidDisplay::setAudioGuard(AudioGuard guard) {
  audio_guard_ = std::move(guard);
}

void MiniAcidDisplay::dismissSplash() {
  splash_active_ = false;
}

void MiniAcidDisplay::nextPage() {
  page_index_ = (page_index_ + 1) % kPageCount;
}

void MiniAcidDisplay::previousPage() {
  page_index_ = (page_index_ - 1 + kPageCount) % kPageCount;
}

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

  if (pages_[page_index_]) {
    pages_[page_index_]->draw(gfx_, content_x, content_y, content_w, content_h);
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
    IGfxColor transportColor = mini_acid_.songModeEnabled() ? IGfxColor::Green() : IGfxColor::Blue();
    if (mini_acid_.isPlaying()) {
      gfx_.fillRect(info_x, info_y - 1, transport_info_w - 4, kTitleHeight, transportColor);
    } else {
      gfx_.fillRect(info_x, info_y - 1, transport_info_w - 4, kTitleHeight, IGfxColor::Gray());
    }
    snprintf(buf, sizeof(buf), "  %0.0fbpm", mini_acid_.bpm());
    if (mini_acid_.isPlaying()) {
      IGfxColor textColor = mini_acid_.songModeEnabled() ? IGfxColor::Black() : IGfxColor::White();
      gfx_.setTextColor(textColor);
    }
    gfx_.drawText(info_x, info_y, buf);
    gfx_.setTextColor(IGfxColor::White());
  }
  return kTitleHeight;
}

void MiniAcidDisplay::drawPageHint(int x, int y) {
  char buf[32];
  snprintf(buf, sizeof(buf), "[< %d/%d >]", page_index_ + 1, kPageCount);
  gfx_.setTextColor(COLOR_LABEL);
  gfx_.drawText(x, y, buf);
  gfx_.setTextColor(COLOR_WHITE);
}

bool MiniAcidDisplay::handleEvent(UIEvent event) {
  switch(event.event_type) {
    case MINIACID_KEY_DOWN:
      printf("Key down: scancode=%d key='%c'\n", event.scancode, event.key);
      if (event.key == '-') {
        mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, -5);
        printf("Volume down\n");
        return true;
      } else if (event.key == '=') {
        mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, 5);
        printf("Volume up\n");
        return true;
      }
      break;
    default:
      break;
  }

  if (page_index_ >= 0 && page_index_ < kPageCount && pages_[page_index_]) {
    bool handled = pages_[page_index_]->handleEvent(event);
    if (handled) update();
    return handled;
  }
  return false;
}
