#!/usr/bin/env python3
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = Path(__file__).with_name("_performance_source_regressions_base.py")

spec = importlib.util.spec_from_file_location(
    "performance_source_regressions_base", BASE
)
if spec is None or spec.loader is None:
    raise RuntimeError(f"cannot load regression base: {BASE}")
base = importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)


def test_transport_note_mode_keys_remain_live() -> None:
    keyboard = (ROOT / "src/input/performance_keyboard.cpp").read_text(
        encoding="utf-8"
    )
    header = (ROOT / "src/input/performance_keyboard.h").read_text(
        encoding="utf-8"
    )
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(
        encoding="utf-8"
    )

    start = keyboard.index("bool PerformanceKeyboard::keyDown")
    end = keyboard.index("bool PerformanceKeyboard::keyUp", start)
    block = keyboard[start:end]

    layout_pos = block.index("if (!isPerformanceKey(physicalKey)) return false;")
    note_mode_pos = block.index("if (!noteModeEnabled_) return false;")
    enabled_pos = block.index("if (!enabled_) return true;")
    note_pos = block.index("noteForKey", enabled_pos)

    base.require(layout_pos < note_mode_pos < enabled_pos < note_pos,
                 "layout and NOTE mode must be resolved before live NoteOn")
    base.require("transportPlaying_" not in block[note_mode_pos:note_pos],
                 "transport playback must not block Cardputer keyboard NoteOn")
    base.require("return enabled_ && noteModeEnabled_;" in header,
                 "live input ownership must no longer depend on transport state")
    base.require('constexpr char kLowerRow[] = "asdfghjkl";' in keyboard,
                 "lower performance row must retain K/L collision keys")
    base.require('constexpr char kUpperRow[] = "qwertyuiop";' in keyboard,
                 "upper performance row must retain I/O/P collision keys")

    transport_start = keyboard.index(
        "void PerformanceKeyboard::setTransportPlaying")
    transport_end = keyboard.index(
        "void PerformanceKeyboard::setTarget", transport_start)
    transport_block = keyboard[transport_start:transport_end]
    base.require("if (playing) panic();" not in transport_block,
                 "transport start must not panic held performance keys")
    base.require("stopGeneratedOutput();" in transport_block and
                 "resetStepClock();" in transport_block,
                 "clock-domain changes must clean generated notes before re-anchoring")

    service_start = keyboard.index("void PerformanceKeyboard::service(uint32_t")
    service_end = keyboard.index(
        "void PerformanceKeyboard::triggerDirectTransformed", service_start)
    service_block = keyboard[service_start:service_end]
    base.require("if (transportPlaying_)" in service_block and
                 "serviceTransportStepClock(nowMicros)" in service_block,
                 "running transport must select the project-timeline step clock")
    base.require("projectTransportTimeline().trySnapshot(snapshot)" in keyboard and
                 "snapshot.absoluteSteps()" in keyboard,
                 "step generation must read the coherent project transport phase")
    base.require("snapshotCardputerUsbMidiBlockAnchor" in keyboard and
                 "anchorPlaybackMicros" in keyboard,
                 "hardware timing must use the dispatcher playback anchor")
    base.require("const uint64_t nextOrdinal = currentOrdinal + 1u;" in keyboard and
                 "nextOrdinal % static_cast<uint64_t>(kEuclideanSteps)" in keyboard,
                 "Euclidean phase must derive from the next absolute sixteenth")
    base.require("kTransportScheduleLeadSteps = 0.5" in keyboard and
                 "transportAnchorBlockSequence_" in header and
                 "transportAnchorMicros_" in header,
                 "transport steps must be prepared ahead from a stable block anchor")
    base.require("kGeneratedNoteOnStaleMicros = 12000u" in keyboard and
                 "lateness > kGeneratedNoteOnStaleMicros" in keyboard and
                 "leadMicros < -static_cast<int32_t>(kGeneratedNoteOnStaleMicros)" in keyboard,
                 "late generated NoteOn must be shed instead of caught up")
    base.require("kMaxScheduledEvents = 112" in header,
                 "dense 8-note x4 ratchet scheduling needs overlap headroom")
    base.require("INPUT LOCK | PATTERN PLAYER ACTIVE" not in page and
                 'stepTools ? "LIVE SYNC" : (directPoly ? "POLY EXT" : "LIVE INPUT")' in page,
                 "PERFORM must show live transport input instead of the old lock")

    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    route_pos = display.index("performance_keyboard_.keyDown(event.key)")
    fallback_pos = display.index(
        "if (event.key == ']') { nextPage(); return true; }", route_pos
    )
    base.require(route_pos < fallback_pos,
                 "NOTE-mode routing must run before legacy global fallback")


base.test_transport_note_mode_keys_remain_live = (
    test_transport_note_mode_keys_remain_live
)


def test_manual_polyphony_is_external_and_bounded() -> None:
    event_header = (ROOT / "src/input/musical_event.h").read_text(
        encoding="utf-8"
    )
    keyboard_h = (ROOT / "src/input/performance_keyboard.h").read_text(
        encoding="utf-8"
    )
    keyboard_cpp = (ROOT / "src/input/performance_keyboard.cpp").read_text(
        encoding="utf-8"
    )
    internal = (ROOT / "src/input/internal_synth_output.cpp").read_text(
        encoding="utf-8"
    )
    usb = (ROOT / "src/midi/usb_midi_output.cpp").read_text(
        encoding="utf-8"
    )
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(
        encoding="utf-8"
    )

    base.require("PerformanceKeyboardPoly" in event_header,
                 "manual POLY must have a distinct event source")
    base.require("enum class PerformanceVoiceMode" in keyboard_h and
                 "PerformanceVoiceMode::Mono" in keyboard_h,
                 "MONO must remain the default runtime voice mode")
    base.require("bool PerformanceKeyboard::directPolyphonyEnabled() const" in keyboard_cpp and
                 "!transformedPlaybackEnabled()" in keyboard_cpp,
                 "manual POLY must not replace ARP/Chord generated ownership")
    base.require("MusicalEventSource::PerformanceKeyboardPoly" in keyboard_cpp and
                 "emitPolyNoteOn" in keyboard_cpp and
                 "emitPolyNoteOff" in keyboard_cpp,
                 "direct POLY keys need exact per-note NoteOn/NoteOff events")

    key_up = keyboard_cpp[
        keyboard_cpp.index("bool PerformanceKeyboard::keyUp"):
        keyboard_cpp.index("void PerformanceKeyboard::releaseMissingKeys")
    ]
    base.require("if (directPolyphonyEnabled())" in key_up and
                 "emitPolyNoteOff(released.note);" in key_up,
                 "POLY key-up must release the exact physical note")
    reconcile = keyboard_cpp[
        keyboard_cpp.index("void PerformanceKeyboard::releaseMissingKeys"):
        keyboard_cpp.index("void PerformanceKeyboard::setEnabled")
    ]
    base.require("if (directPolyphonyEnabled())" in reconcile and
                 "emitPolyNoteOff(held_[read].note);" in reconcile,
                 "matrix reconciliation must independently clean missing POLY notes")

    base.require("event.source == MusicalEventSource::PerformanceKeyboardPoly" in internal,
                 "internal Synth A/B must ignore external manual POLY events")
    base.require("isGeneratedPerformanceSource" in usb and
                 "source == MusicalEventSource::PerformanceKeyboardPoly" in usb and
                 "acquireGeneratedNote(event.target, event.note, event.velocity)" in usb and
                 "releaseGeneratedNote(event.target, event.note, event.velocity)" in usb,
                 "USB POLY must reuse the bounded per-note ownership path")
    base.require("wireOwners_" in usb and "generatedActive_" in usb,
                 "manual POLY must remain inside existing bounded ownership storage")

    base.require("case '9':" in page and
                 "keyboard_.toggleVoiceMode();" in page and
                 "VOICE: POLY / EXT MIDI" in page and
                 "PERFORMANCE TOOLS: 1-9" in page and
                 "EXT MIDI ONLY" in page,
                 "PERFORM UI must expose and explain MONO/POLY mode")


base.test_manual_polyphony_is_external_and_bounded = (
    test_manual_polyphony_is_external_and_bounded
)


def test_smf_file_bpm_copy_is_bounded() -> None:
    page = (
        ROOT / "src/ui/pages/smf_player_page_structural.cpp"
    ).read_text(encoding="utf-8")

    start = page.index("const bool fileBpmShortcut")
    end = page.index("if (event.event_type != GROOVEPUTER_KEY_DOWN", start)
    block = page[start:end]

    base.require("event.alt && !event.ctrl" in block and
                 "(event.key == 'o' || event.key == 'O')" in block,
                 "FILE BPM copy must have an explicit non-conflicting shortcut")
    base.require("SmfPlayerState::Playing" in block and
                 "SmfPlayerState::Armed" in block and
                 "miniAcid_.isPlaying()" in block,
                 "FILE BPM copy must reject active MIDI and GP transports")
    base.require("state.originalBpmX10" in block and
                 "miniAcid_.setBpm(fileBpm)" in block,
                 "FILE BPM copy must read metadata directly and update GP BPM")
    base.require("TransportClockSource::GroovePuterInternal" in block,
                 "FILE BPM copy must select GP as outbound Clock owner")
    base.require("withAudioGuard" in block,
                 "GP BPM mutation must use the existing audio guard")
    base.require("toggleTempoMode" not in block and
                 "resetTempo" not in block and
                 "togglePlayPause" not in block,
                 "FILE BPM copy must not drive the asynchronous SMF command queue")
    base.require("ALT+O FileBPM" in page,
                 "MIDI Player footer must expose the FILE BPM shortcut")


base.test_smf_file_bpm_copy_is_bounded = test_smf_file_bpm_copy_is_bounded


if __name__ == "__main__":
    base.main()
