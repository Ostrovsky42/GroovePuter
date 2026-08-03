#pragma once

#include "src/state/ui_session_state.h"

namespace GroovePuterPlatform {

bool loadCardputerUiSession(GroovePuterState::UiSessionState& state);
bool saveCardputerUiSession(const GroovePuterState::UiSessionState& state);

}  // namespace GroovePuterPlatform
