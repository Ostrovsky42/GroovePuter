#pragma once

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"

class DrumAutomationPage : public Container {
 public:
  explicit DrumAutomationPage(MiniAcid& mini_acid);
  bool handleEvent(UIEvent& ui_event) override;
  void draw(IGfx& gfx) override;

 private:
  enum class Row : uint8_t {
    Lane = 0,
    Target = 1,
    NodeIndex = 2,
    NodeStep = 3,
    NodeValue = 4,
    NodeCurve = 5,
    GrooveSwing = 6,
    GrooveHumanize = 7,
    Count = 8,
  };

  const DrumPatternSet& patternSet() const;
  DrumPatternSet& editPatternSet();
  const AutomationLane& lane() const;
  AutomationLane& editLane();
  bool hasNode() const;
  AutomationNode& editNode();
  const AutomationNode& node() const;

  void normalizeNodeIndex();
  void adjustRowValue(int delta);
  void addNode();
  void removeNode();

  const char* targetName(uint8_t target) const;
  const char* curveName(uint8_t curve) const;
  int maxRowIndex() const;
  void moveRow(int delta);

  MiniAcid& mini_acid_;
  int lane_index_ = 0;
  int node_index_ = 0;
  Row row_ = Row::Lane;
};
