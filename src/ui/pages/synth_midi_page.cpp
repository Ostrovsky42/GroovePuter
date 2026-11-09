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
constexpr int kFocusableRows = 3;   // DURATION is fixed: 4 bars
constexpr int kRowHeight = 14;
constexpr int kStepCoarse = 5;
constexpr int kStepFine = 1;
constexpr uint8_t kStandardCutoff = 64;
constexpr uint8_t kStandardTarget = 110;
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
  auto& channel = GroovePuterMidi::gCutoffControl[voice_index_];
  const bool sweeping = channel.sweeping();
  const int barsDone = channel.barsElapsed();
  const bool known = channel.known();
  if (requested_ >= 0 && channel.known() && channel.value() == requested_) requested_ = -1;

  // Identity: the synth is the title, the external device is secondary.
  const int headerY = content.y + 2;
  const char* synthName = voice_index_ == 0 ? "SYNTH A" : "SYNTH B";
  gfx.setTextColor(palette.voice(voice_index_));
  gfx.drawText(x, headerY, synthName);
  // Left of the synth tab strip (which starts at x 172), so the two never overlap.
  char route[24]{};
  const uint8_t routeChannel = voice_index_ == 0 ? GroovePuterMidi::kSynthAChannel : GroovePuterMidi::kSynthBChannel;
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

  // CUTOFF: the held value, as a bar over 0..127 and the number.
  drawFocus(0);
  gfx.setTextColor(focus_ == 0 ? palette.accent : palette.text);
  gfx.drawText(x + 8, rowY(0) + 2, "CUTOFF");
  if (known) drawBar(0, channel.value(), palette.accent);
  // Until the user sets a value the receiver's value is unknown: show it so.
  char value[8]{};
  if (known) std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(channel.value()));
  else std::snprintf(value, sizeof(value), "--");
  gfx.setTextColor(palette.text);
  gfx.drawText(valueRight - gfx.textWidth(value), rowY(0) + 2, value);

  // SWEEP: OFF, or four segments showing the bars passed plus the state.
  drawFocus(1);
  gfx.setTextColor(focus_ == 1 ? palette.accent : palette.text);
  gfx.drawText(x + 8, rowY(1) + 2, "SWEEP");
  if (sweeping) {
    const int segW = (kBarW - 3) / 4;
    for (int i = 0; i < 4; ++i) {
      const int sx = barX + i * (segW + 1);
      const int sy = rowY(1) + (kRowH - kBarH) / 2;
      if (i < barsDone) {
        gfx.fillRect(sx, sy, segW, kBarH, palette.accent2);
      } else {
        gfx.drawRect(sx, sy, segW, kBarH, palette.dim);
      }
    }
    gfx.setTextColor(palette.accent2);
    gfx.drawText(valueRight - gfx.textWidth("RUNNING"), rowY(1) + 2, "RUNNING");
  } else {
    gfx.setTextColor(palette.secondary);
    gfx.drawText(valueRight - gfx.textWidth("OFF"), rowY(1) + 2, "OFF");
  }

  // TARGET: the value the sweep will reach, on the same 0..127 scale.
  drawFocus(2);
  gfx.setTextColor(focus_ == 2 ? palette.accent : palette.text);
  gfx.drawText(x + 8, rowY(2) + 2, "TARGET");
  drawBar(2, target_, palette.secondary);
  char target[8]{};
  std::snprintf(target, sizeof(target), "%u", static_cast<unsigned>(target_));
  gfx.setTextColor(palette.text);
  gfx.drawText(valueRight - gfx.textWidth(target), rowY(2) + 2, target);

  // TIME: fixed and shown without edit arrows, so it promises nothing editable.
  gfx.setTextColor(palette.text);
  gfx.drawText(x + 8, rowY(3) + 2, "TIME");
  gfx.setTextColor(palette.secondary);
  gfx.drawText(valueRight - gfx.textWidth("4 BAR"), rowY(3) + 2, "4 BAR");

  // Context hint: what Enter does right now.
  gfx.setTextColor(palette.secondary);
  gfx.drawText(x + 8, rowY(3) + kRowStep + 3,
               sweeping ? "ENTER: CANCEL SWEEP" : "ENTER: START 4-BAR SWEEP");
}

bool SynthMidiPage::handleEvent(UIEvent& ui_event) {
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
      adjust(-1, fine);
      return true;
    case GROOVEPUTER_RIGHT:
      adjust(1, fine);
      return true;
    default:
      break;
  }
  // Ctrl+A returns the focused value to its standard setting, as the TB303 knobs do.
  const char key = ui_event.ctrl && ui_event.key >= 1 && ui_event.key <= 26
      ? static_cast<char>('a' + ui_event.key - 1)
      : ui_event.key;
  if (ui_event.ctrl && !ui_event.alt && !ui_event.meta && (key == 'a' || key == 'A')) {
    if (focus_ == 0) {
      requested_ = kStandardCutoff;
      GroovePuterMidi::gCutoffControl[voice_index_].request(GroovePuterMidi::CutoffRequest::kManual, kStandardCutoff);
      return true;
    }
    if (focus_ == 2) {
      target_ = kStandardTarget;
      return true;
    }
    return false;
  }
  if (!ui_event.alt && !ui_event.ctrl && !ui_event.meta &&
      (ui_event.key == '\n' || ui_event.key == '\r') && focus_ == 1) {
    toggleSweep();
    return true;
  }
  return false;
}

void SynthMidiPage::adjust(int direction, bool fine) {
  const int step = fine ? kStepFine : kStepCoarse;
  if (focus_ == 0) {
    // A manual value takes over from a running sweep, by design.
    const auto& control = GroovePuterMidi::gCutoffControl[voice_index_];
    // Step from the last value this page requested, not from the published one: the
    // dispatcher publishes once per pass, so two quick presses would otherwise step
    // from the same base and add only one step.
    const int base = requested_ >= 0 ? requested_
                   : control.known() ? control.value() : kStandardCutoff;
    int next = base + direction * step;
    next = next < 0 ? 0 : next > 127 ? 127 : next;
    requested_ = next;
    GroovePuterMidi::gCutoffControl[voice_index_].request(
        GroovePuterMidi::CutoffRequest::kManual, static_cast<uint8_t>(next));
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
    channel.request(GroovePuterMidi::CutoffRequest::kCancelSweep, 0);
    UI::showToast("SWEEP STOPPED", 900);
    return;
  }
  // The sweep is musical: it needs the transport running and never arms itself.
  if (!channel.known()) {
    UI::showToast("SET CUTOFF FIRST", 1200);
    return;
  }
  if (!mini_acid_.isPlaying()) {
    UI::showToast("PLAY FIRST", 1000);
    return;
  }
  channel.request(GroovePuterMidi::CutoffRequest::kStartSweep, target_);
  UI::showToast("SWEEP 4 BARS", 900);
}
