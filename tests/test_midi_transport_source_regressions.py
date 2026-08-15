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
    capabilities = (ROOT / "src/midi/midi_transport_capabilities.h").read_text(encoding="utf-8")
    profile_runtime = (ROOT / "src/midi/midi_device_profile_runtime.h").read_text(encoding="utf-8")
    transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")
    transport_h = (ROOT / "src/platform/cardputer_usb_midi_transport.h").read_text(encoding="utf-8")
    service = (ROOT / "src/platform/cardputer_usb_midi_service.h").read_text(encoding="utf-8")
    settings_session = (ROOT / "src/platform/cardputer_midi_settings_session.cpp").read_text(encoding="utf-8")
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
            "Clock" in event_header and "Start" in event_header and
            "Continue" in event_header and "Stop" in event_header,
            "transport events need a dedicated Clock/Start/Continue/Stop model")
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
            "MidiTransportEventType::Continue" in publisher and
            "MidiTransportEventType::Stop" in publisher,
            "Start/Continue/Stop must be lifecycle transitions, not UI actions")
    require("restartFromBeginning" in publisher and
            "startPulsePosition" in publisher and
            "samplesPerStep" in publisher,
            "Clock timing and resume semantics must derive from the audio block")
    for token in ("millis(", "delay(", "vTaskDelay("):
        require(token not in publisher,
                f"transport publisher must not use wall-clock loops: {token}")

    require("transportClockPublisher_.beginBlock" in facade and
            "restartFromBeginning" in facade and
            "transportQueue_" in facade,
            "AudioTask render bracket must publish transport timing")
    require("MusicalEventQueue& patternQueue" in service,
            "dispatcher service must receive the facade that owns both queues")

    for forbidden in (sketch, engine, ui_text):
        require("sendTimingClock(" not in forbidden and
                "sendStart(" not in forbidden and
                "sendContinue(" not in forbidden and
                "sendStop(" not in forbidden and
                "sendSongPositionPointer(" not in forbidden and
                "writeRealtimePacket(" not in forbidden,
                "TinyUSB transport writes must stay out of loop/DSP/UI code")
    require("USBMIDI" not in engine and "TinyUSB" not in engine,
            "DSP must remain independent of USB transport")

    require("sendTimingClock()" in transport_api and
            "sendStart()" in transport_api and
            "sendContinue()" in transport_api and
            "sendStop()" in transport_api and
            "sendSongPositionPointer(uint16_t midiBeats)" in transport_api,
            "platform-neutral transport API needs optional Continue and SPP surfaces")
    require("struct MidiTransportCapabilities" in capabilities and
            "MidiContinueBehavior" in capabilities and
            "songPositionTx" in capabilities,
            "optional transport behavior must be capability-gated")
    require("midiTransportCapabilitiesForProfile" in capabilities and
            "MidiDeviceProfile::GeneralMidi" in capabilities and
            "MidiDeviceProfile::Custom" in capabilities and
            "MidiTransportCapabilityRuntime" in capabilities,
            "device profile must select the runtime transport capability policy")
    require("clampSongPositionPointer" in capabilities and
            "songPositionPointerFromPpqnTicks" in capabilities and
            "0x3FFFu" in capabilities and
            "songPositionPointerLsb" in capabilities and
            "songPositionPointerMsb" in capabilities,
            "SPP helpers must preserve PPQN conversion and the 14-bit payload")
    require("profileRuntime.initialize(loadedSettings)" in settings_session and
            "midiDeviceProfileRuntime()" in settings_session and
            "midiTransportCapabilityRuntime().setDeviceProfile(settings_.profile)" in profile_runtime,
            "persisted MIDI device profile must initialize the single runtime owner, which publishes transport capabilities")

    seqtrak_start = capabilities.index(
        "constexpr MidiTransportCapabilities seqtrakValidatedTransportCapabilities()")
    seqtrak_end = capabilities.index(
        "constexpr MidiTransportCapabilities conservativeCustomTransportCapabilities()",
        seqtrak_start,
    )
    seqtrak_profile = capabilities[seqtrak_start:seqtrak_end]
    require("capabilities.continueTx = true" not in seqtrak_profile and
            "capabilities.songPositionTx = true" not in seqtrak_profile,
            "SEQTRAK profile must not claim unvalidated Continue/SPP TX")

    generic_start = capabilities.index(
        "constexpr MidiTransportCapabilities genericClassCompliantTransportCapabilities()")
    generic_end = capabilities.index(
        "constexpr MidiTransportCapabilities seqtrakValidatedTransportCapabilities()",
        generic_start,
    )
    generic_profile = capabilities[generic_start:generic_end]
    require("capabilities.continueTx = true" in generic_profile and
            "capabilities.songPositionTx = true" in generic_profile,
            "General MIDI profile must opt into class-compliant Continue/SPP")

    require("sendTimingClock() override" in transport_h and
            "sendStart() override" in transport_h and
            "sendContinue() override" in transport_h and
            "sendStop() override" in transport_h and
            "sendSongPositionPointer(uint16_t sixteenthNotes) override" in transport_h,
            "Cardputer transport must implement the extended lifecycle surface")
    continue_start = transport_h.index("bool sendContinue() override")
    continue_end = transport_h.index("bool sendSongPositionPointer", continue_start)
    continue_block = transport_h[continue_start:continue_end]
    require("capabilities.songPositionTx" in continue_block and
            "songPositionPointerFromPpqnTicks" in continue_block and
            "sendSongPositionPointer(position)" in continue_block,
            "PROJECT resume must derive and send capability-gated SPP")
    require(continue_block.index("sendSongPositionPointer(position)") <
            continue_block.index("0xFB"),
            "SPP must be serialized before Continue by the sole dispatcher owner")
    require("MidiContinueBehavior::RestartFromBeginning" in continue_block and
            "writeRealtimePacket(0xFA)" in continue_block,
            "unsupported Continue must use the validated Start fallback")

    spp_start = transport_h.index("bool sendSongPositionPointer")
    spp_end = transport_h.index("void flush()", spp_start)
    spp_block = transport_h[spp_start:spp_end]
    require("capabilities().songPositionTx" in spp_block and
            "0x03" in spp_block and "0xF2" in spp_block and
            "songPositionPointerLsb" in spp_block and
            "songPositionPointerMsb" in spp_block,
            "SPP packet must be profile-gated and use USB-MIDI CIN 0x3")

    require("kCinSingleByte = 0x0F" in transport and
            "kStatusTimingClock = 0xF8" in transport and
            "kStatusStart = 0xFA" in transport and
            "kStatusContinue = 0xFB" in transport and
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
            "g_transport.sendContinue()" in transport and
            "g_transport.sendStop()" in transport,
            "MidiDispatchTask translation unit must own all realtime USB writes")
    require("kControlDrainBudget" in transport,
            "live controls must be bounded so they cannot starve Clock deadlines")

    combined_transport = "\n".join((event_header, queue, publisher, facade,
                                     transport_api, capabilities, transport_h, transport))
    require("Preferences" not in combined_transport and "NVS" not in combined_transport,
            "transport sync must not depend on settings persistence")
    require("MidiTransport" not in scenes_h and "MidiTransport" not in scenes_cpp and
            "midiTransport" not in scenes_h and "midiTransport" not in scenes_cpp,
            "scene schema must remain unchanged")

    print("MIDI transport source regressions: OK")


if __name__ == "__main__":
    main()
