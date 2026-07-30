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
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    build = (ROOT / "scripts/build.sh").read_text(encoding="utf-8")
    upload = (ROOT / "scripts/upload.sh").read_text(encoding="utf-8")
    scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("MidiOutput" not in event_header,
            "USB MIDI must remain an output sink, not a MusicalEventTarget")
    require("event.source == MusicalEventSource::PerformanceKeyboard" in sink,
            "first USB spike must explicitly accept only PerformanceKeyboard events")
    require("event.target == MusicalEventTarget::SynthA" in sink,
            "first USB spike must explicitly accept only SynthA events")
    require("MusicalEventSource::PatternPlayer" not in sink,
            "PatternPlayer USB routing belongs to a later PR")
    require("AudioMutationGate" not in sink and "AudioMutationGate" not in transport,
            "USB output must not pause or mutate the audio renderer")

    setup_start = sketch.index("void setup()")
    registration_call = "  registerCardputerUsbMidiSink(g_musicalEventRouter);"
    require(registration_call in sketch[setup_start:],
            "setup must connect the shared router to the platform sink")
    require(registration_call not in sketch[:setup_start],
            "USB sink registration must not run from global initialization")
    require("g_musicalEventRouter.addSink(g_internalSynthOutput)" in sketch and
            sketch.index("g_musicalEventRouter.addSink(g_internalSynthOutput)") <
            sketch.index(registration_call, setup_start),
            "USB sink registration must follow engine/internal sink initialization")
    require("router.addSink(g_output)" in transport,
            "USB MIDI must be registered as an independent router sink")
    require("UsbMidiRouteConfig" in transport and "7," in transport,
            "SynthA must route to zero-based channel 7 / MIDI channel 8")
    require("USBMIDI.h" not in sketch and "USB.h" not in sketch,
            "TinyUSB headers must stay isolated from the main UI translation unit")
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
                f"out-of-scope MIDI feature entered first spike: {token}")

    require("UsbMidi" not in scenes_h and "UsbMidi" not in scenes_cpp,
            "USB MIDI settings must remain runtime-only in this PR")
    require("usbMidi" not in scenes_h and "usbMidi" not in scenes_cpp,
            "scene schema must not gain USB MIDI fields")

    print("USB MIDI source regressions: OK")


if __name__ == "__main__":
    main()
