#pragma once
#ifndef GROOVEPUTER_PLAYER_HUB_NAVIGATION_H
#define GROOVEPUTER_PLAYER_HUB_NAVIGATION_H

#include "workflow_mode.h"

namespace PlayerHubNavigation {

// Context is UI-only. It asks the existing Hub page to open its MIDI
// projection and remember that Back should return to the still-live Player
// page. It does not carry transport, scheduler, file or snapshot ownership.
constexpr int kOpenMidiFromPlayerContext = 0x4D49;  // "MI"
constexpr int kHubPage = WorkflowPages::kPattern;
constexpr int kPlayerPage = WorkflowPages::kPlayer;

}  // namespace PlayerHubNavigation

#endif  // GROOVEPUTER_PLAYER_HUB_NAVIGATION_H
