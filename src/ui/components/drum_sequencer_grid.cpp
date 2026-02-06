#include "drum_sequencer_grid.h"

DrumSequencerGridComponent::DrumSequencerGridComponent(GroovePuter& mini_acid, Callbacks callbacks)
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
      if (gridFocus && i == cursorStep && v == cursorVoice) {
        gfx.drawRect(cx, cy, cw - 1, ch - 1, COLOR_STEP_SELECTED);
      }
    }
  }
}

bool DrumSequencerGridComponent::computeLayout(GridLayout& layout) const {
  const Rect& bounds = getBoundaries();
  layout.bounds_x = bounds.x;
  layout.bounds_y = bounds.y;
  layout.bounds_w = bounds.w;
  layout.bounds_h = bounds.h;
  if (layout.bounds_w <= 0 || layout.bounds_h <= 0) return false;

  int label_w = 18;
  layout.grid_x = layout.bounds_x + label_w;
  layout.grid_y = layout.bounds_y;
  layout.grid_w = layout.bounds_w - label_w;
  layout.grid_h = layout.bounds_h;
  if (layout.grid_w < 8) layout.grid_w = 8;

  layout.cell_w = layout.grid_w / SEQ_STEPS;
  if (layout.cell_w < 2) return false;
  layout.accent_h = 4;
  layout.accent_gap = 2;
  if (layout.bounds_h < (NUM_DRUM_VOICES * 3 + layout.accent_h + layout.accent_gap)) {
    layout.accent_h = 3;
    layout.accent_gap = 1;
  }
  layout.accent_y = layout.bounds_y;
  layout.accent_bottom = layout.accent_y + layout.accent_h;

  layout.grid_y = layout.bounds_y + layout.accent_h + layout.accent_gap;
  layout.grid_h = layout.bounds_h - (layout.accent_h + layout.accent_gap);
  if (layout.grid_h < NUM_DRUM_VOICES * 3) return false;

  layout.stripe_h = layout.grid_h / NUM_DRUM_VOICES;
  if (layout.stripe_h < 3) layout.stripe_h = 3;

  layout.grid_right = layout.grid_x + layout.cell_w * SEQ_STEPS;
  layout.grid_bottom = layout.grid_y + layout.stripe_h * NUM_DRUM_VOICES;
  return true;
}
