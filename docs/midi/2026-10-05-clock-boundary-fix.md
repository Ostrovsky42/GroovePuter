# MIDI Clock audio-block boundary fix — 2026-10-05

Base: hardware-accepted 0.9.16 integration source
`4f1304916389c5af315faa24127555c062a32fa9`.

## Evidence

The two-position G4/Synth 2 capture at requested 50 BPM contained four
approximately 99-ms Clock intervals where the usual cadence was ~49.5 ms.
Neither captured device stream contained replacement pulses. Original MIDI
trace SHA-256:
`2759451df3292900ba789a256f8a82b6416f9c4824426ffbd2800b5cfcd4a45c`.
This was PC arrival-time evidence; exact hardware loss provenance was not
established by that observation alone.

The production publisher independently reproduced a missing pulse due at
frame 511.75 of a 512-frame block. Nearest-frame rounding produced 512, which
was discarded. The following block's phase had advanced past the pulse.
A synthetic minute at 50 BPM / 22050 Hz / 512-frame blocks emitted 1199
instead of 1200 clocks, with no queue overflow.

## Change

Carry a pulse that rounds onto the next block to its frame zero, and suppress
the same near-zero candidate from that block's phase anchor. Clear the carry
on Stop, invalid render input and reset. Use a boundary tolerance derived from
the precision of the float phase over a 16-step bar: the old tolerance could
miss an exact pulse when the phase anchor rounded slightly past its boundary.

The fix changes only the Clock publisher. It adds no allocation and preserves
the transport lifecycle, queue overflow policy and external-master behavior.
It does not change MIDI note numbers, SEQTRAK's local tempo after GP Stop,
recording quantization, or player ORIGINAL/PROJECT selection.

## Regression proof

The in-block pulse regression first failed against the original code. The
updated suite exercises the real publisher and queue:

- one pulse due at frame 511.75 survives at the next block's frame zero;
- an exact block-end pulse occurs once in the next block;
- deferred pulses survive a tempo change and do not leak through Stop/reset;
- 24 synthetic minute runs across 48, 50, 50.5, 120, 128 and 182 BPM,
  22050/44100 Hz and 256/512-frame blocks verify exact pulse counts, valid
  timestamps, continuous cadence, one Start and no queue drops;
- existing lifecycle, Song continuity, tempo-change, event-ordering and
  critical-overflow checks remain in the same executable.

Focused command:

```sh
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -I . \
  tests/test_midi_transport_sync.cpp -o build/clock-fix/test_midi_transport_sync
build/clock-fix/test_midi_transport_sync
```

## Device verification still required

Repeat with a fresh GP Start captured, the same two-position G4 phrase and a
separate test pattern. Keep GP Clock running while silencing its notes and
replaying the recorded SEQTRAK pattern. Check for missing/duplicate pulses and
accumulating phase shift. Firmware upload and software gates do not prove
audio tuning or correct SEQTRAK recording behavior.

## Product image and post-fix capture — 2026-10-06

Built and flashed the FS1B dynamic product profile with
`PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc`,
without USB acceptance diagnostics. App SHA-256:
`67b5a78b0ed21dd524b51d8061f0618a15cf15115ce582c9c6c6f32b359a87c3`.
Static DRAM: **183220 / 191488 bytes**. Esptool verified all four uploaded
regions. USB MIDI re-enumerated after a watchdog reset; no serial monitor
is expected for this product profile. The compiled publisher matches the
working source (apart from Arduino's injected line directive).

Capture: `build/midi-clock/post-fix-50bpm-20261006-002215/` in this worktree.
The directory name records the requested test, but **actual Clock was 128 BPM**;
notes were subsequently changed, so this is not a repeat of the controlled
50-BPM/two-position test. The user reported that it seemed to work.

- MIDI duration: 281.716 s, 26647 events; 4884 GP Clock pulses and 129 GP NoteOns.
- Four GP Start/Stop segments contained 711, 1578, 1573 and 1022 Clock pulses.
- Comparing first-to-last Clock duration with pulse count at 128 BPM gives
  endpoint residuals of -0.130, -0.106, +0.070 and -0.132 pulses: no whole-pulse
  deficit or excess was observed inside these captured Clock runs.
- Three long PC-arrival intervals (183, 265 and 175 ms) were followed by
  batches of Clock events, including identical timestamps. This trace cannot
  establish wire jitter from those intervals. They differ from the earlier
  doubled intervals without replacement pulses.
- GP NoteOn grid residual: median signed 0.963 ms, p95 absolute 2.985 ms,
  max absolute 18.994 ms. Arrival timing and edited phrases limit interpretation;
  these values do not validate SEQTRAK quantization or audio tuning.
- In two segments GP Clock ended ~2.00 s / 1.44 s before the GP Stop event.
  UI actions were not captured, so this is retained as an unresolved observation,
  not attributed to the boundary bug. SEQTRAK emitted additional clocks in
  those tails and returned to approximately 182 BPM after external Clock ended.
- The WAV finalized correctly: stereo 16-bit / 44100 Hz, 280.450 s.
  MIDI/audio alignment is not calibrated. The PC MIDI bridge and both recorders
  were stopped after the capture.

MIDI SHA-256:
`6abbd92fe5417f53abae387ea93843599013ab2fc79086c72e1a79049b156bad`.
Audio SHA-256:
`2345f0436885beb41f9c8540ee14a5ed43244470df4eb52777fb67d7889ac6be`.

The narrow Clock-loss fix has software regression proof and encouraging device
observations. Controlled 50-BPM recording/replay with GP Clock kept running
remains required before claiming the complete recording issue is closed.

## Software acceptance

The combined command completed with exit code 0 on the final runtime source:

```sh
bash scripts/ci/run_core.sh
bash scripts/ci/run_midi_targets.sh
```

Core: **45/45 groups PASS**, including Host regressions and Unified Song slots.
MIDI Targets: **11/11 groups PASS**. The focused Clock regression also passed
under C++17 with `-Wall -Wextra -Werror -pedantic`. Product FS1B build and memory
checks passed; `git diff --check` passed. Logs remain in `build/clock-fix/`.
Only tests and this report were edited after the product image was built;
its compiled publisher hash matches the final runtime source. This is a local
fix branch, not a replacement of the accepted 0.9.16 release/tag.
