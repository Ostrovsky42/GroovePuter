#!/usr/bin/env python3
from pathlib import Path

ROOT = Path.cwd()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    visual = (ROOT / "src/midi/smf_midi_visual.h").read_text(encoding="utf-8")
    service = (ROOT / "src/platform/cardputer_smf_player.cpp").read_text(encoding="utf-8")
    page = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/ui/pages/smf_player_page.h").read_text(encoding="utf-8")

    require("kCapacity = 16" in visual and "std::array" in visual,
            "MIDI visual events must use bounded fixed storage")
    require("std::vector" not in visual and "new " not in visual and "malloc(" not in visual,
            "MIDI visual path must remain allocation-free")
    require("midiVisualTimeline_.queue" in service and
            "snapshot_.midiVisual" in service,
            "SMF service must publish actual accepted NoteOn activity")
    require("drawMidiWaveOverlay" in page and
            page.index("MusicVisuals::drawProgressBar") < page.index("drawMidiWaveOverlay"),
            "wave overlay must be drawn over the current MIDI progress track")
    require("midiWaveEnvelope_" in header and "midiWavePhase_" in header,
            "animation state must stay local to the UI page")
    require("TinyUSB" not in page and "USBMIDI" not in page,
            "MIDI Player UI must not become a USB owner")
    print("SMF MIDI wave source regressions: OK")


if __name__ == "__main__":
    main()
