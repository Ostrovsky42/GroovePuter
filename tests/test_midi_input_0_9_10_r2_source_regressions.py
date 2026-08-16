#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MESSAGE = ROOT / "src/input/midi_input_message.h"
QUEUE = ROOT / "src/input/midi_input_queue.h"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    message = MESSAGE.read_text(encoding="utf-8")
    queue = QUEUE.read_text(encoding="utf-8")

    # Core ingress must remain transport-independent. Comments may name future
    # adapters, but production include dependencies may not.
    includes = "\n".join(
        line for line in (message + "\n" + queue).splitlines()
        if line.lstrip().startswith("#include")
    )
    for forbidden in (
        "Arduino", "TinyUSB", "USBMIDI", "tusb", "FreeRTOS",
        "Bluetooth", "BLE", "HardwareSerial", "Preferences", "NVS",
        "seqtrak", "ui/",
    ):
        require(forbidden not in includes,
                f"R2 core ingress must not include transport/UI dependency: {forbidden}")

    require("using MidiInputTransportId = uint8_t" in message,
            "transport identity must remain opaque and compact")
    require("using MidiInputSessionId = uint16_t" in message,
            "session identity must remain explicit")
    require("timestampMicros" in message,
            "normalized input must preserve arrival timestamp for later recording")
    require("status < 0x80u || status >= 0xF0u" in message,
            "System Common/Realtime must remain outside channel-voice normalization")
    require("data2 == 0u" in message and "MidiInputMessageType::NoteOff" in message,
            "zero-velocity NoteOn must normalize to NoteOff")
    require("sizeof(NormalizedMidiInputMessage) == 12u" in message,
            "R2 message size contract must remain explicit")

    require("kStorageSize = 64u" in queue and
            "kCapacity = kStorageSize - 1u" in queue,
            "R2 ingress queue must remain fixed and bounded")
    require("MidiRealtimeWord head_" in queue and "MidiRealtimeWord tail_" in queue,
            "R2 queue must retain SPSC acquire/release ownership")
    require("overflowEpoch_" in queue and "discardPendingFromConsumer" in queue,
            "overflow recovery signal must exist before live NoteOff routing")
    require("droppedOverflow_" in queue and "highWaterMark_" in queue,
            "R2 must expose bounded queue diagnostics")
    for forbidden in ("std::vector", "std::deque", "std::list", "new ", "malloc(", "calloc("):
        require(forbidden not in queue,
                f"R2 ingress queue must not allocate dynamically: {forbidden}")

    # R2 defines contracts only. No existing production source may instantiate
    # or route through them yet; adapter/router integration starts in later R3.
    references = []
    for path in (ROOT / "src").rglob("*"):
        if not path.is_file() or path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
            continue
        if path in {MESSAGE, QUEUE}:
            continue
        text = path.read_text(encoding="utf-8")
        if "MidiInputQueue" in text or "NormalizedMidiInputMessage" in text:
            references.append(path.relative_to(ROOT).as_posix())
    require(not references,
            "R2 must not add runtime ingress integration yet: " + ", ".join(references))

    print("0.9.10 R2 source/ownership boundaries: PASS")


if __name__ == "__main__":
    main()
