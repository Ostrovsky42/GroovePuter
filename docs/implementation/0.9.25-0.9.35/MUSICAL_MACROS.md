# Musical macro contract — 0.9.28 domain / 0.9.29 playback

The 0.9.28 domain engine is a pure, transient projection over an existing fixed-capacity `RuntimeSynthEventBuffer`. It takes a const source, a distinct unpublished destination buffer, a four-value state, optional project-key context and a seed. The caller provides the destination scratch space; the transformer adds no full-phrase temporary to its own stack. It has no Scene/storage/Undo access and does not persist macro values.

The value range for every macro is `0..100`; zero is neutral. Invalid values are rejected. Applying the same source, state, harmony context and seed produces the same candidate. Rejection leaves the caller's destination untouched. Runtime events remain fixed-capacity, and the projection performs no heap allocation.

## Current mappings

The transform order is `MUTATION → TENSION → ENERGY → SPACE`.

| Macro | Current bounded effect | Protected material |
|---|---|---|
| TENSION | Adds up to 12 velocity to a weak-beat note when its next later onset is within two semitones. The boost scales linearly with the macro value. | Pitch, onset, event count and strong-beat attacks remain unchanged. No new passing notes are invented without harmonic context. |
| ENERGY | Scales every event's velocity from 100% to 150%. For ghost events, probability moves toward 100% as Energy rises. | Existing event count, onsets and pitches stay unchanged; velocity and probability clamp to legal MIDI ranges. |
| SPACE | Scales gate duration from 100% down to 50%. Ghost-event probability scales from 100% down to 0%. Events remain present in the candidate even at probability zero. | No onset or pitch is removed or moved; duration never reaches zero. |
| MUTATION | For each weak-beat event in the known project scale, a seed-derived decision may shift the pitch by one scale step up or down. The macro value is the per-event selection threshold. | Strong-beat notes, out-of-scale notes, event count, onsets and durations remain unchanged. Unknown harmony disables pitch mutation. |

Energy and Space compose in that order: Energy raises ghost probability, then Space attenuates it. Velocity boosts saturate at 127. In 0.9.28 the caller had to publish a completed candidate through the existing playback owner at a safe musical boundary.

## 0.9.29 live playback and UI

`MiniAcid` now publishes the four values as one packed 32-bit atomic state. The sequencer reads one coherent snapshot per tick. On a Pattern or Phrase onset, it builds one transient event immediately before the existing `RuntimeSynthPlaybackState` accepts it. Internal synthesis and Pattern MIDI consume that same accepted event through the existing playback and routing owners. Song playback uses the currently selected Pattern or Phrase through the same path.

The real-time path does not call the batch `apply()` function and does not allocate a phrase-sized buffer. For TENSION, the caller supplies the next strictly later onset found by a bounded scan of at most 16 Pattern events or 128 Phrase events. Without TENSION, that scan is skipped. MUTATION reads the current Scene key/scale only when the macro is active; unknown or invalid harmony falls back without changing pitch.

The Performance tools add a seventh `MACRO` tab. Left/Right changes the selected value by 5%; Enter resets that value; `RESET ALL` returns all four to zero. Values are session-only, do not enter Scene serialization or Undo, and reset on process restart. Leaving and returning to the Performance page keeps the live values for the current session. The macro row cursor resets when its UI view is restored to keep `UiViewContinuityState` at 16 bytes.

No Commit/materialize action is exposed. Save/Load works on the unchanged musical source; a live macro view is not written into the saved Pattern or Phrase.

## Boundaries and follow-up

- The per-event transform preserves event count, onsets and source material. The macros do not add notes or vary differently on each loop; MUTATION is deterministic per source event and seed.
- Tension uses a local stepwise approach heuristic. It does not add dissonant passing notes or infer chords. Harmonic-aware tension remains gated on the 0.9.30 harmonic context and conservative fallback.
- Mutation follows the known project scale, not a chord-specific scale. Unknown harmony skips pitch mutation; an invalid declared key rejects the candidate and leaves the destination untouched.
- Performance quality is a data-level hypothesis only. SDL confirms the controls, selected values and Song row transition; it cannot confirm musical quality. No hardware listening or physical MIDI test has run.
- The macro state is deliberately absent from Scene serialization. There is no hidden write and no implicit Commit action.
- `apply()` remains control-side batch work and must not run in the audio callback. The live sequencer uses the bounded per-event transform and hands its result to the existing playback state.
