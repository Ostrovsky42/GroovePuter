# Musical macro contract — 0.9.28

The 0.9.28 engine is a pure, transient projection over an existing fixed-capacity `RuntimeSynthEventBuffer`. It takes a const source, a distinct unpublished destination buffer, a four-value state, optional project-key context and a seed. The caller provides the destination scratch space; the transformer adds no full-phrase temporary to its own stack. It has no Scene/storage/Undo access and does not persist macro values. An explicit materialization action is not implemented; saving the source phrase therefore cannot capture a live macro view.

The value range for every macro is `0..100`; zero is neutral. Invalid values are rejected. Applying the same source, state, harmony context and seed produces the same candidate. Rejection leaves the caller's destination untouched. Runtime events remain fixed-capacity, and the projection performs no heap allocation.

## Current mappings

The transform order is `MUTATION → TENSION → ENERGY → SPACE`.

| Macro | Current bounded effect | Protected material |
|---|---|---|
| TENSION | Adds up to 12 velocity to a weak-beat note when its next later onset is within two semitones. The boost scales linearly with the macro value. | Pitch, onset, event count and strong-beat attacks remain unchanged. No new passing notes are invented without harmonic context. |
| ENERGY | Scales every event's velocity from 100% to 150%. For ghost events, probability moves toward 100% as Energy rises. | Existing event count, onsets and pitches stay unchanged; velocity and probability clamp to legal MIDI ranges. |
| SPACE | Scales gate duration from 100% down to 50%. Ghost-event probability scales from 100% down to 0%. Events remain present in the candidate even at probability zero. | No onset or pitch is removed or moved; duration never reaches zero. |
| MUTATION | For each weak-beat event in the known project scale, a seed-derived decision may shift the pitch by one scale step up or down. The macro value is the per-event selection threshold. | Strong-beat notes, out-of-scale notes, event count, onsets and durations remain unchanged. Unknown harmony disables pitch mutation. |

Energy and Space compose in that order: Energy raises ghost probability, then Space attenuates it. Velocity boosts saturate at 127. The caller must publish the completed candidate through the existing playback owner at a safe musical boundary; the engine itself does not replace a live buffer.

## Boundaries and follow-up

- Macro processing is not yet wired to the user interface or a live playback publication point; that integration belongs to 0.9.29.
- Tension uses a local stepwise approach heuristic. It does not add dissonant passing notes or infer chords. Harmonic-aware tension remains gated on the 0.9.30 harmonic context and conservative fallback.
- Mutation follows the known project scale, not a chord-specific scale. Unknown harmony skips pitch mutation; an invalid declared key rejects the candidate and leaves the destination untouched.
- Performance quality is a data-level hypothesis only. The macro response has not been auditioned on hardware.
- The macro state is deliberately absent from Scene serialization. There is no hidden write and no implicit Commit action.
- `apply()` is control-side work and must not run in the audio callback. Publish its finished destination only after it is complete and at a safe musical boundary.
