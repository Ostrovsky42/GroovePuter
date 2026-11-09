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
constexpr int kRowCount = 4;        // CUTOFF, SWEEP, TARGET, DURATION
constexpr int kFocusableRows = 3;   // DURATION is fixed: 4 bars
constexpr int kRowHeight = 14;
constexpr int kStepCoarse = 5;
constexpr int kStepFine = 1;
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
  auto& channel = GroovePuterMidi::gCutoffControl[voice_index_];
  const bool sweeping = channel.sweeping();

  // Which SEQTRAK part this page drives: SYNTH 1 is CH8, SYNTH 2 is CH9.
  char route[40]{};
  std::snprintf(route, sizeof(route), "SEQTRAK SYNTH %d  CH%02d  CC74",
                voice_index_ == 0 ? 1 : 2, voice_index_ == 0 ? 8 : 9);
  gfx.setTextColor(UI::themePalette().voice(voice_index_));
  gfx.drawText(x, content.y + 2, route);

  const char* labels[kRowCount] = {"CUTOFF", "SWEEP", "TARGET", "DURATION"};
  char values[kRowCount][24]{};
  std::snprintf(values[0], sizeof(values[0]), "<  %u  >", static_cast<unsigned>(channel.value()));
  std::snprintf(values[1], sizeof(values[1]), "%s", sweeping ? "RUNNING" : "OFF");
  std::snprintf(values[2], sizeof(values[2]), "<  %u  >", static_cast<unsigned>(target_));
  std::snprintf(values[3], sizeof(values[3]), "4 BAR");

  for (int row = 0; row < kRowCount; ++row) {
    const int y = content.y + 18 + row * kRowHeight;
    const bool focused = row == focus_ && row < kFocusableRows;
    if (focused) gfx.fillRect(x - 2, y - 1, width + 4, kRowHeight, UI::themePalette().voice(voice_index_));
    gfx.setTextColor(focused ? IGfxColor::Black() : IGfxColor::White());
    gfx.drawText(x + 2, y + 2, labels[row]);
    gfx.drawText(x + width - gfx.textWidth(values[row]) - 2, y + 2, values[row]);
  }

  gfx.setTextColor(IGfxColor(0x808080));
  gfx.drawText(x, content.y + 18 + kRowCount * kRowHeight + 4,
               sweeping ? "ENTER STOPS SWEEP" : "ENTER STARTS 4-BAR SWEEP");
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
    const int base = GroovePuterMidi::gCutoffControl[voice_index_].value();
    int next = base + direction * step;
    next = next < 0 ? 0 : next > 127 ? 127 : next;
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
  if (!mini_acid_.isPlaying()) {
    UI::showToast("PLAY FIRST", 1000);
    return;
  }
  channel.request(GroovePuterMidi::CutoffRequest::kStartSweep, target_);
  UI::showToast("SWEEP 4 BARS", 900);
}
