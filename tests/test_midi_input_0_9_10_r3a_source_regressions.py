#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROUTER = ROOT / "src/input/midi_input_router.h"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    router = ROUTER.read_text(encoding="utf-8")
    includes = "\n".join(
        line for line in router.splitlines()
        if line.lstrip().startswith("#include")
    )

    for forbidden in (
        "Arduino", "TinyUSB", "USBMIDI", "tusb", "FreeRTOS",
        "Bluetooth", "BLE", "HardwareSerial", "Preferences", "NVS",
        "output_ownership", "midi_device_profile", "seqtrak", "ui/",
        "miniacid_engine", "clamped_live_note_identity",
    ):
        require(forbidden not in includes,
                f"R3a input owner must stay transport/output/DSP/UI independent: {forbidden}")

    require("bool enabled{false}" in router,
            "MIDI input must preserve disabled-by-default behavior in R3a")
    require("kMaxActiveNotes = 24u" in router,
            "R3a active-note ownership must remain explicitly bounded")
    require("kDefaultDrainBudget = 32u" in router,
            "R3a consumer work must remain bounded per service pass")
    require("kSynthNoteMin = 24u" in router and "kSynthNoteMax = 71u" in router,
            "R3a routed synth pitch range must remain explicit")
    require("sourceNote" in router and "routedNote" in router,
            "owner must retain raw source note separately from routed synth pitch")
    require("MidiInputTransportId" in router and "MidiInputSessionId" in router,
            "stable owner identity must include transport + session")
    require("owner.transportId == message.transportId" in router and
            "owner.sessionId == message.sessionId" in router and
            "owner.channel == message.channel" in router and
            "owner.sourceNote == message.note()" in router,
            "physical NoteOff identity must remain transport/session/channel/source-note")
    require("owner.target == target" in router and
            "owner.routedNote == routedNote" in router,
            "resolved target+pitch ownership lookup must remain explicit")
    require("routedPitchReplacements" in router and
            "findRoutedPitchOwner" in router,
            "newest source must replace an older owner of the same resolved synth pitch")
    require("releaseAllOwnedNotes();\n        ++diagnostics_.configPanics;\n        config_ = next;" in router,
            "config changes must clean old owners before adopting new routing")
    require("Never publish an unowned NoteOn" in router,
            "capacity exhaustion must fail closed before NoteOn publication")
    require("queue.discardPendingFromConsumer();" in router and
            "releaseAllOwnedNotes();" in router and
            "overflowRecoveries" in router,
            "queue overflow must force discard + input-owner cleanup")
    require("MusicalEventSource::MidiInput" in router,
            "post-route events must reuse the canonical MidiInput source")
    require("MidiInputTarget::SynthA" in router and
            "MidiInputTarget::SynthB" in router and
            "MidiInputTarget::Drums" not in router,
            "R3a target scope must remain Synth A/B only; Drums is R4")
    require("sizeof(MidiInputRouter) <= 320u" in router,
            "R3a router/owner memory ceiling must remain explicit")

    # The input core duplicates only the two numeric live-pitch boundaries to
    # avoid importing DSP headers. Lock them to the existing engine identity.
    clamped = (ROOT / "src/dsp/clamped_live_note_identity.h").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
    require("kMinNote = 24" in clamped and "kMaxNote = 71" in clamped,
            "ClampedLiveNoteIdentity live range changed; audit MidiInputRouter range")
    require("kMin303Note = 24" in engine and "kMax303Note = 71" in engine,
            "MiniAcid live range changed; audit MidiInputRouter range")

    # R3a proves the core owner before physical wiring. Runtime instantiation and
    # the existing TinyUSB reader are changed only in R3b after this gate is green.
    runtime_refs = []
    for path in (ROOT / "src").rglob("*"):
        if not path.is_file() or path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
            continue
        if path == ROUTER:
            continue
        text = path.read_text(encoding="utf-8")
        if "MidiInputRouter" in text:
            runtime_refs.append(path.relative_to(ROOT).as_posix())
    require(not runtime_refs,
            "R3a must not wire the router into runtime yet: " + ", ".join(runtime_refs))

    # Common fan-out must not accidentally become MIDI THRU. The current USB
    # output owns no MidiInput lane; freeze that fact until an explicit THRU
    # product policy is introduced.
    usb_output_h = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8")
    usb_output_cpp = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")
    require("MusicalEventSource::MidiInput" not in usb_output_h and
            "MusicalEventSource::MidiInput" not in usb_output_cpp,
            "R3a forbids implicit MIDI THRU lanes in UsbMidiOutput")

    print("0.9.10 R3a source/ownership boundaries: PASS")


if __name__ == "__main__":
    main()
