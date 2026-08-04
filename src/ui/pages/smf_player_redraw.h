#pragma once
#ifndef GROOVEPUTER_SMF_PLAYER_REDRAW_H
#define GROOVEPUTER_SMF_PLAYER_REDRAW_H

#include <cstdint>
#include <cstring>

#include "../screen_geometry.h"
#include "../ui_core.h"
#include "../ui_theme.h"
#include "smf_player_session_state.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/smf_track_mute.h"
#include "src/midi/transport_clock_runtime.h"

namespace GroovePuterUi {

// Intercepts LayoutManager::clearContent only while the MIDI Player is the
// active page. Existing draw functions remain unchanged; changed text rows are
// erased before they are redrawn, while the animated progress/wave region is
// refreshed every frame. Other pages retain the original full-clear behavior.
class SmfPlayerPartialRedrawState {
public:
    bool interceptContentClear(IGfx& gfx) {
        SmfPlayerSessionState& session = smfPlayerSessionState();
        if (!session.active()) return false;

        const SmfPlayerSessionSnapshot view = session.snapshot();
        const uint32_t activationEpoch = session.activationEpoch();
        GroovePuterMidi::ISmfPlayerService* player =
            GroovePuterMidi::smfPlayerService();

        if (!initialized_ || activationEpoch != activationEpoch_ ||
            view.browserVisible || view.performanceVisible ||
            view.inspectorVisible || player == nullptr) {
            clearFull(gfx);
            initialized_ = true;
            activationEpoch_ = activationEpoch;
            havePlayerSnapshot_ = false;
            if (player != nullptr && !view.browserVisible &&
                !view.performanceVisible && !view.inspectorVisible) {
                capture(*player);
            }
            return true;
        }

        const GroovePuterMidi::SmfPlayerSnapshot current = player->snapshot();
        const GroovePuterMidi::SmfTrackMuteSnapshot tracks =
            GroovePuterMidi::smfTrackMuteState().snapshot();
        const GroovePuterMidi::TransportClockRuntimeSnapshot clock =
            GroovePuterMidi::transportClockRuntime().snapshot();

        if (!havePlayerSnapshot_) {
            clearFull(gfx);
        } else {
            if (current.state != previous_.state ||
                current.rawRouting != previous_.rawRouting ||
                current.tempoMode != previous_.tempoMode ||
                current.velocityBoost != previous_.velocityBoost) {
                clearLine(gfx, 0);
            }
            if (std::strncmp(current.filename, previous_.filename,
                             sizeof(current.filename)) != 0) {
                clearLine(gfx, 1);
            }
            if (current.bar != previous_.bar ||
                current.beat != previous_.beat ||
                current.bpmX10 != previous_.bpmX10) {
                clearLine(gfx, 2);
            }

            // The progress bar and MIDI wave overlay are the only intentionally
            // animated region. Refresh it every frame without clearing the rest
            // of the page.
            clearLine(gfx, 3);

            if (current.currentTick != previous_.currentTick ||
                current.totalBars != previous_.totalBars ||
                tracks.trackCount != previousTracks_.trackCount ||
                tracks.selectedTrack != previousTracks_.selectedTrack ||
                tracks.mutedMask != previousTracks_.mutedMask) {
                clearLine(gfx, 4);
            }

            if (current.tempoScalePermille != previous_.tempoScalePermille ||
                current.originalBpmX10 != previous_.originalBpmX10 ||
                clock.source != previousClock_.source ||
                clock.externalState != previousClock_.externalState ||
                clock.externalFollowEnabled !=
                    previousClock_.externalFollowEnabled ||
                clock.externalRunning != previousClock_.externalRunning ||
                clock.externalTempoValid !=
                    previousClock_.externalTempoValid ||
                clock.externalBpmQ16 != previousClock_.externalBpmQ16) {
                clearLine(gfx, 5);
            }
            if (clock.source != previousClock_.source) {
                clearLine(gfx, 6);
            }
            if (std::strncmp(current.message, previous_.message,
                             sizeof(current.message)) != 0) {
                clearLine(gfx, 7);
            }
        }

        previous_ = current;
        previousTracks_ = tracks;
        previousClock_ = clock;
        havePlayerSnapshot_ = true;
        return true;
    }

private:
    static int lineY(int line) {
        return Layout::CONTENT.y + Layout::CONTENT_PAD_Y +
               line * Layout::LINE_HEIGHT;
    }

    static void clearLine(IGfx& gfx, int line) {
        const UI::ThemePalette palette = UI::themePalette();
        const int y = lineY(line) - 1;
        gfx.fillRect(Layout::CONTENT.x,
                     y,
                     Layout::CONTENT.w,
                     Layout::LINE_HEIGHT + 2,
                     palette.background);
    }

    static void clearFull(IGfx& gfx) {
        const UI::ThemePalette palette = UI::themePalette();
        gfx.fillRect(Layout::CONTENT.x,
                     Layout::CONTENT.y,
                     Layout::CONTENT.w,
                     Layout::CONTENT.h,
                     palette.background);
    }

    void capture(GroovePuterMidi::ISmfPlayerService& player) {
        previous_ = player.snapshot();
        previousTracks_ = GroovePuterMidi::smfTrackMuteState().snapshot();
        previousClock_ = GroovePuterMidi::transportClockRuntime().snapshot();
        havePlayerSnapshot_ = true;
    }

    GroovePuterMidi::SmfPlayerSnapshot previous_{};
    GroovePuterMidi::SmfTrackMuteSnapshot previousTracks_{};
    GroovePuterMidi::TransportClockRuntimeSnapshot previousClock_{};
    uint32_t activationEpoch_{0};
    bool initialized_{false};
    bool havePlayerSnapshot_{false};
};

inline SmfPlayerPartialRedrawState& smfPlayerPartialRedrawState() {
    static SmfPlayerPartialRedrawState state;
    return state;
}

inline bool interceptSmfPlayerContentClear(IGfx& gfx) {
    return smfPlayerPartialRedrawState().interceptContentClear(gfx);
}

}  // namespace GroovePuterUi

#endif  // GROOVEPUTER_SMF_PLAYER_REDRAW_H
