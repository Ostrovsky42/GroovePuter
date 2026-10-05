#include "global_midi_sync_overlay.h"

#include <cstdio>

#include "src/dsp/miniacid_engine.h"
#include "src/midi/transport_clock_runtime.h"
#include "ui_input.h"
#include "ui_theme.h"
#include "fonts/Adafruit5x7.h"

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
        auto& runtime = GroovePuterMidi::transportClockRuntime();
        if (confirm) runtime.toggleExternalFollowEnabled();
        else runtime.setExternalFollowEnabled(nav == GROOVEPUTER_RIGHT);
        return true;
    }

    return true;
}

namespace {
// The same bitmap as the standard UI font, doubled only for the tempo readout.
// Keep the rest of the panel on the instrument's normal 6x8 text grid.
void drawTempo(IGfx& gfx, int x, int y, const char* text, IGfxColor color) {
    for (; *text; ++text, x += 12) {
        const unsigned char c = static_cast<unsigned char>(*text);
        if (c < 32 || c > 127) continue;
        const auto& glyph = adafruit_5x7::kFont5x7[c - 32];
        for (int col = 0; col < 5; ++col) {
            for (int row = 0; row < 7; ++row) {
                if (glyph[col] & (1u << row))
                    gfx.fillRect(x + col * 2, y + row * 2, 2, 2, color);
            }
        }
    }
}
}

void GlobalMidiSyncOverlay::draw(IGfx& gfx, const MiniAcid& miniAcid) const {
    if (!visible_) return;

    const UI::ThemePalette p = UI::themePalette();
    const auto clock = GroovePuterMidi::transportClockRuntime().snapshot();
    const bool seqMaster = clock.source ==
        GroovePuterMidi::TransportClockSource::SeqtrakExternal;
    const bool following = seqMaster && clock.externalFollowEnabled;
    const bool followFocus = selectedRow_ == 1 && seqMaster;
    const int w = gfx.width();
    const int footerY = gfx.height() - 12;

    gfx.setFont(GfxFont::kFont5x7);
    gfx.fillRect(0, 0, w, gfx.height(), p.background);
    gfx.fillRect(0, 0, w, 17, p.panel);
    gfx.setTextColor(p.text);
    gfx.drawText(6, 5, "TEMPO / SYNC");
    gfx.setTextColor(p.accent);
    gfx.drawText(w - 42, 5, "GLOBAL");
    gfx.setTextColor(p.secondary);
    gfx.drawText(6, 21, "MELODY / PATTERN / DRUMS");

    const int cardW = (w - 18) / 2;
    for (int i = 0; i < 2; ++i) {
        const bool active = (i == 1) == seqMaster;
        const int x = 6 + i * (cardW + 6);
        gfx.fillRect(x, 34, cardW, 23, active ? p.accent : p.panel);
        if (!followFocus && active)
            gfx.drawRect(x - 1, 33, cardW + 2, 25, p.focus);
        gfx.setTextColor(active ? p.invert : p.secondary);
        const char* label = i == 0 ? "GROOVEPUTER" : "SEQTRAK";
        gfx.drawText(x + (cardW - gfx.textWidth(label)) / 2, 42, label);
    }

    gfx.fillRect(6, 62, w - 12, 15, p.panel);
    if (followFocus) gfx.drawRect(6, 62, w - 12, 15, p.focus);
    gfx.setTextColor(seqMaster ? p.text : p.secondary);
    gfx.drawText(12, 66, "FOLLOW SEQ CLOCK");
    const char* follow = seqMaster
        ? (clock.externalFollowEnabled ? "ON" : "OFF") : "--";
    gfx.setTextColor(following ? p.active : p.secondary);
    gfx.drawText(w - 12 - gfx.textWidth(follow), 66, follow);

    char tempo[12];
    if (seqMaster && (!following || !clock.externalTempoValid))
        std::snprintf(tempo, sizeof(tempo), "--.-");
    else
        std::snprintf(tempo, sizeof(tempo), "%.1f", seqMaster
            ? clock.externalBpm() : static_cast<double>(miniAcid.bpm()));
    drawTempo(gfx, 12, 84, tempo, p.text);
    gfx.setTextColor(p.secondary);
    gfx.drawText(12, 102, "BPM");

    const char* status = "CLOCK OUT";
    const char* detail = miniAcid.isPlaying() ? "PLAYING" : "STOPPED";
    IGfxColor statusColor = miniAcid.isPlaying() ? p.active : p.secondary;
    if (seqMaster) {
        detail = following ? (clock.externalRunning ? "PLAYING" : "STOPPED")
                           : "LOCAL TEMPO";
        statusColor = p.secondary;
        if (!following) status = "FOLLOW OFF";
        else {
            using GroovePuterMidi::ExternalClockLockState;
            switch (clock.externalState) {
                case ExternalClockLockState::Waiting:
                    status = "WAITING"; detail = "PRESS SEQ PLAY"; break;
                case ExternalClockLockState::Locking:
                    status = "SYNCING"; statusColor = p.warning; break;
                case ExternalClockLockState::Locked:
                    status = "IN SYNC"; statusColor = p.active; break;
                case ExternalClockLockState::Hold:
                    status = "CLOCK HOLD"; statusColor = p.warning; break;
                case ExternalClockLockState::Lost:
                    status = "CLOCK LOST"; statusColor = p.danger;
                    detail = "CHECK SEQTRAK"; break;
            }
        }
    }
    gfx.setTextColor(statusColor);
    gfx.drawText(126, 85, status);
    gfx.setTextColor(p.secondary);
    gfx.drawText(126, 99, detail);

    gfx.setTextColor(p.secondary);
    gfx.drawText(6, 113, seqMaster
        ? (following ? "PLAY / STOP FROM SEQTRAK" : "SEQ CLOCK FOLLOW IS DISABLED")
        : "STOP: SEQ USES ITS OWN BPM");
    gfx.fillRect(0, footerY, w, 12, p.panel);
    gfx.setTextColor(p.text);
    gfx.drawText(6, footerY + 3, followFocus
        ? "UP MASTER  L/R OFF/ON  ESC BACK"
        : (seqMaster ? "L/R MASTER  DOWN FOLLOW  ESC BACK"
                     : "L/R MASTER  ENTER CHANGE  ESC BACK"));
    gfx.setTextColor(COLOR_TEXT);
}
