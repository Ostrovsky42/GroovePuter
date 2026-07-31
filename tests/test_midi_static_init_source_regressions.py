#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    header = (ROOT / "src/midi/usb_midi_output.h").read_text(encoding="utf-8")
    source = (ROOT / "src/midi/usb_midi_output.cpp").read_text(encoding="utf-8")

    ctor_start = source.index("UsbMidiOutput::UsbMidiOutput")
    configure_start = source.index("void UsbMidiOutput::configureLanes", ctor_start)
    ctor = source[ctor_start:configure_start]

    require("lanes_[" not in ctor,
            "global UsbMidiOutput constructor must not initialize routing lanes")
    require("wireOwners_[" not in ctor,
            "global UsbMidiOutput constructor must not clear wire ownership")
    require("configureLanes();" in source[source.index("bool UsbMidiOutput::begin"):],
            "routing lanes must be configured from begin() after Arduino startup")
    require("configureLanes();\n    clearActiveState();\n    begun_ = transport_.begin();" in source,
            "begin() must configure/clear routing before starting MIDI transport")
    require("MidiVoiceLane lanes_[kLaneCount];" in header,
            "lane storage must remain passive static storage before begin()")
    require("wireOwners_[kMidiChannelCount][kMidiNoteCount];" in header,
            "wire-owner storage must remain passive static storage before begin()")
    require("if (!begun_) return;\n    pollConnection();" in source,
            "events arriving before begin() must fail closed without reading lanes")

    print("MIDI static-init source regressions: OK")


if __name__ == "__main__":
    main()
