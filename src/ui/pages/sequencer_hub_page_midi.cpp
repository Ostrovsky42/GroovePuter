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
constexpr uint8_t kVisibleMidiRows = 7u;
constexpr uint8_t kArrangementSegments = kSmfStructuralFormSegments;
constexpr int kLayerLabelWidth = 60;
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

void drawArrangementRow(IGfx& gfx,
                        int screenWidth,
                        int contentTop,
                        int contentHeight,
                        uint8_t row,
                        uint8_t layerIndex,
                        const SmfStructuralLayerSnapshot& layer,
                        const SmfTrackInfoSnapshot* info,
                        bool isMuted,
                        bool isSelected) {
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
    std::snprintf(clippedLabel, sizeof(clippedLabel), "%.8s", label);
    gfx.setTextColor(isMuted ? kMutedText : kBodyText);
    gfx.drawText(12, textY, clippedLabel);

    for (uint8_t segment = 0u; segment < kArrangementSegments; ++segment) {
        const int x0 = gridX +
            (static_cast<int>(segment) * gridWidth) / kArrangementSegments;
        const int x1 = gridX +
            (static_cast<int>(segment + 1u) * gridWidth) / kArrangementSegments;
        const int cellWidth = std::max(1, x1 - x0 - kCellGap);
        gfx.fillRect(x0, y0, cellWidth, rowHeight,
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
                      bool partial) {
    const int width = gfx.width();
    const int height = gfx.height();
    const int bottomY = std::max(0, height - kOverlayBandHeight);
    gfx.fillRect(0, 0, width, kOverlayBandHeight, kOverlayScrim);
    gfx.fillRect(0, bottomY, width, kOverlayBandHeight, kOverlayScrim);

    char line[64]{};
    gfx.setTextColor(kBodyText);
    std::snprintf(line, sizeof(line), "%.23s%s",
                  player.filename[0] ? player.filename : "NO FILE",
                  partial ? "*" : "");
    gfx.drawText(3, 2, line);

    std::snprintf(line, sizeof(line), "BAR %lu/%lu",
                  static_cast<unsigned long>(player.bar),
                  static_cast<unsigned long>(std::max<uint32_t>(player.totalBars, 1u)));
    gfx.drawText(std::max(3, width - gfx.textWidth(line) - 3), 2, line);

    char channel[8]{};
    char range[12]{};
    formatTrackChannel(selectedInfo, channel, sizeof(channel));
    formatPitchRange(selectedLayer, range, sizeof(range));
    std::snprintf(line, sizeof(line), "%s N%u %s",
                  channel,
                  static_cast<unsigned>(selectedLayer.noteCount),
                  range);
    gfx.drawText(3, bottomY + 2, line);

    constexpr const char* kHints = "H/E 1-9 ENT";
    gfx.setTextColor(kMutedText);
    gfx.drawText(std::max(3, width - gfx.textWidth(kHints) - 3),
                 bottomY + 2,
                 kHints);
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
    gfx.fillRect(0, 0, screenWidth, screenHeight, kScreenBackground);

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
        drawArrangementRow(
            gfx,
            screenWidth,
            rowsTop,
            rowsHeight,
            row,
            index,
            layer,
            info,
            muted(projection.mute, layer.trackIndex),
            index == selected);
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
                     projection.layers.partial);
}