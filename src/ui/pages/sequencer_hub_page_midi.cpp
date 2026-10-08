#define GROOVEPUTER_SEQUENCER_HUB_WRAPPER_CONSUMER 1
#include "sequencer_hub_page.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>

#include "../components/music_visuals.h"
#include "../player_hub_navigation.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "synth_sequencer_page.h"
#include "src/state/generation_shape_state.h"
#include "src/state/phrase_generation_request_state.h"
#include "src/midi/smf_effective_route.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/smf_session_generation.h"
#include "src/midi/smf_structural_inspector.h"
#include "src/midi/smf_track_inspector.h"
#include "src/midi/smf_track_level.h"
#include "src/midi/smf_track_mute.h"
#include "src/midi/smf_track_output_route.h"
#include "src/midi/transport_clock_runtime.h"

namespace {
using namespace GroovePuterMidi;
constexpr uint8_t kVisibleMidiRows = 7u;
constexpr uint8_t kArrangementSegments = kSmfStructuralFormSegments;
// Name (6 chars) and where the track sounds (5 chars): SYN1, DX, DRUM, OFF.
constexpr int kLayerLabelWidth = 86;
constexpr int kOverlayBandHeight = 11;
constexpr int kCellGap = 1;

constexpr IGfxColor kScreenBackground(0x020508);
constexpr IGfxColor kRowBackground(0x061019);
constexpr IGfxColor kSelectedRowBackground(0x111C26);
constexpr IGfxColor kMutedRowBackground(0x04090D);
constexpr IGfxColor kMutedSelectedRowBackground(0x091016);
constexpr IGfxColor kOverlayScrim(0x020406);
constexpr IGfxColor kBodyText(0xD2DAE2);
constexpr IGfxColor kMutedText(0x56616B);
constexpr IGfxColor kAccent(0xE6B85C);
// Route label colours: a per-track choice, and a route that needs attention
// (OFF: the track reaches nothing; two parts sharing SYN1/SYN2/DX).
constexpr IGfxColor kRouteManual(0x7FC8F8);
constexpr IGfxColor kRouteWarn(0xE07A5F);

constexpr IGfxColor kActivityRamp[8] = {
    IGfxColor(0x0A2632),
    IGfxColor(0x0D3442),
    IGfxColor(0x104252),
    IGfxColor(0x155364),
    IGfxColor(0x1C6677),
    IGfxColor(0x267B8B),
    IGfxColor(0x3494A3),
    IGfxColor(0x51B4BF),
};

constexpr IGfxColor kMutedActivityRamp[8] = {
    IGfxColor(0x071217),
    IGfxColor(0x08181E),
    IGfxColor(0x0A1E25),
    IGfxColor(0x0C252D),
    IGfxColor(0x0F2D36),
    IGfxColor(0x123640),
    IGfxColor(0x17414B),
    IGfxColor(0x1D4C56),
};

static_assert(kArrangementSegments == 16u,
              "HUB MIDI arrangement grid requires sixteen form segments");

struct HubMidiProjection {
    SmfStructuralInspectorSnapshot layers{};
    SmfTrackInspectorSnapshot tracks{};
    SmfTrackMuteSnapshot mute{};
    SmfTrackOutputRouteSnapshot routes{};
    uint32_t generation{0};

    bool ready() const {
        return smfSnapshotGenerationsMatch(
                   generation,
                   layers.generation,
                   tracks.generation,
                   mute.generation) &&
               routes.generation == generation;
    }
};

struct HubMidiSoloState {
    uint32_t generation{0};
    uint16_t track{0};
    uint64_t restoreMask{0};
    bool active{false};
};

HubMidiSoloState& hubMidiSoloState() {
    static HubMidiSoloState state{};
    return state;
}

void syncHubMidiSoloGeneration(uint32_t generation) {
    HubMidiSoloState& solo = hubMidiSoloState();
    if (solo.active && solo.generation != generation) {
        solo = HubMidiSoloState{};
    }
}

void clearHubMidiSoloTracking() {
    hubMidiSoloState() = HubMidiSoloState{};
}

bool restoreHubMidiSoloBeforeManualMute(uint32_t generation) {
    HubMidiSoloState& solo = hubMidiSoloState();
    if (!solo.active) return true;
    if (solo.generation != generation) {
        solo = HubMidiSoloState{};
        return true;
    }
    const uint64_t restoreMask = solo.restoreMask;
    if (!smfTrackMuteState().replaceMutedMask(restoreMask, generation)) {
        return false;
    }
    solo = HubMidiSoloState{};
    return true;
}

bool selectedTrackIsSolo(uint32_t generation, uint16_t track) {
    const HubMidiSoloState& solo = hubMidiSoloState();
    return solo.active && solo.generation == generation && solo.track == track;
}

uint64_t allSmfTracksMask(uint16_t trackCount) {
    if (trackCount == 0u) return 0u;
    if (trackCount >= 64u) return ~uint64_t{0};
    return (uint64_t{1} << trackCount) - 1u;
}

HubMidiProjection captureHubMidiProjection() {
    HubMidiProjection projection{};
    projection.layers = smfStructuralInspectorState().snapshot();
    projection.tracks = smfTrackInspectorState().snapshot();
    projection.mute = smfTrackMuteState().snapshot();
    projection.routes = smfTrackOutputRouteState().snapshot(
        projection.mute.trackCount);
    projection.generation = smfSessionGeneration();
    return projection;
}

bool playerExpectsMidiProjection(SmfPlayerState state) {
    return state == SmfPlayerState::Loading ||
           state == SmfPlayerState::Stopped ||
           state == SmfPlayerState::Armed ||
           state == SmfPlayerState::Playing ||
           state == SmfPlayerState::Paused;
}

bool projectionIsSyncing(const SmfPlayerSnapshot& player,
                         const HubMidiProjection& projection) {
    return player.state == SmfPlayerState::Loading ||
           (playerExpectsMidiProjection(player.state) && !projection.ready());
}

bool smfStateIsActive(SmfPlayerState state) {
    return state == SmfPlayerState::Playing ||
           state == SmfPlayerState::Armed;
}

bool toggleHubMidiTransport(MiniAcid& miniAcid) {
    ISmfPlayerService* service = smfPlayerService();
    if (!service) {
        UI::showToast("SMF player unavailable", 1200);
        return true;
    }

    const SmfPlayerSnapshot state = service->snapshot();
    if (state.state == SmfPlayerState::Unloaded ||
        state.state == SmfPlayerState::Error) {
        UI::showToast("LOAD MIDI IN PLAYER", 900);
        return true;
    }

    const bool wasActive = smfStateIsActive(state.state);
    // Same rule as the Player page: while following MIDI IN a playing file is
    // silenced in place with the rest of GroovePuter, never paused.
    if (wasActive && externalClockOwnsTransport()) {
        UI::showToast(toggleFollowOutputMute(), 900);
        return true;
    }
    const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
    if (!wasActive && state.tempoMode == SmfTempoMode::Project &&
        !miniAcid.isPlaying() &&
        clock.source == TransportClockSource::GroovePuterInternal) {
        UI::showToast("G START FIRST / THEN SPACE", 1100);
        return true;
    }

    const bool queued = service->togglePlayPause();
    if (!queued) {
        UI::showToast("MIDI PLAYER BUSY", 800);
    } else if (wasActive) {
        UI::showToast("MIDI: PAUSE", 700);
    } else if (state.tempoMode == SmfTempoMode::Project) {
        UI::showToast(clock.source == TransportClockSource::SeqtrakExternal
                          ? (!clock.externalFollowEnabled
                                 ? "MIDI ARMED / FOLLOW OFF"
                                 : (clock.externalRunning
                                        ? "MIDI: ARM NEXT BAR"
                                        : "MIDI ARMED / PLAY MASTER"))
                          : "MIDI: ARM NEXT BAR",
                      900);
    } else {
        UI::showToast("MIDI: PLAY", 700);
    }
    return true;
}

bool muted(const SmfTrackMuteSnapshot& state, uint16_t track) {
    return track < 64u &&
           (state.mutedMask & (uint64_t{1} << track)) != 0u;
}

const char* roleLabel(SmfStructuralRole role) {
    switch (role) {
        case SmfStructuralRole::Drums: return "DRUMS";
        case SmfStructuralRole::Bass: return "BASS";
        case SmfStructuralRole::Chords: return "CHORD";
        case SmfStructuralRole::Pad: return "PAD";
        case SmfStructuralRole::Lead: return "LEAD";
        default: return "MIDI";
    }
}

const char* seqtrakDestinationName(int8_t destinationChannel) {
    static constexpr const char* kNames[kSmfSeqtrakOutputChannelCount] = {
        "KICK", "SNARE", "CLAP", "HAT-C", "HAT-O",
        "PERC", "CYM", "SYN1", "SYN2", "DX",
    };
    if (destinationChannel < 0 ||
        destinationChannel >=
            static_cast<int8_t>(kSmfSeqtrakOutputChannelCount)) {
        return "AUTO";
    }
    return kNames[static_cast<uint8_t>(destinationChannel)];
}

void formatTrackChannel(const SmfTrackInfoSnapshot* info,
                        char* output,
                        std::size_t outputSize) {
    if (!output || outputSize == 0u) return;
    if (!info || info->channelMask == 0u) {
        std::snprintf(output, outputSize, "CH--");
        return;
    }
    if (info->usesMultipleChannels()) {
        std::snprintf(output, outputSize, "MULTI");
        return;
    }
    const int channel = info->primaryChannel();
    if (channel < 0) std::snprintf(output, outputSize, "CH--");
    else std::snprintf(output, outputSize, "CH%d", channel + 1);
}

void formatRouteDestination(int8_t destinationChannel,
                            bool detailed,
                            char* output,
                            std::size_t outputSize) {
    if (!output || outputSize == 0u) return;
    if (destinationChannel == kSmfTrackOutputRouteAuto) {
        std::snprintf(output, outputSize, "AUTO");
        return;
    }
    if (destinationChannel < 0 ||
        destinationChannel >=
            static_cast<int8_t>(kSmfSeqtrakOutputChannelCount)) {
        std::snprintf(output, outputSize, "?");
        return;
    }
    if (detailed) {
        std::snprintf(output,
                      outputSize,
                      "CH%d %s",
                      static_cast<int>(destinationChannel) + 1,
                      seqtrakDestinationName(destinationChannel));
    } else {
        std::snprintf(output,
                      outputSize,
                      "CH%d",
                      static_cast<int>(destinationChannel) + 1);
    }
}

int8_t cycleRouteDestination(int8_t current, int delta) {
    constexpr int kChoiceCount = kSmfSeqtrakOutputChannelCount + 1;
    int choice = static_cast<int>(current) + 1;
    choice = (choice + delta) % kChoiceCount;
    if (choice < 0) choice += kChoiceCount;
    return static_cast<int8_t>(choice - 1);
}

void formatMidiNote(uint8_t note, char* output, std::size_t outputSize) {
    static constexpr const char* kPitchClasses[12] = {
        "C", "C#", "D", "D#", "E", "F",
        "F#", "G", "G#", "A", "A#", "B",
    };
    if (!output || outputSize == 0u) return;
    const int octave = static_cast<int>(note / 12u) - 1;
    std::snprintf(output, outputSize, "%s%d", kPitchClasses[note % 12u], octave);
}

void formatPitchRange(const SmfStructuralLayerSnapshot& layer,
                      char* output,
                      std::size_t outputSize) {
    if (!output || outputSize == 0u) return;
    if (!layer.hasNotes()) {
        std::snprintf(output, outputSize, "---");
        return;
    }
    char low[5]{};
    char high[5]{};
    formatMidiNote(layer.minNote, low, sizeof(low));
    formatMidiNote(layer.maxNote, high, sizeof(high));
    std::snprintf(output, outputSize, "%s-%s", low, high);
}

IGfxColor rowBackground(bool isMuted, bool isSelected) {
    if (isMuted) {
        return isSelected ? kMutedSelectedRowBackground : kMutedRowBackground;
    }
    return isSelected ? kSelectedRowBackground : kRowBackground;
}

IGfxColor activityColor(uint8_t level, bool isMuted) {
    if (level == 0u) {
        return isMuted ? kMutedRowBackground : kRowBackground;
    }
    const uint8_t index = static_cast<uint8_t>(std::min<uint8_t>(level, 8u) - 1u);
    return isMuted ? kMutedActivityRamp[index] : kActivityRamp[index];
}

// Unmuted tracks per SEQTRAK output, to flag two parts on one melodic voice.
struct HubOutputLoad {
    uint8_t unmuted[kSmfSeqtrakOutputChannelCount]{};
};

SmfEffectiveRoute hubTrackRoute(const HubMidiProjection& projection,
                                bool rawRouting,
                                uint16_t track) {
    const SmfTrackInfoSnapshot* info = track < projection.tracks.trackCount
        ? &projection.tracks.tracks[track]
        : nullptr;
    return effectiveSmfTrackRoute(rawRouting, info,
                                  projection.routes.destinationFor(track));
}

HubOutputLoad hubOutputLoad(const HubMidiProjection& projection, bool rawRouting) {
    HubOutputLoad load{};
    const uint16_t count = std::min<uint16_t>(
        projection.tracks.trackCount,
        static_cast<uint16_t>(kSmfTrackInspectorMaxTracks));
    for (uint16_t track = 0u; track < count; ++track) {
        if (!projection.tracks.tracks[track].audible()) continue;
        if (muted(projection.mute, track)) continue;
        const SmfEffectiveRoute route = hubTrackRoute(projection, rawRouting, track);
        if (route.kind == SmfEffectiveRouteKind::Output && route.output >= 0) {
            ++load.unmuted[route.output];
        }
    }
    return load;
}

IGfxColor hubRouteColor(const SmfEffectiveRoute& route,
                        const HubOutputLoad& load,
                        bool isMuted) {
    if (isMuted) return kMutedText;
    if (route.kind == SmfEffectiveRouteKind::Off) return kRouteWarn;
    if (route.kind == SmfEffectiveRouteKind::Output &&
        smfOutputIsMelodic(route.output) && load.unmuted[route.output] > 1u) {
        return kRouteWarn;
    }
    return route.overridden ? kRouteManual : kBodyText;
}

void formatHubTrackName(const SmfTrackInfoSnapshot* info,
                        uint16_t track,
                        int width,
                        char* output,
                        std::size_t outputSize) {
    if (info && info->hasName()) {
        std::snprintf(output, outputSize, "%.*s", width, info->name);
    } else {
        std::snprintf(output, outputSize, "T%02u", static_cast<unsigned>(track + 1u));
    }
}

void drawArrangementRow(IGfx& gfx,
                        int screenWidth,
                        int contentTop,
                        int contentHeight,
                        uint8_t row,
                        uint8_t layerIndex,
                        const SmfStructuralLayerSnapshot& layer,
                        const SmfTrackInfoSnapshot* info,
                        bool isMuted,
                        bool isSelected,
                        const char* routeLabel,
                        IGfxColor routeColor) {
    const int y0 = contentTop +
                   (static_cast<int>(row) * contentHeight) / kVisibleMidiRows;
    const int y1 = contentTop +
                   (static_cast<int>(row + 1u) * contentHeight) /
                       kVisibleMidiRows;
    const int rowHeight = std::max(1, y1 - y0);
    const int textY = y0 + std::max(0, (rowHeight - 7) / 2);
    const int gridX = std::min(kLayerLabelWidth, screenWidth);
    const int gridWidth = std::max(0, screenWidth - gridX);
    const IGfxColor background = rowBackground(isMuted, isSelected);

    gfx.fillRect(0, y0, screenWidth, rowHeight, background);

    const char hotkey = layerIndex < 9u
        ? static_cast<char>('1' + layerIndex)
        : '-';
    char hotkeyText[2]{hotkey, '\0'};
    gfx.setTextColor(isSelected ? kAccent : (isMuted ? kMutedText : kBodyText));
    gfx.drawText(3, textY, hotkeyText);

    const char* label = info && info->hasName() ? info->name : roleLabel(layer.role);
    char clippedLabel[9]{};
    std::snprintf(clippedLabel, sizeof(clippedLabel), "%.6s", label);
    gfx.setTextColor(isMuted ? kMutedText : kBodyText);
    gfx.drawText(12, textY, clippedLabel);
    // Where this track sounds right now: read here instead of by ear.
    gfx.setTextColor(routeColor);
    gfx.drawText(52, textY, routeLabel ? routeLabel : "");

    for (uint8_t segment = 0u; segment < kArrangementSegments; ++segment) {
        const int x0 = gridX +
            (static_cast<int>(segment) * gridWidth) / kArrangementSegments;
        const int x1 = gridX +
            (static_cast<int>(segment + 1u) * gridWidth) / kArrangementSegments;
        const int columnWidth = std::max(1, x1 - x0);
        const int maxCellWidth = std::max(1, columnWidth - kCellGap);
        const int maxCellHeight = std::max(1, rowHeight - 4);
        const int cellSize = std::min(maxCellWidth, maxCellHeight);
        const int cellX = x0 + std::max(0, (columnWidth - cellSize) / 2);
        const int cellY = y0 + std::max(0, (rowHeight - cellSize) / 2);
        gfx.fillRect(cellX,
                     cellY,
                     cellSize,
                     cellSize,
                     activityColor(layer.form[segment], isMuted));
    }
}

int arrangementPlayheadX(const SmfPlayerSnapshot& player,
                         int gridX,
                         int gridWidth) {
    if (gridWidth <= 1) return gridX;
    if (player.endTick != 0u) {
        const uint32_t tick = std::min(player.currentTick, player.endTick);
        return gridX + static_cast<int>(
            (static_cast<uint64_t>(tick) * (gridWidth - 1)) / player.endTick);
    }
    const uint32_t totalBars = std::max<uint32_t>(player.totalBars, 1u);
    const uint32_t bar = std::min(std::max<uint32_t>(player.bar, 1u), totalBars);
    return gridX + static_cast<int>(
        (static_cast<uint64_t>(bar - 1u) * (gridWidth - 1)) / totalBars);
}

void drawOverlayBands(IGfx& gfx,
                      const SmfPlayerSnapshot& player,
                      const SmfStructuralLayerSnapshot& selectedLayer,
                      const SmfTrackInfoSnapshot* selectedInfo,
                      const SmfEffectiveRoute& route,
                      uint8_t trackLevel,
                      bool soloActive,
                      bool partial) {
    const int width = gfx.width();
    const int height = gfx.height();
    const int bottomY = std::max(0, height - kOverlayBandHeight);
    gfx.fillRect(0, 0, width, kOverlayBandHeight, kOverlayScrim);
    gfx.fillRect(0, bottomY, width, kOverlayBandHeight, kOverlayScrim);

    char line[64]{};
    gfx.setTextColor(kBodyText);
    std::snprintf(line, sizeof(line), "%.17s%s",
                  player.filename[0] ? player.filename : "NO FILE",
                  partial ? "*" : "");
    gfx.drawText(3, 2, line);
    gfx.setTextColor(kMutedText);
    gfx.drawText(3 + 19 * 6, 2, "O:OUTS");
    gfx.setTextColor(kBodyText);

    std::snprintf(line, sizeof(line), "BAR %lu/%lu",
                  static_cast<unsigned long>(player.bar),
                  static_cast<unsigned long>(std::max<uint32_t>(player.totalBars, 1u)));
    gfx.drawText(std::max(3, width - gfx.textWidth(line) - 3), 2, line);

    char channel[8]{};
    char destination[8]{};
    char range[12]{};
    formatTrackChannel(selectedInfo, channel, sizeof(channel));
    formatEffectiveSmfRoute(route, destination, sizeof(destination));
    formatPitchRange(selectedLayer, range, sizeof(range));
    if (player.rawRouting) {
        std::snprintf(line,
                      sizeof(line),
                      "RAW %s V%u N%u %s",
                      channel,
                      static_cast<unsigned>(trackLevel),
                      static_cast<unsigned>(selectedLayer.noteCount),
                      range);
    } else {
        std::snprintf(line,
                      sizeof(line),
                      "%s>%s V%u N%u %s",
                      channel,
                      destination,
                      static_cast<unsigned>(trackLevel),
                      static_cast<unsigned>(selectedLayer.noteCount),
                      range);
    }
    const char* hints = soloActive ? "S:OFF <>RTE" : "S:SOLO <>RTE";
    gfx.setTextColor(kBodyText);
    gfx.drawText(3, bottomY + 2, line);

    gfx.setTextColor(kMutedText);
    gfx.drawText(std::max(3, width - gfx.textWidth(hints) - 3),
                 bottomY + 2,
                 hints);
}

void drawProjectionMessage(IGfx& gfx,
                           const char* title,
                           const char* detail) {
    gfx.fillRect(0, 0, gfx.width(), gfx.height(), kScreenBackground);
    gfx.fillRect(0, 0, gfx.width(), kOverlayBandHeight, kOverlayScrim);
    gfx.fillRect(0,
                 std::max(0, gfx.height() - kOverlayBandHeight),
                 gfx.width(),
                 kOverlayBandHeight,
                 kOverlayScrim);
    gfx.setTextColor(kBodyText);
    gfx.drawText(3, 2, "HUB MIDI");
    gfx.drawText(8, std::max(18, gfx.height() / 2 - 8), title);
    gfx.setTextColor(kMutedText);
    gfx.drawText(8, std::max(29, gfx.height() / 2 + 3), detail);
    gfx.drawText(3, std::max(0, gfx.height() - 9), "H/ESC PLAYER");
}

bool selectProjectedLayer(const HubMidiProjection& projection,
                          uint8_t layerIndex) {
    if (!projection.ready() || layerIndex >= projection.layers.layerCount) {
        return false;
    }
    return smfTrackMuteState().selectTrack(
        projection.layers.layers[layerIndex].trackIndex,
        projection.generation);
}

// O: who occupies which SEQTRAK output, so a free voice can be chosen at a
// glance. Muted tracks are counted but not named; two unmuted parts on one
// SYN1/SYN2/DX, and tracks that reach nothing (OFF), are flagged.
void drawHubMidiOutputs(IGfx& gfx,
                        const SmfPlayerSnapshot& player,
                        const HubMidiProjection& projection) {
    const int width = gfx.width();
    gfx.fillRect(0, 0, width, gfx.height(), kScreenBackground);
    gfx.fillRect(0, 0, width, kOverlayBandHeight, kOverlayScrim);
    gfx.setTextColor(kBodyText);
    gfx.drawText(3, 2, "SEQTRAK OUTPUTS");
    gfx.setTextColor(kMutedText);
    gfx.drawText(std::max(3, width - gfx.textWidth("O:BACK") - 3), 2, "O:BACK");
    if (player.rawRouting) {
        gfx.setTextColor(kBodyText);
        gfx.drawText(3, 24, "RAW ROUTING:");
        gfx.drawText(3, 36, "SOURCE CHANNELS PASS THROUGH");
        return;
    }

    const HubOutputLoad load = hubOutputLoad(projection, false);
    const uint16_t count = std::min<uint16_t>(
        projection.tracks.trackCount,
        static_cast<uint16_t>(kSmfTrackInspectorMaxTracks));
    constexpr int kLineHeight = 11;
    constexpr int kTop = kOverlayBandHeight + 2;
    char line[64]{};
    char name[8]{};

    bool anyDrumSplit = false;
    for (uint16_t track = 0u; track < count; ++track) {
        if (!projection.tracks.tracks[track].audible()) continue;
        if (hubTrackRoute(projection, false, track).kind ==
            SmfEffectiveRouteKind::DrumSplit) {
            anyDrumSplit = true;
        }
    }

    for (int8_t output = 0;
         output < static_cast<int8_t>(kSmfSeqtrakOutputChannelCount);
         ++output) {
        int used = std::snprintf(line, sizeof(line), "%-5s ", seqtrakOutputName(output));
        unsigned mutedCount = 0u;
        bool any = false;
        for (uint16_t track = 0u; track < count; ++track) {
            if (!projection.tracks.tracks[track].audible()) continue;
            const SmfEffectiveRoute route = hubTrackRoute(projection, false, track);
            if (route.kind != SmfEffectiveRouteKind::Output || route.output != output) continue;
            any = true;
            if (muted(projection.mute, track)) {
                ++mutedCount;
                continue;
            }
            formatHubTrackName(&projection.tracks.tracks[track], track, 6, name, sizeof(name));
            if (used < static_cast<int>(sizeof(line)) - 1) {
                used += std::snprintf(line + used, sizeof(line) - used, "%s ", name);
            }
        }
        if (output <= 6 && anyDrumSplit && used < static_cast<int>(sizeof(line)) - 1) {
            used += std::snprintf(line + used, sizeof(line) - used, "+GM ");
            any = true;
        }
        if (mutedCount > 0u && used < static_cast<int>(sizeof(line)) - 1) {
            used += std::snprintf(line + used, sizeof(line) - used, "(%uM)", mutedCount);
        }
        if (!any && used < static_cast<int>(sizeof(line)) - 1) {
            std::snprintf(line + used, sizeof(line) - used, "--");
        }
        const bool clash = smfOutputIsMelodic(output) && load.unmuted[output] > 1u;
        gfx.setTextColor(clash ? kRouteWarn : (any ? kBodyText : kMutedText));
        gfx.drawText(3, kTop + output * kLineHeight, line);
    }

    // Tracks that reach nothing, and GM drum tracks split over KICK..CYM.
    int used = std::snprintf(line, sizeof(line), "OFF   ");
    bool anyOff = false;
    for (uint16_t track = 0u; track < count; ++track) {
        if (!projection.tracks.tracks[track].audible()) continue;
        if (muted(projection.mute, track)) continue;
        const SmfEffectiveRoute route = hubTrackRoute(projection, false, track);
        if (route.kind != SmfEffectiveRouteKind::Off &&
            route.kind != SmfEffectiveRouteKind::Multi) {
            continue;
        }
        anyOff = true;
        formatHubTrackName(&projection.tracks.tracks[track], track, 6, name, sizeof(name));
        if (used < static_cast<int>(sizeof(line)) - 1) {
            used += std::snprintf(line + used, sizeof(line) - used, "%s%s ", name,
                                  route.kind == SmfEffectiveRouteKind::Multi ? "*" : "");
        }
    }
    if (!anyOff) std::snprintf(line + used, sizeof(line) - used, "--");
    gfx.setTextColor(anyOff ? kRouteWarn : kMutedText);
    gfx.drawText(3, kTop + static_cast<int>(kSmfSeqtrakOutputChannelCount) * kLineHeight, line);
}
}  // namespace

void SequencerHubPage::onEnter(int context) {
    midiRouteEdit_ = false;
    midiOutputsView_ = false;
    midiRouteDraft_ = kSmfTrackOutputRouteAuto;
    if (context == PlayerHubNavigation::kOpenMidiFromPlayerContext) {
        midiOverview_ = true;
        midiReturnToPlayer_ = true;
        midiGeneration_ = 0u;
        syncMidiSessionSelection();
        return;
    }
    midiReturnToPlayer_ = false;
}

void SequencerHubPage::draw(IGfx& gfx) {
    if (midiOverview_) {
        drawMidiOverview(gfx);
        return;
    }
    SequencerHubPageBase::draw(gfx);
}

bool SequencerHubPage::handleEvent(UIEvent& event) {
    if (event.event_type == GROOVEPUTER_KEY_DOWN &&
        !event.alt && !event.ctrl && !event.meta &&
        (event.key == 'm' || event.key == 'M') &&
        mode_ == Mode::OVERVIEW) {
        midiOverview_ = !midiOverview_;
        midiReturnToPlayer_ = false;
        midiRouteEdit_ = false;
        if (midiOverview_) {
            midiGeneration_ = 0u;
            syncMidiSessionSelection();
        }
        UI::showToast(midiOverview_ ? "HUB: MIDI" : "HUB: INTERNAL", 700);
        return true;
    }

    if (midiOverview_) return handleMidiOverviewEvent(event);
    return SequencerHubPageBase::handleEvent(event);
}

void SequencerHubPage::syncMidiSessionSelection() {
    const HubMidiProjection projection = captureHubMidiProjection();
    if (!projection.ready()) return;
    syncHubMidiSoloGeneration(projection.generation);

    if (midiGeneration_ != projection.generation) {
        midiGeneration_ = projection.generation;
        midiSelected_ = 0u;
        midiScroll_ = 0u;
        midiRouteEdit_ = false;
        midiRouteDraft_ = kSmfTrackOutputRouteAuto;
        for (uint8_t index = 0u; index < projection.layers.layerCount; ++index) {
            if (projection.layers.layers[index].trackIndex ==
                projection.mute.selectedTrack) {
                midiSelected_ = index;
                break;
            }
        }
    }

    if (projection.layers.layerCount == 0u) {
        midiSelected_ = 0u;
        midiScroll_ = 0u;
        midiRouteEdit_ = false;
        return;
    }
    if (midiSelected_ >= projection.layers.layerCount) {
        midiSelected_ = static_cast<uint8_t>(projection.layers.layerCount - 1u);
    }
    syncMidiScroll(projection.layers.layerCount);
}

void SequencerHubPage::returnFromMidiOverview() {
    midiRouteEdit_ = false;
    midiOutputsView_ = false;
    midiRouteDraft_ = kSmfTrackOutputRouteAuto;
    if (midiReturnToPlayer_) {
        midiOverview_ = false;
        midiReturnToPlayer_ = false;
        requestPageTransition(PlayerHubNavigation::kPlayerPage);
        return;
    }
    midiOverview_ = false;
}

bool SequencerHubPage::toggleMidiLayer(uint8_t layerIndex) {
    ISmfPlayerService* service = smfPlayerService();
    const SmfPlayerSnapshot player = service ? service->snapshot() : SmfPlayerSnapshot{};
    const HubMidiProjection projection = captureHubMidiProjection();
    if (projectionIsSyncing(player, projection) || !projection.ready() ||
        layerIndex >= projection.layers.layerCount) {
        return false;
    }

    syncHubMidiSoloGeneration(projection.generation);
    if (!restoreHubMidiSoloBeforeManualMute(projection.generation)) return false;

    SmfTrackMuteState& state = smfTrackMuteState();
    const uint16_t track = projection.layers.layers[layerIndex].trackIndex;
    if (!state.toggleTrack(track, projection.generation)) return false;

    const SmfTrackMuteSnapshot after = state.snapshot();
    if (after.generation != projection.generation) return true;

    char toast[32];
    std::snprintf(toast, sizeof(toast), "MIDI %u TRK %02u %s",
                  static_cast<unsigned>(layerIndex + 1u),
                  static_cast<unsigned>(track + 1u),
                  muted(after, track) ? "MUTED" : "ON");
    UI::showToast(toast, 700);
    return true;
}

bool SequencerHubPage::handleMidiOverviewEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;

    const bool hubShortcut =
        !event.alt && !event.ctrl && (event.key == 'h' || event.key == 'H');
    if (hubShortcut) {
        returnFromMidiOverview();
        return true;
    }
    if (event.scancode == GROOVEPUTER_ESCAPE || UIInput::isBack(event)) {
        returnFromMidiOverview();
        return true;
    }

    if (event.meta && !event.alt && !event.ctrl &&
        (UIInput::isLeft(event) || UIInput::isRight(event))) {
        ISmfPlayerService* levelService = smfPlayerService();
        const SmfPlayerSnapshot levelPlayer =
            levelService ? levelService->snapshot() : SmfPlayerSnapshot{};
        const HubMidiProjection levelProjection = captureHubMidiProjection();
        if (projectionIsSyncing(levelPlayer, levelProjection) ||
            !levelProjection.ready() || levelProjection.layers.layerCount == 0u) {
            UI::showToast("MIDI LAYERS: SYNCING", 800);
            return true;
        }
        if (midiGeneration_ != levelProjection.generation) syncMidiSessionSelection();
        const uint8_t levelSelected = std::min<uint8_t>(
            midiSelected_, levelProjection.layers.layerCount - 1u);
        const uint16_t levelTrack =
            levelProjection.layers.layers[levelSelected].trackIndex;
        uint8_t levelPercent = smfTrackLevelState().levelFor(levelTrack);
        const int delta = UIInput::isRight(event) ? 5 : -5;
        if (!smfTrackLevelState().adjustLevel(
                levelTrack, delta, levelProjection.generation, levelPercent)) {
            UI::showToast("MIDI LAYERS: SYNCING", 800);
            return true;
        }
        char toast[40]{};
        std::snprintf(toast, sizeof(toast), "MIDI TRK %02u VOL %u%%",
                      static_cast<unsigned>(levelTrack + 1u),
                      static_cast<unsigned>(levelPercent));
        UI::showToast(toast, 700);
        return true;
    }

    if (event.alt || event.ctrl || event.meta) return true;

    // Retain the old page aliases without advertising them in the compact Hub UI.
    if (event.key == 'p' || event.key == 'P') {
        midiReturnToPlayer_ = true;
        returnFromMidiOverview();
        return true;
    }
    if (event.key == 'm' || event.key == 'M') {
        midiOverview_ = false;
        midiReturnToPlayer_ = false;
        midiOutputsView_ = false;
        return true;
    }
    if (event.key == 'o' || event.key == 'O') {
        midiOutputsView_ = !midiOutputsView_;
        return true;
    }
    if (event.key == ' ') {
        return toggleHubMidiTransport(mini_acid_);
    }

    ISmfPlayerService* service = smfPlayerService();
    const SmfPlayerSnapshot player = service ? service->snapshot() : SmfPlayerSnapshot{};
    const HubMidiProjection projection = captureHubMidiProjection();
    if (projectionIsSyncing(player, projection)) {
        midiRouteEdit_ = false;
        UI::showToast("MIDI LAYERS: SYNCING", 800);
        return true;
    }
    if (!projection.ready()) {
        midiRouteEdit_ = false;
        UI::showToast("LOAD MIDI IN PLAYER", 800);
        return true;
    }
    syncHubMidiSoloGeneration(projection.generation);
    if (midiGeneration_ != projection.generation) syncMidiSessionSelection();
    if (projection.layers.layerCount == 0u) return true;

    const uint8_t selected = std::min<uint8_t>(
        midiSelected_, projection.layers.layerCount - 1u);
    const uint16_t selectedTrack =
        projection.layers.layers[selected].trackIndex;

    if (event.key == 's' || event.key == 'S') {
        if (selectedTrack >= projection.mute.trackCount || selectedTrack >= 64u) {
            UI::showToast("MIDI SOLO UNAVAILABLE", 800);
            return true;
        }

        SmfTrackMuteState& muteState = smfTrackMuteState();
        HubMidiSoloState& solo = hubMidiSoloState();
        if (solo.active && solo.generation == projection.generation &&
            solo.track == selectedTrack) {
            const uint64_t restoreMask = solo.restoreMask;
            if (muteState.replaceMutedMask(restoreMask, projection.generation)) {
                clearHubMidiSoloTracking();
                UI::showToast("MIDI SOLO OFF", 700);
            } else {
                clearHubMidiSoloTracking();
                UI::showToast("MIDI LAYERS: SYNCING", 800);
            }
            return true;
        }

        const uint64_t restoreMask =
            solo.active && solo.generation == projection.generation
                ? solo.restoreMask
                : projection.mute.mutedMask;
        uint64_t soloMask = allSmfTracksMask(projection.mute.trackCount);
        soloMask &= ~(uint64_t{1} << selectedTrack);
        if (!muteState.replaceMutedMask(soloMask, projection.generation)) {
            clearHubMidiSoloTracking();
            UI::showToast("MIDI LAYERS: SYNCING", 800);
            return true;
        }

        solo.generation = projection.generation;
        solo.track = selectedTrack;
        solo.restoreMask = restoreMask;
        solo.active = true;
        char toast[32]{};
        std::snprintf(toast,
                      sizeof(toast),
                      "MIDI SOLO TRK %02u",
                      static_cast<unsigned>(selectedTrack + 1u));
        UI::showToast(toast, 700);
        return true;
    }

    int routeMove = 0;
    if (event.scancode == GROOVEPUTER_LEFT) routeMove = -1;
    else if (event.scancode == GROOVEPUTER_RIGHT) routeMove = 1;
    if (routeMove != 0) {
        if (player.rawRouting) {
            UI::showToast("SEQTRAK ROUTING REQUIRED", 900);
            return true;
        }

        const int8_t destination = cycleRouteDestination(
            projection.routes.destinationFor(selectedTrack), routeMove);
        if (!smfTrackOutputRouteState().setDestination(
                selectedTrack,
                destination,
                projection.generation,
                projection.mute.trackCount)) {
            UI::showToast("ROUTE SESSION CHANGED", 900);
            return true;
        }

        const bool saveQueued = service &&
            service->persistTrackOutputRoutes(projection.generation);
        char destinationText[20]{};
        char toast[40]{};
        if (destination == kSmfTrackOutputRouteAuto) {
            // AUTO alone says nothing about the sound: name what it resolves to.
            char resolved[8]{};
            // projection predates this change: resolve the new AUTO directly.
            const SmfTrackInfoSnapshot* info =
                selectedTrack < projection.tracks.trackCount
                    ? &projection.tracks.tracks[selectedTrack]
                    : nullptr;
            formatEffectiveSmfRoute(
                effectiveSmfTrackRoute(false, info, kSmfTrackOutputRouteAuto),
                resolved, sizeof(resolved));
            std::snprintf(destinationText, sizeof(destinationText), "AUTO %s", resolved);
        } else {
            formatRouteDestination(destination,
                                   true,
                                   destinationText,
                                   sizeof(destinationText));
        }
        std::snprintf(toast,
                      sizeof(toast),
                      saveQueued
                          ? "TRK %02u > %s"
                          : "TRK %02u > %s / SAVE BUSY",
                      static_cast<unsigned>(selectedTrack + 1u),
                      destinationText);
        UI::showToast(toast, saveQueued ? 700 : 1000);
        return true;
    }

    // C was the old route-edit entry. Keep it as a harmless reminder rather
    // than reintroducing a hidden Enter-to-commit mode.
    if (event.key == 'c' || event.key == 'C') {
        UI::showToast("ROUTE: USE LEFT / RIGHT", 900);
        return true;
    }

    if (event.key == 'a' || event.key == 'A') {
        clearHubMidiSoloTracking();
        if (smfTrackMuteState().clear(projection.generation)) {
            UI::showToast("ALL MIDI TRACKS ON", 800);
        } else {
            UI::showToast("MIDI LAYERS: SYNCING", 800);
        }
        return true;
    }
    // GRAB: Y shows what would be taken (layer, bars, target voice), Y again
    // within kGrabConfirmMs takes it. Range = the player's A-B loop, else
    // GEN LENGTH bars from the current bar. Only while paused or stopped.
    if (event.key == 'y' || event.key == 'Y') {
        if (player.state != SmfPlayerState::Paused &&
            player.state != SmfPlayerState::Stopped) {
            midiGrabArmed_ = false;
            UI::showToast("PAUSE TO GRAB (SPACE)", 1000);
            return true;
        }
        if (midiGrabPending_) {
            UI::showToast("GRAB BUSY", 700);
            return true;
        }
        const auto& layer = projection.layers.layers[selected];
        const uint32_t totalBars = std::max<uint32_t>(player.totalBars, 1u);
        uint32_t startBar = std::max<uint32_t>(player.bar, 1u);
        uint32_t endBar = startBar + GroovePuterState::requestedPhraseBars() - 1u;
        if (player.loopMode == SmfLoopMode::Section && player.loopStartBar != 0u &&
            player.loopEndBar >= player.loopStartBar) {
            startBar = player.loopStartBar;
            endBar = player.loopEndBar;
        }
        endBar = std::min(endBar, totalBars);
        const int voice = GroovePuterState::melodyTargetVoice() == 1 ? 1 : 0;
        const uint32_t now = millis();
        const bool confirm = midiGrabArmed_ &&
            now - midiGrabArmedMs_ <= kGrabConfirmMs &&
            midiGrabTrack_ == layer.trackIndex &&
            midiGrabChannels_ == layer.channelMask &&
            midiGrabStartBar_ == startBar && midiGrabEndBar_ == endBar &&
            midiGrabVoice_ == voice;
        char toast[40]{};
        if (!confirm) {
            midiGrabArmed_ = true;
            midiGrabArmedMs_ = now;
            midiGrabTrack_ = layer.trackIndex;
            midiGrabChannels_ = layer.channelMask;
            midiGrabStartBar_ = startBar;
            midiGrabEndBar_ = endBar;
            midiGrabVoice_ = static_cast<int8_t>(voice);
            std::snprintf(toast, sizeof(toast), "Y: T%02u B%lu-%lu > MEL %c",
                          static_cast<unsigned>(layer.trackIndex + 1u),
                          static_cast<unsigned long>(startBar),
                          static_cast<unsigned long>(endBar),
                          static_cast<char>('A' + voice));
            UI::showToast(toast, kGrabConfirmMs);
            return true;
        }
        midiGrabArmed_ = false;
        SmfGrabRequest request{};
        request.trackIndex = layer.trackIndex;
        request.channelMask = layer.channelMask != 0u ? layer.channelMask : 0xFFFFu;
        request.startBar = startBar;
        request.endBar = endBar;
        if (!service || !service->requestGrab(request)) {
            UI::showToast("GRAB BUSY", 800);
            return true;
        }
        midiGrabPending_ = true;
        UI::showToast("GRABBING...", 2000);
        return true;
    }
    if (event.key >= '1' && event.key <= '9') {
        if (!toggleMidiLayer(static_cast<uint8_t>(event.key - '1'))) {
            UI::showToast("MIDI LAYER UNAVAILABLE", 800);
        }
        return true;
    }

    int move = 0;
    if (UIInput::isUp(event)) move = -1;
    else if (UIInput::isDown(event)) move = 1;
    if (move != 0) {
        const int count = static_cast<int>(projection.layers.layerCount);
        int movedSelection = (static_cast<int>(midiSelected_) + move) % count;
        if (movedSelection < 0) movedSelection += count;
        midiSelected_ = static_cast<uint8_t>(movedSelection);
        syncMidiScroll(projection.layers.layerCount);
        if (!selectProjectedLayer(projection, midiSelected_)) {
            UI::showToast("MIDI LAYERS: SYNCING", 800);
        }
        return true;
    }
    if (event.key == '\n' || event.key == '\r') {
        if (!toggleMidiLayer(midiSelected_)) {
            UI::showToast("MIDI LAYER UNAVAILABLE", 800);
        }
        return true;
    }
    return true;
}

void SequencerHubPage::syncMidiScroll(uint8_t layerCount) {
    if (layerCount <= kVisibleMidiRows) {
        midiScroll_ = 0u;
        return;
    }
    if (midiSelected_ < midiScroll_) midiScroll_ = midiSelected_;
    const uint8_t end = static_cast<uint8_t>(midiScroll_ + kVisibleMidiRows);
    if (midiSelected_ >= end) {
        midiScroll_ = static_cast<uint8_t>(midiSelected_ - kVisibleMidiRows + 1u);
    }
    const uint8_t maxScroll = static_cast<uint8_t>(layerCount - kVisibleMidiRows);
    if (midiScroll_ > maxScroll) midiScroll_ = maxScroll;
}

void SequencerHubPage::drawMidiOverview(IGfx& gfx) {
    ISmfPlayerService* service = smfPlayerService();
    const SmfPlayerSnapshot player = service ? service->snapshot() : SmfPlayerSnapshot{};
    const HubMidiProjection projection = captureHubMidiProjection();

    if (projectionIsSyncing(player, projection)) {
        drawProjectionMessage(gfx, "SYNCING", "CURRENT SMF SESSION");
        return;
    }

    if (!projection.ready()) {
        drawProjectionMessage(
            gfx,
            player.state == SmfPlayerState::Error ? "MIDI LOAD ERROR" : "NO MIDI LAYERS",
            "LOAD FILE IN PLAYER");
        return;
    }

    syncHubMidiSoloGeneration(projection.generation);
    if (midiGeneration_ != projection.generation) syncMidiSessionSelection();
    if (projection.layers.layerCount == 0u) {
        drawProjectionMessage(gfx, "NO AUDIBLE LAYERS", "H/ESC RETURN");
        return;
    }

    const uint8_t selected = std::min<uint8_t>(
        midiSelected_, projection.layers.layerCount - 1u);
    const int screenWidth = gfx.width();
    const int screenHeight = gfx.height();
    const int rowsTop = std::min(kOverlayBandHeight, screenHeight);
    const int rowsBottom = std::max(rowsTop, screenHeight - kOverlayBandHeight);
    const int rowsHeight = std::max(0, rowsBottom - rowsTop);
    if (midiOutputsView_) {
        drawHubMidiOutputs(gfx, player, projection);
        return;
    }
    gfx.fillRect(0, 0, screenWidth, screenHeight, kScreenBackground);
    const HubOutputLoad load = hubOutputLoad(projection, player.rawRouting);

    for (uint8_t row = 0u; row < kVisibleMidiRows; ++row) {
        const uint8_t index = static_cast<uint8_t>(midiScroll_ + row);
        if (index >= projection.layers.layerCount) {
            const int y0 = rowsTop +
                           (static_cast<int>(row) * rowsHeight) /
                               kVisibleMidiRows;
            const int y1 = rowsTop +
                           (static_cast<int>(row + 1u) * rowsHeight) /
                               kVisibleMidiRows;
            gfx.fillRect(0, y0, screenWidth, std::max(1, y1 - y0),
                         kScreenBackground);
            continue;
        }
        const auto& layer = projection.layers.layers[index];
        const SmfTrackInfoSnapshot* info =
            layer.trackIndex < projection.tracks.trackCount
                ? &projection.tracks.tracks[layer.trackIndex]
                : nullptr;
        const bool rowMuted = muted(projection.mute, layer.trackIndex);
        const SmfEffectiveRoute rowRoute =
            hubTrackRoute(projection, player.rawRouting, layer.trackIndex);
        char routeLabel[8]{};
        formatEffectiveSmfRoute(rowRoute, routeLabel, sizeof(routeLabel));
        drawArrangementRow(
            gfx,
            screenWidth,
            rowsTop,
            rowsHeight,
            row,
            index,
            layer,
            info,
            rowMuted,
            index == selected,
            routeLabel,
            hubRouteColor(rowRoute, load, rowMuted));
    }

    const int gridX = std::min(kLayerLabelWidth, screenWidth);
    const int gridWidth = std::max(0, screenWidth - gridX);
    const int playheadX = arrangementPlayheadX(player, gridX, gridWidth);
    gfx.fillRect(playheadX, rowsTop, 2, rowsHeight, kAccent);

    const auto& selectedLayer = projection.layers.layers[selected];
    const SmfTrackInfoSnapshot* selectedInfo =
        selectedLayer.trackIndex < projection.tracks.trackCount
            ? &projection.tracks.tracks[selectedLayer.trackIndex]
            : nullptr;
    drawOverlayBands(gfx,
                     player,
                     selectedLayer,
                     selectedInfo,
                     hubTrackRoute(projection, player.rawRouting,
                                   selectedLayer.trackIndex),
                     smfTrackLevelState().levelFor(selectedLayer.trackIndex),
                     selectedTrackIsSolo(projection.generation,
                                         selectedLayer.trackIndex),
                     projection.layers.partial);
}


// Takes a finished GRAB from the player mailbox into the target voice's
// Working Melody (one Undo step, on the synth page).
void SequencerHubPage::tick() {
    SequencerHubPageBase::tick();
    if (!midiGrabPending_) return;
    ISmfPlayerService* service = smfPlayerService();
    if (!service) {
        midiGrabPending_ = false;
        return;
    }
    const SmfGrabResult result = service->grabResult();
    if (result.state == SmfGrabState::Working) return;
    if (result.state == SmfGrabState::Failed) {
        midiGrabPending_ = false;
        UI::showToast(result.message[0] ? result.message : "GRAB FAILED", 1400);
        service->acknowledgeGrab();
        return;
    }
    if (result.state != SmfGrabState::Ready) {
        midiGrabPending_ = false;
        return;
    }
    // Ready stays in the mailbox until it is copied out; without memory for
    // the copy, try again on the next tick rather than lose or wedge it.
    std::unique_ptr<PhraseRuntime::RuntimeSynthEventBuffer> melody(
        new (std::nothrow) PhraseRuntime::RuntimeSynthEventBuffer());
    if (!melody) return;
    if (!service->takeGrabbedMelody(*melody)) {
        midiGrabPending_ = false;
        UI::showToast("GRAB FAILED", 1000);
        return;
    }
    midiGrabPending_ = false;
    const int voice = midiGrabVoice_ == 1 ? 1 : 0;
    char toast[40]{};
    if (SynthSequencerPage::replaceMelodyFor(mini_acid_, audio_guard_, voice, *melody)) {
        std::snprintf(toast, sizeof(toast), "GRAB %uN %uB > MEL %c",
                      static_cast<unsigned>(result.notes),
                      static_cast<unsigned>(result.bars),
                      static_cast<char>('A' + voice));
        UI::showToast(toast, 1600);
    } else {
        UI::showToast("GRAB: MELODY CHANGED, RETRY", 1400);
    }
}
