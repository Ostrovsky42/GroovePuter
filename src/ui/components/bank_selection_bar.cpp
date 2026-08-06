#include "bank_selection_bar.h"

#include <utility>

#include "../ui_colors.h"
#include "../ui_utils.h"
#include "../ui_themes.h"

BankSelectionBarComponent::BankSelectionBarComponent(std::string label, std::string letters)
    : label_(std::move(label)), letters_(std::move(letters)) {}

void BankSelectionBarComponent::setState(const State& state) {
  state_ = state;
}

void BankSelectionBarComponent::setCallbacks(Callbacks callbacks) {
  callbacks_ = std::move(callbacks);
}

int BankSelectionBarComponent::barHeight(IGfx& gfx) const {
  Layout layout{};
  if (!computeLayout(gfx, layout)) return 0;
  last_layout_ = layout;
  last_layout_valid_ = true;
  return layout.box_size;
}

bool BankSelectionBarComponent::computeLayout(IGfx& gfx, Layout& layout) const {
  const Rect& bounds = getBoundaries();
  layout.bounds_x = bounds.x;
  layout.bounds_y = bounds.y;
  layout.bounds_w = bounds.w;
  if (layout.bounds_w <= 0) return false;

  layout.label_h = gfx.fontHeight();
  layout.label_y = layout.bounds_y + 1;
  layout.box_size = layout.label_h + 2;
  int bank_count = state_.bank_count;
  if (bank_count < 1) bank_count = 1;

  layout.label_w = textWidth(gfx, label_.c_str());
  int total_w = layout.label_w + layout.spacing + (layout.box_size + layout.spacing) * bank_count -
                layout.spacing;
  layout.bank_x = layout.bounds_x + layout.bounds_w - total_w;
  layout.bank_y = layout.bounds_y;
  return true;
}

char BankSelectionBarComponent::bankLetter(int index) const {
  if (index >= 0 && index < static_cast<int>(letters_.size())) {
    return letters_[index];
  }
  if (index >= 0 && index < 26) {
    return static_cast<char>('A' + index);
  }
  return '?';
}

bool BankSelectionBarComponent::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_MOUSE_DOWN) return false;
  if (ui_event.button != MOUSE_BUTTON_LEFT) return false;
  if (!contains(ui_event.x, ui_event.y)) return false;
  if (!last_layout_valid_) return false;

  const Layout& layout = last_layout_;
  if (ui_event.y < layout.bank_y || ui_event.y >= layout.bank_y + layout.box_size) return false;

  int bank_count = state_.bank_count;
  if (bank_count < 1) return false;
  int box_x = layout.bank_x + layout.label_w + layout.spacing;
  int rel_x = ui_event.x - box_x;
  if (rel_x < 0) return false;
  int stride = layout.box_size + layout.spacing;
  int index = rel_x / stride;
  if (index < 0 || index >= bank_count) return false;
  int cell_x = box_x + index * stride;
  if (ui_event.x >= cell_x + layout.box_size) return false;
  if (callbacks_.onSelect) {
    callbacks_.onSelect(index);
  }
  return true;
}

void BankSelectionBarComponent::draw(IGfx& gfx) {
  Layout layout{};
  if (!computeLayout(gfx, layout)) return;
  last_layout_ = layout;
  last_layout_valid_ = true;

  bool songMode = state_.song_mode;
  int bank_count = state_.bank_count;
  if (bank_count < 0) bank_count = 0;

  const auto& palette = getPalette(g_currentTheme);

  gfx.setTextColor(palette.muted);
  gfx.drawText(layout.bank_x, layout.label_y, label_.c_str());

  int box_x = layout.bank_x + layout.label_w + layout.spacing;
  for (int i = 0; i < bank_count; ++i) {
    int cell_x = box_x + i * (layout.box_size + layout.spacing);
    IGfxColor bg = songMode ? palette.muted : palette.panel;
    IGfxColor border = songMode ? palette.led : palette.ink;
    gfx.fillRect(cell_x, layout.bank_y, layout.box_size, layout.box_size, bg);
    if (state_.selected_index == i) {
      IGfxColor sel = songMode ? palette.led : palette.accent;
      IGfxColor sel_border = songMode ? palette.led : palette.ink;
      gfx.fillRect(cell_x - 1, layout.bank_y - 1, layout.box_size + 2, layout.box_size + 2, sel);
      gfx.drawRect(cell_x - 1, layout.bank_y - 1, layout.box_size + 2, layout.box_size + 2, sel_border);
    }
    gfx.drawRect(cell_x, layout.bank_y, layout.box_size, layout.box_size, border);
    if (state_.show_cursor && state_.cursor_index == i) {
      gfx.drawRect(cell_x - 2, layout.bank_y - 2, layout.box_size + 4, layout.box_size + 4,
                   palette.led);
    }
    char label[2] = { bankLetter(i), '\0' };
    int tw = textWidth(gfx, label);
    int tx = cell_x + (layout.box_size - tw) / 2;
    int ty = layout.bank_y + layout.box_size / 2 - gfx.fontHeight() / 2;
    // Invert text color if selected
    bool isSelected = (state_.selected_index == i);
    gfx.setTextColor(isSelected ? palette.bg : palette.ink);
    gfx.drawText(tx, ty, label);
    gfx.setTextColor(palette.ink);
  }
  gfx.setTextColor(palette.ink);
}
