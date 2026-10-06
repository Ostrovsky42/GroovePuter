#include "global_midi_sync_overlay.h"

#include <cstdio>
#include <cstring>

#include "src/dsp/miniacid_engine.h"
#include "src/midi/transport_clock_runtime.h"
#include "ui_input.h"
#include "ui_theme.h"
#include "fonts/Adafruit5x7.h"

namespace {
bool followsMidiClock(const GroovePuterMidi::TransportClockRuntime& runtime) {
    return runtime.source() == GroovePuterMidi::TransportClockSource::SeqtrakExternal &&
           runtime.externalFollowEnabled();
}
}

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
        selectedRow_ = nav == GROOVEPUTER_UP ? 0 : 1;
        return true;
    }

    auto& runtime = GroovePuterMidi::transportClockRuntime();
    // Row 0: BPM. Read-only while the tempo comes from incoming MIDI Clock.
    if (selectedRow_ == 0 && !followsMidiClock(runtime) &&
        (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT)) {
        tempoDelta_ = (nav == GROOVEPUTER_RIGHT ? 1 : -1) * (event.shift ? 5 : 1);
        return true;
    }

    // Row 1: clock source. Choosing MIDI IN always means following it; the
    // legacy "external but follow off" state is only shown, never created here.
    if (selectedRow_ == 1 &&
        (nav == GROOVEPUTER_LEFT || nav == GROOVEPUTER_RIGHT || confirm)) {
        const bool midiIn = confirm ? !followsMidiClock(runtime)
                                    : nav == GROOVEPUTER_RIGHT;
        if (midiIn) runtime.setExternalFollowEnabled(true);
        runtime.setSource(midiIn
            ? GroovePuterMidi::TransportClockSource::SeqtrakExternal
            : GroovePuterMidi::TransportClockSource::GroovePuterInternal);
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

    using GroovePuterMidi::ExternalClockLockState;
    const UI::ThemePalette p = UI::themePalette();
    const auto clock = GroovePuterMidi::transportClockRuntime().snapshot();
    const bool midiIn = clock.source ==
        GroovePuterMidi::TransportClockSource::SeqtrakExternal;
    const bool following = midiIn && clock.externalFollowEnabled;
    const bool held = clock.externalState == ExternalClockLockState::Hold ||
                      clock.externalState == ExternalClockLockState::Lost;
    const int w = gfx.width();
    const int footerY = gfx.height() - 12;

    gfx.setFont(GfxFont::kFont5x7);
    gfx.fillRect(0, 0, w, gfx.height(), p.background);
    gfx.fillRect(0, 0, w, 17, p.panel);
    gfx.setTextColor(p.text);
    gfx.drawText(6, 5, "TEMPO");
    const char* transport = miniAcid.isPlaying() ? "PLAYING" : "STOPPED";
    gfx.setTextColor(miniAcid.isPlaying() ? p.active : p.secondary);
    gfx.drawText(w - 6 - gfx.textWidth(transport), 5, transport);

    // BPM: the main control, focused on open.
    char tempo[12];
    if (following && !clock.externalTempoValid)
        std::snprintf(tempo, sizeof(tempo), "--.-");
    else
        std::snprintf(tempo, sizeof(tempo), "%.1f", following
            ? clock.externalBpm() : static_cast<double>(miniAcid.projectBpm()));
    if (selectedRow_ == 0) gfx.drawRect(6, 23, w - 12, 40, p.focus);
    drawTempo(gfx, 14, 29, tempo, following ? p.secondary : p.text);
    gfx.setTextColor(p.secondary);
    gfx.drawText(14 + static_cast<int>(std::strlen(tempo)) * 12 + 4, 36,
                 following ? (held && clock.externalTempoValid ? "LAST BPM" : "BPM IN")
                           : "BPM");
    gfx.drawText(14, 50, following ? "SET BY MIDI CLOCK"
                                   : "L/R 1  SHIFT+L/R 5");

    // Clock source.
    if (selectedRow_ == 1) gfx.drawRect(6, 68, w - 12, 15, p.focus);
    gfx.setTextColor(p.text);
    gfx.drawText(14, 72, "CLOCK");
    const char* source = midiIn ? "< MIDI IN" : "INTERNAL >";
    gfx.setTextColor(midiIn ? p.accent : p.text);
    gfx.drawText(w - 14 - gfx.textWidth(source), 72, source);

    if (midiIn) {
        const char* status = "FOLLOW OFF";
        const char* detail = "USING PROJECT BPM";
        IGfxColor color = p.secondary;
        if (following) {
            switch (clock.externalState) {
                case ExternalClockLockState::Waiting:
                    status = "WAITING"; detail = ""; break;
                case ExternalClockLockState::Locking:
                    status = "SYNCING"; detail = ""; color = p.warning; break;
                case ExternalClockLockState::Locked:
                    status = "IN SYNC"; detail = ""; color = p.active; break;
                case ExternalClockLockState::Hold:
                    status = "CLOCK HOLD"; detail = ""; color = p.warning; break;
                case ExternalClockLockState::Lost:
                    status = "CLOCK LOST"; detail = "CHECK USB MIDI"; color = p.danger; break;
            }
        }
        gfx.setTextColor(color);
        gfx.drawText(14, 92, status);
        gfx.setTextColor(p.secondary);
        if (*detail) gfx.drawText(14 + gfx.textWidth(status) + 12, 92, detail);
        gfx.drawText(14, 104, "PLAY / STOP FROM THE OTHER DEVICE");
    } else {
        gfx.setTextColor(p.secondary);
        gfx.drawText(14, 92, "MIDI IN: FOLLOW A DAW / DEVICE");
    }

    gfx.fillRect(0, footerY, w, 12, p.panel);
    gfx.setTextColor(p.text);
    gfx.drawText(6, footerY + 3, "UP/DN SELECT  L/R CHANGE  ESC");
    gfx.setTextColor(COLOR_TEXT);
}
