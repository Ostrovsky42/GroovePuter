# Melody polyphony (0.9.17)

Owner decision 2026-10-07: chords live in the Melody (not in steps). Chords go
to SEQTRAK over MIDI; GroovePuter's own synth voices stay monophonic.

## What exists

| Layer | Today |
|---|---|
| Melody format (`RuntimeSynthEventBuffer`, file `GPML`) | overlapping events already valid; 128 events max |
| Playback (`phraseEventAt_`) | first event with that `startTick` wins, the rest are dropped |
| Playback state (`RuntimeSynthPlaybackState`) | one note: a new onset releases the old one |
| Internal voice (Synth A/B engines) | monophonic by construction |
| MIDI out, Pattern/Melody (`UsbMidiOutput` lane) | `replaceActiveNote`: one note per target |
| MIDI out, PERFORM POLY / ARP | per-note sets, receiver Mono/Poly switch (SEQTRAK CC26) |
| Editor | time cursor; selection = one note covering the cell |

## Decisions (defaults, change before the layer is built)

1. **Up to 4 sounding notes per synth.** A 5th onset steals the oldest.
2. **Internal voice plays the top note** of what is sounding; when the top note
   ends it moves to the next highest still held. Steps keep today's mono
   behaviour exactly (slides, ties, retrig).
3. **MIDI carries every note.** A Melody with overlapping notes switches its
   SEQTRAK track to Poly (CC26, as PERFORM POLY does); a monophonic Melody and
   steps keep Mono (portamento/slide unchanged).
4. **Editor:** `A` adds a note to the chord at the cursor (a third above the
   selected note, then edit pitch with Up/Down); `C` cycles the selected note
   within the chord. Enter/Backspace/J/Alt+L/R act on the selected note.
5. **Recording:** keys pressed within ~30 ms land on the same tick as a chord.
6. **Capacity stays 128 events** (each chord note is an event). The editor says
   `MELODY FULL` instead of silently refusing. Raising it costs DRAM twice
   (working buffer and the undo receipt).

## Layers, each its own commit with tests

1. Poly playback state + chord onsets in `phraseEventAt_` (host tests:
   chord starts/ends, stealing, mono path byte-identical for steps).
2. Internal top-note voice (test: chord on, top released, falls to next).
3. MIDI: poly ownership for Melody notes + CC26 Poly/Mono switching
   (output tests; no stuck notes on stop, mute, slot switch, Undo).
4. Editor `A` / `C` + display of stacked notes (UI test).
5. Keyboard chord recording (host test with near-simultaneous notes).
6. Hardware with SEQTRAK: chords recorded and played back, no stuck notes.

## Risks

- Stuck notes: every release path (stop, Space mute, slot switch, Undo, Song
  boundary) must release all held notes, not "the" note.
- SEQTRAK track left in Poly after leaving a chord Melody: switch back on the
  next monophonic onset, as PERFORM does.
- RNG order (ghost/probability) per chord note must stay deterministic.
