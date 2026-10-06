#include "play_rec_overlay.h"

#include <cstdio>
#include "src/dsp/miniacid_engine.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/transport_clock_runtime.h"
#include "ui_input.h"
#include "ui_theme.h"

bool PlayRecOverlay::handleEvent(const UIEvent& event) {
    if (!visible_) return false;
    if (event.event_type != GROOVEPUTER_KEY_DOWN) return true;
    if (event.scancode == GROOVEPUTER_ESCAPE || UIInput::isBack(event) ||
        (event.alt && !event.ctrl && !event.meta &&
         (event.key == 't' || event.key == 'T'))) {
        close();
        return true;
    }
    if (event.alt || event.ctrl || event.meta) return true;
    const int nav = UIInput::navCode(event);
    if (nav == GROOVEPUTER_LEFT) midiSelected_ = false;
    else if (nav == GROOVEPUTER_RIGHT) midiSelected_ = true;
    else if (event.key == ' ' || UIInput::isConfirm(event))
        action_ = midiSelected_ ? Action::MidiTransport : Action::GrooveTransport;
    else if (event.key == 'y' || event.key == 'Y') action_ = Action::Sync;
    else if (event.key == 'p' || event.key == 'P') action_ = Action::Player;
    return true;
}

void PlayRecOverlay::draw(IGfx& gfx, const MiniAcid& engine) const {
    if (!visible_) return;
    using namespace GroovePuterMidi;
    const auto p = UI::themePalette();
    const auto clock = transportClockRuntime().snapshot();
    const bool seq = clock.source == TransportClockSource::SeqtrakExternal;
    auto* player = smfPlayerService();
    const auto midi = player ? player->snapshot() : SmfPlayerSnapshot{};
    const int w = gfx.width();
    gfx.setFont(GfxFont::kFont5x7);
    gfx.fillRect(0, 0, w, gfx.height(), p.background);
    gfx.fillRect(0, 0, w, 17, p.panel);
    gfx.setTextColor(p.text);
    gfx.drawText(6, 5, "PLAY / REC");
    gfx.setTextColor(p.accent);
    gfx.drawText(w - 42, 5, "GLOBAL");
    gfx.setTextColor(p.secondary);
    gfx.drawText(6, 21, "CHOOSE WHAT SPACE CONTROLS");
    const int cardW = (w - 18) / 2;
    for (int i = 0; i < 2; ++i) {
        const bool selected = (i == 1) == midiSelected_;
        const int x = 6 + i * (cardW + 6);
        gfx.fillRect(x, 34, cardW, 23, selected ? p.accent : p.panel);
        if (selected) gfx.drawRect(x - 1, 33, cardW + 2, 25, p.focus);
        gfx.setTextColor(selected ? p.invert : p.secondary);
        const char* label = i == 0 ? "GROOVE" : "MIDI FILE";
        gfx.drawText(x + (cardW - gfx.textWidth(label)) / 2, 42, label);
    }

    const char* state = engine.isPlaying() ? "PLAYING" : "STOPPED";
    const char* action = seq ? (clock.externalFollowEnabled ? "SPACE: FOLLOW OFF" : "SPACE: FOLLOW ON")
                             : (engine.isPlaying() ? "SPACE: STOP" : "SPACE: PLAY");
    char source[40];
    if (midiSelected_) {
        switch (midi.state) {
            case SmfPlayerState::Unloaded: state = "NO MIDI FILE"; break;
            case SmfPlayerState::Loading: state = "LOADING"; break;
            case SmfPlayerState::Stopped: state = "STOPPED"; break;
            case SmfPlayerState::Armed: state = "ARMED"; break;
            case SmfPlayerState::Playing: state = "PLAYING"; break;
            case SmfPlayerState::Paused: state = "PAUSED"; break;
            case SmfPlayerState::Error: state = "FILE ERROR"; break;
        }
        if (!player || midi.state == SmfPlayerState::Unloaded || midi.state == SmfPlayerState::Error)
            action = "P: OPEN PLAYER";
        else if (midi.state == SmfPlayerState::Loading) action = "WAIT FOR FILE";
        else if (midi.state == SmfPlayerState::Playing || midi.state == SmfPlayerState::Armed)
            action = "SPACE: PAUSE";
        else if (midi.tempoMode == SmfTempoMode::Project && seq)
            action = clock.externalFollowEnabled ? "SPACE: ARM / SEQ PLAY" : "Y: ENABLE SEQ FOLLOW";
        else if (midi.tempoMode == SmfTempoMode::Project && !engine.isPlaying())
            action = "START GROOVE FIRST";
        else action = midi.tempoMode == SmfTempoMode::Project ? "SPACE: ARM NEXT BAR" : "SPACE: PLAY";
        std::snprintf(source, sizeof(source), "TEMPO: %s",
            midi.tempoMode == SmfTempoMode::Original ? "MIDI FILE" : (seq ? "SEQTRAK" : "PROJECT"));
    } else {
        std::snprintf(source, sizeof(source), "%s  A:%s B:%s + DRUMS",
            engine.songModeEnabled() ? "SONG" : "CYCLE",
            engine.currentSequencedSource(0) == MiniAcid::SequencedSource::Phrase ? "MEL" : "PAT",
            engine.currentSequencedSource(1) == MiniAcid::SequencedSource::Phrase ? "MEL" : "PAT");
        if (seq && !clock.externalFollowEnabled) state = "FOLLOW OFF";
        else if (seq && !engine.isPlaying()) state = "WAITING FOR SEQ";
    }
    gfx.setTextColor(p.text);
    gfx.drawText(6, 64, state);
    gfx.setTextColor(p.accent2);
    gfx.drawText(w - 6 - gfx.textWidth(action), 77, action);
    gfx.setTextColor(p.secondary);
    gfx.drawText(6, 89, source);
    gfx.drawLine(6, 101, w - 7, 101, p.dim);
    gfx.setTextColor(p.warning);
    gfx.drawText(6, 105, "REC: MANUAL ON SEQTRAK");
    gfx.setTextColor(p.secondary);
    gfx.drawText(6, 115, "TRACK / LENGTH: SET ON SEQTRAK");
    const int footer = gfx.height() - 10;
    gfx.fillRect(0, footer, w, 10, p.panel);
    gfx.setTextColor(p.text);
    gfx.drawText(6, footer + 1, "L/R TARGET  Y SYNC  P PLAYER  ESC");
    gfx.setTextColor(COLOR_TEXT);
}
