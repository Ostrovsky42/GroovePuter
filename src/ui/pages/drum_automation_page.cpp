#include "drum_automation_page.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "../layout_manager.h"
#include "../ui_common.h"
#include "../ui_input.h"

DrumAutomationPage::DrumAutomationPage(MiniAcid& mini_acid)
    : mini_acid_(mini_acid) {}

const DrumPatternSet& DrumAutomationPage::patternSet() const {
  return mini_acid_.sceneManager().getCurrentDrumPattern();
}

DrumPatternSet& DrumAutomationPage::editPatternSet() {
  return mini_acid_.sceneManager().editCurrentDrumPattern();
}

const AutomationLane& DrumAutomationPage::lane() const {
  int laneIdx = lane_index_;
  if (laneIdx < 0) laneIdx = 0;
  if (laneIdx >= DrumPatternSet::kMaxLanes) laneIdx = DrumPatternSet::kMaxLanes - 1;
  return patternSet().lanes[laneIdx];
}

AutomationLane& DrumAutomationPage::editLane() {
  int laneIdx = lane_index_;
  if (laneIdx < 0) laneIdx = 0;
  if (laneIdx >= DrumPatternSet::kMaxLanes) laneIdx = DrumPatternSet::kMaxLanes - 1;
  return editPatternSet().lanes[laneIdx];
}

bool DrumAutomationPage::hasNode() const {
  return lane().nodeCount > 0;
}

AutomationNode& DrumAutomationPage::editNode() {
  normalizeNodeIndex();
  return editLane().nodes[node_index_];
}

const AutomationNode& DrumAutomationPage::node() const {
  int idx = node_index_;
  if (idx < 0) idx = 0;
  const AutomationLane& l = lane();
  int maxIdx = l.nodeCount > 0 ? static_cast<int>(l.nodeCount) - 1 : 0;
  if (idx > maxIdx) idx = maxIdx;
  return l.nodes[idx];
}

void DrumAutomationPage::normalizeNodeIndex() {
  int count = static_cast<int>(lane().nodeCount);
  if (count <= 0) {
    node_index_ = 0;
    return;
  }
  if (node_index_ < 0) node_index_ = 0;
  if (node_index_ >= count) node_index_ = count - 1;
}

void DrumAutomationPage::addNode() {
  AutomationLane& l = editLane();
  if (l.nodeCount >= AutomationLane::kMaxNodes) return;
  int idx = static_cast<int>(l.nodeCount);
  AutomationNode& n = l.nodes[idx];
  if (idx == 0) {
    int step = mini_acid_.currentStep();
    if (step < 0) step = 0;
    if (step > 15) step = 15;
    n.step = static_cast<uint8_t>(step);
    n.value = 0.5f;
    n.curveType = 0;
  } else {
    const AutomationNode& prev = l.nodes[idx - 1];
    n.step = static_cast<uint8_t>((prev.step + 2) % 16);
    n.value = prev.value;
    n.curveType = prev.curveType;
  }
  l.nodeCount = static_cast<uint8_t>(idx + 1);
  node_index_ = idx;
}

void DrumAutomationPage::removeNode() {
  AutomationLane& l = editLane();
  if (l.nodeCount == 0) return;
  normalizeNodeIndex();
  for (int i = node_index_; i + 1 < static_cast<int>(l.nodeCount); ++i) {
    l.nodes[i] = l.nodes[i + 1];
  }
  l.nodeCount = static_cast<uint8_t>(l.nodeCount - 1);
  normalizeNodeIndex();
}

const char* DrumAutomationPage::targetName(uint8_t target) const {
  switch (target) {
    case DRUM_AUTOMATION_REVERB_MIX: return "REV MIX";
    case DRUM_AUTOMATION_COMPRESSION: return "COMP";
    case DRUM_AUTOMATION_TRANSIENT_ATTACK: return "TRANS ATT";
    case DRUM_AUTOMATION_ENGINE_SWITCH: return "ENGINE";
    case DRUM_AUTOMATION_NONE:
    default:
      return "OFF";
  }
}

const char* DrumAutomationPage::curveName(uint8_t curve) const {
  switch (curve) {
    case 1: return "EASE IN";
    case 2: return "EASE OUT";
    default: return "LINEAR";
  }
}

int DrumAutomationPage::maxRowIndex() const {
  return static_cast<int>(Row::Count) - 1;
}

void DrumAutomationPage::moveRow(int delta) {
  int idx = static_cast<int>(row_) + delta;
  const int max = maxRowIndex();
  while (idx < 0) idx += (max + 1);
  while (idx > max) idx -= (max + 1);
  row_ = static_cast<Row>(idx);
}

void DrumAutomationPage::adjustRowValue(int delta) {
  if (delta == 0) return;
  switch (row_) {
    case Row::Lane: {
      int next = lane_index_ + delta;
      const int count = DrumPatternSet::kMaxLanes;
      while (next < 0) next += count;
      while (next >= count) next -= count;
      lane_index_ = next;
      normalizeNodeIndex();
      break;
    }
    case Row::Target: {
      static const uint8_t kTargets[] = {
          DRUM_AUTOMATION_NONE,
          DRUM_AUTOMATION_REVERB_MIX,
          DRUM_AUTOMATION_COMPRESSION,
          DRUM_AUTOMATION_TRANSIENT_ATTACK,
          DRUM_AUTOMATION_ENGINE_SWITCH,
      };
      int idx = 0;
      uint8_t current = lane().targetParam;
      for (int i = 0; i < static_cast<int>(sizeof(kTargets)); ++i) {
        if (kTargets[i] == current) {
          idx = i;
          break;
        }
      }
      idx += delta;
      const int count = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));
      while (idx < 0) idx += count;
      while (idx >= count) idx -= count;
      editLane().targetParam = kTargets[idx];
      break;
    }
    case Row::NodeIndex: {
      if (!hasNode()) {
        if (delta > 0) addNode();
        break;
      }
      int idx = node_index_ + delta;
      int count = static_cast<int>(lane().nodeCount);
      while (idx < 0) idx += count;
      while (idx >= count) idx -= count;
      node_index_ = idx;
      break;
    }
    case Row::NodeStep: {
      if (!hasNode()) break;
      AutomationNode& n = editNode();
      int step = static_cast<int>(n.step) + delta;
      while (step < 0) step += 16;
      while (step >= 16) step -= 16;
      n.step = static_cast<uint8_t>(step);
      break;
    }
    case Row::NodeValue: {
      if (!hasNode()) break;
      AutomationNode& n = editNode();
      n.value = std::clamp(n.value + 0.05f * static_cast<float>(delta), 0.0f, 1.0f);
      break;
    }
    case Row::NodeCurve: {
      if (!hasNode()) break;
      AutomationNode& n = editNode();
      int curve = static_cast<int>(n.curveType) + delta;
      while (curve < 0) curve += 3;
      while (curve > 2) curve -= 3;
      n.curveType = static_cast<uint8_t>(curve);
      break;
    }
    case Row::GrooveSwing: {
      PatternGroove& groove = editPatternSet().groove;
      if (groove.swing < 0.0f) {
        if (delta > 0) groove.swing = 0.0f;
      } else {
        float next = groove.swing + 0.02f * static_cast<float>(delta);
        if (next < 0.0f) groove.swing = -1.0f;
        else groove.swing = std::clamp(next, 0.0f, 0.66f);
      }
      break;
    }
    case Row::GrooveHumanize: {
      PatternGroove& groove = editPatternSet().groove;
      if (groove.humanize < 0.0f) {
        if (delta > 0) groove.humanize = 0.0f;
      } else {
        float next = groove.humanize + 0.05f * static_cast<float>(delta);
        if (next < 0.0f) groove.humanize = -1.0f;
        else groove.humanize = std::clamp(next, 0.0f, 1.0f);
      }
      break;
    }
    default:
      break;
  }
}

bool DrumAutomationPage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;
  if (UIInput::isTab(ui_event)) return false;

  int nav = UIInput::navCode(ui_event);
  if (nav == GROOVEPUTER_UP) {
    moveRow(-1);
    return true;
  }
  if (nav == GROOVEPUTER_DOWN) {
    moveRow(1);
    return true;
  }
  if (nav == GROOVEPUTER_LEFT) {
    adjustRowValue(-1);
    return true;
  }
  if (nav == GROOVEPUTER_RIGHT) {
    adjustRowValue(1);
    return true;
  }

  char key = ui_event.key;
  if (!key) return false;

  if (key == '\n' || key == '\r') {
    if (row_ == Row::NodeIndex) {
      addNode();
      return true;
    }
    if (row_ == Row::GrooveSwing) {
      editPatternSet().groove.swing = -1.0f;
      return true;
    }
    if (row_ == Row::GrooveHumanize) {
      editPatternSet().groove.humanize = -1.0f;
      return true;
    }
    return true;
  }

  if (key == 'n' || key == 'N') {
    addNode();
    return true;
  }
  if (key == 'x' || key == 'X' || key == '\b' || key == 0x7F) {
    removeNode();
    return true;
  }
  return false;
}

void DrumAutomationPage::draw(IGfx& gfx) {
  UI::drawStandardHeader(gfx, mini_acid_, "DRUM AUTOMATION");
  LayoutManager::clearContent(gfx);

  const int x = Layout::CONTENT.x + Layout::CONTENT_PAD_X;
  const int listW = Layout::COL_2 - x - 2;
  const int valX = x + 70;

  char bufLane[40];
  char bufTarget[32];
  char bufNodeIdx[40];
  char bufNodeStep[20];
  char bufNodeValue[20];
  char bufNodeCurve[20];
  char bufSwing[24];
  char bufHumanize[24];

  const AutomationLane& activeLane = lane();
  const bool nodeOk = hasNode();
  const AutomationNode& activeNode = nodeOk ? node() : activeLane.nodes[0];

  std::snprintf(bufLane, sizeof(bufLane), "L%d/%d  N:%d",
                lane_index_ + 1, DrumPatternSet::kMaxLanes, activeLane.nodeCount);
  std::snprintf(bufTarget, sizeof(bufTarget), "%s", targetName(activeLane.targetParam));
  if (nodeOk) {
    std::snprintf(bufNodeIdx, sizeof(bufNodeIdx), "%d/%d",
                  node_index_ + 1, static_cast<int>(activeLane.nodeCount));
    std::snprintf(bufNodeStep, sizeof(bufNodeStep), "S%02d", activeNode.step + 1);
    std::snprintf(bufNodeValue, sizeof(bufNodeValue), "%d%%",
                  static_cast<int>(std::round(activeNode.value * 100.0f)));
    std::snprintf(bufNodeCurve, sizeof(bufNodeCurve), "%s", curveName(activeNode.curveType));
  } else {
    std::snprintf(bufNodeIdx, sizeof(bufNodeIdx), "NONE");
    std::snprintf(bufNodeStep, sizeof(bufNodeStep), "--");
    std::snprintf(bufNodeValue, sizeof(bufNodeValue), "--");
    std::snprintf(bufNodeCurve, sizeof(bufNodeCurve), "--");
  }

  const PatternGroove& groove = patternSet().groove;
  if (groove.swing >= 0.0f) {
    std::snprintf(bufSwing, sizeof(bufSwing), "%d%%",
                  static_cast<int>(std::round(groove.swing * 100.0f)));
  } else {
    std::snprintf(bufSwing, sizeof(bufSwing), "AUTO");
  }
  if (groove.humanize >= 0.0f) {
    std::snprintf(bufHumanize, sizeof(bufHumanize), "%d%%",
                  static_cast<int>(std::round(groove.humanize * 100.0f)));
  } else {
    std::snprintf(bufHumanize, sizeof(bufHumanize), "AUTO");
  }

  auto drawRow = [&](int line, const char* label, const char* value, bool focused) {
    const int y = LayoutManager::lineY(line);
    if (focused) {
      gfx.drawRect(x, y - 1, listW, Layout::LINE_HEIGHT - 1, COLOR_ACCENT);
    }
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(x + 2, y + 1, label);
    gfx.setTextColor(focused ? COLOR_ACCENT : COLOR_WHITE);
    gfx.drawText(valX, y + 1, value);
  };

  drawRow(0, "LANE", bufLane, row_ == Row::Lane);
  drawRow(1, "TARGET", bufTarget, row_ == Row::Target);
  drawRow(2, "NODE", bufNodeIdx, row_ == Row::NodeIndex);
  drawRow(3, "STEP", bufNodeStep, row_ == Row::NodeStep);
  drawRow(4, "VALUE", bufNodeValue, row_ == Row::NodeValue);
  drawRow(5, "CURVE", bufNodeCurve, row_ == Row::NodeCurve);
  drawRow(6, "SWING", bufSwing, row_ == Row::GrooveSwing);
  drawRow(7, "HUMAN", bufHumanize, row_ == Row::GrooveHumanize);

  // Compact lane graph on the right side.
  const int gx = Layout::COL_2 + 2;
  const int gy = LayoutManager::lineY(1);
  const int gw = Layout::SCREEN_W - gx - 4;
  const int gh = Layout::LINE_HEIGHT * 5;
  gfx.drawRect(gx, gy, gw, gh, COLOR_GRAY);

  for (int s = 0; s <= 15; s += 4) {
    int sx = gx + 1 + (s * (gw - 3)) / 15;
    gfx.drawLine(sx, gy + 1, sx, gy + gh - 2, COLOR_DARKER);
  }

  int playStep = mini_acid_.currentStep();
  float stepProg = mini_acid_.getStepProgress();
  if (playStep < 0) playStep = 0;
  if (playStep > 15) playStep %= 16;
  
  // Use floating point for sub-step smooth playhead motion
  float smoothStep = (float)playStep + stepProg;
  int playheadX = gx + 1 + (int)(smoothStep * (float)(gw - 3) / 15.0f);
  
  gfx.drawLine(playheadX, gy + 1, playheadX, gy + gh - 2, COLOR_STEP_HILIGHT);

  if (activeLane.nodeCount > 0) {
    struct Point {
      int step;
      float value;
      int srcIndex;
    };
    Point points[AutomationLane::kMaxNodes];
    int pointCount = static_cast<int>(activeLane.nodeCount);
    if (pointCount > AutomationLane::kMaxNodes) pointCount = AutomationLane::kMaxNodes;
    for (int i = 0; i < pointCount; ++i) {
      points[i].step = std::clamp(static_cast<int>(activeLane.nodes[i].step), 0, 15);
      points[i].value = std::clamp(activeLane.nodes[i].value, 0.0f, 1.0f);
      points[i].srcIndex = i;
    }
    std::sort(points, points + pointCount, [](const Point& a, const Point& b) {
      return a.step < b.step;
    });

    int prevX = -1;
    int prevY = -1;
    for (int i = 0; i < pointCount; ++i) {
      int px = gx + 1 + (points[i].step * (gw - 3)) / 15;
      int py = gy + gh - 2 - static_cast<int>(std::round(points[i].value * static_cast<float>(gh - 3)));
      if (prevX >= 0) {
        gfx.drawLine(prevX, prevY, px, py, COLOR_INFO);
      }
      IGfxColor pColor = (points[i].srcIndex == node_index_ && row_ >= Row::NodeIndex && row_ <= Row::NodeCurve)
                           ? COLOR_ACCENT
                           : COLOR_INFO;
      gfx.fillRect(px - 1, py - 1, 3, 3, pColor);
      prevX = px;
      prevY = py;
    }
  }

  UI::drawStandardFooter(gfx, "TAB:SubPg ARW:Edit N:+ X:-", "ENT:AUTO");
}
