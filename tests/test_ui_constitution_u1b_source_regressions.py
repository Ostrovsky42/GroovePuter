#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATUS = (ROOT / "src/ui/ui_status_chrome.h").read_text(encoding="utf-8")
COMMON = (ROOT / "src/ui/ui_common.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    # U1B separates the target's actual sequenced source from transport owner.
    require("enum class UiSequencedSource" in STATUS,
            "status model has no typed per-target sequenced-source axis")
    require("enum class UiTransportOwner" in STATUS,
            "status model has no typed transport-owner axis")
    require("struct UiStatusRouting" in STATUS,
            "status model does not pack the two semantic axes into bounded storage")
    require("UiStatusSource" not in STATUS,
            "old conflated source/transport status enum still exists")

    # Synth source truth must come from the authoritative engine-owned P3 source.
    require("currentSequencedSource(0)" in COMMON,
            "Synth A status does not read authoritative engine SequencedSource")
    require("currentSequencedSource(1)" in COMMON,
            "Synth B status does not read authoritative engine SequencedSource")

    # Song mode and SMF state are transport/playback ownership, not PAT/PHR.
    require("UiTransportOwner::Song" in COMMON,
            "Song mode is not represented as transport ownership")
    require("UiTransportOwner::Smf" in COMMON,
            "SMF ownership is not represented as transport ownership")
    require("status.source = miniAcid.songModeEnabled()" not in COMMON,
            "status still derives source from Song mode")
    require("UiStatusSource::" not in COMMON,
            "ui_common still uses the old conflated status-source mechanism")

    # Pattern address is valid only when this target is actually Pattern-backed.
    require("UiSequencedSource::Pattern" in COMMON,
            "Pattern-address projection is not guarded by target sequenced source")


if __name__ == "__main__":
    main()
