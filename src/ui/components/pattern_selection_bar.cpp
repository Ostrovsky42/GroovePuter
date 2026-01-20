#include "pattern_selection_bar.h"

#include <cstdio>
#include <utility>

#include "../ui_colors.h"
#include "../ui_utils.h"

PatternSelectionBarComponent::PatternSelectionBarComponent(std::string label)
    : label_(std::move(label)) {}

void PatternSelectionBarComponent::setState(const State& state) {
  state_ = state;
}

void PatternSelectionBarComponent::setCallbacks(Callbacks callbacks) {
  callbacks_ = std::move(callbacks);
}

int PatternSelectionBarComponent::barHeight(IGfx& gfx) const {
  Layout layout{};
  if (!computeLayout(gfx, layout)) return 0;
  last_layout_ = layout;
  last_layout_valid_ = true;
  return layout.bar_height;
}

bool PatternSelectionBarComponent::computeLayout(IGfx& gfx, Layout& layout) const {
  const Rect& bounds = getBoundaries();
  layout.bounds_x = bounds.x;
  layout.bounds_y = bounds.y;
  layout.bounds_w = bounds.w;
  if (layout.bounds_w <= 0) return false;

  layout.columns = state_.columns > 0 ? state_.columns : 8;
  if (layout.columns < 1) layout.columns = 1;

  layout.pattern_size = (layout.bounds_w - layout.spacing * (layout.columns - 1) - 2) / layout.columns;
  if (layout.pattern_size < 12) layout.pattern_size = 12;
  layout.pattern_height = layout.pattern_size / 2;
  layout.row_y = layout.bounds_y;

  int count = state_.pattern_count;
  if (count < 1) count = 1;
  layout.rows = (count + layout.columns - 1) / layout.columns;
  layout.row_spacing = layout.rows > 1 ? 2 : 0;
  layout.bar_height = layout.rows * layout.pattern_height +
                      (layout.rows - 1) * layout.row_spacing;
  return true;
}

bool PatternSelectionBarComponent::handleEvent(UIEvent& ui_event) {
  // Handle keyboard events when focused
  if (ui_event.event_type == MINIACID_KEY_DOWN && isFocused()) {
    int cursor = state_.cursor_index;
    if (cursor < 0) cursor = 0;
    if (cursor >= state_.pattern_count) cursor = state_.pattern_count - 1;
    
    switch (ui_event.scancode) {
      case MINIACID_LEFT:
        if (cursor > 0) {
          if (callbacks_.onCursorMove) {
            callbacks_.onCursorMove(cursor - 1);
          } else if (callbacks_.onSelect) {
            callbacks_.onSelect(cursor - 1);
          }
          return true;
        }
        return false;  // At left edge, let parent handle
      case MINIACID_RIGHT:
        if (cursor < state_.pattern_count - 1) {
          if (callbacks_.onCursorMove) {
            callbacks_.onCursorMove(cursor + 1);
          } else if (callbacks_.onSelect) {
            callbacks_.onSelect(cursor + 1);
          }
          return true;
        }
        return false;  // At right edge, let parent handle
      default:
        break;
    }
    
    if (ui_event.key == '\n' || ui_event.key == '\r') {
      if (callbacks_.onSelect) {
        callbacks_.onSelect(cursor);
      }
      return true;
    }
  }
  
  if (ui_event.event_type != MINIACID_MOUSE_DOWN) return false;
  if (ui_event.button != MOUSE_BUTTON_LEFT) return false;
  if (!contains(ui_event.x, ui_event.y)) return false;

  if (!last_layout_valid_) return false;
  const Layout& layout = last_layout_;
  if (ui_event.y < layout.row_y ||
      ui_event.y >= layout.row_y + layout.rows * layout.pattern_height +
                         (layout.rows - 1) * layout.row_spacing) {
    return false;
  }

  int col = (ui_event.x - layout.bounds_x) / (layout.pattern_size + layout.spacing);
  int row = (ui_event.y - layout.row_y) / (layout.pattern_height + layout.row_spacing);
  if (col < 0 || col >= layout.columns || row < 0 || row >= layout.rows) return false;

  int index = row * layout.columns + col;
  int cell_x = layout.bounds_x + col * (layout.pattern_size + layout.spacing);
  int cell_y = layout.row_y + row * (layout.pattern_height + layout.row_spacing);
  if (ui_event.x >= cell_x + layout.pattern_size || ui_event.y >= cell_y + layout.pattern_height) {
    return false;
  }
  if (index < 0 || index >= state_.pattern_count) return false;
  if (callbacks_.onSelect) {
    callbacks_.onSelect(index);
  }
  return true;
}

void PatternSelectionBarComponent::draw(IGfx& gfx) {
  Layout layout{};
  if (!computeLayout(gfx, layout)) return;
  last_layout_ = layout;
  last_layout_valid_ = true;

  bool songMode = state_.song_mode;
  int count = state_.pattern_count;
  if (count < 0) count = 0;
  int cursor = state_.cursor_index;
  int selected = state_.selected_index;
  bool showCursor = state_.show_cursor || isFocused();

  for (int i = 0; i < count; ++i) {
    int row = i / layout.columns;
    int col = i % layout.columns;
    int cell_x = layout.bounds_x + col * (layout.pattern_size + layout.spacing);
    int cell_y = layout.row_y + row * (layout.pattern_height + layout.row_spacing);
    bool isCursor = showCursor && cursor == i;
    IGfxColor bg = songMode ? COLOR_GRAY_DARKER : COLOR_PANEL;
    gfx.fillRect(cell_x, cell_y, layout.pattern_size, layout.pattern_height, bg);
    if (selected == i) {
      IGfxColor sel = songMode ? IGfxColor::Yellow() : COLOR_PATTERN_SELECTED_FILL;
      IGfxColor border = songMode ? IGfxColor::Yellow() : COLOR_LABEL;
      gfx.fillRect(cell_x - 1, cell_y - 1, layout.pattern_size + 2, layout.pattern_height + 2, sel);
      gfx.drawRect(cell_x - 1, cell_y - 1, layout.pattern_size + 2, layout.pattern_height + 2, border);
    }
    gfx.drawRect(cell_x, cell_y, layout.pattern_size, layout.pattern_height,
                 songMode ? COLOR_LABEL : COLOR_WHITE);
    if (isCursor) {
      gfx.drawRect(cell_x - 2, cell_y - 2, layout.pattern_size + 4, layout.pattern_height + 4,
                   COLOR_STEP_SELECTED);
    }
    char label[4];
    snprintf(label, sizeof(label), "%d", i + 1);
    int tw = textWidth(gfx, label);
    int tx = cell_x + (layout.pattern_size - tw) / 2;
    int ty = cell_y + layout.pattern_height / 2 - gfx.fontHeight() / 2;
    gfx.setTextColor(songMode ? COLOR_LABEL : COLOR_WHITE);
    gfx.drawText(tx, ty, label);
    gfx.setTextColor(COLOR_WHITE);
  }
}
