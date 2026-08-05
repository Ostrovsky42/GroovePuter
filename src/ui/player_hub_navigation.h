#pragma once
#ifndef GROOVEPUTER_PLAYER_HUB_NAVIGATION_H
#define GROOVEPUTER_PLAYER_HUB_NAVIGATION_H

#include <cstdint>

#include "workflow_mode.h"

namespace PlayerHubNavigation {

// Context is UI-only. It asks the existing Hub page to open its MIDI
// projection and remember that Back should return to the Player page. It does
// not carry transport, scheduler, file or snapshot ownership.
constexpr int kOpenMidiFromPlayerContext = 0x4D49;  // "MI"
constexpr int kReturnToPlayerContext = 0x504C;      // "PL"
constexpr int kHubPage = WorkflowPages::kPattern;
constexpr int kPlayerPage = WorkflowPages::kPlayer;

// The page cache may evict either page under low internal heap. Preserve only
// the bounded UI view state needed for a lossless Player -> Hub -> Player
// round-trip. Loaded file, playback position, mute state and selected physical
// track remain owned by their existing runtime services.
struct PlayerViewState {
    uint32_t generation{0};
    int channelInspectorScroll{0};
    bool valid{false};
    bool browserVisible{false};
    bool performanceVisible{false};
    bool channelInspectorVisible{false};
    bool muteMixerVisible{false};
    bool structuralInspectorVisible{false};
};

inline PlayerViewState& playerViewState() {
    static PlayerViewState state{};
    return state;
}

}  // namespace PlayerHubNavigation

#endif  // GROOVEPUTER_PLAYER_HUB_NAVIGATION_H
