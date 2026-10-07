# MIDI Clock bench

Measures GroovePuter's MIDI timing against an exact clock on a Linux laptop.
Built on 2026-10-06/07 to fix MIDI IN follow; it found the drift, the
one-pulse lead and the output-latency lag that host tests alone had missed.

Setup: Cardputer USB role **COMPUTER**, cable to the laptop, replug after
flashing (until then it enumerates as USB JTAG and has no MIDI port).
Needs `amidi` (alsa-utils), a C compiler and Python 3. No other libraries.

## Follow (laptop is the master)

TEMPO (`Alt+Y`): CLOCK = **MIDI IN**. Load a pattern; do not press Play.

    tools/midi_bench/bench.sh follow 128 60     # BPM, seconds, [pre-roll s, default 3]
    tools/midi_bench/bench.sh follow 90 40

The laptop sends clock for the pre-roll (as SEQTRAK does while stopped), then
FA immediately followed by the downbeat F8, and stops with FC.

Accepted result (combined `a9a7f996`): bar starts 0..+2 ms from the second
bar on at 128 and 90 BPM, no drift over 60 s, no self-stop. Known: the first
downbeat after Start is +31..46 ms late.

## Master (GroovePuter is the master)

TEMPO: CLOCK = **INTERNAL**. Run, then press Stop and Play on GroovePuter
while it listens:

    tools/midi_bench/bench.sh master 40

Reports GroovePuter's real clock tempo and every note against its own grid.
Accepted: notes 0.0 ms mean, 1.5 ms sd on its own clock.

## Pitfalls

- Open the port before pressing Play. Up to 16 USB packets queue while
  nobody reads; they arrive at t=0, and a stale FA shifts the step grid by a
  whole pulse (this once produced a false "notes 18 ms early").
- "GroovePuter sent its own clock" in follow mode means CLOCK is INTERNAL.
- SEQTRAK sends its step-1 notes with the first F8 after FA: that F8 is the
  downbeat (position 0), not +1/24 quarter.
- Timestamps are taken in userspace on the receiving side: trustworthy for
  millisecond offsets and drift, not for sub-millisecond jitter claims.

Logs land in `build/midi_bench/`. The analyzers also run on saved logs:
`follow_offsets.py LOG BPM`, `bar_offsets.py LOG BPM`, `master_offsets.py LOG`.
