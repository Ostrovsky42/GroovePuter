#!/usr/bin/env python3
"""Capture the Cardputer MIDI stream and turn it into a verdict.

Watching `aseqdump` scroll by answers nothing. This captures a bounded window,
timestamps every event on arrival, and reports the four things that actually
matter for this firmware:

  * stuck notes   - NoteOn without a matching NoteOff (ownership/cleanup bugs)
  * gaps          - silence longer than a threshold (stutter, SD or USB stalls)
  * clock stability - jitter and drift of MIDI Clock (transport sync)
  * channel map   - which channels/notes actually arrived (routing check)

Running it also keeps the ALSA port open, which is what drains the device's
bulk IN endpoint; capturing and un-stalling the endpoint are the same action.

Usage:
  ./scripts/midi_probe.py 60                 # capture 60 s, auto-detect port
  ./scripts/midi_probe.py 60 --port 24:0
  ./scripts/midi_probe.py 60 --save logs/run.txt
  ./scripts/midi_probe.py --replay logs/run.txt      # re-analyse a capture

Exit code is 0 when the verdict passes, 1 when an invariant is violated, so it
can gate a release the same way the host tests do.

Timestamps come from userspace on the receiving side: they are reliable for
millisecond-scale gaps and stutter, not for sub-millisecond jitter claims.
"""

from __future__ import annotations

import argparse
import re
import selectors
import shutil
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass, field

DETECT_PATTERNS = ("GroovePuter", "MiniAcid", "Cardputer", "M5Stack", "ESP32")

# " 24:0   Note on                 0, note 60, velocity 100"
EVENT_RE = re.compile(
    r"^\s*(?P<port>\d+:\d+)\s+(?P<kind>[A-Za-z][A-Za-z ]*[A-Za-z])\s*(?P<rest>.*)$"
)
CHANNEL_RE = re.compile(r"^(?P<channel>\d+)")
NOTE_RE = re.compile(r"note (?P<note>\d+)")
VELOCITY_RE = re.compile(r"velocity (?P<velocity>\d+)")


@dataclass
class Event:
    at: float
    kind: str
    channel: int | None = None
    note: int | None = None
    velocity: int | None = None


@dataclass
class Verdict:
    duration: float = 0.0
    counts: dict[str, int] = field(default_factory=dict)
    stuck: list[tuple[int, int]] = field(default_factory=list)
    active_at_end: list[tuple[int, int]] = field(default_factory=list)
    tail_silence_ms: float = 0.0
    max_gap_ms: float = 0.0
    gap_threshold_ms: float = 0.0
    long_gaps: list[float] = field(default_factory=list)
    clock_count: int = 0
    clock_mean_ms: float = 0.0
    clock_stdev_ms: float = 0.0
    clock_max_dev_ms: float = 0.0
    implied_bpm: float = 0.0
    channels: dict[int, dict[str, int]] = field(default_factory=dict)
    failures: list[str] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.failures


@dataclass
class CaptureResult:
    events: list[Event] = field(default_factory=list)
    ended_at: float = 0.0


def parse_line(line: str, at: float) -> Event | None:
    match = EVENT_RE.match(line)
    if not match:
        return None
    kind = " ".join(match.group("kind").split())
    if kind.lower().startswith("port") or kind.lower() == "client name":
        return None
    rest = match.group("rest")
    channel = note = velocity = None
    channel_match = CHANNEL_RE.match(rest.strip())
    if channel_match:
        channel = int(channel_match.group("channel"))
    note_match = NOTE_RE.search(rest)
    if note_match:
        note = int(note_match.group("note"))
    velocity_match = VELOCITY_RE.search(rest)
    if velocity_match:
        velocity = int(velocity_match.group("velocity"))
    return Event(at=at, kind=kind, channel=channel, note=note, velocity=velocity)


def is_note_on(event: Event) -> bool:
    # Running-status NoteOn with velocity 0 is a NoteOff, as in the SMF parser.
    return event.kind == "Note on" and (event.velocity or 0) > 0


def is_note_off(event: Event) -> bool:
    return event.kind == "Note off" or (
        event.kind == "Note on" and (event.velocity or 0) == 0
    )


def analyze(events: list[Event], gap_ms: float = 250.0,
            clock_dev_ms: float = 5.0, gap_multiplier: float = 8.0,
            capture_end: float | None = None,
            cleanup_settle_ms: float = 500.0,
            chord_window_ms: float = 5.0) -> Verdict:
    verdict = Verdict()
    if not events:
        verdict.failures.append("no MIDI received at all")
        return verdict

    verdict.duration = events[-1].at - events[0].at
    if capture_end is None:
        capture_end = events[-1].at
    verdict.tail_silence_ms = max(0.0, (capture_end - events[-1].at) * 1000.0)
    for event in events:
        verdict.counts[event.kind] = verdict.counts.get(event.kind, 0) + 1

    held: dict[tuple[int, int], int] = {}
    note_on_times: list[float] = []
    for event in events:
        if event.channel is None or event.note is None:
            continue
        key = (event.channel, event.note)
        if is_note_on(event):
            held[key] = held.get(key, 0) + 1
            note_on_times.append(event.at)
            bucket = verdict.channels.setdefault(
                event.channel, {"noteOn": 0, "notes": 0})
            bucket["noteOn"] += 1
        elif is_note_off(event):
            if held.get(key):
                held[key] -= 1
                if held[key] == 0:
                    del held[key]
    for channel, bucket in verdict.channels.items():
        bucket["notes"] = len({
            event.note for event in events
            if event.channel == channel and is_note_on(event)
        })

    open_notes = sorted(held.keys())
    if open_notes and verdict.tail_silence_ms >= cleanup_settle_ms:
        verdict.stuck = open_notes
    else:
        verdict.active_at_end = open_notes

    # A fixed millisecond threshold would flag ordinary quarter notes at a slow
    # tempo. A stall is a gap that dwarfs the surrounding rhythm, so scale the
    # threshold by the median spacing and keep the fixed value as a floor.
    # Treat simultaneous NoteOns as one musical onset. Including chord members
    # and NoteOffs drives the median interval toward zero and makes ordinary
    # eighth-note rests look like transport stalls.
    onset_times: list[float] = []
    chord_window_seconds = chord_window_ms / 1000.0
    for at in note_on_times:
        if not onset_times or at - onset_times[-1] > chord_window_seconds:
            onset_times.append(at)
    intervals_ms = [
        (current - previous) * 1000.0
        for previous, current in zip(onset_times, onset_times[1:])
    ]
    verdict.gap_threshold_ms = gap_ms
    if len(intervals_ms) >= 4:
        median_ms = statistics.median(intervals_ms)
        verdict.gap_threshold_ms = max(gap_ms, gap_multiplier * median_ms)
    for gap in intervals_ms:
        verdict.max_gap_ms = max(verdict.max_gap_ms, gap)
        if gap > verdict.gap_threshold_ms:
            verdict.long_gaps.append(gap)

    clock_times = [event.at for event in events if event.kind == "Clock"]
    verdict.clock_count = len(clock_times)
    if len(clock_times) >= 3:
        intervals = [
            (b - a) * 1000.0 for a, b in zip(clock_times, clock_times[1:])
        ]
        verdict.clock_mean_ms = statistics.fmean(intervals)
        verdict.clock_stdev_ms = statistics.pstdev(intervals)
        verdict.clock_max_dev_ms = max(
            abs(value - verdict.clock_mean_ms) for value in intervals)
        if verdict.clock_mean_ms > 0:
            # 24 PPQN: one quarter note is 24 clock pulses.
            verdict.implied_bpm = 60000.0 / (verdict.clock_mean_ms * 24.0)

    if verdict.stuck:
        verdict.failures.append(
            f"{len(verdict.stuck)} stuck note(s): "
            + ", ".join(f"ch{channel}/note{note}"
                        for channel, note in verdict.stuck[:8]))
    if verdict.active_at_end:
        verdict.failures.append(
            f"capture ended with {len(verdict.active_at_end)} active note(s); "
            "stop/panic playback and leave the probe open for cleanup")
    if verdict.long_gaps:
        verdict.failures.append(
            f"{len(verdict.long_gaps)} gap(s) over "
            f"{verdict.gap_threshold_ms:.0f} ms "
            f"(worst {max(verdict.long_gaps):.0f} ms)")
    if verdict.clock_count >= 3 and verdict.clock_max_dev_ms > clock_dev_ms:
        verdict.failures.append(
            f"clock deviation {verdict.clock_max_dev_ms:.1f} ms exceeds "
            f"{clock_dev_ms:.0f} ms")
    return verdict


def report(verdict: Verdict) -> None:
    print()
    print(f"duration        {verdict.duration:.1f} s")
    print("events          " + (", ".join(
        f"{kind}={count}" for kind, count in sorted(verdict.counts.items()))
        or "none"))
    if verdict.channels:
        print("channels        " + ", ".join(
            f"ch{channel}: {bucket['noteOn']} on / {bucket['notes']} distinct"
            for channel, bucket in sorted(verdict.channels.items())))
    print(f"max note gap    {verdict.max_gap_ms:.0f} ms "
          f"(threshold {verdict.gap_threshold_ms:.0f} ms, "
          f"{len(verdict.long_gaps)} over)")
    if verdict.clock_count >= 3:
        print(f"clock           {verdict.clock_count} pulses, "
              f"mean {verdict.clock_mean_ms:.2f} ms, "
              f"sd {verdict.clock_stdev_ms:.2f} ms, "
              f"max dev {verdict.clock_max_dev_ms:.2f} ms, "
              f"~{verdict.implied_bpm:.1f} BPM")
    else:
        print("clock           none")
    print(f"stuck notes     {len(verdict.stuck)}")
    print(f"active at end   {len(verdict.active_at_end)} "
          f"(tail {verdict.tail_silence_ms:.0f} ms)")
    print()
    if verdict.ok:
        print("VERDICT: OK")
    else:
        print("VERDICT: FAIL")
        for failure in verdict.failures:
            print(f"  - {failure}")


def detect_port() -> str | None:
    listing = subprocess.run(
        ["aseqdump", "-l"], capture_output=True, text=True, check=False).stdout
    for pattern in DETECT_PATTERNS:
        for line in listing.splitlines():
            if pattern.lower() in line.lower():
                token = line.split()[0]
                if re.fullmatch(r"\d+:\d+", token):
                    return token
    return None


def capture(port: str, seconds: float, save: str | None,
            command: list[str] | None = None) -> CaptureResult:
    # stdbuf keeps aseqdump line-buffered; without it the pipe batches events
    # and every arrival timestamp becomes meaningless.
    if command is None:
        command = ["aseqdump", "-p", port]
        if shutil.which("stdbuf"):
            command = ["stdbuf", "-oL"] + command

    events: list[Event] = []
    sink = open(save, "w", encoding="utf-8") if save else None
    started = time.monotonic()
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
    try:
        assert process.stdout is not None
        # A silent stream is the interesting case (that is what a stalled
        # endpoint looks like), so the window must close on the clock rather
        # than on the next arriving line.
        selector = selectors.DefaultSelector()
        selector.register(process.stdout, selectors.EVENT_READ)
        while True:
            remaining = seconds - (time.monotonic() - started)
            if remaining <= 0:
                break
            if not selector.select(timeout=min(remaining, 0.25)):
                continue
            line = process.stdout.readline()
            if not line:
                break
            now = time.monotonic()
            relative_now = now - started
            if sink:
                sink.write(f"{relative_now:.6f}\t{line.rstrip()}\n")
            event = parse_line(line, relative_now)
            if event:
                events.append(event)
        selector.close()
    except KeyboardInterrupt:
        pass
    finally:
        ended_at = time.monotonic() - started
        process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
        if sink:
            sink.write(f"# capture_end\t{ended_at:.6f}\n")
            sink.close()
    return CaptureResult(events=events, ended_at=ended_at)


def replay(path: str) -> CaptureResult:
    events: list[Event] = []
    ended_at: float | None = None
    with open(path, encoding="utf-8") as handle:
        for raw in handle:
            if raw.startswith("# capture_end\t"):
                try:
                    ended_at = float(raw.split("\t", 1)[1])
                except ValueError:
                    pass
                continue
            stamp, _, line = raw.partition("\t")
            try:
                at = float(stamp)
            except ValueError:
                continue
            event = parse_line(line, at)
            if event:
                events.append(event)
    if ended_at is None:
        ended_at = events[-1].at if events else 0.0
    return CaptureResult(events=events, ended_at=ended_at)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("seconds", nargs="?", type=float, default=30.0)
    parser.add_argument("--port", help="ALSA client:port, e.g. 24:0")
    parser.add_argument("--save", help="write the raw timestamped capture here")
    parser.add_argument("--replay", help="analyse a previously saved capture")
    parser.add_argument("--gap-ms", type=float, default=250.0,
                        help="silence longer than this fails the run")
    parser.add_argument("--gap-multiplier", type=float, default=8.0,
                        help="gap threshold as a multiple of the median note "
                             "spacing; the larger of this and --gap-ms wins")
    parser.add_argument("--clock-dev-ms", type=float, default=5.0,
                        help="max allowed MIDI Clock interval deviation")
    parser.add_argument("--settle-ms", type=float, default=500.0,
                        help="quiet tail required before open notes count as stuck")
    parser.add_argument("--chord-ms", type=float, default=5.0,
                        help="group NoteOns this close as one musical onset")
    args = parser.parse_args()

    if args.replay:
        capture_result = replay(args.replay)
    else:
        if not shutil.which("aseqdump"):
            print("aseqdump not found. Install alsa-utils.", file=sys.stderr)
            return 127
        port = args.port or detect_port()
        if not port:
            print("No Cardputer MIDI port found. Try ./scripts/midi_sink.sh "
                  "--list", file=sys.stderr)
            return 1
        print(f"Capturing {args.seconds:.0f} s from {port} — start playback now.")
        capture_result = capture(port, args.seconds, args.save)

    verdict = analyze(capture_result.events, gap_ms=args.gap_ms,
                      clock_dev_ms=args.clock_dev_ms,
                      gap_multiplier=args.gap_multiplier,
                      capture_end=capture_result.ended_at,
                      cleanup_settle_ms=args.settle_ms,
                      chord_window_ms=args.chord_ms)
    report(verdict)
    return 0 if verdict.ok else 1


if __name__ == "__main__":
    sys.exit(main())
