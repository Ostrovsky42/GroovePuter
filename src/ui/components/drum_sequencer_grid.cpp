#include "drum_sequencer_grid.h"
#include "../retro_widgets.h"
#include "../amber_widgets.h"
#include "../retro_ui_theme.h"
#include "../amber_ui_theme.h"

namespace retro = RetroWidgets;
namespace amber = AmberWidgets;

DrumSequencerGridComponent::DrumSequencerGridComponent(MiniAcid& mini_acid, Callbacks callbacks)
    : mini_acid_(mini_acid), callbacks_(std::move(callbacks)) {}

bool DrumSequencerGridComponent::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_MOUSE_DOWN) return false;
  if (ui_event.button != MOUSE_BUTTON_LEFT) return false;
  if (!contains(ui_event.x, ui_event.y)) return false;

  GridLayout layout{};
  if (!computeLayout(layout)) return false;
  int step = (ui_event.x - layout.grid_x) / layout.cell_w;
  if (step < 0 || step >= SEQ_STEPS) {
    return false;
  }

  if (ui_event.y >= layout.accent_y && ui_event.y < layout.accent_bottom) {
    if (callbacks_.onToggleAccent) {
      callbacks_.onToggleAccent(step);
    }
    return true;
  }

  if (ui_event.y < layout.grid_y || ui_event.y >= layout.grid_bottom) return false;
  int voice = (ui_event.y - layout.grid_y) / layout.stripe_h;
  if (voice < 0 || voice >= NUM_DRUM_VOICES) return false;
  if (callbacks_.onToggle) callbacks_.onToggle(step, voice);
  return true;
}

void DrumSequencerGridComponent::draw(IGfx& gfx) {
  GridLayout layout{};
  if (!computeLayout(layout)) return;

  switch (style_) {
    case GrooveboxStyle::RETRO_CLASSIC:
      drawRetroClassicStyle(gfx, layout);
      break;
    case GrooveboxStyle::AMBER:
      drawAmberStyle(gfx, layout);
      break;
    default:
      drawMinimalStyle(gfx, layout);
      break;
  }
}

void DrumSequencerGridComponent::drawMinimalStyle(IGfx& gfx, const GridLayout& layout) {
  const char* voiceLabels[NUM_DRUM_VOICES] = {"BD", "SD", "CH", "OH", "MT", "HT", "RS", "CP"};
  for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
    int labelStripeH = layout.stripe_h;
    if (labelStripeH < 3) labelStripeH = 3;
    int ly = layout.grid_y + v * labelStripeH + (labelStripeH - gfx.fontHeight()) / 2;
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(layout.bounds_x, ly, voiceLabels[v]);
  }
  gfx.setTextColor(COLOR_WHITE);

  int cursorStep = callbacks_.cursorStep ? callbacks_.cursorStep() : 0;
  int cursorVoice = callbacks_.cursorVoice ? callbacks_.cursorVoice() : 0;
  bool gridFocus = callbacks_.gridFocused ? callbacks_.gridFocused() : false;
  auto isSelected = [this](int step, int voice) {
    return callbacks_.isSelected ? callbacks_.isSelected(step, voice) : false;
  };

  const bool* kick = mini_acid_.patternKickSteps();
  const bool* snare = mini_acid_.patternSnareSteps();
  const bool* hat = mini_acid_.patternHatSteps();
  const bool* openHat = mini_acid_.patternOpenHatSteps();
  const bool* midTom = mini_acid_.patternMidTomSteps();
  const bool* highTom = mini_acid_.patternHighTomSteps();
  const bool* rim = mini_acid_.patternRimSteps();
  const bool* clap = mini_acid_.patternClapSteps();
  const bool* accentSteps = mini_acid_.patternDrumAccentSteps();
  int highlight = callbacks_.currentStep ? callbacks_.currentStep() : 0;

  const bool* hits[NUM_DRUM_VOICES] = {kick, snare, hat, openHat, midTom, highTom, rim, clap};
  const IGfxColor colors[NUM_DRUM_VOICES] = {COLOR_DRUM_KICK, COLOR_DRUM_SNARE, COLOR_DRUM_HAT,
                                             COLOR_DRUM_OPEN_HAT, COLOR_DRUM_MID_TOM,
                                             COLOR_DRUM_HIGH_TOM, COLOR_DRUM_RIM, COLOR_DRUM_CLAP};

  // Accent row
  for (int i = 0; i < SEQ_STEPS; ++i) {
    int cw = layout.cell_w;
    int cx = layout.grid_x + i * cw;
    IGfxColor fill = accentSteps[i] ? COLOR_ACCENT : COLOR_GRAY_DARKER;
    gfx.fillRect(cx, layout.accent_y, cw - 1, layout.accent_h, fill);
    gfx.drawRect(cx, layout.accent_y, cw - 1, layout.accent_h, COLOR_WHITE);
    if (highlight == i) {
      gfx.drawRect(cx - 1, layout.accent_y - 1, cw + 1, layout.accent_h + 1, COLOR_STEP_HILIGHT);
    }
  }

  // Grid cells
  for (int i = 0; i < SEQ_STEPS; ++i) {
    int cw = layout.cell_w;
    int ch = layout.stripe_h;
    if (ch < 3) ch = 3;
    int cx = layout.grid_x + i * cw;
    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
      int cy = layout.grid_y + v * layout.stripe_h;
      bool hit = hits[v][i];
      IGfxColor fill = hit ? colors[v] : COLOR_GRAY;
      if (!hit && (i % 4 == 0)) {
        fill = COLOR_LIGHT_GRAY;
      }
      gfx.fillRect(cx, cy, cw - 1, ch - 1, fill);
      if (highlight == i) {
        gfx.drawRect(cx - 1, cy - 1, cw + 1, ch + 1, COLOR_STEP_HILIGHT);
      }
      if (isSelected(i, v)) {
        gfx.drawRect(cx - 1, cy - 1, cw + 1, ch + 1, COLOR_ACCENT);
      }
      if (gridFocus && i == cursorStep && v == cursorVoice) {
        gfx.drawRect(cx, cy, cw - 1, ch - 1, COLOR_STEP_SELECTED);
      }
    }
  }
}

void DrumSequencerGridComponent::drawRetroClassicStyle(IGfx& gfx, const GridLayout& layout) {
    const char* voiceLabels[NUM_DRUM_VOICES] = {"BD", "SD", "CH", "OH", "MT", "HT", "RS", "CP"};
    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
        int ly = layout.grid_y + v * layout.stripe_h + (layout.stripe_h - gfx.fontHeight()) / 2;
        gfx.setTextColor(IGfxColor(RetroTheme::TEXT_SECONDARY));
        gfx.drawText(layout.bounds_x, ly, voiceLabels[v]);
    }

    int cursorStep = callbacks_.cursorStep ? callbacks_.cursorStep() : 0;
    int cursorVoice = callbacks_.cursorVoice ? callbacks_.cursorVoice() : 0;
    bool gridFocus = callbacks_.gridFocused ? callbacks_.gridFocused() : false;
    auto isSelected = [this](int step, int voice) {
      return callbacks_.isSelected ? callbacks_.isSelected(step, voice) : false;
    };
    int highlight = callbacks_.currentStep ? callbacks_.currentStep() : 0;
    const bool* accentSteps = mini_acid_.patternDrumAccentSteps();

    const bool* hits[NUM_DRUM_VOICES] = {
        mini_acid_.patternKickSteps(), mini_acid_.patternSnareSteps(),
        mini_acid_.patternHatSteps(), mini_acid_.patternOpenHatSteps(),
        mini_acid_.patternMidTomSteps(), mini_acid_.patternHighTomSteps(),
        mini_acid_.patternRimSteps(), mini_acid_.patternClapSteps()
    };

    for (int i = 0; i < SEQ_STEPS; ++i) {
        int cx = layout.grid_x + i * layout.cell_w;
        IGfxColor fill = accentSteps[i] ? IGfxColor(RetroTheme::STATUS_ACCENT) : IGfxColor(RetroTheme::BG_DARK_GRAY);
        gfx.fillRect(cx, layout.accent_y, layout.cell_w - 1, layout.accent_h, fill);
        gfx.drawRect(cx, layout.accent_y, layout.cell_w - 1, layout.accent_h, IGfxColor(RetroTheme::GRID_DIM));
        if (highlight == i) {
            retro::drawGlowBorder(gfx, cx, layout.accent_y, layout.cell_w - 1, layout.accent_h, IGfxColor(RetroTheme::STATUS_PLAYING), 1);
        }
    }

    for (int i = 0; i < SEQ_STEPS; ++i) {
        int cx = layout.grid_x + i * layout.cell_w;
        for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
            int cy = layout.grid_y + v * layout.stripe_h;
            bool hit = hits[v][i];
            IGfxColor fill = hit ? IGfxColor(RetroTheme::NEON_CYAN) : IGfxColor(RetroTheme::BG_PANEL);
            if (!hit && (i % 4 == 0)) fill = IGfxColor(RetroTheme::BG_DARK_GRAY);

            gfx.fillRect(cx, cy, layout.cell_w - 1, layout.stripe_h - 1, fill);
            gfx.drawRect(cx, cy, layout.cell_w - 1, layout.stripe_h - 1, IGfxColor(RetroTheme::GRID_DIM));

            if (highlight == i) {
                retro::drawGlowBorder(gfx, cx, cy, layout.cell_w - 1, layout.stripe_h - 1, IGfxColor(RetroTheme::STATUS_PLAYING), 1);
            }
            if (isSelected(i, v)) {
                retro::drawGlowBorder(gfx, cx, cy, layout.cell_w - 1, layout.stripe_h - 1, IGfxColor(RetroTheme::STATUS_ACCENT), 1);
            }
            if (gridFocus && i == cursorStep && v == cursorVoice) {
                gfx.drawRect(cx, cy, layout.cell_w - 1, layout.stripe_h - 1, IGfxColor(RetroTheme::SELECT_BRIGHT));
            }
        }
    }
}

void DrumSequencerGridComponent::drawAmberStyle(IGfx& gfx, const GridLayout& layout) {
    const char* voiceLabels[NUM_DRUM_VOICES] = {"BD", "SD", "CH", "OH", "MT", "HT", "RS", "CP"};
    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
        int ly = layout.grid_y + v * layout.stripe_h + (layout.stripe_h - gfx.fontHeight()) / 2;
        gfx.setTextColor(IGfxColor(AmberTheme::TEXT_SECONDARY));
        gfx.drawText(layout.bounds_x, ly, voiceLabels[v]);
    }

    int cursorStep = callbacks_.cursorStep ? callbacks_.cursorStep() : 0;
    int cursorVoice = callbacks_.cursorVoice ? callbacks_.cursorVoice() : 0;
    bool gridFocus = callbacks_.gridFocused ? callbacks_.gridFocused() : false;
    auto isSelected = [this](int step, int voice) {
      return callbacks_.isSelected ? callbacks_.isSelected(step, voice) : false;
    };
    int highlight = callbacks_.currentStep ? callbacks_.currentStep() : 0;
    const bool* accentSteps = mini_acid_.patternDrumAccentSteps();

    const bool* hits[NUM_DRUM_VOICES] = {
        mini_acid_.patternKickSteps(), mini_acid_.patternSnareSteps(),
        mini_acid_.patternHatSteps(), mini_acid_.patternOpenHatSteps(),
        mini_acid_.patternMidTomSteps(), mini_acid_.patternHighTomSteps(),
        mini_acid_.patternRimSteps(), mini_acid_.patternClapSteps()
    };

    for (int i = 0; i < SEQ_STEPS; ++i) {
        int cx = layout.grid_x + i * layout.cell_w;
        IGfxColor fill = accentSteps[i] ? IGfxColor(AmberTheme::NEON_ORANGE) : IGfxColor(AmberTheme::BG_DARK_GRAY);
        gfx.fillRect(cx, layout.accent_y, layout.cell_w - 1, layout.accent_h, fill);
        gfx.drawRect(cx, layout.accent_y, layout.cell_w - 1, layout.accent_h, IGfxColor(AmberTheme::GRID_DIM));
        if (highlight == i) {
            amber::drawGlowBorder(gfx, cx, layout.accent_y, layout.cell_w - 1, layout.accent_h, IGfxColor(AmberTheme::STATUS_PLAYING), 1);
        }
    }

    for (int i = 0; i < SEQ_STEPS; ++i) {
        int cx = layout.grid_x + i * layout.cell_w;
        for (int v = 0; v < NUM_DRUM_VOICES; ++v) {
            int cy = layout.grid_y + v * layout.stripe_h;
            bool hit = hits[v][i];
            IGfxColor fill = hit ? IGfxColor(AmberTheme::NEON_CYAN) : IGfxColor(AmberTheme::BG_PANEL);
            if (!hit && (i % 4 == 0)) fill = IGfxColor(AmberTheme::BG_DARK_GRAY);

            gfx.fillRect(cx, cy, layout.cell_w - 1, layout.stripe_h - 1, fill);
            gfx.drawRect(cx, cy, layout.cell_w - 1, layout.stripe_h - 1, IGfxColor(AmberTheme::GRID_DIM));

            if (highlight == i) {
                amber::drawGlowBorder(gfx, cx, cy, layout.cell_w - 1, layout.stripe_h - 1, IGfxColor(AmberTheme::STATUS_PLAYING), 1);
            }
            if (isSelected(i, v)) {
                amber::drawGlowBorder(gfx, cx, cy, layout.cell_w - 1, layout.stripe_h - 1, IGfxColor(AmberTheme::NEON_ORANGE), 1);
            }
            if (gridFocus && i == cursorStep && v == cursorVoice) {
                gfx.drawRect(cx, cy, layout.cell_w - 1, layout.stripe_h - 1, IGfxColor(AmberTheme::SELECT_BRIGHT));
            }
        }
    }
}

bool DrumSequencerGridComponent::computeLayout(GridLayout& layout) const {
  const Rect& bounds = getBoundaries();
  if (bounds.w <= 0 || bounds.h <= 0) return false;

  layout.bounds_x = bounds.x;
  layout.bounds_y = bounds.y;
  layout.bounds_w = bounds.w;
  layout.bounds_h = bounds.h;

  layout.cell_w = bounds.w / SEQ_STEPS;
  if (layout.cell_w < 1) layout.cell_w = 1;

  layout.accent_h = 6; 
  layout.accent_gap = 2;
  
  layout.grid_x = bounds.x;
  int available_h = bounds.h - (layout.accent_h + layout.accent_gap);
  layout.stripe_h = available_h / NUM_DRUM_VOICES;
  if (layout.stripe_h < 1) layout.stripe_h = 1;

  layout.accent_y = bounds.y;
  layout.grid_y = bounds.y + layout.accent_h + layout.accent_gap;
  
  layout.grid_w = layout.cell_w * SEQ_STEPS;
  layout.grid_h = layout.stripe_h * NUM_DRUM_VOICES;
  
  layout.grid_right = layout.grid_x + layout.grid_w;
  layout.grid_bottom = layout.grid_y + layout.grid_h;
  
  layout.accent_bottom = layout.accent_y + layout.accent_h;

  return true;
}
