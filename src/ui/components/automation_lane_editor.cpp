#include "automation_lane_editor.h"

#include "../ui_colors.h"
#include "../ui_utils.h"

namespace {
int clampIndex(int value, int maxValue) {
  if (value < 0) return 0;
  if (value > maxValue) return maxValue;
  return value;
}
}

AutomationLaneEditor::AutomationLaneEditor(MiniAcid& mini_acid, AudioGuard& audio_guard, int voice_index)
  : mini_acid_(mini_acid),
    audio_guard_(audio_guard),
    voice_index_(voice_index),
    param_id_(TB303ParamId::Cutoff),
    cursor_x_(0),
    cursor_y_(0) {}

void AutomationLaneEditor::setParamId(TB303ParamId id) {
  param_id_ = id;
}

void AutomationLaneEditor::withAudioGuard(const std::function<void()>& fn) {
  if (audio_guard_) {
    audio_guard_(fn);
    return;
  }
  fn();
}

void AutomationLaneEditor::clampCursor() {
  cursor_x_ = clampIndex(cursor_x_, kXSteps - 1);
  cursor_y_ = clampIndex(cursor_y_, ySteps() - 1);
}

void AutomationLaneEditor::setCursorFromPoint(int x, int y) {
  Rect bounds = graphBounds();
  if (bounds.w <= 1 || bounds.h <= 1) return;
  int rel_x = x - bounds.x;
  int rel_y = y - bounds.y;
  int span_x = bounds.w - 1;
  int span_y = bounds.h - 1;
  if (span_x < 1 || span_y < 1) return;
  int steps_y = ySteps();
  if (steps_y < 1) steps_y = 1;
  int xi = (rel_x * (kXSteps - 1) + span_x / 2) / span_x;
  int yi = ((span_y - rel_y) * (steps_y - 1) + span_y / 2) / span_y;
  cursor_x_ = clampIndex(xi, kXSteps - 1);
  cursor_y_ = clampIndex(yi, steps_y - 1);
}

uint8_t AutomationLaneEditor::cursorValue() const {
  int steps_y = ySteps();
  if (steps_y <= 1) return 0;
  const Parameter& param = mini_acid_.parameter303(param_id_, voice_index_);
  const AutomationLane* lane = mini_acid_.automationLane303(param_id_, voice_index_);
  bool option_lane = param.hasOptions() || (lane && lane->hasOptions());
  if (option_lane) {
    return static_cast<uint8_t>(clampIndex(cursor_y_, steps_y - 1));
  }
  int value = (cursor_y_ * 255 + (steps_y - 1) / 2) / (steps_y - 1);
  return static_cast<uint8_t>(clampIndex(value, 255));
}

int AutomationLaneEditor::valueToYIndex(uint8_t value) const {
  int steps_y = ySteps();
  if (steps_y <= 1) return 0;
  const Parameter& param = mini_acid_.parameter303(param_id_, voice_index_);
  const AutomationLane* lane = mini_acid_.automationLane303(param_id_, voice_index_);
  bool option_lane = param.hasOptions() || (lane && lane->hasOptions());
  if (option_lane) {
    return clampIndex(static_cast<int>(value), steps_y - 1);
  }
  int idx = (static_cast<int>(value) * (steps_y - 1) + 127) / 255;
  return clampIndex(idx, steps_y - 1);
}

int AutomationLaneEditor::ySteps() const {
  const Parameter& param = mini_acid_.parameter303(param_id_, voice_index_);
  const AutomationLane* lane = mini_acid_.automationLane303(param_id_, voice_index_);
  if (param.hasOptions() || (lane && lane->hasOptions())) {
    int count = lane && lane->hasOptions() ? lane->optionCount : param.optionCount();
    if (count < 1) count = 1;
    return count;
  }
  return kDefaultYSteps;
}

int AutomationLaneEditor::xToPixel(int x) const {
  Rect bounds = graphBounds();
  if (bounds.w <= 1) return bounds.x;
  return bounds.x + (x * (bounds.w - 1)) / (kXSteps - 1);
}

int AutomationLaneEditor::yIndexToPixel(int yIndex) const {
  Rect bounds = graphBounds();
  if (bounds.h <= 1) return bounds.y;
  int steps_y = ySteps();
  if (steps_y <= 1) return bounds.y + (bounds.h - 1) / 2;
  return bounds.y + (bounds.h - 1) - (yIndex * (bounds.h - 1)) / (steps_y - 1);
}

Rect AutomationLaneEditor::graphBounds() const {
  Rect bounds = getBoundaries();
  if (bounds.w <= kGraphPadding * 2 || bounds.h <= kGraphPadding * 2) {
    return bounds;
  }
  bounds.x += kGraphPadding;
  bounds.y += kGraphPadding;
  bounds.w -= kGraphPadding * 2;
  bounds.h -= kGraphPadding * 2;
  return bounds;
}

bool AutomationLaneEditor::removeNodeAtCursor() {
  AutomationLane* lane = mini_acid_.editAutomationLane303(param_id_, voice_index_);
  if (!lane) return false;
  AutomationNode* nodes = lane->nodes();
  if (!nodes) return false;
  uint8_t target_y = cursorValue();
  int target_x = cursor_x_;
  int node_idx = -1;
  for (int i = 0; i < lane->nodeCount; ++i) {
    if (nodes[i].x == target_x && nodes[i].y == target_y) {
      node_idx = i;
      break;
    }
  }
  if (node_idx < 0) return false;
  for (int i = node_idx + 1; i < lane->nodeCount; ++i) {
    nodes[i - 1] = nodes[i];
  }
  if (lane->nodeCount > 0) {
    lane->nodeCount -= 1;
  }
  return true;
}

bool AutomationLaneEditor::addNodeAtCursor() {
  AutomationLane* lane = mini_acid_.editAutomationLane303(param_id_, voice_index_);
  if (!lane) return false;
  const Parameter& param = mini_acid_.parameter303(param_id_, voice_index_);
  if ((param.hasOptions() || lane->hasOptions()) && !lane->hasOptions()) {
    int count = param.optionCount();
    if (count > kAutomationMaxOptions) count = kAutomationMaxOptions;
    const char* labels[kAutomationMaxOptions]{};
    for (int i = 0; i < count; ++i) {
      labels[i] = param.optionLabelAt(i);
    }
    lane->setOptions(labels, count);
  }
  if (lane->nodeCount >= kAutomationMaxNodes) return false;
  if (!lane->ensureCapacity(lane->nodeCount + 1)) return false;
  AutomationNode* nodes = lane->nodes();
  if (!nodes) return false;

  int target_x = cursor_x_;
  uint8_t target_y = cursorValue();
  int same_count = 0;
  for (int i = 0; i < lane->nodeCount; ++i) {
    if (nodes[i].x == target_x) {
      if (nodes[i].y == target_y) return false;
      ++same_count;
    }
  }
  if (same_count >= 2) return false;

  int insert_pos = 0;
  while (insert_pos < lane->nodeCount && nodes[insert_pos].x < target_x) {
    ++insert_pos;
  }
  while (insert_pos < lane->nodeCount && nodes[insert_pos].x == target_x) {
    ++insert_pos;
  }
  for (int i = lane->nodeCount; i > insert_pos; --i) {
    nodes[i] = nodes[i - 1];
  }
  nodes[insert_pos] = AutomationNode{static_cast<uint8_t>(target_x), target_y};
  lane->nodeCount += 1;
  return true;
}

bool AutomationLaneEditor::toggleNodeAtCursor() {
  bool removed = false;
  withAudioGuard([&]() { removed = removeNodeAtCursor(); });
  if (removed) return true;
  bool added = false;
  withAudioGuard([&]() { added = addNodeAtCursor(); });
  return added;
}

bool AutomationLaneEditor::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type == MINIACID_MOUSE_DOWN) {
    if (!contains(ui_event.x, ui_event.y)) return false;
    setCursorFromPoint(ui_event.x, ui_event.y);
    return toggleNodeAtCursor();
  }
  if (ui_event.event_type == MINIACID_MOUSE_DRAG) {
    if (!contains(ui_event.x, ui_event.y)) return false;
    setCursorFromPoint(ui_event.x, ui_event.y);
    return true;
  }
  if (ui_event.event_type != MINIACID_KEY_DOWN) return false;
  if (!isFocused()) return false;

  if (ui_event.alt &&
      (ui_event.scancode == MINIACID_LEFT || ui_event.scancode == MINIACID_RIGHT)) {
    const AutomationLane* lane = mini_acid_.automationLane303(param_id_, voice_index_);
    const AutomationNode* nodes = lane ? lane->nodes() : nullptr;
    if (!lane || !nodes || lane->nodeCount <= 0) return false;

    int current_idx = -1;
    uint8_t current_value = cursorValue();
    for (int i = 0; i < lane->nodeCount; ++i) {
      if (nodes[i].x == cursor_x_ && nodes[i].y == current_value) {
        current_idx = i;
        break;
      }
    }

    int target_idx = -1;
    if (current_idx >= 0) {
      if (ui_event.scancode == MINIACID_RIGHT) {
        target_idx = (current_idx + 1) % lane->nodeCount;
      } else {
        target_idx = (current_idx - 1 + lane->nodeCount) % lane->nodeCount;
      }
    } else if (ui_event.scancode == MINIACID_RIGHT) {
      for (int i = 0; i < lane->nodeCount; ++i) {
        if (nodes[i].x > cursor_x_) {
          target_idx = i;
          break;
        }
      }
      if (target_idx < 0) target_idx = 0;
    } else {
      for (int i = lane->nodeCount - 1; i >= 0; --i) {
        if (nodes[i].x < cursor_x_) {
          target_idx = i;
          break;
        }
      }
      if (target_idx < 0) target_idx = lane->nodeCount - 1;
    }

    cursor_x_ = nodes[target_idx].x;
    cursor_y_ = valueToYIndex(nodes[target_idx].y);
    return true;
  }

  int dx = 0;
  int dy = 0;
  switch (ui_event.scancode) {
    case MINIACID_LEFT:
      dx = -1;
      break;
    case MINIACID_RIGHT:
      dx = 1;
      break;
    case MINIACID_UP:
      dy = 1;
      break;
    case MINIACID_DOWN:
      dy = -1;
      break;
    default:
      break;
  }
  if (dx != 0 || dy != 0) {
    int new_x = cursor_x_ + dx;
    int new_y = cursor_y_ + dy;
    
    // Check if movement would go out of bounds
    bool out_of_bounds = false;
    if (new_x < 0 || new_x >= kXSteps) out_of_bounds = true;
    if (new_y < 0 || new_y >= ySteps()) out_of_bounds = true;
    
    // If out of bounds, let parent handle focus navigation
    if (out_of_bounds) {
      return false;
    }
    
    // Move cursor within bounds
    cursor_x_ = new_x;
    cursor_y_ = new_y;
    return true;
  }

  if (ui_event.key == '\n' || ui_event.key == '\r') {
    return toggleNodeAtCursor();
  }
  if (ui_event.key == '\b') {
    bool removed = false;
    withAudioGuard([&]() { removed = removeNodeAtCursor(); });
    return removed;
  }

  return false;
}

void AutomationLaneEditor::draw(IGfx& gfx) {
  const Rect& bounds = getBoundaries();
  if (bounds.w <= 0 || bounds.h <= 0) return;
  Rect graph_bounds = graphBounds();

  gfx.fillRect(bounds.x, bounds.y, bounds.w, bounds.h, COLOR_PANEL);

  for (int x = 0; x < kXSteps; ++x) {
    int px = xToPixel(x);
    drawLineColored(gfx, px, graph_bounds.y, px, graph_bounds.y + graph_bounds.h - 1, COLOR_GRAY_DARKER);
  }
  gfx.drawRect(bounds.x, bounds.y, bounds.w, bounds.h,
    isFocused() ? COLOR_STEP_SELECTED : COLOR_LIGHT_GRAY);

  int current_step = mini_acid_.currentStep();
  if (mini_acid_.isPlaying() && current_step >= 0) {
    float progress = mini_acid_.currentStepProgress();
    float play_pos = static_cast<float>(current_step) + progress;
    if (play_pos < 0.0f) play_pos = 0.0f;
    float max_pos = static_cast<float>(kXSteps - 1);
    if (play_pos > max_pos) play_pos = max_pos;
    int play_x = bounds.x;
    if (graph_bounds.w > 1) {
      play_x = graph_bounds.x + static_cast<int>(play_pos * (graph_bounds.w - 1) / max_pos + 0.5f);
    }
    drawLineColored(gfx, play_x, graph_bounds.y, play_x,
                    graph_bounds.y + graph_bounds.h - 1, COLOR_STEP_HILIGHT);
  }

  const Parameter& param = mini_acid_.parameter303(param_id_, voice_index_);
  const AutomationLane* lane = mini_acid_.automationLane303(param_id_, voice_index_);
  const AutomationNode* nodes = lane ? lane->nodes() : nullptr;
  bool option_lane = param.hasOptions() || (lane && lane->hasOptions());
  bool lane_enabled = lane && lane->enabled;
  if (lane && nodes && lane->nodeCount > 0) {
    IGfxColor lane_color = lane_enabled ? COLOR_WAVE : COLOR_LIGHTER_GRAY;
    for (int i = 1; i < lane->nodeCount; ++i) {
      const AutomationNode& prev = nodes[i - 1];
      const AutomationNode& next = nodes[i];
      int x0 = xToPixel(prev.x);
      int x1 = xToPixel(next.x);
      int y0 = yIndexToPixel(valueToYIndex(prev.y));
      if (option_lane) {
        if (prev.x == next.x) {
          int y1 = yIndexToPixel(valueToYIndex(next.y));
          drawLineColored(gfx, x0, y0, x1, y1, COLOR_GRAY_DARKER);
        } else {
          drawLineColored(gfx, x0, y0, x1, y0, lane_color);
        }
      } else {
        int y1 = yIndexToPixel(valueToYIndex(next.y));
        IGfxColor color = prev.x == next.x ? COLOR_GRAY_DARKER : lane_color;
        drawLineColored(gfx, x0, y0, x1, y1, color);
      }
    }
    for (int i = 0; i < lane->nodeCount; ++i) {
      const AutomationNode& node = nodes[i];
      int x = xToPixel(node.x);
      int y = yIndexToPixel(valueToYIndex(node.y));
      IGfxColor node_color = lane_enabled ? COLOR_WAVE : COLOR_LIGHTER_GRAY;
      gfx.fillRect(x - 1, y - 1, 3, 3, node_color);
      gfx.drawRect(x - 2, y - 2, 5, 5, node_color);
    }
  }

  if (option_lane) {
    int option_count = lane && lane->hasOptions() ? lane->optionCount : param.optionCount();
    if (option_count > 0) {
      int font_h = gfx.fontHeight();
      int max_text_y = bounds.y + bounds.h - font_h;
      if (max_text_y < bounds.y) max_text_y = bounds.y;
      gfx.setTextColor(COLOR_LIGHTER_GRAY);
      for (int i = 0; i < option_count; ++i) {
        const char* label = lane && lane->hasOptions() ? lane->optionLabelAt(i) : param.optionLabelAt(i);
        if (!label || !label[0]) continue;
        int y_index = 0;
        if (option_count <= 1) {
          y_index = 0;
        } else {
          y_index = valueToYIndex(static_cast<uint8_t>(i));
        }
        int text_y = yIndexToPixel(y_index) - font_h / 2;
        if (text_y < bounds.y) text_y = bounds.y;
        if (text_y > max_text_y) text_y = max_text_y;
        int text_w = textWidth(gfx, label);
        int text_x = bounds.x + bounds.w - text_w - 2;
        if (text_x < bounds.x + 1) text_x = bounds.x + 1;
        gfx.drawText(text_x, text_y, label);
      }
    }
  }

  int cursor_px = xToPixel(cursor_x_);
  int cursor_py = yIndexToPixel(cursor_y_);
  gfx.drawRect(cursor_px - 3, cursor_py - 3, 7, 7,
               isFocused() ? COLOR_STEP_SELECTED : COLOR_GRAY);
}
