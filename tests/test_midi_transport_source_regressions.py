#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    event_header = (ROOT / "src/midi/scheduled_midi_transport_event.h").read_text(encoding="utf-8")
    queue = (ROOT / "src/midi/scheduled_midi_transport_event_queue.h").read_text(encoding="utf-8")
    publisher = (ROOT / "src/midi/midi_transport_clock_publisher.h").read_text(encoding="utf-8")
    facade = (ROOT / "src/input/musical_event_queue.h").read_text(encoding="utf-8")
    transport_api = (ROOT / "src/midi/usb_midi_transport.h").read_text(encoding="utf-8")
    transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")
    transport_h = (ROOT / "src/platform/cardputer_usb_midi_transport.h").read_text(encoding="utf-8")
    service = (ROOT / "src/platform/cardputer_usb_midi_service.h").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    musical_event = (ROOT / "src/input/musical_event.h").read_text(encoding="utf-8")
    scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    ui_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore")
        for path in (ROOT / "src/ui").rglob("*")
        if path.is_file() and path.suffix in {".h", ".hpp", ".cpp", ".cc"}
    )

    require("enum class MidiTransportEventType" in event_header and
            "Clock" in event_header and "Start" in event_header and "Stop" in event_header,
            "transport events need a dedicated Clock/Start/Stop model")
    require("MidiTransport" not in musical_event,
            "transport must not be represented as MusicalEvent/MusicalEventTarget")

    require("kStorageSize = 64" in queue and
            "kCriticalReserve = 4" in queue,
            "transport handoff must remain fixed-size with critical reserve")
    require("tryPushClock" in queue and "tryPushLifecycle" in queue,
            "transport queue must separate clock and lifecycle overflow policy")
    require("takePendingCriticalRecovery" in queue and
            "criticalOverflow" in queue,
            "Start/Stop overflow needs an explicit bounded recovery path")
    for token in ("std::vector", "std::deque", "new ", "malloc("):
        require(token not in queue,
                f"realtime transport queue must not allocate: {token}")

    require("kMidiClocksPerQuarter = 24" in publisher and
            "kMidiClocksPerStep" in publisher,
            "publisher must encode 24 PPQN / six clocks per 1/16 step")
    require("previousTransportPlaying_" in publisher and
            "MidiTransportEventType::Start" in publisher and
            "MidiTransportEventType::Stop" in publisher,
            "Start/Stop must be lifecycle transitions, not UI actions")
    require("startPulsePosition" in publisher and
            "samplesPerStep" in publisher,
            "Clock timing must derive from sequencer phase and audio sample timing")
    for token in ("millis(", "delay(", "vTaskDelay("):
        require(token not in publisher,
                f"transport publisher must not use wall-clock loops: {token}")

    require("transportClockPublisher_.beginBlock" in facade and
            "transportQueue_" in facade,
            "AudioTask render bracket must publish transport timing")
    require("MusicalEventQueue& patternQueue" in service,
            "dispatcher service must receive the facade that owns both queues")

    for forbidden in (sketch, engine, ui_text):
        require("sendTimingClock(" not in forbidden and
                "sendStart(" not in forbidden and
                "sendStop(" not in forbidden and
                "writeRealtimePacket(" not in forbidden,
                "TinyUSB realtime writes must stay out of loop/DSP/UI code")
    require("USBMIDI" not in engine and "TinyUSB" not in engine,
            "DSP must remain independent of USB transport")

    require("sendTimingClock()" in transport_api and
            "sendStart()" in transport_api and
            "sendStop()" in transport_api,
            "platform-neutral transport API needs only strict realtime methods")
    require("sendTimingClock() override" in transport_h and
            "sendStart() override" in transport_h and
            "sendStop() override" in transport_h,
            "Cardputer transport must implement the strict realtime surface")
    require("kCinSingleByte = 0x0F" in transport and
            "kStatusTimingClock = 0xF8" in transport and
            "kStatusStart = 0xFA" in transport and
            "kStatusStop = 0xFC" in transport,
            "USB-MIDI realtime packets must use CIN 0xF and standard status bytes")
    require("midiEventPacket_t packet" in transport and
            "writeRealtimePacket" in transport,
            "Cardputer implementation must use the pinned USBMIDI packet API")

    require("transportBeforeMusical" in transport and
            "return true;" in transport[transport.index("bool transportBeforeMusical"):transport.index("bool dispatchTransportEvent")],
            "transport must deterministically win an equal sample deadline")
    require("kClockStaleThresholdUs = 5000" in transport and
            "lateness > kClockStaleThresholdUs" in transport and
            "clockDropped" in transport,
            "late Clock pulses must be dropped instead of catch-up bursting")
    require("clockStaleGenerationDrops" in transport and
            transport.count("scheduledMidiTransportEventGenerationIsCurrent") >= 2,
            "queued Clock must be invalidated before and after deadline waits")
    require("g_transport.sendTimingClock()" in transport and
            "g_transport.sendStart()" in transport and
            "g_transport.sendStop()" in transport,
            "MidiDispatchTask translation unit must own all realtime USB writes")
    require("kControlDrainBudget" in transport,
            "live controls must be bounded so they cannot starve Clock deadlines")

    combined_transport = "\n".join((event_header, queue, publisher, facade,
                                     transport_api, transport_h, transport))
    require("sendContinue" not in combined_transport and
            "Continue" not in event_header and "Continue" not in publisher,
            "GP MASTER outbound transport must remain Start/Stop without Continue")
    require("Preferences" not in combined_transport and "NVS" not in combined_transport,
            "transport sync must not depend on PR #11 settings persistence")
    require("MidiTransport" not in scenes_h and "MidiTransport" not in scenes_cpp and
            "midiTransport" not in scenes_h and "midiTransport" not in scenes_cpp,
            "scene schema must remain unchanged")

    print("MIDI transport source regressions: OK")


if __name__ == "__main__":
    main()
