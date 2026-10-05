#include "global_midi_sync_overlay.h"

#include <cstdio>

#include "src/dsp/miniacid_engine.h"
#include "src/midi/transport_clock_runtime.h"
#include "ui_input.h"
#include "ui_theme.h"

bool GlobalMidiSyncOverlay::handleEvent(UIEvent& event) {
    if (!visible_) return false;
    if (event.event_type != GROOVEPUTER_KEY_DOWN) return true;

    if (event.scancode == GROOVEPUTER_ESCAPE ||
        (event.alt && !event.ctrl && !event.meta &&
         (event.key == 'y' || event.key == 'Y'))) {
        close();
        return true;
    }

    if (event.alt || event.ctrl || event.meta) return true;

    const int nav = UIInput::navCode(event);
    const bool confirm = event.key == '\n' || event.key == '\r';
    if (nav == GROOVEPUTER_UP || nav == GROOVEPUTER_DOWN) {
        const auto source = GroovePuterMidi::transportClockRuntime().source();
        selectedRow_ = (nav == GROOVEPUTER_UP ||
                        source != GroovePuterMidi::TransportClockSource::SeqtrakExternal)
            ? 0
            : 1;
        return true;
    }

    if (selectedRow_ == 0 &&
        (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT || confirm)) {
        auto& runtime = GroovePuterMidi::transportClockRuntime();
        if (confirm) {
            runtime.toggleSource();
        } else {
            runtime.setSource(nav == GROOVEPUTER_LEFT
                ? GroovePuterMidi::TransportClockSource::GroovePuterInternal
                : GroovePuterMidi::TransportClockSource::SeqtrakExternal);
        }
        return true;
    }

    if (selectedRow_ == 1 &&
        GroovePuterMidi::transportClockRuntime().source() ==
            GroovePuterMidi::TransportClockSource::SeqtrakExternal &&
        (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT || confirm)) {
        GroovePuterMidi::transportClockRuntime().toggleExternalFollowEnabled();
        return true;
    }

    return true;
}

void GlobalMidiSyncOverlay::draw(IGfx& gfx, const MiniAcid& miniAcid) const {
    if (!visible_) return;

    const UI::ThemePalette p = UI::themePalette();
    const GroovePuterMidi::TransportClockRuntimeSnapshot clock =
        GroovePuterMidi::transportClockRuntime().snapshot();
    const bool seqMaster =
        clock.source == GroovePuterMidi::TransportClockSource::SeqtrakExternal;

    const int panelW = gfx.width() - 28;
    const int panelH = 153;
    const int x = (gfx.width() - panelW) / 2;
    const int y = (gfx.height() - panelH) / 2;

    gfx.fillRect(0, 0, gfx.width(), gfx.height(), p.background);
    gfx.fillRect(x, y, panelW, panelH, p.panel);
    gfx.drawRect(x, y, panelW, panelH, p.dim);
    gfx.setTextColor(p.accent);
    gfx.drawText(x + 8, y + 6, "TEMPO / MIDI SYNC");

    gfx.setTextColor(p.secondary);
    gfx.drawText(x + 8, y + 22, "APPLIES TO MELODY / PATTERN / DRUMS");

    const int choiceY = y + 38;
    if (selectedRow_ == 0) {
        gfx.fillRect(x + 5, choiceY - 2, panelW - 10, 15, p.inset);
    }
    gfx.setTextColor(seqMaster ? p.dim : p.accent2);
    gfx.drawText(x + 11, choiceY,
                 seqMaster ? "  GROOVEPUTER MASTER" : "> GROOVEPUTER MASTER");
    gfx.setTextColor(seqMaster ? p.accent2 : p.dim);
    gfx.drawText(x + panelW / 2 + 4, choiceY,
                 seqMaster ? "> SEQTRAK MASTER" : "  SEQTRAK MASTER");

    char line[56];
    const int followY = y + 55;
    if (selectedRow_ == 1) {
        gfx.fillRect(x + 5, followY - 2, panelW - 10, 15, p.inset);
    }
    if (seqMaster) {
        std::snprintf(line, sizeof(line), "FOLLOW SEQ CLOCK: %s",
                      clock.externalFollowEnabled ? "ON" : "OFF");
    } else {
        std::snprintf(line, sizeof(line), "FOLLOW SEQ CLOCK: SELECT SEQTRAK");
    }
    gfx.setTextColor(seqMaster ? p.text : p.dim);
    gfx.drawText(x + 11, followY,
                 selectedRow_ == 1 && seqMaster ? "> " : "  ");
    gfx.drawText(x + 23, followY, line);

    if (seqMaster) {
        if (!clock.externalFollowEnabled) {
            std::snprintf(line, sizeof(line), "CLOCK STATE: FOLLOW DISABLED");
        } else {
            std::snprintf(line, sizeof(line), "SEQ CLOCK: %s",
                          GroovePuterMidi::externalClockLockStateName(
                              clock.externalState));
        }
        gfx.setTextColor(clock.externalFollowEnabled ? p.text : p.dim);
        gfx.drawText(x + 8, y + 74, line);

        if (clock.externalFollowEnabled && clock.externalTempoValid) {
            std::snprintf(line, sizeof(line), "TEMPO: %5.1f BPM",
                          clock.externalBpm());
        } else {
            std::snprintf(line, sizeof(line), "TEMPO: --.- BPM");
        }
        gfx.setTextColor(p.text);
        gfx.drawText(x + 8, y + 88, line);
    } else {
        std::snprintf(line, sizeof(line), "PROJECT TEMPO: %5.1f BPM",
                      static_cast<double>(miniAcid.bpm()));
        gfx.setTextColor(p.text);
        gfx.drawText(x + 8, y + 74, line);
        gfx.setTextColor(miniAcid.isPlaying() ? p.accent2 : p.secondary);
        gfx.drawText(x + 8, y + 88,
                     miniAcid.isPlaying()
                         ? "CLOCK OUT: RUNNING WITH PLAY"
                         : "CLOCK OUT: OFF (PLAY STOPPED)");
    }

    gfx.setTextColor(p.dim);
    gfx.drawText(x + 8, y + 104, "MIDI FILE TEMPO: T IN PLAYER");
    gfx.drawText(x + 8, y + 118,
                 seqMaster
                     ? "PLAY / STOP FOLLOWS SEQTRAK"
                     : "STOP RETURNS SEQTRAK TO ITS BPM");
    const char* controls = !seqMaster
        ? "L/R OR ENTER: CHANGE MASTER  ESC CLOSE"
        : (selectedRow_ == 0
            ? "L/R MASTER  DOWN SELECTS FOLLOW  ESC"
            : "L/R OR ENTER: TOGGLE FOLLOW  ESC CLOSE");
    gfx.drawText(x + 8, y + 136, controls);
    gfx.setTextColor(COLOR_TEXT);
}
