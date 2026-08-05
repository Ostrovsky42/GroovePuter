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
constexpr int kFormBlockWidth = 12;
constexpr int kFormBlockHeight = 8;
constexpr int kFormBlockGap = 2;

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

const char* transportLabel(SmfPlayerState state) {
    switch (state) {
        case SmfPlayerState::Loading: return "...";
        case SmfPlayerState::Armed: return "ARM";
        case SmfPlayerState::Playing: return ">";
        case SmfPlayerState::Paused: return "||";
        case SmfPlayerState::Stopped: return "[]";
        case SmfPlayerState::Error: return "!";
        case SmfPlayerState::Unloaded:
        default: return "--";
    }
}

const char* loopLabel(uint8_t bars) {
    switch (bars) {
        case 1u: return "1-BAR LOOP";
        case 2u: return "2-BAR LOOP";
        case 4u: return "4-BAR LOOP";
        default: return "FREE FORM";
    }
}

void formatTrackChannel(const SmfTrackInfoSnapshot* info,
                        char* output,
                        std::size_t outputSize) {
    if (!output || outputSize == 0u) return;
    if (!info || info->channelMask == 0u) {
        std::snprintf(output, outputSize, "CH --");
        return;
    }
    if (info->usesMultipleChannels()) {
        std::snprintf(output, outputSize, "MULTI");
        return;
    }
    const int channel = info->primaryChannel();
    if (channel < 0) std::snprintf(output, outputSize, "CH --");
    else std::snprintf(output, outputSize, "CH %d", channel + 1);
}

uint8_t currentFormSection(uint32_t bar) {
    const uint32_t zeroBasedBar = bar == 0u ? 0u : bar - 1u;
    return static_cast<uint8_t>(std::min<uint32_t>(zeroBasedBar / 16u, 3u));
}

void drawFormStrip(IGfx& gfx,
                   int x,
                   int y,
                   const SmfStructuralLayerSnapshot& layer,
                   uint8_t currentSection,
                   bool isMuted,
                   bool isSelected) {
    const IGfxColor accent = MusicVisuals::accentForStyle();
    const IGfxColor border = isSelected ? accent : COLOR_LABEL;
    const IGfxColor fill = isMuted ? COLOR_WARN
                                   : (isSelected ? accent : COLOR_TEXT);

    for (uint8_t section = 0u; section < 4u; ++section) {
        const int blockX = x + section * (kFormBlockWidth + kFormBlockGap);
        gfx.drawRect(blockX, y, kFormBlockWidth, kFormBlockHeight, border);

        const uint8_t level = std::min<uint8_t>(layer.form[section], 8u);
        const int fillHeight = (static_cast<int>(level) *
                                (kFormBlockHeight - 3) + 7) / 8;
        if (fillHeight > 0) {
            gfx.fillRect(blockX + 2,
                         y + kFormBlockHeight - 1 - fillHeight,
                         kFormBlockWidth - 4,
                         fillHeight,
                         fill);
        }
        if (section == currentSection) {
            gfx.drawLine(blockX,
                         y - 2,
                         blockX + kFormBlockWidth - 1,
                         y - 2,
                         accent);
        }
    }
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

    const bool hubShortcut =
        !event.alt && !event.ctrl && (event.key == 'h' || event.key == 'H');
    if (hubShortcut || UIInput::isBack(event)) {
        returnFromMidiOverview();
        return true;
    }

    if (event.alt || event.ctrl || event.meta) return true;

    // Retain the old aliases without advertising them in the compact Hub UI.
    if (event.key == 'p' || event.key == 'P') {
        midiReturnToPlayer_ = true;
        returnFromMidiOverview();
        return true;
    }
    if (event.key == 'm' || event.key == 'M') {
        midiOverview_ = false;
        midiReturnToPlayer_ = false;
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
    if (projection.ready() && projection.layers.analyzedBars != 0u) {
        std::snprintf(line, sizeof(line), "%s BAR %lu/%u  %.18s",
                      transportLabel(player.state),
                      static_cast<unsigned long>(player.bar),
                      static_cast<unsigned>(projection.layers.analyzedBars),
                      player.filename[0] ? player.filename : "NO FILE");
    } else {
        std::snprintf(line, sizeof(line), "%s BAR %lu  %.22s",
                      transportLabel(player.state),
                      static_cast<unsigned long>(player.bar),
                      player.filename[0] ? player.filename : "NO FILE");
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    if (projectionIsSyncing(player, projection)) {
        gfx.setTextColor(COLOR_WARN);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "SYNCING");
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3),
                     "WAITING FOR CURRENT SMF SESSION");
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(4),
                     "PLAYER TRANSPORT KEEPS RUNNING");
        UI::drawStandardFooter(gfx, "H/ESC Player",
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
        UI::drawStandardFooter(gfx, "H/ESC Player",
                               "Player owns load/transport");
        return;
    }

    if (midiGeneration_ != projection.generation) syncMidiSessionSelection();
    if (projection.layers.layerCount == 0u) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3),
                     "NO AUDIBLE MIDI LAYERS");
        UI::drawStandardFooter(gfx, "H/ESC Player",
                               "Player owns load/transport");
        return;
    }

    const uint8_t selected = std::min<uint8_t>(
        midiSelected_, projection.layers.layerCount - 1u);
    const uint8_t formSection = currentFormSection(player.bar);
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
        const char hotkey = index < 9u
            ? static_cast<char>('1' + index)
            : '-';
        const int rowY = LayoutManager::lineY(row + 1u);

        gfx.setTextColor(isSelected ? MusicVisuals::accentForStyle()
                                    : (isMuted ? COLOR_WARN : COLOR_TEXT));
        std::snprintf(line, sizeof(line), "%c%c %-5s %.10s",
                      hotkey,
                      isSelected ? '>' : ' ',
                      roleLabel(layer.role),
                      label);
        gfx.drawText(Layout::COL_1, rowY, line);

        drawFormStrip(gfx,
                      Layout::COL_1 + 126,
                      rowY,
                      layer,
                      formSection,
                      isMuted,
                      isSelected);

        gfx.setTextColor(isMuted ? COLOR_WARN : COLOR_LABEL);
        gfx.drawText(Layout::COL_1 + 184, rowY,
                     isMuted ? "MUTE" : "ON");
    }

    const auto& selectedLayer = projection.layers.layers[selected];
    const SmfTrackInfoSnapshot* selectedInfo =
        selectedLayer.trackIndex < projection.tracks.trackCount
            ? &projection.tracks.tracks[selectedLayer.trackIndex]
            : nullptr;
    char channel[8]{};
    formatTrackChannel(selectedInfo, channel, sizeof(channel));
    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(line, sizeof(line), "TRACK %02u  %s  %s",
                  static_cast<unsigned>(selectedLayer.trackIndex + 1u),
                  channel,
                  loopLabel(selectedLayer.loopBars));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);

    UI::drawStandardFooter(gfx, "H/ESC Player  1-9 Mute",
                           "UP/DN Select ENT Toggle A AllOn");
}
