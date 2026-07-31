# SMF Streaming Host Test

## Purpose

Validate the bounded-memory SMF runtime foundation before Cardputer USB playback is connected.

The production player must not retain every MIDI event in RAM. It indexes at most 64 track spans, keeps one small read cache per active track cursor, and merges only the nearest event from each track.

## Hardware list

No hardware is required for this host test.

Future hardware acceptance uses:

- M5Stack Cardputer-Adv / ESP32-S3;
- microSD card with `.mid` files under `/midi`;
- USB-C data cable;
- Yamaha SEQTRAK.

## Wiring

None for the host test.

Future Cardputer playback uses USB-C data only. PORT.A GPIO2/GPIO1 and the shared `Wire` bus are unrelated.

## Build / Flash

Run from repository root:

```bash
bash tests/run_host_tests.sh
```

The streaming test is built with:

```text
-std=c++17 -Wall -Wextra -Werror
```

No flash step is required until runtime dispatch/UI are integrated.

## Expected behavior

`test_smf_stream` validates:

- Type 1 header/track indexing;
- PPQN validation;
- fixed maximum of 64 tracks;
- bounded track reads (`<= 64` bytes per cache refill);
- merge of conductor/meta events with note tracks;
- running-status notes;
- exact NoteOff ticks;
- deterministic simultaneous-event order;
- reset back to the first stream event;
- discovery of `MUSIC START` without retaining the full event list.

`test_smf_timing` validates:

- default 120 BPM / 4/4 behavior;
- tempo changes without quantization;
- tick-to-microsecond mapping;
- 4/4 -> 3/4 bar/beat mapping;
- bar-start tick lookup for seek controls.

## Troubleshooting

- **Host test OOMs:** this is a regression; the streaming test itself should use bounded cursor state.
- **Event order differs:** compare tick first, then track index/track-local ordinal for deterministic same-tick ordering.
- **Tempo time is wrong after a change:** verify cumulative microseconds are computed using the previous tempo up to the change tick.
- **Bar number is wrong after time-signature change:** verify the signature event starts a new UI bar boundary when the preceding segment is partial.
- **Cardputer build fails later:** keep Arduino/SD adapters outside the core stream parser so host code remains platform-neutral.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` succeeds.
- [ ] `test_smf_document` succeeds.
- [ ] `test_smf_timing` succeeds.
- [ ] `test_smf_stream` succeeds.
- [ ] no TinyUSB call exists in parser/timing/stream code.
- [ ] no `millis()`/`delay()` scheduler is introduced.
- [ ] runtime stream state is bounded to 64 tracks.
- [ ] per-track read cache remains fixed-size.
- [ ] exact NoteOff and simultaneous-note ticks survive streaming.
- [ ] `MUSIC START` can be found without a full in-memory event vector.
