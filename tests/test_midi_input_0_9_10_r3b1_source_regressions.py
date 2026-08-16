#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARSER = ROOT / "src/midi/usb_midi_channel_voice_parser.h"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    parser = PARSER.read_text(encoding="utf-8")
    includes = "\n".join(
        line for line in parser.splitlines()
        if line.lstrip().startswith("#include")
    )

    for forbidden in (
        "Arduino", "TinyUSB", "tusb", "FreeRTOS", "HardwareSerial",
        "cardputer", "usb_midi_output", "output_ownership", "ui/",
    ):
        require(forbidden not in includes,
                f"USB packet parser must remain pure/platform-independent: {forbidden}")

    require("cin < 0x08u || cin > 0x0Eu" in parser,
            "channel-voice parser must reject system/realtime CIN classes")
    require("statusClass != static_cast<uint8_t>(cin << 4u)" in parser,
            "USB CIN must agree with MIDI status class")
    require("cin == 0x0Cu || cin == 0x0Du" in parser,
            "Program/Channel Pressure one-data-byte framing must remain explicit")
    require("NormalizedMidiInputMessage::fromMidi1ChannelVoice" in parser,
            "USB adapter must reuse canonical R2 normalization")

    later_runtime = (ROOT / "docs/releases/0_9_10_R3B2_USB_RUNTIME_WIRING.md").exists()
    if not later_runtime:
        refs = []
        for path in (ROOT / "src").rglob("*"):
            if not path.is_file() or path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
                continue
            if path == PARSER:
                continue
            text = path.read_text(encoding="utf-8")
            if ("parseUsbMidiChannelVoice" in text or
                    "usb_midi_channel_voice_parser.h" in text):
                refs.append(path.relative_to(ROOT).as_posix())
        require(not refs,
                "R3b1 must not wire channel-voice parser into runtime yet: " + ", ".join(refs))

    transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")
    require("configASSERT(xTaskGetCurrentTaskHandle() == g_dispatchTaskHandle);" in transport,
            "existing TinyUSB MIDI FIFO must remain dispatch-task owned")
    require("tud_midi_rx_cb" not in transport,
            "R3b1 must not introduce a second callback RX owner")

    print("0.9.10 R3b1 USB MIDI parser/source boundaries: PASS")


if __name__ == "__main__":
    main()
