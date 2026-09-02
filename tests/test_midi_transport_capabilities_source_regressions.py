#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TRANSPORT = (ROOT / "src/midi/usb_midi_transport.h").read_text(encoding="utf-8")
CAPABILITIES = (ROOT / "src/midi/midi_transport_capabilities.h").read_text(encoding="utf-8")


def require(source: str, token: str, description: str) -> None:
    if token not in source:
        raise AssertionError(f"missing {description}: {token}")


def main() -> None:
    require(TRANSPORT, "virtual bool sendContinue()", "optional Continue transport method")
    require(
        TRANSPORT,
        "virtual bool sendSongPositionPointer(uint16_t midiBeats)",
        "optional Song Position Pointer method",
    )
    require(CAPABILITIES, "struct MidiTransportCapabilities", "capability model")
    require(CAPABILITIES, "MidiContinueBehavior", "Continue behavior model")
    require(CAPABILITIES, "songPositionPointerTx", "SPP TX capability")
    require(CAPABILITIES, "clampSongPositionPointer", "14-bit SPP clamp")
    require(CAPABILITIES, "0x3FFFu", "14-bit SPP maximum")
    require(CAPABILITIES, "& 0x7Fu", "SPP LSB encoding")
    require(CAPABILITIES, ">> 7", "SPP MSB encoding")

    seqtrak_start = CAPABILITIES.index(
        "constexpr MidiTransportCapabilities seqtrakValidatedTransportCapabilities()"
    )
    seqtrak_end = CAPABILITIES.index(
        "constexpr uint16_t clampSongPositionPointer", seqtrak_start
    )
    seqtrak_profile = CAPABILITIES[seqtrak_start:seqtrak_end]
    if "capabilities.continueTx = true" in seqtrak_profile:
        raise AssertionError("SEQTRAK Continue TX must remain disabled before hardware validation")
    if "capabilities.songPositionTx = true" in seqtrak_profile:
        raise AssertionError("SEQTRAK SPP TX must remain disabled before hardware validation")

    print("midi transport capability source regressions: PASS")


if __name__ == "__main__":
    main()
