#!/usr/bin/env python3
"""The MIDI probe is a measuring instrument, so its verdict is itself tested.

Feeds synthetic aseqdump output through the same parser and analyzer the live
capture uses, so a broken heuristic fails here instead of misleading a hardware
debugging session.
"""
import importlib.util
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# Importing a script leaves a __pycache__ next to it; the repo is not set up to
# ignore one under scripts/.
sys.dont_write_bytecode = True

spec = importlib.util.spec_from_file_location(
    "midi_probe", ROOT / "scripts/midi_probe.py")
assert spec and spec.loader
probe = importlib.util.module_from_spec(spec)
# dataclasses resolves annotations through sys.modules, so register before exec.
sys.modules["midi_probe"] = probe
spec.loader.exec_module(probe)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def events(lines: list[tuple[float, str]]) -> list:
    parsed = []
    for at, line in lines:
        event = probe.parse_line(line, at)
        require(event is not None, f"parser rejected a real aseqdump line: {line!r}")
        parsed.append(event)
    return parsed


def main() -> None:
    # Parsing the exact aseqdump wire format.
    note_on = probe.parse_line(
        " 24:0   Note on                 9, note 36, velocity 100", 0.0)
    require(note_on.kind == "Note on" and note_on.channel == 9 and
            note_on.note == 36 and note_on.velocity == 100,
            "aseqdump Note on must parse into channel/note/velocity")
    require(probe.parse_line(" 24:0   Clock", 0.0).kind == "Clock",
            "realtime Clock lines must parse")
    require(probe.parse_line(" Port    Client name", 0.0) is None,
            "the aseqdump header must not be treated as an event")

    # A NoteOn with velocity 0 is a NoteOff and must not look like a stuck note.
    balanced = events([
        (0.0, " 24:0   Note on                 0, note 60, velocity 100"),
        (0.2, " 24:0   Note on                 0, note 60, velocity 0"),
    ])
    verdict = probe.analyze(balanced)
    require(verdict.ok and not verdict.stuck,
            "running-status NoteOff (velocity 0) must close the note")

    # A NoteOn without its NoteOff is the signature of a cleanup/ownership bug.
    stuck = events([
        (0.0, " 24:0   Note on                 3, note 64, velocity 90"),
        (0.1, " 24:0   Note on                 3, note 67, velocity 90"),
        (0.2, " 24:0   Note off                3, note 67, velocity 0"),
    ])
    verdict = probe.analyze(stuck, capture_end=1.0)
    require(verdict.stuck == [(3, 64)], "an unmatched NoteOn must be reported")
    require(not verdict.ok and "stuck" in verdict.failures[0],
            "stuck notes must fail the verdict")

    # A capture stopped during playback has open notes at its boundary, but that
    # is not evidence that firmware lost their NoteOffs.
    active_boundary = events([
        (0.0, " 24:0   Note on                 3, note 64, velocity 90"),
        (0.1, " 24:0   Note on                 3, note 67, velocity 90"),
    ])
    verdict = probe.analyze(active_boundary, capture_end=0.11)
    require(not verdict.stuck and verdict.active_at_end == [(3, 64), (3, 67)],
            "active notes at a live capture boundary must not be called stuck")
    require(not verdict.ok and any("capture ended" in failure
                                   for failure in verdict.failures),
            "an active capture boundary must remain an incomplete release gate")

    # Silence longer than the threshold is the stutter/stall signature.
    gapped = events([
        (0.0, " 24:0   Note on                 0, note 60, velocity 100"),
        (0.05, " 24:0   Note off                0, note 60, velocity 0"),
        (1.30, " 24:0   Note on                 0, note 62, velocity 100"),
        (1.35, " 24:0   Note off                0, note 62, velocity 0"),
    ])
    verdict = probe.analyze(gapped, gap_ms=250.0)
    require(len(verdict.long_gaps) == 1 and verdict.max_gap_ms > 1200,
            "a 1.25 s silence must be reported as one long gap")
    require(not verdict.ok, "a long gap must fail the verdict")
    require(probe.analyze(gapped, gap_ms=2000.0).ok,
            "a raised threshold must accept the same capture")

    # Steady 24 PPQN clock at 120 BPM is 20.833 ms per pulse.
    steady = events([
        (index * 0.0208333, " 24:0   Clock") for index in range(48)
    ])
    verdict = probe.analyze(steady)
    require(verdict.clock_count == 48, "every clock pulse must be counted")
    require(abs(verdict.implied_bpm - 120.0) < 1.0,
            f"24 PPQN at 20.83 ms must imply ~120 BPM, got {verdict.implied_bpm}")
    require(verdict.ok, "a steady clock must pass")

    # A single displaced pulse is drift the ear notices before the eye does.
    jittery = [event for event in steady]
    jittery[24].at += 0.030
    verdict = probe.analyze(jittery, clock_dev_ms=5.0)
    require(not verdict.ok and any("clock" in failure for failure in
                                   verdict.failures),
            "a 30 ms clock displacement must fail the verdict")

    # Slow music is not a stall: a fixed millisecond threshold would fail every
    # sparse track, so the threshold scales with the median note spacing.
    sparse = []
    for index in range(12):
        at = index * 0.5
        sparse.append((at, " 24:0   Note on                 0, note 60, velocity 100"))
        sparse.append((at + 0.25, " 24:0   Note off                0, note 60, velocity 0"))
    verdict = probe.analyze(events(sparse))
    require(verdict.ok, "half-second note spacing must not be reported as a gap")
    require(verdict.gap_threshold_ms > 250.0,
            "the gap threshold must adapt upward for slow material")

    # The same slow material with a real hole in it must still fail.
    stalled = [entry for entry in sparse if entry[0] < 2.0]
    stalled += [(at + 5.0, line) for at, line in sparse if at >= 2.0]
    verdict = probe.analyze(events(stalled))
    require(not verdict.ok and verdict.long_gaps,
            "a 3 s hole in slow material must still be reported")

    # Routing check: SEQTRAK mapping is verified by which channels carry notes.
    routed = events([
        (0.0, " 24:0   Note on                 9, note 36, velocity 100"),
        (0.1, " 24:0   Note off                9, note 36, velocity 0"),
        (0.2, " 24:0   Note on                 9, note 38, velocity 100"),
        (0.3, " 24:0   Note off                9, note 38, velocity 0"),
        (0.4, " 24:0   Note on                 0, note 60, velocity 100"),
        (0.5, " 24:0   Note off                0, note 60, velocity 0"),
    ])
    verdict = probe.analyze(routed)
    require(verdict.channels[9]["noteOn"] == 2 and
            verdict.channels[9]["notes"] == 2 and
            verdict.channels[0]["noteOn"] == 1,
            "per-channel note histogram must reflect the routed stream")
    require(verdict.ok, "a clean routed capture must pass")

    # Dense chord members and NoteOffs must not collapse the adaptive onset
    # median toward zero and turn a normal 310 ms rest into a false stall.
    chordal = []
    for at in (0.0, 0.1, 0.2, 0.3, 0.61, 0.71, 0.81):
        for note in (60, 64, 67):
            chordal.append((at,
                f" 24:0   Note on                 0, note {note}, velocity 100"))
            chordal.append((at + 0.04,
                f" 24:0   Note off                0, note {note}, velocity 0"))
    verdict = probe.analyze(events(chordal), capture_end=0.9)
    require(verdict.ok and not verdict.long_gaps and
            verdict.gap_threshold_ms >= 799.0,
            "chord members and NoteOffs must not create false gap failures")

    # An empty capture means no receiver or no playback, never silent success.
    require(not probe.analyze([]).ok,
            "an empty capture must never report OK")

    # Silence is the interesting case: a stalled endpoint sends nothing, so the
    # capture window must close on the clock instead of blocking on the next
    # line that never arrives.
    started = time.monotonic()
    captured = probe.capture("unused", 0.4, None, command=["sleep", "10"])
    elapsed = time.monotonic() - started
    require(captured.events == [], "a silent stream must capture no events")
    require(captured.ended_at >= 0.35,
            "capture metadata must retain the actual bounded window")
    require(elapsed < 3.0,
            f"a silent capture must end on its own deadline, took {elapsed:.1f}s")

    print("MIDI probe regressions: OK")


if __name__ == "__main__":
    sys.exit(main())
