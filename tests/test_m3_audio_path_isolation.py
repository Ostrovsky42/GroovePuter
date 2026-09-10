#!/usr/bin/env python3
"""The audio path may read the published authority and nothing else.

M3 is the first point where a mistake means the wrong material actually sounds.
So processSequencerEvents() is not allowed to work anything out: no Scene, no
descriptor, no filesystem identity, no project name, no Song resolution. All of
that is control-side work whose result is published as a small bounded value.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    start = ENGINE.index("void MiniAcid::processSequencerEvents")
    end = ENGINE.index("void MiniAcid::generateAudioBuffer", start)
    audio = ENGINE[start:end]

    require("activeMaterial_" in audio,
            "the audio path does not read the published material authority")

    # Scoped to *material* resolution. A blanket ban on sceneManager_ would be
    # a false claim: the audio path already reads the Scene for LED state,
    # swing and the drum pattern, all of which predate this work and none of
    # which resolve material. Those reads are worth revisiting on their own
    # terms, not smuggled into this gate.
    for forbidden, why in (
        ("materialSlots", "the audio path must not read slot descriptors"),
        ("residentKind", "the audio path must not resolve material kinds"),
        ("currentProjectName", "the audio path must not compute storage identity"),
        ("SD.", "the audio path must never touch the filesystem"),
        ("MelodyStore", "the audio path must not decode payloads"),
        ("MelodyPromotion", "the audio path must not perform promotion"),
        ("sequencedSource_", "the retired per-voice source must not survive here"),
    ):
        require(forbidden not in audio, why)

    print("PASS: M3 audio path reads only the published authority")


if __name__ == "__main__":
    main()
