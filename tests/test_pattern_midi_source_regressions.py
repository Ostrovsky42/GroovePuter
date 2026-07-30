#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    event_header = (ROOT / "src/input/musical_event.h").read_text(encoding="utf-8")
    sink = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")
    sink_h = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8")
    transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    internal = (ROOT / "src/input/internal_synth_output.cpp").read_text(encoding="utf-8")
    queue = (ROOT / "src/input/musical_event_queue.h").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")
    upload = (ROOT / "scripts/upload.sh").read_text(encoding="utf-8")
    scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("MidiOutput" not in event_header,
            "USB MIDI must remain an output sink, not a MusicalEventTarget")
    require("MusicalEventSource::PerformanceKeyboard" in sink and
            "MusicalEventSource::PatternPlayer" in sink,
            "USB sink must expose separate live and PatternPlayer lanes")
    require("MusicalEventTarget::SynthA" in sink and
            "MusicalEventTarget::SynthB" in sink,
            "Stage 1 must route both synth targets")
    require("kLaneCount = 3" in sink_h,
            "Stage 1 must keep exactly three fixed ownership lanes")
    require("patternSynthAChannel{7}" in sink_h and
            "patternSynthBChannel{8}" in sink_h,
            "fixed Stage 1 routes must be MIDI channels 8 and 9")

    require("publishPatternNoteOn_" in engine and
            "publishPatternNoteOff_" in engine and
            "publishPatternAllNotesOff_" in engine,
            "PatternPlayer lifecycle must publish normalized events")
    require("MusicalEventSource::PatternPlayer" in engine,
            "engine events must identify PatternPlayer as their source")
    require("USBMIDI" not in engine and "TinyUSB" not in engine,
            "DSP engine must not depend on hardware USB APIs")
    require("event.source != MusicalEventSource::PerformanceKeyboard" in internal,
            "internal live sink must ignore PatternPlayer fan-out without locking audio")

    require("std::atomic" in queue and "kCapacity = 64" in queue,
            "audio-to-control handoff must be fixed and lock-free")
    for token in ("std::vector", "std::deque", "new ", "malloc("):
        require(token not in queue, f"realtime queue must not allocate: {token}")
    require("takePendingAllNotesOffMask" in queue,
            "critical queue overflow must degrade to a target-scoped panic")

    setup_start = sketch.index("void setup()")
    loop_start = sketch.index("void loop()")
    require("setPatternEventQueue(&g_patternMusicalEventQueue)" in sketch[setup_start:loop_start],
            "setup must connect PatternPlayer to the fixed event queue")
    require("drainPatternMusicalEvents();" in sketch[loop_start:],
            "control loop must drain PatternPlayer events into the shared router")
    require("g_musicalEventRouter.route(event)" in sketch,
            "queued PatternPlayer events must use MusicalEventRouter")

    require("router.addSink(g_output)" in transport,
            "USB MIDI must remain an independent router sink")
    require("UsbMidiRouteConfig" in transport and "7," in transport and "8," in transport,
            "platform route config must retain channels 8/9")
    require("USBMIDI.h" not in sketch and "USB.h" not in sketch,
            "TinyUSB headers must stay isolated from the main UI translation unit")

    tinyusb_options = "USBMode=default,CDCOnBoot=cdc,UploadMode=cdc"
    require(tinyusb_options in build and tinyusb_options in upload,
            "build and upload must use the same pinned TinyUSB FQBN")
    require("#if ARDUINO_USB_MODE" in transport,
            "hardware transport must fail closed outside TinyUSB OTG mode")

    forbidden_tokens = (
        "controlChange(", "programChange(", "pitchBend(", "SysEx",
        "MIDI_CLOCK", "MIDI_START", "MIDI_STOP",
    )
    for token in forbidden_tokens:
        require(token not in transport and token not in sink and token not in engine,
                f"out-of-scope MIDI feature entered Stage 1: {token}")

    require("UsbMidi" not in scenes_h and "UsbMidi" not in scenes_cpp,
            "USB MIDI settings must remain runtime-only in Stage 1")
    require("usbMidi" not in scenes_h and "usbMidi" not in scenes_cpp,
            "scene schema must not gain USB MIDI fields")

    print("Pattern MIDI source regressions: OK")


if __name__ == "__main__":
    main()
