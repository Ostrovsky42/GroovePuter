#include "synth_sequencer_page.h"

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include "../../../platform_sdl/arduino_compat.h"
#endif

#include <cstdio>

#include "pattern_edit_page.h"
#include "tb303_params_page.h"
#include "../help_dialog_frames.h"
#include "../screen_geometry.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "../ui_theme.h"

namespace {
// The parent owns one compact tab indicator across NOTES, KNOBS and MORE.
// NOTES places it after the eight fixed pattern numbers. Params pages keep it
// immediately left of their mode label at the right edge.
constexpr int kNotesTabStripX = 190;
constexpr int kParamsTabStripX = 172;
constexpr int kTabStripW = 32;
constexpr int kTabStripH = 11;
constexpr int kPatternNumbersX = 106;
constexpr int kPatternNumbersEndX = kPatternNumbersX + 8 * 10 - 1;
static_assert(kNotesTabStripX > kPatternNumbersEndX,
              "NOTES tab must stay after the pattern numbers");
static_assert(kNotesTabStripX + kTabStripW <= Layout::SCREEN_W,
              "NOTES tab must stay on screen");

inline IGfxColor synthTabColor(int voiceIndex) {
  return voiceIndex == 0 ? IGfxColor(0x33C8FF) : IGfxColor(0xFF4FCB);
}
}  // namespace

SynthSequencerPage::SynthSequencerPage(IGfx& gfx,
                                       MiniAcid& mini_acid,
                                       AudioGuard audio_guard,
                                       int voice_index)
    : voice_index_(voice_index) {
  fallback_title_ = (voice_index_ == 0) ? "SYNTH A" : "SYNTH B";

  pattern_page_ = std::make_shared<PatternEditPage>(gfx, mini_acid, audio_guard, voice_index_);
  params_page_ = std::make_shared<TB303ParamsPage>(gfx, mini_acid, audio_guard, voice_index_);
  addPage(pattern_page_);
  addPage(params_page_);
  setSynthTab(SynthTab::Notes);
}

void SynthSequencerPage::setSynthTab(SynthTab tab) {
  synth_tab_ = tab;
  switch (synth_tab_) {
    case SynthTab::Notes:
      setActivePageIndex(0);
      if (params_page_) params_page_->showMoreTab(false);
      break;
    case SynthTab::Knobs:
      setActivePageIndex(1);
      if (params_page_) params_page_->showMoreTab(false);
      break;
    case SynthTab::More:
      setActivePageIndex(1);
      if (params_page_) params_page_->showMoreTab(true);
      break;
  }
}

const char* SynthSequencerPage::activeTabName() const {
  switch (synth_tab_) {
    case SynthTab::Notes: return "NOTES";
    case SynthTab::Knobs: return "KNOBS";
    case SynthTab::More: return "MORE";
  }
  return "NOTES";
}

void SynthSequencerPage::drawTabIndicator(IGfx& gfx) const {
  const char* label = "[N]KM";
  switch (synth_tab_) {
    case SynthTab::Notes: label = "[N]KM"; break;
    case SynthTab::Knobs: label = "N[K]M"; break;
    case SynthTab::More: label = "NK[M]"; break;
  }

  const bool notesTab = synth_tab_ == SynthTab::Notes;
  // MINIMAL has no free inline label slot: its pattern-number cells span the
  // row below status chrome. Suppress the parent strip there rather than hide
  // another pattern address. Full tab names remain in toast/help.
  if (notesTab &&
      UI::currentStyle != VisualStyle::RETRO_CLASSIC &&
      UI::currentStyle != VisualStyle::AMBER) {
    return;
  }
  const int x = notesTab ? kNotesTabStripX : kParamsTabStripX;
  const int y = Layout::CONTENT.y;
  gfx.fillRect(x, y, kTabStripW, kTabStripH, IGfxColor::Black());
  gfx.setTextColor(synthTabColor(voice_index_));
  gfx.drawText(x + (kTabStripW - gfx.textWidth(label)) / 2,
               y + 1,
               label);
}

void SynthSequencerPage::draw(IGfx& gfx) {
  MultiPage::draw(gfx);
  const UI::ThemePalette palette = UI::themePalette();
  gfx.fillRect(Layout::PERFORMANCE_HUD.x,
               Layout::PERFORMANCE_HUD.y,
               Layout::PERFORMANCE_HUD.w,
               Layout::PERFORMANCE_HUD.h,
               palette.background);
  drawTabIndicator(gfx);
}

bool SynthSequencerPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == GROOVEPUTER_KEY_DOWN && UIInput::isTab(ui_event)) {
    if (ui_event.ctrl || ui_event.alt || ui_event.meta) return false;

    const uint32_t now = millis();
    if (last_tab_switch_ms_ != 0 && (now - last_tab_switch_ms_) < 250u) {
      return true;
    }
    last_tab_switch_ms_ = now;

    const int next = (static_cast<int>(synth_tab_) + 1) % 3;
    setSynthTab(static_cast<SynthTab>(next));

    char toast[40];
    std::snprintf(toast, sizeof(toast), "SYNTH %c: %s",
                  voice_index_ == 0 ? 'A' : 'B', activeTabName());
    UI::showToast(toast, 700);
    return true;
  }

  return MultiPage::handleEvent(ui_event);
}

const std::string& SynthSequencerPage::getTitle() const {
  if (synth_tab_ == SynthTab::Notes && pattern_page_) {
    return pattern_page_->getTitle();
  }
  if (params_page_) return params_page_->getTitle();
  return fallback_title_;
}

void SynthSequencerPage::setContext(int context) {
  setSynthTab(SynthTab::Notes);
  if (pattern_page_) pattern_page_->setContext(context);
}

void SynthSequencerPage::setVisualStyle(VisualStyle style) {
  if (pattern_page_) pattern_page_->setVisualStyle(style);
  if (params_page_) params_page_->setVisualStyle(style);
}

void SynthSequencerPage::tick() {
  if (synth_tab_ == SynthTab::Notes && pattern_page_) {
    pattern_page_->tick();
  }
}

std::unique_ptr<MultiPageHelpDialog> SynthSequencerPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int SynthSequencerPage::getHelpFrameCount() const {
  return 2;
}

void SynthSequencerPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) {
    case 0:
      drawHelpPage303PatternEdit(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    case 1:
      drawHelpPage303(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}
