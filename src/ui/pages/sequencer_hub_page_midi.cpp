#define GROOVEPUTER_SEQUENCER_HUB_WRAPPER_CONSUMER 1
#include "sequencer_hub_page.h"

#include <algorithm>
#include <cstdio>

#include "../ui_common.h"
#include "../ui_input.h"
#include "../components/music_visuals.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/smf_structural_inspector.h"
#include "src/midi/smf_track_inspector.h"
#include "src/midi/smf_track_mute.h"

namespace {
using namespace GroovePuterMidi;
constexpr uint8_t kVisibleMidiRows = 6;

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
}  // namespace

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
        const auto layers = smfStructuralInspectorState().snapshot();
        if (layers.layerCount == 0u) {
            midiSelected_ = 0u;
            midiScroll_ = 0u;
        } else if (midiSelected_ >= layers.layerCount) {
            midiSelected_ = static_cast<uint8_t>(layers.layerCount - 1u);
            syncMidiScroll(layers.layerCount);
        }
        UI::showToast(midiOverview_ ? "HUB: MIDI" : "HUB: INTERNAL", 700);
        return true;
    }

    if (midiOverview_) return handleMidiOverviewEvent(event);
    return SequencerHubPageBase::handleEvent(event);
}

bool SequencerHubPage::toggleMidiLayer(uint8_t layerIndex) {
    const auto layers = smfStructuralInspectorState().snapshot();
    if (layerIndex >= layers.layerCount) return false;
    SmfTrackMuteState& state = smfTrackMuteState();
    const uint16_t track = layers.layers[layerIndex].trackIndex;
    if (!state.selectTrack(track) || !state.toggleSelected()) return false;

    char toast[32];
    std::snprintf(toast, sizeof(toast), "MIDI %u TRK %02u %s",
                  static_cast<unsigned>(layerIndex + 1u),
                  static_cast<unsigned>(track + 1u),
                  state.snapshot().selectedMuted() ? "MUTED" : "ON");
    UI::showToast(toast, 700);
    return true;
}

bool SequencerHubPage::handleMidiOverviewEvent(UIEvent& event) {
    if (event.event_type != GROOVEPUTER_KEY_DOWN) return false;
    if (event.alt || event.ctrl || event.meta) return true;

    const auto layers = smfStructuralInspectorState().snapshot();
    if (event.key == 'm' || event.key == 'M' ||
        event.key == 'b' || event.key == 'B' || UIInput::isBack(event)) {
        midiOverview_ = false;
        return true;
    }
    if (event.key == ' ') {
        UI::showToast("MIDI TRANSPORT: PLAYER", 900);
        return true;
    }
    if (event.key == 'a' || event.key == 'A') {
        smfTrackMuteState().clear();
        UI::showToast("ALL MIDI TRACKS ON", 800);
        return true;
    }
    if (event.key >= '1' && event.key <= '9') {
        toggleMidiLayer(static_cast<uint8_t>(event.key - '1'));
        return true;
    }
    if (layers.layerCount == 0u) return true;

    if (UIInput::isUp(event)) {
        midiSelected_ = midiSelected_ == 0u
            ? static_cast<uint8_t>(layers.layerCount - 1u)
            : static_cast<uint8_t>(midiSelected_ - 1u);
        syncMidiScroll(layers.layerCount);
        return true;
    }
    if (UIInput::isDown(event)) {
        midiSelected_ = static_cast<uint8_t>((midiSelected_ + 1u) % layers.layerCount);
        syncMidiScroll(layers.layerCount);
        return true;
    }
    if (event.key == '\n' || event.key == '\r') {
        toggleMidiLayer(midiSelected_);
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
    const auto layers = smfStructuralInspectorState().snapshot();
    const auto tracks = smfTrackInspectorState().snapshot();
    const auto mute = smfTrackMuteState().snapshot();
    ISmfPlayerService* service = smfPlayerService();
    const SmfPlayerSnapshot player = service ? service->snapshot() : SmfPlayerSnapshot{};

    UI::drawStandardHeader(gfx, mini_acid_, "HUB · MIDI");
    LayoutManager::clearContent(gfx);

    char line[64];
    gfx.setTextColor(MusicVisuals::accentForStyle());
    std::snprintf(line, sizeof(line), "%s  BAR %lu  %.20s",
                  smfPlayerStateName(player.state),
                  static_cast<unsigned long>(player.bar),
                  player.filename[0] ? player.filename : "NO FILE");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    if (layers.layerCount == 0u) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3),
                     "NO MIDI LAYERS · LOAD IN PLAYER");
        UI::drawStandardFooter(gfx, "M Internal", "Player owns load/transport");
        return;
    }

    const uint8_t selected = std::min<uint8_t>(midiSelected_, layers.layerCount - 1u);
    for (uint8_t row = 0u; row < kVisibleMidiRows; ++row) {
        const uint8_t index = static_cast<uint8_t>(midiScroll_ + row);
        if (index >= layers.layerCount) break;
        const auto& layer = layers.layers[index];
        const bool isSelected = index == selected;
        const bool isMuted = muted(mute, layer.trackIndex);
        const SmfTrackInfoSnapshot* info = layer.trackIndex < tracks.trackCount
            ? &tracks.tracks[layer.trackIndex]
            : nullptr;
        const char* label = info && info->hasName() ? info->name : "MIDI TRACK";

        gfx.setTextColor(isSelected ? MusicVisuals::accentForStyle()
                                    : (isMuted ? COLOR_WARN : COLOR_TEXT));
        std::snprintf(line, sizeof(line), "%u%c%02u %-3s %-3s %.15s",
                      static_cast<unsigned>(index + 1u),
                      isSelected ? '>' : ' ',
                      static_cast<unsigned>(layer.trackIndex + 1u),
                      isMuted ? "MUT" : "ON ",
                      shortRole(layer.role), label);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(row + 1u), line);
    }

    const auto& selectedLayer = layers.layers[selected];
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

    UI::drawStandardFooter(gfx, "M Internal UP/DN Select",
                           "1-9 Mute ENT Sel A AllOn");
}
