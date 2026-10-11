#pragma once
#ifndef GROOVEPUTER_UI_PAGES_SMF_LOOP_KEYS_H
#define GROOVEPUTER_UI_PAGES_SMF_LOOP_KEYS_H

#include <algorithm>
#include <cstdio>

#include "../ui_common.h"
#include "src/midi/smf_player_service.h"

// MIDI player loop keys, shared by MIDI PLAYER and HUB MIDI so a loop can be
// set where the layers are chosen: L cycles OFF -> SONG -> A-B, A marks the
// start, E marks the end (and loops A-B). The toast predicts the result with
// the same rules the player applies.
enum class SmfLoopKey : uint8_t { CycleMode, MarkStart, MarkEnd };

namespace smf_loop_keys_detail {
using namespace GroovePuterMidi;

inline void handleSmfLoopKey(ISmfPlayerService& player,
                             const SmfPlayerSnapshot& state,
                             SmfLoopKey key) {
    if (state.state == SmfPlayerState::Unloaded || state.state == SmfPlayerState::Error) {
        UI::showToast("LOAD MIDI FIRST", 800);
        return;
    }
    SmfLoopRegion region{};
    region.mode = state.loopMode;
    region.startBar = state.loopStartBar;
    region.endBar = state.loopEndBar;
    const uint32_t totalBars = std::max<uint32_t>(state.totalBars, 1u);
    bool queued = false;
    if (key == SmfLoopKey::MarkStart) {
        markSmfLoopStart(region, state.bar, totalBars);
        queued = player.markLoopStart();
    } else if (key == SmfLoopKey::MarkEnd) {
        markSmfLoopEnd(region, state.bar, totalBars);
        queued = player.markLoopEnd();
    } else {
        cycleSmfLoopMode(region, state.bar, totalBars);
        queued = player.cycleLoopMode();
    }
    char toast[32];
    if (!queued) {
        std::snprintf(toast, sizeof(toast), "MIDI PLAYER BUSY");
    } else if (key == SmfLoopKey::MarkStart && region.mode != SmfLoopMode::Section) {
        std::snprintf(toast, sizeof(toast), "LOOP A = BAR %lu",
                      static_cast<unsigned long>(region.startBar));
    } else if (region.mode == SmfLoopMode::Song) {
        std::snprintf(toast, sizeof(toast), "LOOP: WHOLE SONG");
    } else if (region.mode == SmfLoopMode::Section) {
        std::snprintf(toast, sizeof(toast), "LOOP: BARS %lu-%lu",
                      static_cast<unsigned long>(region.startBar),
                      static_cast<unsigned long>(region.endBar));
    } else {
        std::snprintf(toast, sizeof(toast), "LOOP: OFF");
    }
    UI::showToast(toast, 900);
}
}  // namespace smf_loop_keys_detail

using smf_loop_keys_detail::handleSmfLoopKey;

#endif  // GROOVEPUTER_UI_PAGES_SMF_LOOP_KEYS_H
