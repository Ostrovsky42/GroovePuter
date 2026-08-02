#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    event_header = (ROOT / "src/input/musical_event.h").read_text(encoding="utf-8")
    sink = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")
    transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")
    transport_header = (ROOT / "src/platform/cardputer_usb_midi_transport.h").read_text(
        encoding="utf-8")
    endpoint_health = (ROOT / "src/midi/usb_endpoint_health.h").read_text(
        encoding="utf-8")
    player_service = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(
        encoding="utf-8")
    service = (ROOT / "src/platform/cardputer_usb_midi_service.h").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")
    upload = (ROOT / "scripts/upload.sh").read_text(encoding="utf-8")
    scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("MidiOutput" not in event_header,
            "USB MIDI must remain an output sink, not a MusicalEventTarget")
    require("AudioMutationGate" not in sink and "AudioMutationGate" not in transport,
            "USB output must not pause or mutate the audio renderer")

    setup_start = sketch.index("void setup()")
    registration_call = "registerCardputerUsbMidiSink(\n      g_musicalEventRouter,\n      g_patternMusicalEventQueue,\n      g_externalMidiTransportQueue)"
    require(registration_call in sketch[setup_start:],
            "setup must connect the router and scheduled Pattern queue")
    require(registration_call not in sketch[:setup_start],
            "USB sink registration must not run from global initialization")
    require("g_musicalEventRouter.addSink(g_internalSynthOutput)" in sketch and
            sketch.index("g_musicalEventRouter.addSink(g_internalSynthOutput)") <
            sketch.index(registration_call, setup_start),
            "queued USB registration must follow the immediate internal sink")

    require("router.addSink(g_queueSink)" in transport,
            "router must enqueue live USB events instead of mutating UsbMidiOutput")
    require("router.addSink(g_output)" not in transport,
            "UsbMidiOutput must never be called by the Arduino loop router")
    require('"MidiDispatchTask"' in transport and
            "midiDispatchTask" in transport,
            "Cardputer must create one named USB-MIDI owner task")
    require("g_output.handleMusicalEvent" in transport,
            "the dispatcher must own UsbMidiOutput event delivery")
    require("publishCardputerUsbMidiBlockAnchor" in service and
            "publishCardputerUsbMidiBlockAnchor" in sketch,
            "AudioTask must publish sample-block playback anchors")

    require("USBMIDI.h" not in sketch and "USB.h" not in sketch,
            "TinyUSB headers must stay isolated from the main UI translation unit")
    require("USBMIDI" not in engine and "TinyUSB" not in engine,
            "DSP and PatternPlayer code must remain independent from TinyUSB")
    require("CardputerUsbMidiTransport g_transport;" in transport,
            "USBMIDI descriptor owner must be constructed before Arduino app_main")
    require("static CardputerUsbMidiTransport transport" not in transport,
            "USB transport must not be lazily constructed after TinyUSB starts")
    require("UsbMidiSinkRegistration" not in sketch,
            "application/router mutation must not run from a global constructor")

    tinyusb_options = "USBMode=default,CDCOnBoot=cdc,UploadMode=cdc"
    require(tinyusb_options in build,
            "Cardputer build must select TinyUSB OTG plus CDC upload")
    require(tinyusb_options in upload,
            "Cardputer upload must use the same TinyUSB FQBN")
    require("#if ARDUINO_USB_MODE" in transport,
            "hardware transport must fail closed outside TinyUSB OTG mode")
    require("tud_midi_mounted()" in transport,
            "USB MIDI readiness must use the MIDI interface, not composite CDC mount")
    require("return begun_ && static_cast<bool>(USB);" not in transport,
            "composite USB mount must not be treated as MIDI endpoint readiness")
    require("kSmfCleanupAttemptLimit" in transport and
            "reportTransportFailure" in transport,
            "SMF cleanup must bound its attempts and surface a blocked endpoint")
    require("enterSmfTransportStall" in transport and
            "leaveSmfTransportStall" in transport and
            "reportTransportRecovery" in transport,
            "a host that stops reading the endpoint must park playback in a "
            "resumable stall, not end it")
    require("kSmfStallProbeDelayMs = 1000" in transport,
            "a stalled endpoint is probed on a slow cadence: the fast 10 ms "
            "phase stays bounded by kSmfCleanupAttemptLimit")
    require("abandonAllSmfNotes" not in transport,
            "a stalled endpoint must keep SMF note ownership so the paced retry "
            "stays a real write probe instead of trivially succeeding")
    stall_exit = transport[
        transport.index("void leaveSmfTransportStall()"):
        transport.index("}", transport.index("Serial.println(\"[SMF-WAIT]"))
    ]
    require("g_transport.mounted()" in stall_exit,
            "recovery must require a mounted interface: cleanup can also "
            "complete because an unplug abandoned every note")
    require("kSmfCleanupAttemptLimit = 32" in transport,
            "SMF cleanup must allow the TinyUSB endpoint 320 ms to recover")
    require("beginSmfCleanup(true)" not in transport and
            "MustAbort" not in transport,
            "backpressure that recovers must not be escalated to a transport "
            "failure: only cleanup that cannot complete may stop playback")
    drop_note_on = transport[
        transport.index("if (action == SmfSendFailureAction::DropNoteOn)"):
        transport.index("else if (action == SmfSendFailureAction::BeginCleanup")
    ]
    require("beginSmfCleanup" not in drop_note_on and
            "reportSmfTransportFailure" not in drop_note_on,
            "a NoteOn rejected before wire ownership must be dropped without "
            "cleanup or transport failure")
    require("SmfSendFailureAction::BeginCleanup" in transport and
            "beginSmfCleanup();" in transport,
            "a failed NoteOff must hand recovery to the paced all-notes-off path")
    require("smfMaxSendBlockUs" in transport,
            "USB backpressure duration must be observable in diagnostics")
    require("[USB-DIAG]" in transport and
            "tx=attempt/ok/reject/noMount" in transport and
            "live=q/dispatched/drop" in transport,
            "USB diagnostics must distinguish endpoint rejection from an "
            "undrained live-control queue")
    require("mountUpEvents" in transport_header and
            "mountDownEvents" in transport_header and
            "txRejected" in transport_header,
            "Cardputer transport must count MIDI mount transitions and "
            "TinyUSB write rejection")
    require("UsbEndpointHealth" in transport and
            "kUsbEndpointStallThresholdMs = 50" in transport and
            "observeWrite(true, false" in transport and
            "observeWrite(true, true" in transport and
            "UsbEndpointHealthState::Stalled" in endpoint_health,
            "every physical TX result must feed the shared time-based endpoint "
            "health state before pacing policy is introduced")
    # The dispatcher only blocks on the branches that wait for a deadline. A
    # backlog of already-due events kept it runnable until the CPU0 task
    # watchdog fired, so fairness must not depend on the event mix.
    dispatch_tail = transport[transport.index("void midiDispatchTask("):]
    require("kDispatchFairnessYieldMs" in dispatch_tail and
            "lastFairnessYieldMs" in dispatch_tail,
            "the dispatch loop must yield on a wall-clock budget so a saturated "
            "queue cannot starve the CPU0 idle task")
    # Auto-resume replayed a multi-second backlog and oscillated
    # stall -> resume -> stall; continuing after a stall is a user action.
    recovery = player_service[
        player_service.index("takePendingTransportRecovery()"):
        player_service.index("void CardputerSmfPlayerService::handleProjectTransport")
    ]
    require("startFromTick" not in recovery and "startOriginalFromTick" not in recovery,
            "endpoint recovery must not restart playback by itself")
    require("PRESS PLAY" in recovery,
            "recovery must tell the user that continuing is available")
    start_from_tick = player_service[
        player_service.index("bool CardputerSmfPlayerService::startFromTick"):
        player_service.index("bool CardputerSmfPlayerService::startOriginalFromTick")
    ]
    require("eventQueue_.transportFailed()" in start_from_tick,
            "manual playback commands must not schedule stale deadlines while "
            "the USB endpoint is stalled")
    original_start = player_service[
        player_service.index("bool CardputerSmfPlayerService::startOriginalFromTick"):
        player_service.index("bool CardputerSmfPlayerService::armProjectFromTick")
    ]
    project_start = player_service[
        player_service.index("bool CardputerSmfPlayerService::armProjectFromTick"):
        player_service.index("bool CardputerSmfPlayerService::planProjectLaunch")
    ]
    require("clearTransportFailure" not in original_start and
            "clearTransportFailure" not in project_start,
            "only dispatcher-confirmed endpoint recovery may clear the USB "
            "transport failure")
    cleanup_success = transport[
        transport.index("if (g_output.releaseAllSmfNotes())"):
        transport.index("} else {", transport.index(
            "if (g_output.releaseAllSmfNotes())"))
    ]
    require("recordRecoveredSmfSendBlock();" in cleanup_success,
            "successful cleanup must close the USB backpressure metric")

    forbidden_transport_tokens = (
        "controlChange(",
        "programChange(",
        "pitchBend(",
        "SysEx",
        "MIDI_CLOCK",
        "MIDI_START",
        "MIDI_STOP",
    )
    for token in forbidden_transport_tokens:
        require(token not in transport and token not in sink,
                f"out-of-scope MIDI feature entered USB transport: {token}")

    require("UsbMidi" not in scenes_h and "UsbMidi" not in scenes_cpp,
            "USB MIDI settings must remain runtime-only in this stage")
    require("usbMidi" not in scenes_h and "usbMidi" not in scenes_cpp,
            "scene schema must not gain USB MIDI fields")

    print("USB MIDI source regressions: OK")


if __name__ == "__main__":
    main()
