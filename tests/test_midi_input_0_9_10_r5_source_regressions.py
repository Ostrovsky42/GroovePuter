#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
router = (ROOT / "src/input/midi_input_router.h").read_text(encoding="utf-8")
transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(encoding="utf-8")

for token in (
    "releaseSession(MidiInputTransportId transportId",
    "owner.transportId != transportId",
    "owner.sessionId != sessionId",
    "sessionCleanups",
    "sessionNotesReleased",
):
    assert token in router, token

assert "g_midiInputRouter->releaseSession(" in transport
assert "kCardputerUsbInputTransportId" in transport
assert "g_midiInputSession" in transport
reset_start = transport.index("void resetMidiInputSession()")
reset_end = transport.index("void serviceMidiInputConnection()", reset_start)
reset_body = transport[reset_start:reset_end]
assert "discardPendingFromConsumer()" in reset_body
assert "releaseSession(" in reset_body
assert "->panic()" not in reset_body, "USB disconnect must not panic unrelated transport/session owners"

for forbidden in ("std::vector", "std::map", "std::unordered_map", "new ", "malloc("):
    assert forbidden not in router, forbidden

print("0.9.10 R5 lifecycle/source boundaries: PASS")
