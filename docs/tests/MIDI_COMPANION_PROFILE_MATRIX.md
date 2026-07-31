# MIDI Companion profile matrix

## Purpose

Provide one copy-pasteable mapping reference for host tests, UI labels, and the
later Cardputer/SEQTRAK hardware acceptance.

## SEQTRAK NATIVE

| GroovePuter voice | External channel | SEQTRAK track | Default note | Policy |
|---|---:|---|---:|---|
| Kick | 1 | KICK | 60 | dedicated |
| Snare | 2 | SNARE | 60 | dedicated |
| Clap | 3 | CLAP | 60 | dedicated |
| Closed Hat | 4 | HAT 1 | 60 | dedicated |
| Open Hat | 5 | HAT 2 | 60 | dedicated |
| Mid Tom | 6 | PERC 1 | 60 | shared track |
| High Tom | 7 | PERC 2 | 60 | dedicated |
| Rim | 6 | PERC 1 | 60 | shared with Mid Tom |
| Synth A | 8 | SYNTH 1 | pattern note | melodic |
| Synth B | 9 | SYNTH 2 | pattern note | melodic |

The shared PERC 1 route is explicit because SEQTRAK has seven native drum tracks
while GroovePuter has eight internal drum voices. Users can later change any
route in CUSTOM mode.

## GENERAL MIDI

| GroovePuter voice | External channel | GM note |
|---|---:|---:|
| Kick | 10 | 36 |
| Snare | 10 | 38 |
| Closed Hat | 10 | 42 |
| Open Hat | 10 | 46 |
| Mid Tom | 10 | 43 |
| High Tom | 10 | 47 |
| Rim | 10 | 37 |
| Clap | 10 | 39 |

## Validation rules

```text
channel internal: 0..15
channel UI:       1..16
note:             0..127
drum gate:        1..500 ms
```

## Acceptance checklist

- [ ] profile tests match this matrix;
- [ ] UI displays one-based channels;
- [ ] storage retains zero-based channels;
- [ ] eighth GroovePuter drum voice remains explicit;
- [ ] no default SEQTRAK drum route uses channel 10/DX;
- [ ] hardware validation records the chosen SEQTRAK sound on each shared route.
