#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str) -> None:
    target = ROOT / path
    text = target.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one anchor, found {count}: {old[:80]!r}")
    target.write_text(text.replace(old, new, 1))


replace_once(
    "src/midi/smf_stream.h",
    """    mutable int selected_{-1};
    mutable bool selectedValid_{false};
    uint8_t cachePool_[kSmfStreamCacheBytes]{};
""",
    """    mutable int selected_{-1};
    mutable bool selectedValid_{false};
    // True only for the first full pass performed by loadFile(). reset() does
    // not reactivate it, so seek/restart never re-analyzes the SD source.
    bool captureStructuralAnalysis_{false};
    uint8_t cachePool_[kSmfStreamCacheBytes]{};
""",
)

replace_once(
    "src/midi/smf_stream.cpp",
    '#include "smf_track_inspector.h"\n#include "smf_track_mute.h"\n',
    '#include "smf_structural_inspector.h"\n#include "smf_track_inspector.h"\n#include "smf_track_mute.h"\n',
)
replace_once(
    "src/midi/smf_stream.cpp",
    """    smfTrackMuteState().reset(index.trackCount);
    smfTrackInspectorState().reset(index.trackCount);
    for (std::size_t i = 0; i < kSmfMaxTracks; ++i) hasNext_[i] = false;
""",
    """    smfTrackMuteState().reset(index.trackCount);
    smfTrackInspectorState().reset(index.trackCount);
    smfStructuralInspectorState().reset(index.division, index.trackCount);
    captureStructuralAnalysis_ = true;
    for (std::size_t i = 0; i < kSmfMaxTracks; ++i) hasNext_[i] = false;
""",
)
replace_once(
    "src/midi/smf_stream.cpp",
    """            source_ = nullptr;
            smfTrackInspectorState().reset(0);
            return false;
""",
    """            source_ = nullptr;
            smfTrackInspectorState().reset(0);
            smfStructuralInspectorState().reset(0, 0);
            captureStructuralAnalysis_ = false;
            return false;
""",
)
replace_once(
    "src/midi/smf_stream.cpp",
    """        const int selected = selectedTrack();
        if (selected < 0) return false;
        const std::size_t track = static_cast<std::size_t>(selected);
        out = next_[track];
        prime(track);

        smfTrackInspectorState().observe(out.trackIndex, out.event);
        const bool noteOn = out.event.kind == SmfEventKind::NoteOn;
""",
    """        const int selected = selectedTrack();
        if (selected < 0) {
            if (captureStructuralAnalysis_) {
                smfStructuralInspectorState().finalize();
                captureStructuralAnalysis_ = false;
            }
            return false;
        }
        const std::size_t track = static_cast<std::size_t>(selected);
        out = next_[track];
        prime(track);

        smfTrackInspectorState().observe(out.trackIndex, out.event);
        if (captureStructuralAnalysis_) {
            smfStructuralInspectorState().observe(out.trackIndex, out.event);
        }
        const bool noteOn = out.event.kind == SmfEventKind::NoteOn;
""",
)

replace_once(
    "src/ui/pages/smf_player_page.h",
    """    void drawMuteMixer(IGfx& gfx);
    void drawPerformance(IGfx& gfx);
""",
    """    void drawMuteMixer(IGfx& gfx);
    void drawStructuralInspector(IGfx& gfx);
    void drawPerformance(IGfx& gfx);
""",
)
replace_once(
    "src/ui/pages/smf_player_page.h",
    """    bool channelInspectorVisible_{false};
    bool muteMixerVisible_{false};
    int channelInspectorScroll_{0};
""",
    """    bool channelInspectorVisible_{false};
    bool muteMixerVisible_{false};
    bool structuralInspectorVisible_{false};
    int channelInspectorScroll_{0};
""",
)

replace_once(
    "src/ui/pages/smf_player_page.cpp",
    '#include "src/midi/transport_clock_runtime.h"\n#include "src/midi/smf_track_inspector.h"\n',
    '#include "src/midi/transport_clock_runtime.h"\n#include "src/midi/smf_structural_inspector.h"\n#include "src/midi/smf_track_inspector.h"\n',
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """void formatTrackProgram(const GroovePuterMidi::SmfTrackInfoSnapshot& info,
                        char* output,
                        std::size_t outputSize) {
""",
    """const char* structuralResembles(
        const GroovePuterMidi::SmfStructuralLayerSnapshot& layer) {
    if (layer.swingPercent >= 56u && layer.notesPerBarX10 < 80u) {
        return "LO-FI / BROKEN";
    }
    if (layer.swingPercent >= 56u) return "BROKEN";
    if (layer.gridDenominator >= 16u && layer.notesPerBarX10 >= 80u) {
        return "TECHNO";
    }
    if (layer.activePermille >= 750u &&
        layer.motion == GroovePuterMidi::SmfStructuralMotion::Low) {
        return "AMBIENT";
    }
    return "STRAIGHT / HYBRID";
}

void formatTrackProgram(const GroovePuterMidi::SmfTrackInfoSnapshot& info,
                        char* output,
                        std::size_t outputSize) {
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """    browserVisible_ = false;
    muteMixerVisible_ = false;
    const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
""",
    """    browserVisible_ = false;
    muteMixerVisible_ = false;
    structuralInspectorVisible_ = false;
    const TransportClockRuntimeSnapshot clock = transportClockRuntime().snapshot();
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """        return true;
    }

    if (event.key == 'u' || event.key == 'U') {
        muteMixerVisible_ = !muteMixerVisible_;
""",
    """        return true;
    }

    if (event.key == 's' || event.key == 'S') {
        structuralInspectorVisible_ = !structuralInspectorVisible_;
        if (structuralInspectorVisible_) {
            muteMixerVisible_ = false;
            performanceVisible_ = false;
            channelInspectorVisible_ = false;
            selectAudibleTrackRelative(smfTrackInspectorState().snapshot(), 0);
        }
        return true;
    }

    if (structuralInspectorVisible_) {
        if (event.key == 'b' || event.key == 'B' || event.key == '\b') {
            structuralInspectorVisible_ = false;
            return true;
        }
        if (event.key == 'u' || event.key == 'U') {
            structuralInspectorVisible_ = false;
            muteMixerVisible_ = true;
            return true;
        }
        if (event.key == 'i' || event.key == 'I') {
            structuralInspectorVisible_ = false;
            channelInspectorVisible_ = true;
            channelInspectorScroll_ = 0;
            return true;
        }
        if (event.key == 'd' || event.key == 'D') {
            structuralInspectorVisible_ = false;
            performanceVisible_ = true;
            return true;
        }
        if (event.scancode == GROOVEPUTER_UP ||
            event.scancode == GROOVEPUTER_DOWN) {
            const int delta = event.scancode == GROOVEPUTER_UP ? -1 : 1;
            selectAudibleTrackRelative(smfTrackInspectorState().snapshot(), delta);
            return true;
        }
        if (event.scancode == GROOVEPUTER_LEFT ||
            event.scancode == GROOVEPUTER_RIGHT) {
            return true;
        }
        return true;
    }

    if (event.key == 'u' || event.key == 'U') {
        muteMixerVisible_ = !muteMixerVisible_;
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """        if (muteMixerVisible_) {
            performanceVisible_ = false;
            channelInspectorVisible_ = false;
            selectAudibleTrackRelative(smfTrackInspectorState().snapshot(), 0);
""",
    """        if (muteMixerVisible_) {
            performanceVisible_ = false;
            channelInspectorVisible_ = false;
            structuralInspectorVisible_ = false;
            selectAudibleTrackRelative(smfTrackInspectorState().snapshot(), 0);
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """        if (channelInspectorVisible_) {
            performanceVisible_ = false;
            muteMixerVisible_ = false;
            channelInspectorScroll_ = 0;
""",
    """        if (channelInspectorVisible_) {
            performanceVisible_ = false;
            muteMixerVisible_ = false;
            structuralInspectorVisible_ = false;
            channelInspectorScroll_ = 0;
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """        if (performanceVisible_) {
            channelInspectorVisible_ = false;
            muteMixerVisible_ = false;
        }
""",
    """        if (performanceVisible_) {
            channelInspectorVisible_ = false;
            muteMixerVisible_ = false;
            structuralInspectorVisible_ = false;
        }
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """        browserVisible_ = true;
        channelInspectorVisible_ = false;
        muteMixerVisible_ = false;
        GroovePuterUi::midiFileManager().open();
""",
    """        browserVisible_ = true;
        channelInspectorVisible_ = false;
        muteMixerVisible_ = false;
        structuralInspectorVisible_ = false;
        GroovePuterUi::midiFileManager().open();
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """    UI::drawStandardHeader(gfx, miniAcid_, muteMixerVisible_
        ? "MIDI MUTES"
        : (channelInspectorVisible_
            ? "MIDI CHANNELS"
            : (performanceVisible_ ? "MIDI PERF" : "MIDI PLAYER")));
""",
    """    UI::drawStandardHeader(gfx, miniAcid_, muteMixerVisible_
        ? "MIDI MUTES"
        : (structuralInspectorVisible_
            ? "MIDI STRUCTURE"
            : (channelInspectorVisible_
                ? "MIDI CHANNELS"
                : (performanceVisible_ ? "MIDI PERF" : "MIDI PLAYER"))));
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """    if (browserVisible_) drawBrowser(gfx);
    else if (muteMixerVisible_) drawMuteMixer(gfx);
    else if (channelInspectorVisible_) drawChannelInspector(gfx);
""",
    """    if (browserVisible_) drawBrowser(gfx);
    else if (muteMixerVisible_) drawMuteMixer(gfx);
    else if (structuralInspectorVisible_) drawStructuralInspector(gfx);
    else if (channelInspectorVisible_) drawChannelInspector(gfx);
""",
)

structural_draw = '''void SmfPlayerPage::drawStructuralInspector(IGfx& gfx) {
    const SmfStructuralInspectorSnapshot structure =
        smfStructuralInspectorState().snapshot();
    const SmfTrackMuteSnapshot mute = smfTrackMuteState().snapshot();
    if (structure.layerCount == 0u) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), "NO STRUCTURAL DATA");
        gfx.drawText(Layout::COL_1, LayoutManager::lineY(3), "WAIT FOR MIDI LOAD PASS");
        return;
    }

    uint8_t selected = 0;
    for (uint8_t i = 0; i < structure.layerCount; ++i) {
        if (structure.layers[i].trackIndex == mute.selectedTrack) {
            selected = i;
            break;
        }
    }
    const SmfStructuralLayerSnapshot& layer = structure.layers[selected];
    char line[64];
    char low[5] = "--";
    char high[5] = "--";
    if (layer.hasNotes()) {
        formatMidiNote(layer.minNote, low, sizeof(low));
        formatMidiNote(layer.maxNote, high, sizeof(high));
    }

    gfx.setTextColor(MusicVisuals::accentForStyle());
    std::snprintf(line, sizeof(line), "L%u TRK %02u · %s%s",
                  static_cast<unsigned>(selected + 1u),
                  static_cast<unsigned>(layer.trackIndex + 1u),
                  smfStructuralRoleName(layer.role),
                  structure.partial ? " · PARTIAL 64" : "");
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(0), line);

    gfx.setTextColor(COLOR_TEXT);
    if (layer.gridDenominator == 0u) {
        std::snprintf(line, sizeof(line), "GRID FREE      LOOP %u BAR",
                      static_cast<unsigned>(layer.loopBars));
    } else {
        std::snprintf(line, sizeof(line), "GRID 1/%u      LOOP %u BAR",
                      static_cast<unsigned>(layer.gridDenominator),
                      static_cast<unsigned>(layer.loopBars));
    }
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(1), line);

    std::snprintf(line, sizeof(line), "SWING %u%%      MOTION %s",
                  static_cast<unsigned>(layer.swingPercent),
                  smfStructuralMotionName(layer.motion));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(2), line);

    std::snprintf(line, sizeof(line), "NOTES/B %u.%u   ACTIVE %u%%",
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

    const int graphX = Layout::COL_1 + 68;
    const int graphY = LayoutManager::lineY(5) + 8;
    for (uint8_t bin = 0; bin < 4u; ++bin) {
        const int height = static_cast<int>(layer.form[bin]);
        gfx.fillRect(graphX + bin * 13, graphY - height, 8, height + 1,
                     MusicVisuals::secondaryForStyle());
    }

    std::snprintf(line, sizeof(line), "OVERLAP CHORDS %u%%  LEAD %u%%",
                  static_cast<unsigned>(layer.overlapChordsPercent),
                  static_cast<unsigned>(layer.overlapLeadPercent));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(6), line);

    gfx.setTextColor(COLOR_LABEL);
    std::snprintf(line, sizeof(line), "RESEMBLES %s",
                  structuralResembles(layer));
    gfx.drawText(Layout::COL_1, LayoutManager::lineY(7), line);
}

'''
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    "void SmfPlayerPage::drawChannelInspector(IGfx& gfx) {\n",
    structural_draw + "void SmfPlayerPage::drawChannelInspector(IGfx& gfx) {\n",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    """    } else if (channelInspectorVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Scroll I Player",
                               "U Mutes D Perf B Files");
""",
    """    } else if (structuralInspectorVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Layer S Player",
                               "1-9 Mute U Table I Channels");
    } else if (channelInspectorVisible_) {
        UI::drawStandardFooter(gfx, "UP/DN Scroll I Player",
                               "U Mutes D Perf B Files");
""",
)
replace_once(
    "src/ui/pages/smf_player_page.cpp",
    '"1-9 SMF Mute U Table I Info");\n',
    '"1-9 Mute U Table S Structure");\n',
)

run_tests = ROOT / "tests/run_host_tests.sh"
text = run_tests.read_text()
source_anchor = 'python3 "${ROOT_DIR}/tests/test_smf_midi_wave_source_regressions.py"\n'
if source_anchor not in text:
    raise SystemExit("tests/run_host_tests.sh: source anchor missing")
text = text.replace(
    source_anchor,
    source_anchor + 'python3 "${ROOT_DIR}/tests/test_smf_structural_inspector_source_regressions.py"\n',
    1,
)
compile_anchor = '"${BUILD_DIR}/test_smf_stream"\n'
if compile_anchor not in text:
    raise SystemExit("tests/run_host_tests.sh: compile anchor missing")
compile_block = r'''

"${CXX}" \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I"${ROOT_DIR}" \
  "${ROOT_DIR}/tests/test_smf_structural_inspector.cpp" \
  -o "${BUILD_DIR}/test_smf_structural_inspector"

"${BUILD_DIR}/test_smf_structural_inspector"
'''
text = text.replace(compile_anchor, compile_anchor + compile_block, 1)
run_tests.write_text(text)
