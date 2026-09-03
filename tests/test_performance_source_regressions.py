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
    cleanup_pos = transport_block.index("stopGeneratedOutput();")
    state_pos = transport_block.index("transportPlaying_ = playing;")
    reset_pos = transport_block.index("resetPulseClock(true);")
    base.require(cleanup_pos < state_pos < reset_pos,
                 "transport epoch changes must close generated obligations before state update and pulse re-anchor")

    service_start = keyboard.index("void PerformanceKeyboard::service(uint32_t")
    service_end = keyboard.index(
        "void PerformanceKeyboard::reconcileDirectPolyChord", service_start)
    service_block = keyboard[service_start:service_end]
    base.require("if (transportPlaying_)" in service_block and
                 "serviceTransportPulseClock(nowMicros)" in service_block,
                 "running transport must select the project-timeline pulse clock")
    base.require("projectTransportTimeline().trySnapshot(snapshot)" in keyboard and
                 "snapshot.absoluteSteps()" in keyboard,
                 "pulse generation must read the coherent project transport phase")
    base.require("snapshotCardputerUsbMidiBlockAnchor" in keyboard and
                 "anchorPlaybackMicros" in keyboard,
                 "hardware timing must use the dispatcher playback anchor")
    base.require("euclideanPulseActive(musicalPulseOrdinal_)" in keyboard and
                 "musicalPulseOrdinal_" in header,
                 "Euclidean phase must derive from the monotonic musical pulse ordinal")
    base.require("kTransportScheduleLeadPulses = 0.5" in keyboard and
                 "transportAnchorBlockSequence_" in header and
                 "transportAnchorMicros_" in header,
                 "transport pulses must be prepared ahead from a stable block anchor")
    base.require("kGeneratedNoteOnStaleMicros = 12000u" in keyboard and
                 "lateness > kGeneratedNoteOnStaleMicros" in keyboard and
                 "leadMicros < -static_cast<int32_t>(kGeneratedNoteOnStaleMicros)" in keyboard,
                 "late generated NoteOn must be shed instead of caught up")
    base.require("kMaxScheduledEvents = 112" in header,
                 "dense 8-note x4 ratchet scheduling needs overlap headroom")
    base.require("INPUT LOCK | PATTERN PLAYER ACTIVE" not in page and
                 'stepTools ? "LIVE SYNC" : (directPoly ? "POLY EXT" : "MONO EXT")' in page,
                 "PERFORM must show live-input state instead of the old transport lock")

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
    internal_h = (ROOT / "src/input/internal_synth_output.h").read_text(
        encoding="utf-8"
    )
    internal = (ROOT / "src/input/internal_synth_output.cpp").read_text(
        encoding="utf-8"
    )
    usb_h = (ROOT / "src/midi/usb_midi_output.h").read_text(
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
    base.require("kMaxHeldNotes = 19" in keyboard_h and
                 "kMaxPolyChordNotes = 16" in keyboard_h,
                 "manual POLY and POLY+CHORD limits must be explicit contracts")
    base.require("bool PerformanceKeyboard::directPolyphonyEnabled() const" in keyboard_cpp and
                 "!transformedPlaybackEnabled()" in keyboard_cpp,
                 "plain manual POLY must remain separate from transformed playback")
    base.require("bool PerformanceKeyboard::polyChordSustainEnabled() const" in keyboard_cpp and
                 "chordMode_ != PerformanceChordMode::Off" in keyboard_cpp and
                 "!activeStepEngineEnabled()" in keyboard_cpp,
                 "direct POLY+CHORD sustain must not replace step-generated ownership")

    key_up = keyboard_cpp[
        keyboard_cpp.index("bool PerformanceKeyboard::keyUp"):
        keyboard_cpp.index("void PerformanceKeyboard::releaseMissingKeys")
    ]
    base.require("if (directPolyphonyEnabled())" in key_up and
                 "emitPolyNoteOff(released.note);" in key_up,
                 "POLY key-up must release the exact physical note")
    base.require("emitNoteOff(released.note);" in key_up and
                 "emitNoteOn(held_[heldCount_ - 1])" not in key_up,
                 "plain MONO must emit the released physical NoteOff and never synthesize restoration NoteOn")
    base.require("if (polyChordSustainEnabled())" in key_up and
                 "reconcileDirectPolyChord(lastServiceMicros_);" in key_up,
                 "POLY+CHORD key-up must diff ownership instead of restoring/retriggering a root")

    poly_chord = keyboard_cpp[
        keyboard_cpp.index("void PerformanceKeyboard::reconcileDirectPolyChord"):
        keyboard_cpp.index("void PerformanceKeyboard::triggerDirectTransformed")
    ]
    base.require("buildChord(" in poly_chord and
                 "routeGenerated(MusicalEventType::NoteOff, note, 0);" in poly_chord and
                 "if (desiredContains(note))" in poly_chord and
                 "routeGenerated(MusicalEventType::NoteOn, desired[i], velocity);" in poly_chord,
                 "POLY+CHORD must keep unchanged notes sounding and change only the set difference")
    base.require("stopGeneratedOutput();\n            if (heldCount_ > 0) triggerDirectTransformed" in key_up,
                 "MONO transformed last-root restoration must remain available separately")

    reconcile = keyboard_cpp[
        keyboard_cpp.index("void PerformanceKeyboard::releaseMissingKeys"):
        keyboard_cpp.index("void PerformanceKeyboard::setEnabled")
    ]
    base.require("keyUp(missing[i])" in reconcile,
                 "matrix reconciliation must delegate missing physical keys through exact key-up ownership")

    base.require("struct MonoArbitrationState" in internal_h and
                 "generatedCandidate" in internal_h and
                 "directCandidate" in internal_h and
                 "otherLiveCandidate" in internal_h,
                 "internal Synth A/B must use the fixed-size mono arbitration owner")
    base.require("case MusicalEventSource::Arpeggiator:" in internal_h and
                 "return &generatedCandidate;" in internal_h and
                 "case MusicalEventSource::PerformanceKeyboard:" in internal_h and
                 "return &directCandidate;" in internal_h,
                 "generated and direct PERFORM sources must participate in deterministic internal mono arbitration")
    base.require("case MusicalEventSource::PerformanceKeyboardPoly:" in internal_h and
                 "return nullptr;" in internal_h and
                 "if (event.source == MusicalEventSource::PerformanceKeyboardPoly) return;" in internal,
                 "manual POLY must remain external-MIDI-only while internal Synth A/B stays mono")

    base.require("kSeqtrakMonoPolyController = 26" in usb_h and
                 "kSeqtrakMonoValue = 0" in usb_h and
                 "kSeqtrakPolyValue = 1" in usb_h,
                 "SEQTRAK receiver mode must use the documented CH8..10 CC26 values")
    base.require("isSynthPerformanceSource" in usb and
                 "source == MusicalEventSource::PerformanceKeyboard" in usb and
                 "source == MusicalEventSource::PerformanceKeyboardPoly" in usb and
                 "source == MusicalEventSource::Arpeggiator" in usb,
                 "all synth PERFORM note sources must share exact per-note wire ownership")
    base.require("sourceRequestsPolyReceiver" in usb and
                 "ensurePerformanceReceiverMode" in usb and
                 "sendControlChange" in usb and
                 "kSeqtrakMonoPolyController" in usb,
                 "MONO/POLY must be selected at the receiver rather than by replacing controller notes")
    base.require("CC126/127" in usb and
                 "All Notes Off" in usb,
                 "the implementation must document why standard channel-mode switching is unsafe on shared Pattern/SMF channels")
    base.require("acquireGeneratedNote(event.target, event.note, event.velocity)" in usb and
                 "releaseGeneratedNote(event.target, event.note, event.velocity)" in usb,
                 "MONO and POLY direct notes must use the bounded exact-note ownership path")
    base.require("wireOwners_" in usb_h and "generatedActive_" in usb_h,
                 "manual performance notes must remain inside bounded ownership storage")

    base.require("kMinVelocity = 10" in keyboard_h and
                 "kMaxVelocity = 120" in keyboard_h and
                 "kVelocityStep = 10" in keyboard_h and
                 "kDefaultVelocity = 100" in keyboard_h,
                 "fixed Cardputer velocity must have explicit 10-step bounds and default")
    base.require("bool PerformanceKeyboard::adjustVelocity" in keyboard_cpp and
                 "if (velocity == 0) velocity = keyVelocity_;" in keyboard_cpp,
                 "future Cardputer keyDown events must use the configurable fixed velocity")
    base.require("VELOCITY" in page and
                 "case 3: keyboard_.adjustVelocity(direction); break;" in page and
                 '"-/+ VALUE  ENTER ALT  9 VOICE"' in page,
                 "KEY context must expose velocity through the contextual -/+ dispatcher")

    base.require("case '9':" in page and
                 "keyboard_.toggleVoiceMode();" in page and
                 '"VOICE: %s / RECEIVER"' in page and
                 "INT+USB" not in page,
                 "PERFORM UI must keep the compatibility receiver voice-mode command authoritative")
    base.require("HeldPerformanceSnapshot" not in page and
                 "restoreHeldPerformanceKeys" not in page,
                 "receiver-mode compatibility must not revive cleared physical key ownership")


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
