#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_blocked_note_mode_keys_are_consumed() -> None:
    keyboard = (ROOT / "src/input/performance_keyboard.cpp").read_text(
        encoding="utf-8"
    )
    start = keyboard.index("bool PerformanceKeyboard::keyDown")
    end = keyboard.index("bool PerformanceKeyboard::keyUp", start)
    block = keyboard[start:end]

    layout_pos = block.index("if (!isPerformanceKey(physicalKey)) return false;")
    note_mode_pos = block.index("if (!noteModeEnabled_) return false;")
    blocked_pos = block.index("if (!enabled_ || transportPlaying_) return true;")
    note_pos = block.index("noteForKey", blocked_pos)

    require(layout_pos < note_mode_pos < blocked_pos < note_pos,
            "layout membership must be decided before transport blocks NoteOn")
    require("return true;" in block[blocked_pos:note_pos],
            "transport-blocked performance keys must remain consumed")
    require('constexpr char kLowerRow[] = "asdfghjkl";' in keyboard,
            "lower performance row must retain K/L collision keys")
    require('constexpr char kUpperRow[] = "qwertyuiop";' in keyboard,
            "upper performance row must retain I/O/P collision keys")

    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    route_pos = display.index("performance_keyboard_.keyDown(event.key)")
    fallback_pos = display.index(
        "if (event.key == ']') { nextPage(); return true; }", route_pos
    )
    require(route_pos < fallback_pos,
            "NOTE-mode routing must run before legacy global fallback")


def test_performance_all_notes_off_is_target_scoped() -> None:
    sink = (ROOT / "src/input/internal_synth_output.cpp").read_text(
        encoding="utf-8"
    )
    start = sink.index("case MusicalEventType::AllNotesOff")
    block = sink[start:]

    require("engine_.liveNote(voice)" in block,
            "AllNotesOff must inspect only the event target voice")
    require("engine_.liveNoteOff(voice" in block,
            "AllNotesOff must release only the live-owned target voice")
    require("engine_.allLiveNotesOff()" not in block,
            "performance AllNotesOff must not become a global voice release")


def test_note_mode_is_explicit_and_runtime_only() -> None:
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/input/performance_keyboard.h").read_text(
        encoding="utf-8"
    )
    scenes = (ROOT / "scenes.h").read_text(encoding="utf-8")
    storage = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("keyboard_.toggleNoteMode();" in page,
            "PERFORM must expose an explicit NOTE-mode toggle")
    require("NOTE MODE: %s" in page,
            "PERFORM must display NOTE-mode state")
    require("bool noteModeEnabled_{true};" in header,
            "PERFORM must remain immediately playable by default")
    require("noteModeEnabled" not in scenes and "noteModeEnabled" not in storage,
            "NOTE mode must remain runtime-only in this PR")


def test_live_synth_render_is_not_transport_gated() -> None:
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    start = engine.index("uint32_t tV0 = 0;")
    end = engine.index("uint32_t tD0 = 0;", start)
    voice_block = engine[start:end]

    require("if (playing)" not in voice_block,
            "live synth rendering must work while transport is stopped")
    require("synthVoices_[0]->process()" in voice_block,
            "Synth A must be rendered in the live-audio path")

    drum_end = engine.index("if (detailedProfile) tDrumsTotal", end)
    drum_block = engine[end:drum_end]
    require("if (playing)" in drum_block,
            "drums must remain transport-gated")
    require("sample += sample303;" in engine[drum_end - 120:drum_end + 80],
            "live synth mix must be added outside the drum transport gate")


def test_perform_is_additive_to_legacy_carousel() -> None:
    display_header = (ROOT / "src/ui/miniacid_display.h").read_text(encoding="utf-8")
    perform = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")

    require("int page_index_ = 0;" in display_header,
            "GroovePuter must boot into the original groovebox page")
    require("int previous_page_index_ = 0;" in display_header,
            "legacy Back/page state must start from page zero")
    require("case '[':" not in perform and "case ']':" not in perform,
            "PERFORM must not steal legacy [ ] page navigation")
    require("case ',':" in perform and "case '.':" in perform,
            "PERFORM scale controls must use non-navigation keys")


def test_smf_player_is_additive_and_keeps_single_usb_owner() -> None:
    ui_config = (ROOT / "src/ui/ui_config.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    player_page = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(
        encoding="utf-8"
    )
    player_service = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8"
    )
    player_registry = (ROOT / "src/platform/cardputer_smf_player_registry.cpp").read_text(
        encoding="utf-8"
    )
    usb_dispatch = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(
        encoding="utf-8"
    )
    project = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

    require("kPageCount = 14" in ui_config,
            "MIDI Player must remain a real lazy-loaded UI page")
    require("case kSmfPlayerPage:" in display and
            "std::make_unique<SmfPlayerPage>" in display,
            "MiniAcidDisplay must construct the MIDI Player page")
    require("event.alt && (event.key == 'p' || event.key == 'P')" in display,
            "Alt+P must provide a deterministic hardware shortcut to MIDI Player")
    require("player->togglePlayPause()" in display,
            "Space must own SMF Play/Pause on the player page")
    require("event.shift" not in display[display.index("// On the MIDI Player page Space"):display.index("if (event.key == ' ')", display.index("// On the MIDI Player page Space") + 1)],
            "SMF Space handling must not depend on unavailable Cardputer Shift")

    require('"PLAY FROM %.30s"' in player_page and
            "currentPath_.c_str()" in player_page and
            "requestLoadAndPlay" in player_page,
            "MIDI Player page must expose selectable SD playback and its path")
    require("seekBars(event.shift ? -4 : -1)" in player_page and
            "seekBars(event.shift ? 4 : 1)" in player_page,
            "player seek must preserve +/-1 and Shift +/-4 bar UX")
    require("MIDI PANIC / PAUSE" in player_page,
            "player page must expose scoped panic")
    require("event.key == 'r' || event.key == 'R'" in player_page and
            "player_->restart(SmfPlayerRestartOrigin::MusicStart)" in player_page and
            "R Restart" in player_page,
            "physical R must restart playback from MUSIC START without Shift")
    require("event.key == 'd' || event.key == 'D'" in player_page and
            "drawPerformance" in player_page and
            "state.performance" in player_page and
            "snapshot_.performance = performance" in player_service,
            "physical D must expose SMF performance without Serial or SD logging")
    require("B Files" in player_page and "toggleRouting()" in player_page,
            "player must expose file return and RAW/SEQTRAK routing controls")

    require("MidiImporter importer" in project and "importFile" in project,
            "existing quantized MIDI importer must remain available beside PLAY")

    forbidden_usb = ("USBMIDI", "TinyUSB", "writePacket(", "sendNoteOn(", "sendNoteOff(")
    for token in forbidden_usb:
        require(token not in player_service,
                f"SmfPlayerTask must not own USB/TinyUSB directly: found {token}")
        require(token not in player_registry,
                f"SMF service registry must stay free of USB writes: found {token}")

    require("registerCardputerSmfMidiQueue" in player_registry,
            "Cardputer SMF service must publish only through the scheduled queue")
    require("beginCardputerSmfPlayerService" in player_registry and
            "beginCardputerSmfPlayerService" in
                (ROOT / "GroovePuter.ino").read_text(encoding="utf-8"),
            "SMF runtime must be reserved explicitly during setup")
    require("kMaxTimingEvents = 32" in
                (ROOT / "src/platform/cardputer_smf_player.h").read_text(encoding="utf-8") and
            "timingDocument_ = SmfDocument{}" not in player_service,
            "Cardputer SMF metadata must stay bounded and preserve setup capacity")
    require("catch (const std::bad_alloc&)" in player_service,
            "SMF loading must report allocation failure instead of resetting")
    require("ScheduledSmfMidiEventQueue* g_smfQueue" in usb_dispatch,
            "existing MidiDispatchTask must consume the SMF scheduled queue")
    require("smfSendFailureAction" in usb_dispatch and
            "vTaskDelay(kSmfRetryDelay)" in usb_dispatch and
            "kSmfStaleNoteOnThresholdUs" in usb_dispatch,
            "USB backpressure must use bounded paced retries and stale NoteOn drops")
    require("g_output.handleSmfNoteOn" in usb_dispatch and
            "g_output.handleSmfNoteOff" in usb_dispatch,
            "MidiDispatchTask must remain the physical SMF note owner")
    require("midiDispatchTask" in usb_dispatch,
            "accepted single USB owner task must remain present")


def main() -> None:
    test_blocked_note_mode_keys_are_consumed()
    test_performance_all_notes_off_is_target_scoped()
    test_note_mode_is_explicit_and_runtime_only()
    test_live_synth_render_is_not_transport_gated()
    test_perform_is_additive_to_legacy_carousel()
    test_smf_player_is_additive_and_keeps_single_usb_owner()
    print("performance + SMF source regressions: OK")


if __name__ == "__main__":
    main()
