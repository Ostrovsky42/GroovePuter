#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def main() -> None:
    event = read("src/input/musical_event.h")
    router = read("src/input/musical_event_router.h")
    queue = read("src/input/musical_event_queue.h")
    internal = read("src/input/internal_synth_output.cpp")
    performance_h = read("src/input/performance_keyboard.h")
    performance_cpp = read("src/input/performance_keyboard.cpp")
    transport_h = read("src/platform/cardputer_usb_midi_transport.h")
    transport = read("src/platform/cardputer_usb_midi_transport.cpp")
    ownership = read("src/output/output_ownership.h")
    engine = read("src/dsp/miniacid_engine.cpp")
    pattern_edit = read("src/ui/pages/pattern_edit_page.cpp")

    require("MidiInput" in event,
            "MusicalEventSource::MidiInput must remain available")
    require("NoteOn" in event and "NoteOff" in event and "AllNotesOff" in event,
            "existing musical lifecycle events must remain available")
    for target in ("SynthA", "SynthB", "Drums"):
        require(target in event, f"existing logical target {target} is required")
    require("velocity" in event and "channel" in event and "note" in event,
            "MusicalEvent must preserve channel/note/velocity fields")

    require("kMaxSinks" in router and "sinks_[kMaxSinks]" in router,
            "MusicalEventRouter must retain bounded fixed sink storage")
    require("route(const MusicalEvent&" in router,
            "existing MusicalEventRouter fan-out must remain available")

    require("ScheduledMusicalEventQueue" in queue,
            "MusicalEventQueue must remain the scheduled Pattern facade")

    require("bool readPacket(midiEventPacket_t& packet)" in transport_h,
            "Cardputer transport must expose the existing packet reader")
    require("tud_midi_packet_read" in transport,
            "Cardputer MIDI RX must continue using the existing TinyUSB FIFO")
    require("configASSERT(xTaskGetCurrentTaskHandle() == g_dispatchTaskHandle);" in transport,
            "TinyUSB FIFO access must remain owned by MidiDispatchTask")
    require("kMidiRxDrainBudget = 32" in transport,
            "USB RX drain must remain bounded on the research baseline")
    require("drainIncomingMidiPackets" in transport and
            "parseUsbMidiRealtimeTransport" in transport,
            "existing external Clock/Start/Continue/Stop parser must remain separate")
    require("externalRxIgnored" in transport,
            "non-realtime RX is currently observable as ignored input")

    tinyusb_cpp_users = []
    for path in (ROOT / "src").rglob("*.cpp"):
        text = path.read_text(encoding="utf-8")
        if "tud_midi_" in text:
            tinyusb_cpp_users.append(path.relative_to(ROOT).as_posix())
    require(tinyusb_cpp_users == ["src/platform/cardputer_usb_midi_transport.cpp"],
            "TinyUSB MIDI calls must remain isolated to the platform transport")
    require("tud_midi_rx_cb" not in transport,
            "R1 baseline intentionally has no second TinyUSB MIDI RX callback owner")

    midi_input_ownership = ownership[ownership.index("case MusicalEventSource::MidiInput"):]
    require("later roadmap item" in midi_input_ownership and "return false" in midi_input_ownership,
            "R1 must record the explicit 0.9.6 MidiInput ownership gap")

    require("engine_.liveNoteOn" in internal and "engine_.liveNoteOff" in internal,
            "existing internal Synth A/B live path must remain reusable")
    require("MidiInput is deliberately outside this 0.9.6 migration" in internal,
            "current Drums/MidiInput exclusion must remain explicit until production R4")
    require("triggerRegisteredLocalDrumVoice" in internal and "samplerTrack->triggerPad" in internal,
            "logical Drums must retain local synth + optional sampler-layer integration")

    require("kMaxHeldNotes" in performance_h and "held_[" in performance_h,
            "PerformanceKeyboard must retain bounded held-note storage")
    require("panic();" in performance_cpp and "setTarget" in performance_cpp,
            "Performance target changes must retain cleanup precedent")

    live_on = engine[engine.index("void MiniAcid::liveNoteOn"):engine.index("void MiniAcid::liveNoteOff")]
    require("if (playing) return;" in live_on,
            "R1 must record the current PLAY-state live-synth limitation")
    live_off = engine[engine.index("void MiniAcid::liveNoteOff"):engine.index("void MiniAcid::allLiveNotesOff")]
    require("liveNotes_[idx]" in live_off,
            "existing synth release is tied to retained live-note identity")

    require("writeNoteEntryStep" in pattern_edit and
            "editCurrentSynthPattern" in pattern_edit,
            "Pattern note-entry mutation boundary must remain visible to recording research")

    print("0.9.10 R1 MIDI input archaeology/source boundaries: PASS")


if __name__ == "__main__":
    main()
