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


stop = between(
    ENGINE,
    "void MiniAcid::stop()",
    "void MiniAcid::pauseTransport()",
)
pause = between(
    ENGINE,
    "void MiniAcid::pauseTransport()",
    "void MiniAcid::continueTransport()",
)

# RED/GREEN #2: STOP/PAUSE may still perform independently owned live cleanup,
# but they may not make a second Pattern lifetime decision. The physical
# authority used for live-vs-Pattern arbitration must be captured once before
# Pattern barrier side effects and must not be reconstructed from the mutated
# post-barrier patternOwnedMask_.
for name, body in (("STOP", stop), ("PAUSE", pause)):
    capture = "const uint8_t patternAuthorityAtEntry ="
    barrier_call = "hardBarrierPatternPlayback_();"
    live_cleanup = "cleanupLiveNotesForTransportBarrier_(patternAuthorityAtEntry);"

    require(capture in body, f"RED #2: {name} does not capture barrier-entry Pattern authority")
    require(
        "patternOwnedMask_.load(std::memory_order_acquire)" in body,
        f"RED #2: {name} authority snapshot is not captured from the pre-barrier physical owner mask",
    )
    require(
        barrier_call in body,
        f"RED #2: {name} does not route Pattern cleanup through RuntimeSynthPlaybackState",
    )
    require(
        live_cleanup in body,
        f"RED #2: {name} does not route live cleanup through barrier-entry authority",
    )
    require(
        body.index(capture) < body.index(barrier_call) < body.index(live_cleanup),
        f"RED #2: {name} mutates Pattern authority before capturing live-cleanup arbitration state",
    )

    for forbidden in (
        "publishPatternAllNotesOff_()",
        "publishPatternNoteOff_(",
        "synthVoices_[0]->release()",
        "synthVoices_[1]->release()",
        "patternOwnsInternalSynth(",
    ):
        require(
            forbidden not in body,
            f"RED #2: {name} retains a direct lifetime/backend decision outside the owners: {forbidden}",
        )

    # Compatibility countdown fields may still be cleared to zero in this
    # slice. They may not decide, extend, decrement, or trigger lifetime.
    for forbidden in (
        "if (gateCountdownA_",
        "if (gateCountdownB_",
        "--gateCountdownA_",
        "--gateCountdownB_",
        "gateCountdownA_ +=",
        "gateCountdownB_ +=",
        "gateCountdownA_ >",
        "gateCountdownB_ >",
    ):
        require(
            forbidden not in body,
            f"RED #2: {name} still gives gateCountdown lifetime authority: {forbidden}",
        )

require(
    "void hardBarrierPatternPlayback_();" in HEADER,
    "RED #2: MiniAcid has no common Pattern hard-barrier helper",
)
require(
    "void cleanupLiveNotesForTransportBarrier_(uint8_t patternAuthorityAtEntry);" in HEADER,
    "RED #2: MiniAcid has no barrier-local live cleanup seam",
)
require(
    "void MiniAcid::hardBarrierPatternPlayback_()" in ENGINE,
    "RED #2: common Pattern hard barrier is declared but not implemented",
)
require(
    "void MiniAcid::cleanupLiveNotesForTransportBarrier_(" in ENGINE,
    "RED #2: barrier-local live cleanup is declared but not implemented",
)

target_barrier = between(
    ENGINE,
    "void MiniAcid::hardBarrierPatternPlayback_(int synthIdx)",
    "void MiniAcid::hardBarrierPatternPlayback_()",
)
require(
    "patternPlaybackState_[synthIdx].hardBarrier()" in target_barrier,
    "RED #2: target Pattern hard barrier does not ask RuntimeSynthPlaybackState for Release",
)
require(
    "consumePatternPlaybackActions_(synthIdx" in target_barrier,
    "RED #2: target Pattern hard barrier bypasses common action translation",
)

barrier = between(
    ENGINE,
    "void MiniAcid::hardBarrierPatternPlayback_()",
    "void MiniAcid::cleanupLiveNotesForTransportBarrier_(",
)
require(
    "hardBarrierPatternPlayback_(synth)" in barrier,
    "RED #2: all-target Pattern hard barrier does not delegate to target owner barrier",
)
for forbidden in (
    "publishPatternAllNotesOff_()",
    "publishPatternNoteOff_(",
    "synthVoices_[",
    "gateCountdownA_",
    "gateCountdownB_",
    "patternOwnedMask_",
):
    require(
        forbidden not in target_barrier + barrier,
        f"RED #2: Pattern hard barrier retained a parallel backend/lifetime decision: {forbidden}",
    )

cleanup_start = ENGINE.index("void MiniAcid::cleanupLiveNotesForTransportBarrier_(")
cleanup_end = ENGINE.index("void MiniAcid::", cleanup_start + 1)
cleanup = ENGINE[cleanup_start:cleanup_end]
require(
    "patternAuthorityAtEntry" in cleanup,
    "RED #2: live cleanup ignores barrier-entry authority",
)
require(
    "liveNotes_[idx]" in cleanup,
    "RED #2: live cleanup no longer preserves the existing live-note owner",
)
require(
    "synthVoices_[idx]->release()" in cleanup,
    "RED #2: independently owned live note no longer receives physical cleanup",
)
for forbidden in (
    "patternOwnedMask_",
    "patternOwnsInternalSynth(",
    "publishPatternNoteOff_(",
    "publishPatternAllNotesOff_()",
    "patternPlaybackState_",
):
    require(
        forbidden not in cleanup,
        f"RED #2: live cleanup re-reads/mutates Pattern ownership after the barrier: {forbidden}",
    )

consumer_start = ENGINE.index("void MiniAcid::consumePatternPlaybackActions_(")
consumer_end = ENGINE.index("uint32_t MiniAcid::currentAbsoluteSubtick_", consumer_start)
consumer = ENGINE[consumer_start:consumer_end]
require(
    "publishPatternNoteOff_(idx)" in consumer or "publishPatternNoteOff_(idx," in consumer,
    "RED #2: Runtime Release is not translated target-scoped to Pattern MIDI NoteOff",
)
require(
    "publishPatternAllNotesOff_()" not in consumer,
    "RED #2: ordinary Runtime Release is translated through global Pattern panic",
)

print("P2 STOP/PAUSE single Pattern lifetime owner contract: OK")
