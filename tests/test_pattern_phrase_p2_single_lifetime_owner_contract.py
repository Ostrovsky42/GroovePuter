#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin + len(start))
    return text[begin:finish]


stop = between(ENGINE,
               "void MiniAcid::stop()",
               "void MiniAcid::pauseTransport()")
pause = between(ENGINE,
                "void MiniAcid::pauseTransport()",
                "void MiniAcid::continueTransport()")

# RED #2: transport hard barriers must not independently decide Pattern
# backend lifetime. RuntimeSynthPlaybackState is the only logical lifetime
# owner; STOP/PAUSE may request a hard barrier, but backend fanout must consume
# the owner's returned Release action exactly once.
for name, body in (("STOP", stop), ("PAUSE", pause)):
    for forbidden in (
        "publishPatternAllNotesOff_()",
        "synthVoices_[0]->release()",
        "synthVoices_[1]->release()",
        "gateCountdownA_",
        "gateCountdownB_",
    ):
        require(
            forbidden not in body,
            f"RED #2: {name} still owns Pattern lifetime outside RuntimeSynthPlaybackState: {forbidden}",
        )
    require(
        "hardBarrierPatternPlayback_()" in body,
        f"RED #2: {name} does not route Pattern cleanup through the common hard-barrier path",
    )

require(
    "void hardBarrierPatternPlayback_();" in HEADER,
    "RED #2: MiniAcid has no single common Pattern hard-barrier helper",
)
require(
    "void MiniAcid::hardBarrierPatternPlayback_()" in ENGINE,
    "RED #2: common Pattern hard-barrier helper is declared but not implemented",
)

barrier = between(ENGINE,
                  "void MiniAcid::hardBarrierPatternPlayback_()",
                  "void MiniAcid::")
require(
    "patternPlaybackState_[synth].hardBarrier()" in barrier,
    "RED #2: common hard barrier does not ask RuntimeSynthPlaybackState for the Release decision",
)
require(
    "consumePatternPlaybackActions_" in barrier,
    "RED #2: common hard barrier bypasses internal/MIDI common fanout",
)
for forbidden in (
    "publishPatternAllNotesOff_()",
    "synthVoices_[0]->release()",
    "synthVoices_[1]->release()",
    "gateCountdownA_",
    "gateCountdownB_",
):
    require(
        forbidden not in barrier,
        f"RED #2: common hard barrier retained a parallel backend lifetime decision: {forbidden}",
    )

print("P2 single lifetime owner hard-barrier contract: OK")
