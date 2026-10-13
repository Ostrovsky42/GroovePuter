# Unified Harmonic System v2 — source notes

This companion note records the external open-source evidence used while drafting `2026-10-13-unified-harmonic-system-v2-design.md`.

- `Fannon/midi-chords-and-scales-pack` — MIT; chord and scale vocabulary generator using `@tonaljs/tonal`; useful as interval/chord/scale vocabulary evidence, not progression statistics.
- `ldrolez/free-midi-chords` — MIT; progression source in Roman-numeral-like text form plus mood-like tags and multiple timing styles; useful as progression/cadence/modal corpus evidence.

No external MIDI corpus is intended to be copied wholesale into firmware. The proposed architecture imports/normalizes evidence offline and emits compact deterministic tables only.