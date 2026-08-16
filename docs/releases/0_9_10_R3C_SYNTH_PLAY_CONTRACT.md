# 0.9.10 R3c — Synth live-input PLAY contract

## Decision

0.9.10 freezes external MIDI monitoring for Synth A/B as **STOP-state live input**.

The existing `MiniAcid` engine is the authoritative voice owner and its
`liveNoteOn()` deliberately rejects live NoteOn while sequencer PLAY is active.
R3c does not bypass that boundary, add a competing DSP voice, or couple the
transport-independent MIDI input router to engine playback state.

This is a deliberate production constraint, not an accidental hidden failure:

- STOP: incoming Synth A/B NoteOn/NoteOff uses the existing live-note path.
- PLAY: new live Synth NoteOn is rejected by the existing engine owner.
- NoteOff / panic cleanup remains safe and bounded.
- Drums are handled separately in R4.

Supporting simultaneous Pattern + live Synth performance would require a
separate voice-ownership design covering gate, slide/legato, retrigger and mute
semantics. That redesign is outside 0.9.10.
