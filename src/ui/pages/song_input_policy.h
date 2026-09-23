#pragma once

namespace GroovePuterSongInput {

enum class AltVerticalRoute {
  None,
  PatternAdjust,
  DelegateLegacy,
};

inline AltVerticalRoute classifyAltVerticalRoute(bool alt,
                                                 bool ctrl,
                                                 bool verticalArrow,
                                                 bool cursorOnPlayheadLabel) {
  if (!alt || !verticalArrow) return AltVerticalRoute::None;
  if (ctrl || cursorOnPlayheadLabel) return AltVerticalRoute::DelegateLegacy;
  return AltVerticalRoute::PatternAdjust;
}

}  // namespace GroovePuterSongInput
