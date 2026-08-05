#define GROOVEPUTER_SEQUENCER_HUB_WRAPPER_CONSUMER 1
#include "sequencer_hub_page.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>

#include "../components/music_visuals.h"
#include "../player_hub_navigation.h"
#include "../ui_common.h"
#include "../ui_input.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/smf_session_generation.h"
#include "src/midi/smf_structural_inspector.h"
#include "src/midi/smf_track_inspector.h"
#include "src/midi/smf_track_mute.h"

namespace {
using namespace GroovePuterMidi;
constexpr uint8_t kVisibleMidiRows = 6;

struct HubMidiProjection {
    SmfStructuralInspectorSnapshot layers{};
    SmfTrackInspectorSnapshot tracks{};
    SmfTrackMuteSnapshot mute{};
    uint32_t generation{0};

    bool ready() const {
        return smfSnapshotGenerationsMatch(
            generation,
            layers.generation,
            tracks.generation,
            mute.generation);
    }
};

HubMidiProjection captureHubMidiProjection() {
    HubMidiProjection projection{};
    projection.layers = smfStructuralInspectorState().snapshot();
    projection.tracks = smfTrackInspectorState().snapshot();
    projection.mute = smfTrackMuteState().snapshot();
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

bool muted(const SmfTrackMuteSnapshot& state, uint16_t track) {
    return track < 64u &&
           (state.mutedMask & (uint64_t{1} << track)) != 0u;
}

const char* shortRole(SmfStructuralRole role) {
    switch (role) {
        case SmfStructuralRole::Drums: return "DRM";
        case SmfStructuralRole::Bass: return "BAS";
        case SmfStructuralRole::Chords: return "CHD";
        case SmfStructuralRole::Pad: return "PAD";
        case SmfStructuralRole::Lead: return "LED";
        default: return "OTH";
    }
}

void formatTrackChannel(const SmfTrackInfoSnapshot* info,
                        char* output,
                        std::size_t outputSize) {
    if (!output || outputSize == 0u) return;
    if (!info || info->channelMask == 0u) {
        std::snprintf(output, outputSize, "--");
        return;
    }
    if (info->usesMultipleChannels()) {
        std::snprintf(output, outputSize, "MIX");
        return;
    }
    const int channel = info->primaryChannel();
    if (channel < 0) std::snprintf(output, outputSize, "--");
    else std::snprintf(output, outputSize, "C%02d", channel + 1);
}

void formatDensity(uint16_t activePermille,
                   char* output,
                   std::size_t outputSize) {
    if (!output || outputSize < 4u) return;
    const uint16_t bounded = std::min<uint16_t>(activePermille, 1000u);
    const uint8_t filled = static_cast<uint8_t>((bounded + 332u) / 333u);
    for (uint8_t index = 0u; index < 3u; ++index) {
        output[index] = index < filled ? '#' : '.';
    }
    output[3] = '\0';
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

void configurePlayerPanel(uint32_t generation,
                          bool muteMixer,
                          bool channelInspector) {
    PlayerHubNavigation::PlayerViewState& view =
        PlayerHubNavigation::playerViewState();
    view.generation = generation;
    view.valid = generation != 0u;
    view.browserVisible = false;
    view.performanceVisible = false;
    view.channelInspectorVisible = channelInspector;
    view.muteMixerVisible = muteMixer;
    view.structuralInspectorVisible = false;
    if (channelInspector) view.channelInspectorScroll = 0;
}
}  // namespace

void SequencerHubPage::onEnter(int context) {
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

    if (midiGeneration_ != projection.generation) {
        midiGeneration_ = projection.generation;
        midiSelected_ = 0u;
        midiScroll_ = 0u;
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
        return;
    }
    if (midiSelected_ >= projection.layers.layerCount) {
        midiSelected_ = static_cast<uint8_t>(projection.layers.layerCount - 1u);
    }
    syncMidiScroll(projection.layers.layerCount);
}

void SequencerHubPage::returnFromMidiOverview() {
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
    if (event.alt || event.ctrl || event.meta) return true;

    if (event.key == 'm' || event.key == 'M') {
        midiOverview_ = false;
        midiReturnToPlayer_ = false;
        return true;
    }
    if (event.key == 'p' || event.key == 'P') {
        midiReturnToPlayer_ = true;
        returnFromMidiOverview();
        return true;
    }
    if (event.key == 'b' || event.key == 'B' || UIInput::isBack(event)) {
        returnFromMidiOverview();
        return true;
    }
    if (event.key == ' ') {
        UI::showToast("MIDI TRANSPORT: PLAYER", 900);
        return true;
    }

    ISmfPlayerService* service = smfPlayerService();
    const SmfPlayerSnapshot player = service ? service->snapshot() : SmfPlayerSnapshot{};
    const HubMidiProjection projection = captureHubMidiProjection();
    if (projectionIsSyncing(player, projection)) {
        UI::showToast("MIDI LAYERS: SYNCING", 800);
        return true;
    }
    if (!projection.ready()) {
        UI::showToast("LOAD MIDI IN PLAYER", 800);
        return true;
    }
    if (midiGeneration_ != projection.generation) syncMidiSessionSelection();

    if (event.key == 'u' || event.key == 'U' ||
        event.key == 'i' || event.key == 'I') {
        if (!selectProjectedLayer(projection, midiSelected_)) {
            UI::showToast("MIDI LAYERS: SYNCING", 800);
            return true;
        }
        const bool openMuteMixer = event.key == 'u' || event.key == 'U';
        configurePlayerPanel(
            projection.generation, openMuteMixer, !openMuteMixer);
        midiOverview_ = false;
        midiReturnToPlayer_ = false;
        requestPageTransition(
            PlayerHubNavigation::kPlayerPage,
            PlayerHubNavigation::kReturnToPlayerContext);
        return true;
    }

    if (event.key == 'a' || event.key == 'A') {
        if (smfTrackMuteState().clear(projection.generation)) {
            UI::showToast("ALL MIDI TRACKS ON", 800);
        } else {
            UI::showToast("MIDI LAYERS: SYNCING", 800);
        }
        return true;
    }
    if (event.key >= '1' && event.key <= '9') {
        if (!toggleMidiLayer(static_cast<uint8_t>(event.key - '1'))) {
            UI::showToast("MIDI LAYER UNAVAILABLE", 800);
        }
        return true;
    }
    if (projection.layers.layerCount == 0u) return true;

    int move = 0;
    if (UIInput::isUp(event)) move = -1;
    else if (UIInput::isDown(event)) move = 1;
    else if (event.scancode == GROOVEPUTER_LEFT) move = -kVisibleMidiRows;
    else if (event.scancode == GROOVEPUTER_RIGHT) move = kVisibleMidiRows;
    if (move != 0) {
        const int count = static_cast<int>(projection.layers.layerCount);
        int selected = (static_cast<int>(midiSelected_) + move) % count;
        if (selected < 0) selected += count;
        midiSelected_ = static_cast<uint8_t>(selected);
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

    UI::drawStandardHeader(gfx, mini_acid_, "HUB · MIDI");
    LayoutManager::clearContent(gfx);

    char line[64];
    gfx.setTextColor(MusicVisuals::accentForStyle());
    std::snprintf(line, sizeof(line), "%s  BAR %lu  %.20s",
                  smfPlayerStateName(player.state),
                  static_cast<unsigned long>(player.bar),
                  player.filename[0] ? player.filename : "NO FILE");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    if (projectionIsSyncing(player, projection)) {
        gfx.setTextColor(COLOR_WARN);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "SYNCING");
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3),
                     "WAITING FOR CURRENT SMF SESSION");
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(4),
                     "PLAYER TRANSPORT KEEPS RUNNING");
        UI::drawStandardFooter(gfx, "P Player M Internal",
                               "No stale layers / no reload");
        return;
    }

    if (!projection.ready()) {
        gfx.setTextColor(player.state == SmfPlayerState::Error
                             ? COLOR_DANGER
                             : COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3),
                     player.state == SmfPlayerState::Error
                         ? "MIDI LOAD ERROR · RETURN PLAYER"
                         : "NO MIDI LAYERS · LOAD IN PLAYER");
        UI::drawStandardFooter(gfx, "P Player M Internal",
                               "Player owns load/transport");
        return;
    }

    if (midiGeneration_ != projection.generation) syncMidiSessionSelection();
    if (projection.layers.layerCount == 0u) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3),
                     "NO AUDIBLE MIDI LAYERS");
        UI::drawStandardFooter(gfx, "P Player M Internal",
                               "Player owns load/transport");
        return;
    }

    const uint8_t selected = std::min<uint8_t>(
        midiSelected_, projection.layers.layerCount - 1u);
    for (uint8_t row = 0u; row < kVisibleMidiRows; ++row) {
        const uint8_t index = static_cast<uint8_t>(midiScroll_ + row);
        if (index >= projection.layers.layerCount) break;
        const auto& layer = projection.layers.layers[index];
        const bool isSelected = index == selected;
        const bool isMuted = muted(projection.mute, layer.trackIndex);
        const SmfTrackInfoSnapshot* info =
            layer.trackIndex < projection.tracks.trackCount
                ? &projection.tracks.tracks[layer.trackIndex]
                : nullptr;
        const char* label = info && info->hasName() ? info->name : "MIDI TRACK";
        char channel[5]{};
        char density[4]{};
        formatTrackChannel(info, channel, sizeof(channel));
        formatDensity(layer.activePermille, density, sizeof(density));
        const char hotkey = index < 9u
            ? static_cast<char>('1' + index)
            : '-';

        gfx.setTextColor(isSelected ? MusicVisuals::accentForStyle()
                                    : (isMuted ? COLOR_WARN : COLOR_TEXT));
        std::snprintf(line, sizeof(line), "%c%c%02u %-3s %-3s %s %-3s %.8s",
                      hotkey,
                      isSelected ? '>' : ' ',
                      static_cast<unsigned>(layer.trackIndex + 1u),
                      isMuted ? "MUT" : "ON ",
                      channel,
                      density,
                      shortRole(layer.role),
                      label);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(row + 1u), line);
    }

    const auto& selectedLayer = projection.layers.layers[selected];
    gfx.setTextColor(COLOR_LABEL);
    if (selectedLayer.loopBars == 0u) {
        std::snprintf(line, sizeof(line), "G%s SW%u LOOP-- N%u.%u A%u%%",
                      selectedLayer.gridDenominator == 0u ? "FREE" :
                          (selectedLayer.gridDenominator == 8u ? "8" :
                           (selectedLayer.gridDenominator == 16u ? "16" : "32")),
                      static_cast<unsigned>(selectedLayer.swingPercent),
                      static_cast<unsigned>(selectedLayer.notesPerBarX10 / 10u),
                      static_cast<unsigned>(selectedLayer.notesPerBarX10 % 10u),
                      static_cast<unsigned>(selectedLayer.activePermille / 10u));
    } else {
        std::snprintf(line, sizeof(line), "G%u SW%u L%u N%u.%u A%u%%",
                      static_cast<unsigned>(selectedLayer.gridDenominator),
                      static_cast<unsigned>(selectedLayer.swingPercent),
                      static_cast<unsigned>(selectedLayer.loopBars),
                      static_cast<unsigned>(selectedLayer.notesPerBarX10 / 10u),
                      static_cast<unsigned>(selectedLayer.notesPerBarX10 % 10u),
                      static_cast<unsigned>(selectedLayer.activePermille / 10u));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);

    UI::drawStandardFooter(gfx, "P Player U Mixer I Chans",
                           "1-9 Mute ENT Sel A AllOn");
}
