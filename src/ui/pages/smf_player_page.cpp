#include "smf_player_page.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "../components/music_visuals.h"
#include "src/dsp/miniacid_engine.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/midi/smf_track_mute.h"
#include "src/platform/cardputer_usb_midi_service.h"

#ifdef ARDUINO
#include <SD.h>
#include "../../platform/cardputer_sd.h"
#endif
#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace {
bool smfStateIsActive(GroovePuterMidi::SmfPlayerState state) {
    return state == GroovePuterMidi::SmfPlayerState::Playing ||
           state == GroovePuterMidi::SmfPlayerState::Armed;
}

void formatMidiNote(uint8_t note, char* dst, std::size_t size) {
    static constexpr const char* kNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int octave = static_cast<int>(note / 12u) - 1;
    std::snprintf(dst, size, "%s%d", kNames[note % 12u], octave);
}

const char* inspectorRouteLabel(bool raw, uint8_t sourceChannel) {
    if (raw) return "RAW";
    if (sourceChannel == 0) return "S1";
    if (sourceChannel == 1) return "S2";
    if (sourceChannel == 2) return "DX";
    if (sourceChannel == 9) return "DRM";
    return "OFF";
}
}  // namespace

using namespace GroovePuterMidi;

SmfPlayerPage::SmfPlayerPage(IGfx& gfx,
                             MiniAcid& miniAcid,
                             AudioGuard audioGuard)
    : miniAcid_(miniAcid),
      audioGuard_(audioGuard),
      player_(smfPlayerService()) {
    (void)gfx;
}

void SmfPlayerPage::onEnter(int context) {
    (void)context;
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};
    browserVisible_ = state.state == SmfPlayerState::Unloaded ||
                      state.state == SmfPlayerState::Error;
    if (browserVisible_) GroovePuterUi::midiFileManager().open();
}

bool SmfPlayerPage::loadMidiPath(const char* path) {
    if (!path || path[0] == '\0') {
        UI::showToast("MIDI entry unavailable", 900);
        return true;
    }

    player_ = smfPlayerService();
    if (!player_) {
        UI::showToast("SMF player unavailable", 1200);
        return true;
    }
    const SmfPlayerSnapshot playerState = player_->snapshot();
    if (!player_->requestLoad(path)) {
        UI::showToast("Player queue busy", 1000);
        return true;
    }
    browserVisible_ = false;
    const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
    const bool followSeqtrak = clock.source == TransportClockSource::SeqtrakExternal;
    UI::showToast(playerState.tempoMode == SmfTempoMode::Project
                      ? (followSeqtrak
                             ? (clock.externalFollowEnabled
                                    ? "LOADING / SPACE ARM / SEQ PLAY"
                                    : "LOADING / FOLLOW OFF")
                             : "LOADING / G THEN SPACE")
                      : "LOADING / SPACE TO PLAY",
                  1000);
    return true;
}

bool SmfPlayerPage::togglePlayerTransport() {
    player_ = smfPlayerService();
    if (!player_) {
        UI::showToast("SMF player unavailable", 1200);
        return true;
    }

    const SmfPlayerSnapshot state = player_->snapshot();
    if (state.state == SmfPlayerState::Unloaded ||
        state.state == SmfPlayerState::Error) {
        UI::showToast("ENTER: LOAD MIDI", 900);
        return true;
    }

    const bool wasActive = smfStateIsActive(state.state);
    const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
    if (!wasActive && state.tempoMode == SmfTempoMode::Project &&
        !miniAcid_.isPlaying() &&
        clock.source == TransportClockSource::GroovePuterInternal) {
        UI::showToast("G START FIRST / THEN SPACE", 1100);
        return true;
    }
    const bool queued = player_->togglePlayPause();
    if (!queued) {
        UI::showToast("MIDI PLAYER BUSY", 800);
    } else if (wasActive) {
        UI::showToast("MIDI: PAUSE", 700);
    } else if (state.tempoMode == SmfTempoMode::Project) {
        UI::showToast(clock.source == TransportClockSource::SeqtrakExternal
                          ? (clock.externalFollowEnabled
                                 ? "MIDI ARMED / PLAY SEQTRAK"
                                 : "MIDI ARMED / FOLLOW OFF")
                          : "MIDI: ARM NEXT BAR",
                      900);
    } else {
        UI::showToast("MIDI: PLAY", 700);
    }
    return true;
}

void SmfPlayerPage::toggleGrooveTransport() {
    TransportClockRuntime& clockRuntime = transportClockRuntime();
    if (clockRuntime.source() == TransportClockSource::SeqtrakExternal) {
        const bool enabled = clockRuntime.toggleExternalFollowEnabled();
        UI::showToast(enabled
                          ? "EXT FOLLOW ON / WAIT SEQ"
                          : "EXT FOLLOW OFF / STOP",
                      1000);
        return;
    }
    player_ = smfPlayerService();
    if (miniAcid_.isPlaying()) {
        bool playerPauseQueued = true;
        if (player_) {
            const SmfPlayerSnapshot state = player_->snapshot();
            if (state.tempoMode == SmfTempoMode::Project &&
                smfStateIsActive(state.state)) {
                playerPauseQueued = player_->pause();
            }
        }
        withAudioGuard([this]() { miniAcid_.stop(); });
        UI::showToast(playerPauseQueued
                          ? "GROOVE STOP / MIDI PAUSED"
                          : "GROOVE STOP / MIDI BUSY",
                      900);
    } else {
        withAudioGuard([this]() { miniAcid_.start(); });
        UI::showToast("GROOVE PLAY / SPACE MIDI", 900);
    }
}

bool SmfPlayerPage::handleEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN || event.alt || event.ctrl || event.meta) {
        return false;
    }

    player_ = smfPlayerService();

    if (event.key == 'c' || event.key == 'C') {
        const TransportClockSource source = transportClockRuntime().toggleSource();
        const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
        UI::showToast(source == TransportClockSource::SeqtrakExternal &&
                              !clock.externalFollowEnabled
                          ? "SEQ MASTER / FOLLOW OFF"
                          : transportClockSourceName(source),
                      1000);
        return true;
    }

    if (event.key == ' ') return togglePlayerTransport();
    if (event.key == 'g' || event.key == 'G') {
        toggleGrooveTransport();
        return true;
    }

    if (browserVisible_) {
        char activatedPath[GroovePuterUi::MidiFileManager::kPathBytes]{};
        const auto result = GroovePuterUi::midiFileManager().handleEvent(
            event, activatedPath, sizeof(activatedPath));
        if (result == GroovePuterUi::MidiFileManager::EventResult::FileActivated) {
            return loadMidiPath(activatedPath);
        }
        if (result == GroovePuterUi::MidiFileManager::EventResult::CloseRequested) {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                if (state.state != SmfPlayerState::Unloaded &&
                    state.state != SmfPlayerState::Error) {
                    browserVisible_ = false;
                    return true;
                }
            }
            return false;
        }
        if (result == GroovePuterUi::MidiFileManager::EventResult::Consumed) {
            return true;
        }
        if (event.key == 'm' || event.key == 'M') {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                const bool queued = player_->toggleRouting();
                UI::showToast(queued
                                  ? (state.rawRouting ? "ROUTE: SEQTRAK SAFE" : "ROUTE: RAW")
                                  : "MIDI PLAYER BUSY",
                              850);
            }
            return true;
        }
        if (event.key == 't' || event.key == 'T') {
            if (player_) {
                const SmfPlayerSnapshot state = player_->snapshot();
                const bool toProject = state.tempoMode == SmfTempoMode::Original;
                const bool queued = player_->toggleTempoMode();
                const bool followSeqtrak = transportClockRuntime().source() ==
                    TransportClockSource::SeqtrakExternal;
                UI::showToast(queued
                                  ? (toProject
                                         ? (followSeqtrak
                                                ? "TEMPO: SEQ MASTER"
                                                : "TEMPO: GP MASTER > USB")
                                         : "TEMPO: FILE ORIGINAL")
                                  : "MIDI PLAYER BUSY",
                              900);
            }
            return true;
        }
        return false;
    }

    if (!player_) return false;
    const SmfPlayerSnapshot state = player_->snapshot();

    if (event.key == 'i' || event.key == 'I') {
        channelInspectorVisible_ = !channelInspectorVisible_;
        if (channelInspectorVisible_) {
            performanceVisible_ = false;
            channelInspectorScroll_ = 0;
        }
        return true;
    }
    if (channelInspectorVisible_) {
        const SmfChannelInspectorSnapshot inspector = player_->channelInspector();
        constexpr int kVisibleRows = 6;
        const int maxScroll = std::max(
            0, static_cast<int>(inspector.usedChannelCount()) - kVisibleRows);
        if (event.scancode == GROOVEPUTER_UP) {
            channelInspectorScroll_ = std::max(0, channelInspectorScroll_ - 1);
            return true;
        }
        if (event.scancode == GROOVEPUTER_DOWN) {
            channelInspectorScroll_ = std::min(maxScroll, channelInspectorScroll_ + 1);
            return true;
        }
        if (event.scancode == GROOVEPUTER_LEFT ||
            event.scancode == GROOVEPUTER_RIGHT) {
            return true;
        }
    }

    if (event.scancode == GROOVEPUTER_LEFT) {
        player_->seekBars(event.shift ? -4 : -1);
        return true;
    }
    if (event.scancode == GROOVEPUTER_RIGHT) {
        player_->seekBars(event.shift ? 4 : 1);
        return true;
    }
    if (event.scancode == GROOVEPUTER_UP ||
        event.scancode == GROOVEPUTER_DOWN) {
        const int deltaBpm = event.scancode == GROOVEPUTER_UP ? 1 : -1;
        if (state.tempoMode == SmfTempoMode::Project) {
            if (transportClockRuntime().source() == TransportClockSource::SeqtrakExternal) {
                UI::showToast("SEQ MASTER BPM", 700);
            } else {
                withAudioGuard([this, deltaBpm]() {
                    const float targetBpm = std::max(
                        10.0f,
                        std::min(250.0f,
                                 miniAcid_.bpm() + static_cast<float>(deltaBpm)));
                    miniAcid_.setBpm(targetBpm);
                });
                UI::showToast("GP MASTER BPM / USB CLOCK", 700);
            }
        } else {
            const bool queued = player_->adjustTempoBpm(deltaBpm);
            UI::showToast(queued ? "MIDI: BPM SET / PAUSE" : "MIDI PLAYER BUSY", 800);
        }
        return true;
    }
    if (event.key == 'o' || event.key == 'O') {
        if (state.tempoMode == SmfTempoMode::Project) {
            UI::showToast(transportClockRuntime().source() == TransportClockSource::SeqtrakExternal
                              ? "SEQ MASTER uses SEQTRAK BPM"
                              : "GP MASTER uses GroovePuter BPM",
                          900);
        } else {
            const bool queued = player_->resetTempo();
            UI::showToast(queued ? "MIDI: TEMPO ORIGINAL" : "MIDI PLAYER BUSY", 800);
        }
        return true;
    }
    if (event.key == 't' || event.key == 'T') {
        const bool toProject = state.tempoMode == SmfTempoMode::Original;

        const bool modeQueued = player_->toggleTempoMode();
        if (!modeQueued) {
            UI::showToast("MIDI PLAYER BUSY", 900);
        } else if (toProject) {
            if (transportClockRuntime().source() == TransportClockSource::SeqtrakExternal) {
                UI::showToast("SEQ MASTER: SPACE ARM / SEQ PLAY", 1000);
            } else {
                UI::showToast(miniAcid_.isPlaying()
                                  ? "GP MASTER: ARM NEXT BAR"
                                  : "GP MASTER: G START FIRST",
                              1000);
            }
        } else {
            UI::showToast("FILE TEMPO / ORIGINAL", 1000);
        }
        return true;
    }
    if (event.key == 'j' || event.key == 'J' ||
        event.key == 'l' || event.key == 'L') {
        smfTrackMuteState().selectRelative(
            (event.key == 'j' || event.key == 'J') ? -1 : 1);
        const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();
        char toast[40];
        if (tracks.trackCount == 0) {
            std::snprintf(toast, sizeof(toast), "NO MIDI TRACKS");
        } else {
            std::snprintf(toast, sizeof(toast), "TRACK %u/%u %s",
                          static_cast<unsigned>(tracks.selectedTrack + 1u),
                          static_cast<unsigned>(tracks.trackCount),
                          tracks.selectedMuted() ? "MUTED" : "ON");
        }
        UI::showToast(toast, 800);
        return true;
    }
    if (event.key == 'k' || event.key == 'K') {
        if (event.shift) {
            smfTrackMuteState().clear();
            UI::showToast("ALL MIDI TRACKS ON", 900);
        } else if (smfTrackMuteState().toggleSelected()) {
            const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();
            UI::showToast(tracks.selectedMuted()
                              ? "TRACK MUTE: NEXT NOTES"
                              : "TRACK UNMUTED",
                          900);
        } else {
            UI::showToast("NO MIDI TRACKS", 900);
        }
        return true;
    }
    if (event.key == 'v' || event.key == 'V') {
        const bool queued = player_->cycleVelocityBoost();
        UI::showToast(queued ? "MIDI: VELOCITY BOOST" : "MIDI PLAYER BUSY", 800);
        return true;
    }
    if (event.key == 'r' || event.key == 'R') {
        const bool queued = player_->restart(SmfPlayerRestartOrigin::MusicStart);
        const bool followSeqtrak = transportClockRuntime().source() == TransportClockSource::SeqtrakExternal;
        UI::showToast(queued
                          ? (state.tempoMode == SmfTempoMode::Project && !miniAcid_.isPlaying()
                                 ? (followSeqtrak
                                        ? "MIDI RESTART ARMED - SEQ PLAY"
                                        : "MIDI RESTART ARMED - G START")
                                 : "MIDI: RESTART")
                          : "MIDI PLAYER BUSY",
                      900);
        return true;
    }
    if (event.key == 'd' || event.key == 'D') {
        performanceVisible_ = !performanceVisible_;
        if (performanceVisible_) channelInspectorVisible_ = false;
        return true;
    }
    if (event.key == 'x' || event.key == 'X') {
        const bool queued = player_->panic();
        UI::showToast(queued ? "MIDI PANIC / PAUSE" : "PANIC QUEUE BUSY", 900);
        return true;
    }
    if (event.key == 'b' || event.key == 'B' ||
        event.key == '\n' || event.key == '\r' || event.key == '\b') {
        browserVisible_ = true;
        channelInspectorVisible_ = false;
        GroovePuterUi::midiFileManager().open();
        return true;
    }
    if (event.key == 'm' || event.key == 'M') {
        const bool queued = player_->toggleRouting();
        UI::showToast(queued
                          ? (state.rawRouting ? "ROUTE: SEQTRAK SAFE" : "ROUTE: RAW")
                          : "MIDI PLAYER BUSY",
                      850);
        return true;
    }
    return false;
}

void SmfPlayerPage::drawHeader(IGfx& gfx) {
    UI::drawStandardHeader(gfx, miniAcid_, channelInspectorVisible_
        ? "MIDI CHANNELS"
        : (performanceVisible_ ? "MIDI PERF" : "MIDI PLAYER"));
}

void SmfPlayerPage::drawContent(IGfx& gfx) {
    LayoutManager::clearContent(gfx);
    if (browserVisible_) drawBrowser(gfx);
    else if (channelInspectorVisible_) drawChannelInspector(gfx);
    else if (performanceVisible_) drawPerformance(gfx);
    else drawNowPlaying(gfx);
}

void SmfPlayerPage::drawBrowser(IGfx& gfx) {
    const Rect midiBrowserBounds(Layout::CONTENT.x, Layout::CONTENT.y,
                                 Layout::CONTENT.w, Layout::CONTENT.h);
    GroovePuterUi::midiFileManager().draw(gfx, midiBrowserBounds, "PLAYER");
}

void SmfPlayerPage::drawNowPlaying(IGfx& gfx) {
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};
    const SmfTrackMuteSnapshot tracks = smfTrackMuteState().snapshot();

    const bool playing = state.state == SmfPlayerState::Playing;
    const bool armed = state.state == SmfPlayerState::Armed;
    const bool error = state.state == SmfPlayerState::Error;
    const IGfxColor stateColor = error ? COLOR_DANGER
                                      : ((playing || armed)
                                          ? MusicVisuals::accentForStyle()
                                          : COLOR_WARN);

    int x = Layout::COL_1;
    const int chipY = LayoutManager::lineY(0);
    x += MusicVisuals::drawChip(gfx, x, chipY, smfPlayerStateName(state.state), true, stateColor) + 3;
    x += MusicVisuals::drawChip(gfx, x, chipY,
                                state.rawRouting ? "RAW" : "SEQTRAK", true,
                                MusicVisuals::secondaryForStyle()) + 3;
    x += MusicVisuals::drawChip(gfx, x, chipY,
                                smfTempoModeName(state.tempoMode), true,
                                state.tempoMode == SmfTempoMode::Project
                                    ? MusicVisuals::accentForStyle()
                                    : MusicVisuals::secondaryForStyle()) + 3;

    char chip[20];
    std::snprintf(chip, sizeof(chip), "+%uV", static_cast<unsigned>(state.velocityBoost));
    MusicVisuals::drawChip(gfx, x, chipY, chip, state.velocityBoost > 0);

    gfx.setTextColor(COLOR_TEXT);
    char line[64];
    std::snprintf(line, sizeof(line), "%.38s", state.filename[0] ? state.filename : "--");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    gfx.setTextColor(MusicVisuals::accentForStyle());
    std::snprintf(line, sizeof(line), "BAR %lu.%u     %u.%u BPM",
                  static_cast<unsigned long>(state.bar),
                  static_cast<unsigned>(state.beat),
                  static_cast<unsigned>(state.bpmX10 / 10),
                  static_cast<unsigned>(state.bpmX10 % 10));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), line);

    const uint32_t total = state.endTick > 0 ? state.endTick : 1;
    const uint32_t current = std::min(state.currentTick, total);
    MusicVisuals::drawProgressBar(gfx,
                                  Layout::COL_1,
                                  LayoutManager::lineY(3) + 1,
                                  Layout::CONTENT.w - 12,
                                  9,
                                  current,
                                  total,
                                  stateColor);
    drawMidiWaveOverlay(gfx, state,
                        Rect(Layout::COL_1 + 2, LayoutManager::lineY(3) + 3,
                             Layout::CONTENT.w - 16, 5),
                        MusicVisuals::secondaryForStyle());

    const unsigned percent = static_cast<unsigned>((static_cast<uint64_t>(current) * 100u) / total);
    gfx.setTextColor(tracks.selectedMuted() ? COLOR_WARN : COLOR_LABEL);
    if (tracks.trackCount > 0) {
        std::snprintf(line, sizeof(line), "%lu/%lu BAR %u%%  TRK %u/%u %s",
                      static_cast<unsigned long>(state.bar),
                      static_cast<unsigned long>(state.totalBars),
                      percent,
                      static_cast<unsigned>(tracks.selectedTrack + 1u),
                      static_cast<unsigned>(tracks.trackCount),
                      tracks.selectedMuted() ? "MUTE" : "ON");
    } else {
        std::snprintf(line, sizeof(line), "%lu / %lu BARS    %u%%",
                      static_cast<unsigned long>(state.bar),
                      static_cast<unsigned long>(state.totalBars),
                      percent);
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);

    if (state.tempoMode == SmfTempoMode::Project) {
        const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
        if (clock.source == TransportClockSource::SeqtrakExternal) {
            if (!clock.externalFollowEnabled) {
                std::snprintf(line, sizeof(line), "SEQ MASTER: FOLLOW OFF");
            } else {
                std::snprintf(line, sizeof(line), "SEQ MASTER: %s %5.1f BPM",
                              externalClockLockStateName(clock.externalState),
                              clock.externalTempoValid ? clock.externalBpm() : 0.0);
            }
        } else {
            std::snprintf(line, sizeof(line), "GP MASTER: %s > USB CLOCK",
                          miniAcid_.isPlaying() ? "RUN" : "STOP");
        }
    } else if (state.tempoScalePermille == 1000u) {
        std::snprintf(line, sizeof(line), "TEMPO SOURCE: FILE / ORIGINAL");
    } else {
        std::snprintf(line, sizeof(line), "ORIGINAL %u.%u BPM   O RESET",
                      static_cast<unsigned>(state.originalBpmX10 / 10),
                      static_cast<unsigned>(state.originalBpmX10 % 10));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(5), line);

    const CardputerUsbMidiStatusSnapshot usb = snapshotCardputerUsbMidiStatus();
    const char* usbState = !usb.registered || !usb.mounted
        ? "WAIT"
        : (usb.suspended ? "SLEEP" : (usb.stalled ? "BLOCKED" : "READY"));
    gfx.setTextColor(usb.stalled || !usb.mounted ? COLOR_DANGER : COLOR_TEXT);
    std::snprintf(line, sizeof(line), "USB %s M%u OK%lu NO%lu B%lu H%lu Q%u",
                  usbState,
                  static_cast<unsigned>(usb.mounted),
                  static_cast<unsigned long>(usb.txAccepted),
                  static_cast<unsigned long>(usb.txRejected),
                  static_cast<unsigned long>(usb.txRejectedEndpointBusy),
                  static_cast<unsigned long>(usb.txRejectedEndpointStalled),
                  static_cast<unsigned>(usb.queuedSmfEvents));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6), line);

    const bool usbBlocked = std::strncmp(state.message, "USB MIDI BLOCKED", 16) == 0;
    gfx.setTextColor((error || usbBlocked) ? COLOR_DANGER : COLOR_LABEL);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), state.message);
}


void SmfPlayerPage::drawMidiWaveOverlay(
        IGfx& gfx,
        const SmfPlayerSnapshot& state,
        const Rect& region,
        IGfxColor color) {
    const SmfMidiVisualSnapshot& visual = state.midiVisual;
    if (visual.epoch != lastMidiVisualEpoch_) {
        lastMidiVisualEpoch_ = visual.epoch;
        lastMidiVisualPulse_ = visual.pulseCounter;
        midiWaveEnvelope_ = 0;
        midiWavePhase_ = 0;
    } else if (state.state == SmfPlayerState::Playing &&
               visual.pulseCounter != lastMidiVisualPulse_) {
        lastMidiVisualPulse_ = visual.pulseCounter;
        midiWaveEnvelope_ = std::max<uint8_t>(24, visual.velocity);
        midiWavePhase_ = static_cast<uint16_t>(
            midiWavePhase_ + visual.note * 3u + visual.channel * 11u + 7u);
    } else if (midiWaveEnvelope_ > 0) {
        midiWaveEnvelope_ = midiWaveEnvelope_ > 7u
            ? static_cast<uint8_t>(midiWaveEnvelope_ - 7u)
            : 0u;
    }

    const int midY = region.y + region.h / 2;
    gfx.drawLine(region.x, midY, region.x + region.w - 1, midY, COLOR_LABEL);
    if (midiWaveEnvelope_ == 0 || region.w < 3 || region.h < 3) return;

    const int amplitude = std::max(
        1, ((region.h / 2) * static_cast<int>(midiWaveEnvelope_)) / 127);
    const int cycles = 2 + static_cast<int>(visual.note % 7u);
    int previousX = region.x;
    int previousY = midY;
    constexpr int kPoints = 32;
    for (int point = 1; point < kPoints; ++point) {
        const int x = region.x + (point * (region.w - 1)) / (kPoints - 1);
        const int phase = static_cast<int>(
            (midiWavePhase_ + point * cycles * 4u) & 63u);
        const int triangle = phase < 32 ? phase - 16 : 48 - phase;
        const int accent = ((point + static_cast<int>(visual.pulseCounter)) & 7) == 0
            ? (visual.velocity > 96 ? 5 : 2)
            : 0;
        const int y = midY - ((triangle + accent) * amplitude) / 16;
        gfx.drawLine(previousX, previousY, x, y, color);
        previousX = x;
        previousY = y;
    }
    midiWavePhase_ = static_cast<uint16_t>(midiWavePhase_ + 3u + cycles);
}

void SmfPlayerPage::drawChannelInspector(IGfx& gfx) {
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};
    const SmfChannelInspectorSnapshot inspector = player_
        ? player_->channelInspector()
        : SmfChannelInspectorSnapshot{};

    char line[64];
    gfx.setTextColor(MusicVisuals::accentForStyle());
    std::snprintf(line, sizeof(line), "F%u PPQN %u TRK %u USED %u",
                  static_cast<unsigned>(inspector.format),
                  static_cast<unsigned>(inspector.division),
                  static_cast<unsigned>(inspector.trackCount),
                  static_cast<unsigned>(inspector.usedChannelCount()));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    uint8_t used[kSmfMidiChannelCount]{};
    int usedCount = 0;
    for (uint8_t channel = 0; channel < kSmfMidiChannelCount; ++channel) {
        if ((inspector.usedChannelMask & (1u << channel)) != 0) {
            used[usedCount++] = channel;
        }
    }

    constexpr int kVisibleRows = 6;
    const int maxScroll = std::max(0, usedCount - kVisibleRows);
    channelInspectorScroll_ = std::max(0, std::min(channelInspectorScroll_, maxScroll));

    if (usedCount == 0) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "NO NOTE OR PROGRAM CHANNELS");
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), "LOAD A MIDI FILE FIRST");
        return;
    }

    for (int row = 0; row < kVisibleRows; ++row) {
        const int index = channelInspectorScroll_ + row;
        if (index >= usedCount) break;
        const uint8_t channel = used[index];
        const SmfChannelInfo& info = inspector.channels[channel];

        char low[5] = "--";
        char high[5] = "--";
        if (info.hasNotes()) {
            formatMidiNote(info.minNote, low, sizeof(low));
            formatMidiNote(info.maxNote, high, sizeof(high));
        }
        char program[6] = "P---";
        if (info.hasProgramChange) {
            std::snprintf(program, sizeof(program), "P%03u",
                          static_cast<unsigned>(info.firstProgram));
        }
        const unsigned shownNotes = static_cast<unsigned>(
            info.noteCount > 9999u ? 9999u : info.noteCount);
        const unsigned shownPoly = static_cast<unsigned>(
            info.maxPolyphony > 99u ? 99u : info.maxPolyphony);
        std::snprintf(line, sizeof(line),
                      "C%02u N%04u %-3s-%-3s V%03u X%02u %s %-3s",
                      static_cast<unsigned>(channel + 1u),
                      shownNotes,
                      low,
                      high,
                      static_cast<unsigned>(info.averageVelocity()),
                      shownPoly,
                      program,
                      inspectorRouteLabel(state.rawRouting, channel));
        gfx.setTextColor(info.likelyDrums
                             ? MusicVisuals::accentForStyle()
                             : COLOR_TEXT);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(row + 1), line);
    }

    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7),
                 "I PLAYER UP/DN SCROLL P=PROGRAM");
}

void SmfPlayerPage::drawPerformance(IGfx& gfx) {
    player_ = smfPlayerService();
    const SmfPlayerSnapshot state = player_ ? player_->snapshot() : SmfPlayerSnapshot{};
    const SmfPlayerPerformanceSnapshot& perf = state.performance;

    char line[64];
    gfx.setTextColor(COLOR_ACCENT);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), "LIVE 2 SEC WINDOW");

    gfx.setTextColor(COLOR_TEXT);
    std::snprintf(line, sizeof(line), "TRACKS %u  CACHE/TRK %u B",
                  static_cast<unsigned>(perf.trackCount),
                  static_cast<unsigned>(perf.cacheBytesPerTrack));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    std::snprintf(line, sizeof(line), "READ %lu  SEEK %lu",
                  static_cast<unsigned long>(perf.reads),
                  static_cast<unsigned long>(perf.seeks));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), line);

    std::snprintf(line, sizeof(line), "BYTES %lu  READ MAX %lu US",
                  static_cast<unsigned long>(perf.bytes),
                  static_cast<unsigned long>(perf.maxReadMicros));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), line);

    std::snprintf(line, sizeof(line), "SCHED %lu  QUEUED %lu",
                  static_cast<unsigned long>(perf.scheduleCalls),
                  static_cast<unsigned long>(perf.queuedEvents));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);

    std::snprintf(line, sizeof(line), "SCHED MAX %lu US",
                  static_cast<unsigned long>(perf.maxScheduleMicros));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(5), line);

    if (perf.minQueueDepth < 0) {
        std::snprintf(line, sizeof(line), "QUEUE MIN -- / %u",
                      static_cast<unsigned>(perf.queueFillLimit));
    } else {
        std::snprintf(line, sizeof(line), "QUEUE MIN %d / %u",
                      static_cast<int>(perf.minQueueDepth),
                      static_cast<unsigned>(perf.queueFillLimit));
    }
    gfx.setTextColor(perf.minQueueDepth >= 0 && perf.minQueueDepth <= 2
                         ? COLOR_DANGER
                         : COLOR_TEXT);
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6), line);

    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(line, sizeof(line), "LOOKAHEAD %u MS  %s",
                  static_cast<unsigned>(perf.lookaheadMs),
                  smfTempoModeName(state.tempoMode));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);
}

void SmfPlayerPage::drawFooter(IGfx& gfx) {
    const bool seqMaster = transportClockRuntime().source() == TransportClockSource::SeqtrakExternal;
    if (browserVisible_) {
        UI::drawStandardFooter(gfx, "ENT Open R Name X Delete",
                               seqMaster ? "F Refresh C Master G Follow"
                                         : "F Refresh C Master T Tempo");
    } else if (channelInspectorVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Scroll I Player",
                               "D Perf B Files Space MIDI");
    } else if (performanceVisible_) {
        UI::drawStandardFooter(gfx, "D Player B Files I Channels",
                               seqMaster ? "C Master G Follow T Tempo"
                                         : "C Master Space MIDI T Tempo");
    } else {
        UI::drawStandardFooter(gfx,
                               seqMaster ? "Space MIDI G Follow C Master"
                                         : "Space MIDI C Master R RESTART",
                               "I Channels J/L Track K Mute");
    }
}
