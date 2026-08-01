#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    transport = (ROOT / "src/platform/cardputer_usb_midi_transport.cpp").read_text(
        encoding="utf-8"
    )
    transport_h = (ROOT / "src/platform/cardputer_usb_midi_transport.h").read_text(
        encoding="utf-8"
    )
    player = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(
        encoding="utf-8"
    )
    parser = (ROOT / "src/midi/usb_midi_realtime_parser.h").read_text(
        encoding="utf-8"
    )

    require("midi_.readPacket(&packet)" in transport,
            "the sole platform transport must own TinyUSB RX")
    require("readPacket(midiEventPacket_t& packet)" in transport_h,
            "the Cardputer transport must expose bounded packet polling")
    require("kMidiRxDrainBudget = 32" in transport and
            "drainIncomingMidiPackets()" in transport,
            "MidiDispatchTask must bound every RX drain pass")
    require(all(status in parser for status in ("0xf8", "0xfa", "0xfb", "0xfc")),
            "the pure parser must recognize Clock/Start/Continue/Stop")
    require("g_externalMidiTransportQueue" in sketch and
            "g_externalClockFollower.processBlock" in sketch,
            "AudioTask must consume the external transport queue")
    require(sketch.index("g_externalClockFollower.processBlock") <
            sketch.index("beginMidiRenderBlock"),
            "external transport must apply at the audio block boundary")
    require("transportClockSourcePublishesOutboundClock" in sketch and
            "outboundTransportSuppressed" in transport,
            "SEQ MASTER needs producer and physical-write echo suppression")
    require("event.key == 'c' || event.key == 'C'" in player and
            "transportClockRuntime().toggleSource()" in player,
            "MIDI Player must expose an explicit master-source control")
    require("USBMIDI" not in sketch and "tud_midi" not in sketch and
            "USBMIDI" not in player and "tud_midi" not in player,
            "USB access must stay out of AudioTask and UI")

    print("seqtrak master source regressions: OK")


if __name__ == "__main__":
    main()
