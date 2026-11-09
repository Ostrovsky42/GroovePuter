#include "synth_midi_page.h"

#include <cstdio>

#include "src/dsp/miniacid_engine.h"
#include "../layout_manager.h"
#include "../ui_common.h"
#include "../screen_geometry.h"
#include "../ui_colors.h"
#include "../ui_theme.h"
#include "../ui_input.h"
#include "src/midi/cutoff_sweep.h"

namespace {
constexpr int kFocusableRows = 4;   // controller, SWEEP, TARGET, TIME
constexpr int kStepCoarse = 5;
constexpr int kStepFine = 1;
constexpr int kStandardValue = 64;  // Ctrl+A on the controller row
constexpr int kStandardTarget = 110;

using Channel = GroovePuterMidi::CutoffControlChannel;
using Request = GroovePuterMidi::CutoffRequest;
}  // namespace

SynthMidiPage::SynthMidiPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voiceIndex)
    : gfx_(gfx),
      mini_acid_(mini_acid),
      audio_guard_(audio_guard),
      voice_index_(voiceIndex) {
  title_ = (voice_index_ == 0) ? "SYNTH A MIDI" : "SYNTH B MIDI";
}

const std::string& SynthMidiPage::getTitle() const { return title_; }

void SynthMidiPage::draw(IGfx& gfx) {
  UI::drawStandardHeader(gfx, mini_acid_, title_.c_str());
  LayoutManager::clearContent(gfx);

  const auto& content = Layout::CONTENT;
  const int x = content.x + Layout::CONTENT_PAD_X;
  const int width = content.w - 2 * Layout::CONTENT_PAD_X;
  const UI::ThemePalette palette = UI::themePalette();
  const Channel& channel = GroovePuterMidi::gCutoffControl[voice_index_];
  const bool sweeping = channel.sweeping();
  const int barsDone = channel.barsElapsed();
  const bool known = channel.known();
  const int controller = channel.controller() < GroovePuterMidi::kControllerCount ? channel.controller() : 0;
  // The dispatcher has caught up with this page's last request: stop stepping from it.
  if (requested_ >= 0 && known && channel.value() == requested_) requested_ = -1;
  const char* controllerName = GroovePuterMidi::kControllers[controller].name;

  // Identity: the synth leads, the route is secondary and sits left of the tab strip.
  const int headerY = content.y + 2;
  const char* synthName = voice_index_ == 0 ? "SYNTH A" : "SYNTH B";
  gfx.setTextColor(palette.voice(voice_index_));
  gfx.drawText(x, headerY, synthName);
  const uint8_t routeChannel = voice_index_ == 0 ? GroovePuterMidi::kSynthAChannel : GroovePuterMidi::kSynthBChannel;
  char route[24]{};
  std::snprintf(route, sizeof(route), "SEQTRAK CH%02d", routeChannel + 1);
  gfx.setTextColor(palette.secondary);
  gfx.drawText(x + gfx.textWidth(synthName) + 10, headerY, route);
  gfx.fillRect(x, content.y + 13, width, 1, palette.grid);

  constexpr int kRowH = 13;
  constexpr int kRowStep = 15;
  constexpr int kBarW = 84;
  constexpr int kBarH = 6;
  const int rowsY = content.y + 17;
  const int valueRight = x + width - 2;
  const int barX = x + 70;

  auto rowY = [&](int row) { return rowsY + row * kRowStep; };
  auto drawFocus = [&](int row) {
    if (row != focus_) return;
    gfx.drawRect(x - 1, rowY(row) - 1, width + 2, kRowH, palette.accent);
    gfx.setTextColor(palette.accent);
    gfx.drawText(x + 1, rowY(row) + 2, ">");
  };
  auto drawBar = [&](int row, int value, IGfxColor color) {
    const int y = rowY(row) + (kRowH - kBarH) / 2;
    gfx.drawRect(barX, y, kBarW, kBarH, palette.dim);
    const int filled = value * kBarW / 127;
    if (filled > 0) gfx.fillRect(barX + 1, y + 1, filled > kBarW - 2 ? kBarW - 2 : filled, kBarH - 2, color);
  };

  // Controller row: its name, its value (-- until the user sets one), its bar.
  drawFocus(0);
  gfx.setTextColor(focus_ == 0 ? palette.accent : palette.text);
  gfx.drawText(x + 8, rowY(0) + 2, controllerName);
  if (known) drawBar(0, channel.value(), palette.accent);
  char value[8]{};
  if (known) std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(channel.value()));
  else std::snprintf(value, sizeof(value), "--");
  gfx.setTextColor(palette.text);
  gfx.drawText(valueRight - gfx.textWidth(value), rowY(0) + 2, value);

  // SWEEP row: the shape when idle (RAMP or RETURN), four segments while running.
  drawFocus(1);
  gfx.setTextColor(focus_ == 1 ? palette.accent : palette.text);
  gfx.drawText(x + 8, rowY(1) + 2, "SWEEP");
  const int bars = GroovePuterMidi::kDurations[channel.durationIndex()];
  if (sweeping && bars <= 4) {
    const int segW = (kBarW - (bars - 1)) / bars;
    for (int i = 0; i < bars; ++i) {
      const int sx = barX + i * (segW + 1);
      const int sy = rowY(1) + (kRowH - kBarH) / 2;
      if (i < barsDone) gfx.fillRect(sx, sy, segW, kBarH, palette.accent2);
      else gfx.drawRect(sx, sy, segW, kBarH, palette.dim);
    }
    gfx.setTextColor(palette.accent2);
    gfx.drawText(valueRight - gfx.textWidth("RUNNING"), rowY(1) + 2, "RUNNING");
  } else if (sweeping) {
    char count[12]{};
    std::snprintf(count, sizeof(count), "%d/%d", barsDone, bars);
    gfx.setTextColor(palette.accent2);
    gfx.drawText(valueRight - gfx.textWidth(count), rowY(1) + 2, count);
  } else {
    const char* shape = channel.returning() ? "RETURN" : "RAMP";
    gfx.setTextColor(palette.secondary);
    gfx.drawText(valueRight - gfx.textWidth(shape), rowY(1) + 2, shape);
  }

  // TARGET: the value the sweep reaches, on the same scale.
  drawFocus(2);
  gfx.setTextColor(focus_ == 2 ? palette.accent : palette.text);
  gfx.drawText(x + 8, rowY(2) + 2, "TARGET");
  drawBar(2, target_, palette.secondary);
  char target[8]{};
  std::snprintf(target, sizeof(target), "%u", static_cast<unsigned>(target_));
  gfx.setTextColor(palette.text);
  gfx.drawText(valueRight - gfx.textWidth(target), rowY(2) + 2, target);

  // TIME: how long the sweep takes; Left and Right choose 1, 2, 4 or 8 bars.
  drawFocus(3);
  gfx.setTextColor(focus_ == 3 ? palette.accent : palette.text);
  gfx.drawText(x + 8, rowY(3) + 2, "TIME");
  char length[12]{};
  std::snprintf(length, sizeof(length), bars == 1 ? "1 BAR" : "%d BARS", bars);
  gfx.setTextColor(palette.text);
  gfx.drawText(valueRight - gfx.textWidth(length), rowY(3) + 2, length);

  // Context hint: what Enter does right now.
  gfx.setTextColor(palette.secondary);
  const char* hint = sweeping ? "ENTER: CANCEL SWEEP"
      : focus_ == 0 ? "ENTER: CONTROLLER  CTRL+A: STANDARD"
      : focus_ == 1 ? "ENTER: START  L/R: RAMP/RETURN"
      : focus_ == 2 ? "L/R: TARGET  CTRL+A: STANDARD"
      : "L/R: LENGTH";
  gfx.drawText(x + 8, rowY(3) + kRowStep + 3, hint);

  if (popup_.isOpen()) {
    const char* names[GroovePuterMidi::kControllerCount];
    for (int i = 0; i < GroovePuterMidi::kControllerCount; ++i) names[i] = GroovePuterMidi::kControllers[i].name;
    popup_.draw(gfx, names);
  }
}

bool SynthMidiPage::handleEvent(UIEvent& ui_event) {
  if (popup_.isOpen()) {
    const auto result = popup_.handleEvent(ui_event);
    if (result == UI::SelectionPopupResult::Confirmed) {
      selectController(static_cast<uint8_t>(popup_.selectedIndex()));
    }
    return true;
  }
  if (UIInput::isGlobalNav(ui_event)) return false;
  const int nav = UIInput::navCode(ui_event);
  const bool fine = ui_event.ctrl;
  switch (nav) {
    case GROOVEPUTER_UP:
      focus_ = focus_ > 0 ? focus_ - 1 : kFocusableRows - 1;
      return true;
    case GROOVEPUTER_DOWN:
      focus_ = focus_ < kFocusableRows - 1 ? focus_ + 1 : 0;
      return true;
    case GROOVEPUTER_LEFT:
      if (focus_ == 1) toggleShape();
      else if (focus_ == 3) adjustLength(-1);
      else adjust(-1, fine);
      return true;
    case GROOVEPUTER_RIGHT:
      if (focus_ == 1) toggleShape();
      else if (focus_ == 3) adjustLength(1);
      else adjust(1, fine);
      return true;
    default:
      break;
  }
  if (!ui_event.alt && !ui_event.meta && (ui_event.key == '\n' || ui_event.key == '\r')) {
    if (focus_ == 0 && !ui_event.ctrl) { openControllerList(); return true; }
    if (focus_ == 1 && !ui_event.ctrl) { toggleSweep(); return true; }
  }
  // Ctrl+A returns the focused value to its standard setting, as the TB303 knobs do.
  const char key = ui_event.ctrl && ui_event.key >= 1 && ui_event.key <= 26
      ? static_cast<char>('a' + ui_event.key - 1)
      : ui_event.key;
  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta && (key == 'a' || key == 'A')) {
    if (focus_ == 0) {
      const uint8_t controller = GroovePuterMidi::gCutoffControl[voice_index_].controller();
      requested_ = kStandardValue;
      GroovePuterMidi::gCutoffControl[voice_index_].request(Request::kManual,
          static_cast<uint8_t>(kStandardValue), controller);
      return true;
    }
    if (focus_ == 2) {
      target_ = kStandardTarget;
      return true;
    }
    return false;
  }
  return false;
}

void SynthMidiPage::openControllerList() {
  const uint8_t current = GroovePuterMidi::gCutoffControl[voice_index_].controller();
  popup_.open("CONTROLLER", GroovePuterMidi::kControllerCount, current < GroovePuterMidi::kControllerCount ? current : 0);
}

void SynthMidiPage::selectController(uint8_t index) {
  requested_ = -1;
  GroovePuterMidi::gCutoffControl[voice_index_].request(Request::kSelectController, 0, index);
}

void SynthMidiPage::adjustLength(int direction) {
  auto& channel = GroovePuterMidi::gCutoffControl[voice_index_];
  int next = channel.durationIndex() + direction;
  if (next < 0) next = 0;
  if (next >= GroovePuterMidi::kDurationCount) next = GroovePuterMidi::kDurationCount - 1;
  channel.request(Request::kSetDuration, static_cast<uint8_t>(next));
}

void SynthMidiPage::toggleShape() {
  const bool returning = GroovePuterMidi::gCutoffControl[voice_index_].returning();
  GroovePuterMidi::gCutoffControl[voice_index_].request(Request::kSetShape, returning ? 0 : 1);
}

void SynthMidiPage::adjust(int direction, bool fine) {
  const int step = fine ? kStepFine : kStepCoarse;
  auto& channel = GroovePuterMidi::gCutoffControl[voice_index_];
  if (focus_ == 0) {
    // Step from the last value this page requested, not from the published one: the
    // dispatcher publishes once per pass, so two quick presses would otherwise step
    // from the same base and add only one step.
    const int base = requested_ >= 0 ? requested_
                   : channel.known() ? channel.value() : kStandardValue;
    int next = base + direction * step;
    next = next < 0 ? 0 : next > 127 ? 127 : next;
    requested_ = next;
    channel.request(Request::kManual, static_cast<uint8_t>(next), channel.controller());
    return;
  }
  if (focus_ == 2) {
    int next = target_ + direction * step;
    target_ = static_cast<uint8_t>(next < 0 ? 0 : next > 127 ? 127 : next);
  }
}

void SynthMidiPage::toggleSweep() {
  auto& channel = GroovePuterMidi::gCutoffControl[voice_index_];
  if (channel.sweeping()) {
    channel.request(Request::kCancelSweep, 0);
    UI::showToast("SWEEP STOPPED", 900);
    return;
  }
  if (!channel.known()) {
    UI::showToast("SET THE VALUE FIRST", 1200);
    return;
  }
  // The sweep is musical: it needs the transport running and never arms itself.
  if (!mini_acid_.isPlaying()) {
    UI::showToast("PLAY FIRST", 1000);
    return;
  }
  channel.request(Request::kStartSweep, target_, channel.controller());
  char toast[32];
  std::snprintf(toast, sizeof(toast), "SWEEP %s %d BARS",
                channel.returning() ? "RETURN" : "RAMP",
                GroovePuterMidi::kDurations[channel.durationIndex()]);
  UI::showToast(toast, 900);
}
