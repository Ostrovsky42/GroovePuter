# Synth pitch and note lifecycle change

## Scope

This change is the first implementation PR derived from the pre-0.9 synth-engine audit.

Changed:

- AY tone-period calculation now uses a separate 1.7734 MHz PSG clock.
- SN76489 unsupported low notes fold upward by octaves while preserving pitch class.
- SN76489 stack labels and intervals now describe upward musical intervals.
- Live NoteOff normalizes through the same C1..B4 clamp as live NoteOn.
- Host tests cover the pitch and note-identity contracts.
- Cardputer-Adv hardware acceptance is documented.

Not changed:

- synth persistence or migration;
- TB303 amplitude envelope, sub oscillator, filter, DC, or velocity;
- distortion, delay, or gain staging;
- SID or WAVEMORPH DSP;
- pattern generation;
- scene schema.

## Compatibility

The Scene schema is unchanged.

SN76489 sound changes intentionally in the lower register and for the former `Oct` stack:

- low notes no longer collapse to the same 109.35 Hz tone;
- `Oct+` now means root + one octave up + two octaves up.

AY patches retain the same parameters and articulation, but pitch is corrected across C1..B4.
