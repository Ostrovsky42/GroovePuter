#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    settings_h = (ROOT / "src/midi/midi_companion_settings.h").read_text(encoding="utf-8")
    settings_cpp = (ROOT / "src/midi/midi_companion_settings.cpp").read_text(encoding="utf-8")
    codec_h = (ROOT / "src/midi/midi_companion_settings_codec.h").read_text(encoding="utf-8")
    codec_cpp = (ROOT / "src/midi/midi_companion_settings_codec.cpp").read_text(encoding="utf-8")

    combined = settings_h + settings_cpp + codec_h + codec_cpp

    require("MidiDeviceProfile" in settings_h and "SeqtrakNative" in settings_h,
            "foundation must define a SEQTRAK-native device profile")
    require("GeneralMidi" in settings_h and "Custom" in settings_h,
            "foundation must retain generic and user-editable profiles")
    require("DrumMidiRoute" in settings_h and "kMidiDrumVoiceCount = 8" in settings_h,
            "all eight GroovePuter drum voices need explicit routes")
    require("MidiLiveTarget" in settings_h,
            "live Synth A/B selection must be represented without UI coupling")
    require("kSchemaVersion = 1" in codec_h and "kEncodedSize = 44" in codec_h,
            "global settings persistence must be fixed-size and versioned")
    require("crc32" in codec_cpp and "'G', 'P', 'M', 'D'" in codec_cpp,
            "settings blobs need an integrity check and stable magic")
    require("DefaultsFromMissing" in codec_h and "DefaultsFromCorrupt" in codec_h,
            "missing/corrupt global settings must resolve to explicit defaults")
    require("StorageError" in codec_h,
            "transient storage errors must remain distinguishable from missing data")

    for forbidden in (
        "TinyUSB", "USBMIDI", "M5Cardputer", "M5Unified", "Arduino.h",
        "MiniAcid", "MusicalEventRouter", "scenes.h", "scenes.cpp",
    ):
        require(forbidden not in combined,
                f"pure companion foundation must not depend on runtime/platform code: {forbidden}")

    require("std::vector" not in combined and "new " not in combined and "malloc(" not in combined,
            "foundation settings and codec must remain bounded and allocation-free")

    print("MIDI companion foundation source regressions: OK")


if __name__ == "__main__":
    main()
