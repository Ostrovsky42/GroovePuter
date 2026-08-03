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
    midi_only_build = (ROOT / "scripts/build_seqtrak_midi_only.sh").read_text(
        encoding="utf-8")
    dram_check = (ROOT / "scripts/check_cardputer_dram_budget.sh").read_text(
        encoding="utf-8")
    upload = (ROOT / "scripts/upload.sh").read_text(encoding="utf-8")
    scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("MidiOutput" not in event_header,
            "USB MIDI must remain an output sink, not a MusicalEventTarget")
    require("AudioMutationGate" not in sink and "AudioMutationGate" not in transport,
            "USB output must not pause or mutate the audio renderer")

    setup_start = sketch.index("void setup()")
    registration_call = "registerCardputerUsbMidiSink("
    registration_pos = sketch.index(registration_call, setup_start)
    require(registration_pos >= setup_start,
            "setup must connect the router and scheduled Pattern queue")
    require(registration_call not in sketch[:setup_start],
            "USB sink registration must not run from global initialization")
    require("if (!registerCardputerUsbMidiSink(" in sketch[setup_start:],
            "setup must surface MIDI dispatcher registration failure")
    require("g_musicalEventRouter.addSink(g_internalSynthOutput)" in sketch and
            sketch.index("g_musicalEventRouter.addSink(g_internalSynthOutput)") <
            registration_pos,
            "queued USB registration must follow the immediate internal sink")

    require("router.addSink(g_queueSink)" in transport,
            "router must enqueue live USB events instead of mutating UsbMidiOutput")
    require("router.addSink(g_output)" not in transport,
            "UsbMidiOutput must never be called by the Arduino loop router")
    require('"MidiDispatchTask"' in transport and
            "midiDispatchTask" in transport,
            "Cardputer must create one named USB-MIDI owner task")
    require("xTaskCreateStaticPinnedToCore(" in transport and
            "kMidiDispatchStackBytes = 4096" in transport and
            "g_dispatchTaskStack" in transport and
            "g_dispatchTaskBuffer" in transport,
            "dispatcher must retain its 4KB stack without depending on a fragmented heap")
    require("[MIDI-INIT] static task creation failed" in transport and
            "heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)" in transport,
            "static dispatcher creation failure must retain heap diagnostics")
    require("g_output.handleMusicalEvent" in transport,
            "the dispatcher must own UsbMidiOutput event delivery")
    require("publishCardputerUsbMidiBlockAnchor" in service and
            "publishCardputerUsbMidiBlockAnchor" in sketch,
            "AudioTask must publish sample-block playback anchors")
    require("CardputerUsbMidiStatusSnapshot" in service and
            "snapshotCardputerUsbMidiStatus" in service,
            "MIDI-only firmware must expose a read-only endpoint snapshot for UI diagnostics")
    require("#if defined(ARDUINO)" in service and
            "inline CardputerUsbMidiStatusSnapshot" in service,
            "desktop UI must receive a neutral USB snapshot without linking Cardputer TinyUSB")
    player_page = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(
        encoding="utf-8")
    require("snapshotCardputerUsbMidiStatus()" in player_page and
            "USB %s M%u OK%lu NO%lu B%lu H%lu Q%u" in player_page,
            "MIDI Player must show endpoint health without relying on CDC logs")

    require("USBMIDI.h" not in sketch and "USB.h" not in sketch,
            "TinyUSB headers must stay isolated from the main UI translation unit")
    serial_begin = sketch[
        sketch.index("void setup()"):
        sketch.index("AudioDiagnostics::instance().enable(false)")
    ]
    require("#if ARDUINO_USB_CDC_ON_BOOT" in serial_begin and
            serial_begin.index("#if ARDUINO_USB_CDC_ON_BOOT") <
            serial_begin.index("Serial.begin(115200)"),
            "the MIDI-only profile must not start UART0 on GPIO43, which is "
            "the Cardputer-Adv I2S word-select pin")
    require("USBMIDI" not in engine and "TinyUSB" not in engine,
            "DSP and PatternPlayer code must remain independent from TinyUSB")
    require("CardputerUsbMidiTransport g_transport;" in transport,
            "MIDI descriptor owner must be constructed before Arduino app_main")
    require("static CardputerUsbMidiTransport transport" not in transport,
            "USB transport must not be lazily constructed after TinyUSB starts")
    require("UsbMidiSinkRegistration" not in sketch,
            "application/router mutation must not run from a global constructor")

    transport_begin = transport[
        transport.index("bool CardputerUsbMidiTransport::begin()"):
        transport.index("bool CardputerUsbMidiTransport::mounted()")
    ]
    require("#if !ARDUINO_USB_CDC_ON_BOOT" in transport_begin and
            "USB.begin()" in transport_begin and
            "descriptorRegistered_" in transport_begin,
            "the MIDI-only profile must explicitly start TinyUSB: Arduino "
            "app_main only starts it when an on-boot USB interface is enabled")
    require("if (!USB.begin())" in transport_begin and
            "return false;" in transport_begin,
            "MIDI-only TinyUSB startup failure must fail transport registration")
    require("if (begun_) return true;" in transport_begin,
            "the dispatcher must not restart an already initialized USB device")
    descriptor_loader = transport[
        transport.index("uint16_t loadCardputerMidiDescriptor("):
        transport.index("class MidiBlockAnchorClock")
    ]
    require("tinyusb_get_free_duplex_endpoint()" in descriptor_loader and
            "tinyusb_get_free_in_endpoint" not in descriptor_loader and
            "tinyusb_get_free_out_endpoint" not in descriptor_loader,
            "CDC+MIDI must use one duplex endpoint number: the stock split "
            "allocator cross-pairs EP3/EP4 and stalls ESP32-S3 MIDI IN")
    require("USBMIDI midi_" not in transport_header and
            "tud_midi_packet_write" in transport and
            "tud_midi_packet_read" in transport,
            "the transport must not instantiate the stock split-endpoint "
            "USBMIDI descriptor owner")


    tinyusb_options = "USBMode=default,CDCOnBoot=cdc,UploadMode=cdc"
    require(tinyusb_options in build,
            "Cardputer build must select TinyUSB OTG plus CDC upload")
    require(tinyusb_options in upload,
            "Cardputer upload must use the same TinyUSB FQBN")
    require("check_cardputer_dram_budget.sh" in midi_only_build and
            "191488" in midi_only_build and
            ".dram0.data" in dram_check and ".dram0.bss" in dram_check,
            "SEQTRAK MIDI-only build must enforce the measured global DRAM budget")
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
    # A suspended bus stops the host polling the IN endpoint entirely while
    # TinyUSB still reports the interface mounted, which is indistinguishable
    # from a receiver refusing data unless suspend state is logged.
    require("tud_suspended()" in transport and
            "pollSuspendState" in transport,
            "USB suspend must be observable: the Arduino core owns the TinyUSB "
            "suspend callbacks, so edges are polled")
    require("[USB-EDGE]" in transport,
            "entering and leaving a stall must be timestamped against mount and "
            "suspend edges, not only summarised every five seconds")
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
    # Pin the invariant, not the call form: every branch of the single physical
    # write must report its outcome to the shared time-based health state.
    write_packet = transport[
        transport.index("bool CardputerUsbMidiTransport::writePacket("):
        transport.index("bool CardputerUsbMidiTransport::writeChannelPacket(")
    ]
    require("UsbEndpointHealth" in transport and
            "kUsbEndpointStallThresholdMs = 50" in transport and
            "UsbEndpointHealthState::Stalled" in endpoint_health,
            "endpoint health must be shared and time-based, not per-producer")
    require(write_packet.count("observeEndpointWrite(") == 3 and
            "observeEndpointWrite(false, false)" in write_packet and
            "observeEndpointWrite(true, false)" in write_packet and
            "observeEndpointWrite(true, true)" in write_packet,
            "every physical TX result - not mounted, rejected, accepted - must "
            "feed the shared endpoint health state")
    require("usbd_edpt_busy(0, g_midiInEndpoint)" in write_packet and
            "usbd_edpt_stalled(0, g_midiInEndpoint)" in write_packet and
            "txRejectedEndpointBusy" in transport_header and
            "txRejectedEndpointStalled" in transport_header,
            "a rejected packet must retain lower-level IN endpoint busy/stall "
            "evidence instead of labelling every rejection as host backpressure")
    require("txPacer_.waitMicros" in write_packet and
            write_packet.index("txPacer_.waitMicros") <
            write_packet.index("tud_midi_packet_write") and
            "kPacketSpacingMicros = 1000" in transport_header,
            "the sole physical writer must pace USB bursts at DIN-compatible "
            "packet spacing before touching the TinyUSB FIFO")
    require("g_endpointHealth.observeWrite(mounted, accepted, nowMs)" in transport,
            "the logging wrapper must forward the outcome unchanged")
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
