#define GROOVEPUTER_SMF_PLAYER_WRAPPER_CONSUMER 1
#include "smf_player_page.h"

#include <algorithm>
#include <cstdio>

#include "../components/music_visuals.h"
#include "src/dsp/miniacid_engine.h"
#include "src/midi/smf_structural_inspector.h"
#include "src/midi/smf_track_mute.h"

namespace {

using namespace GroovePuterMidi;

void formatMidiNote(uint8_t note, char* dst, std::size_t size) {
    static constexpr const char* kNames[] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    const int octave = static_cast<int>(note / 12u) - 1;
    std::snprintf(dst, size, "%s%d", kNames[note % 12u], octave);
}

int layerPosition(const SmfStructuralInspectorSnapshot& snapshot,
                  uint16_t physicalTrack) {
    for (uint8_t index = 0; index < snapshot.layerCount; ++index) {
        if (snapshot.layers[index].trackIndex == physicalTrack) return index;
    }
    return -1;
}

bool selectLayerRelative(const SmfStructuralInspectorSnapshot& snapshot,
                         int delta) {
    if (snapshot.layerCount == 0u) return false;
    SmfTrackMuteState& muteState = smfTrackMuteState();
    const SmfTrackMuteSnapshot mute = muteState.snapshot();
    int position = layerPosition(snapshot, mute.selectedTrack);
    if (position < 0) position = 0;
    else if (delta != 0) {
        position = (position + delta) % static_cast<int>(snapshot.layerCount);
        if (position < 0) position += snapshot.layerCount;
    }
    return muteState.selectTrack(snapshot.layers[position].trackIndex);
}

const char* resembles(const SmfStructuralLayerSnapshot& layer) {
    // Interpretation remains deliberately secondary to the measured values.
    if (layer.swingPercent >= 56u && layer.notesPerBarX10 < 80u) {
        return "LO-FI / BROKEN";
    }
    if (layer.swingPercent >= 56u) return "BROKEN";
    if (layer.gridDenominator >= 16u && layer.notesPerBarX10 >= 80u) {
        return "TECHNO";
    }
    if (layer.activePermille >= 750u &&
        layer.motion == SmfStructuralMotion::Low) {
        return "AMBIENT";
    }
    return "STRAIGHT / HYBRID";
}

void drawStructuralInspector(IGfx& gfx) {
    const SmfStructuralInspectorSnapshot snapshot =
        smfStructuralInspectorState().snapshot();
    if (snapshot.layerCount == 0u) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2),
                     "NO STRUCTURAL DATA");
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3),
                     "WAIT FOR MIDI LOAD PASS");
        return;
    }

    const SmfTrackMuteSnapshot mute = smfTrackMuteState().snapshot();
    int selected = layerPosition(snapshot, mute.selectedTrack);
    if (selected < 0) selected = 0;
    const SmfStructuralLayerSnapshot& layer = snapshot.layers[selected];

    char line[64];
    char low[5] = "--";
    char high[5] = "--";
    if (layer.channelMask != 0u) {
        formatMidiNote(layer.minNote, low, sizeof(low));
        formatMidiNote(layer.maxNote, high, sizeof(high));
    }

    gfx.setTextColor(MusicVisuals::accentForStyle());
    std::snprintf(line, sizeof(line), "L%u TRK %02u  %s%s",
                  static_cast<unsigned>(selected + 1),
                  static_cast<unsigned>(layer.trackIndex + 1u),
                  smfStructuralRoleName(layer.role),
                  snapshot.partial ? "  PARTIAL 64" : "");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    gfx.setTextColor(COLOR_TEXT);
    if (layer.gridDenominator == 0u) {
        std::snprintf(line, sizeof(line), "GRID FREE       LOOP %u BAR",
                      static_cast<unsigned>(layer.loopBars));
    } else {
        std::snprintf(line, sizeof(line), "GRID 1/%u       LOOP %u BAR",
                      static_cast<unsigned>(layer.gridDenominator),
                      static_cast<unsigned>(layer.loopBars));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    std::snprintf(line, sizeof(line), "SWING %u%%       MOTION %s",
                  static_cast<unsigned>(layer.swingPercent),
                  smfStructuralMotionName(layer.motion));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), line);

    std::snprintf(line, sizeof(line), "NOTES/B %u.%u    ACTIVE %u%%",
                  static_cast<unsigned>(layer.notesPerBarX10 / 10u),
                  static_cast<unsigned>(layer.notesPerBarX10 % 10u),
                  static_cast<unsigned>(layer.activePermille / 10u));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), line);

    std::snprintf(line, sizeof(line), "NOTE REGISTER %s-%s  POLY %u",
                  low, high, static_cast<unsigned>(layer.maxPolyphony));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(4), line);

    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(line, sizeof(line), "FORM %u %u %u %u",
                  static_cast<unsigned>(layer.form[0]),
                  static_cast<unsigned>(layer.form[1]),
                  static_cast<unsigned>(layer.form[2]),
                  static_cast<unsigned>(layer.form[3]));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(5), line);
    const int graphX = Layout::COL_1 + 72;
    const int graphBaseY = LayoutManager::lineY(5) + 8;
    for (uint8_t bin = 0; bin < 4u; ++bin) {
        const int height = std::max(1, static_cast<int>(layer.form[bin]));
        gfx.fillRect(graphX + bin * 13, graphBaseY - height,
                     8, height, MusicVisuals::secondaryForStyle());
    }

    std::snprintf(line, sizeof(line), "OVERLAP CHORDS %u%%  LEAD %u%%",
                  static_cast<unsigned>(layer.overlapChordsPercent),
                  static_cast<unsigned>(layer.overlapLeadPercent));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6), line);

    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(line, sizeof(line), "RESEMBLES %s", resembles(layer));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);
}

}  // namespace

bool SmfPlayerPage::handleEvent(UIEvent& event) {
    const bool numericMuteHotkey = event.key >= '1' && event.key <= '9';
    if (event.event_type != GROOVEPUTER_KEY_DOWN || event.alt || event.ctrl ||
        (event.meta && !numericMuteHotkey)) {
        return SmfPlayerPageBase::handleEvent(event);
    }

    // Keep Stage 1A hotkeys page-first in every MIDI Player subview.
    if (numericMuteHotkey) return SmfPlayerPageBase::handleEvent(event);

    if (!browserVisible_ && (event.key == 's' || event.key == 'S')) {
        structuralInspectorVisible_ = !structuralInspectorVisible_;
        if (structuralInspectorVisible_) {
            muteMixerVisible_ = false;
            performanceVisible_ = false;
            channelInspectorVisible_ = false;
            selectLayerRelative(smfStructuralInspectorState().snapshot(), 0);
        }
        return true;
    }

    if (!structuralInspectorVisible_) {
        return SmfPlayerPageBase::handleEvent(event);
    }

    if (event.key == 'b' || event.key == 'B' || event.key == '\b') {
        structuralInspectorVisible_ = false;
        return true;
    }
    if (event.key == 'u' || event.key == 'U' ||
        event.key == 'i' || event.key == 'I' ||
        event.key == 'd' || event.key == 'D') {
        structuralInspectorVisible_ = false;
        return SmfPlayerPageBase::handleEvent(event);
    }
    if (event.scancode == GROOVEPUTER_UP ||
        event.scancode == GROOVEPUTER_DOWN) {
        const int delta = event.scancode == GROOVEPUTER_UP ? -1 : 1;
        selectLayerRelative(smfStructuralInspectorState().snapshot(), delta);
        return true;
    }

    // Transport, seek, tempo, panic and routing retain their existing behavior.
    return SmfPlayerPageBase::handleEvent(event);
}

void SmfPlayerPage::drawHeader(IGfx& gfx) {
    if (structuralInspectorVisible_) {
        UI::drawStandardHeader(gfx, miniAcid_, "MIDI STRUCTURE");
        return;
    }
    SmfPlayerPageBase::drawHeader(gfx);
}

void SmfPlayerPage::drawContent(IGfx& gfx) {
    if (structuralInspectorVisible_) {
        LayoutManager::clearContent(gfx);
        drawStructuralInspector(gfx);
        return;
    }
    SmfPlayerPageBase::drawContent(gfx);
}

void SmfPlayerPage::drawFooter(IGfx& gfx) {
    if (structuralInspectorVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Layer S/B Player",
                               "1-9 Mute U Table I Channels");
        return;
    }
    SmfPlayerPageBase::drawFooter(gfx);
}
