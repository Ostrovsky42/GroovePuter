# Harmonic Follow — 0.9.30 audit

## Result

The generation path already follows prepared harmony. Milestone 0.9.30 reuses that path and does not add a competing chord model or change persisted project data.

The accepted generation flow is:

```text
Song / Genre request
  -> prepared Legacy progression or PreparedGraphHarmony
  -> phrase harmonic clock (event positions)
  -> role-specific pitch policy
  -> generated Pattern candidate
  -> transactional materialization / commit
  -> Scene remains the editable source of truth
```

For Graph material, Song harmony is stored separately as a bounded `SongHarmonicState`. A playback snapshot is available only when the saved route still matches the exact Song participants, material IDs and Pattern versions.

## Existing constraints

- `HarmonicRhythmPlan` owns when chord events change; the progression or Graph route owns what the chord is.
- `rolePitchPolicy()` constrains Bass and Chord roles to chord tones and allows Lead the active scale plus chord tones.
- Graph projection changes pitches on a staged candidate. It retains rhythm, gates, velocities, articulation and other Pattern fields, and leaves the destination unchanged on error.
- Candidate harmony is applied during explicit generation before commit. It is not re-applied to an already accepted Scene Pattern when playback harmony changes.
- Invalid or stale context is rejected. `AUTO` may resolve to the existing Legacy route before materialization when Graph capability is unavailable.

## Live macro boundary

The 0.9.29 sequencer macro path currently receives only the project key and scale. `SongHarmonicState` is persisted through `PatternPagingService`; reading it in the sequencer callback would violate the real-time contract. The existing harmony snapshot bridge serves Performance keyboard context and is not yet a coherent chord publication to the sequencer macro owner.

Therefore live chord-aware macro TENSION is deferred. Implementing it requires a bounded control-side publication tied to the currently playing material and event position. Until then, TENSION remains the tested local next-onset heuristic and MUTATION uses known project key/scale only. No chord-level listening claim is made.

## Verification evidence

- `bash tests/run_hgs_tests.sh` — passed, including Graph route builder, role policy, candidate projection, Song state, persistence, storage placement/version checks and live snapshot validation.
- `bash tests/run_performance_macro_engine_tests.sh` — passed; unknown harmony leaves macro-mutated pitches unchanged.
- `bash tests/run_0_9_9_phrase_p1r_tests.sh` — focused P1R source firewall, semantic seams, Graph materialization boundary, deterministic repeat, ASan and UBSan gates passed. The aggregate then failed in its separate older M1-P1 test on a stale expected melodic identity; the same failure reproduces at the exact 0.9.24 baseline. Details are recorded in `VERSION_LOG.md`.

These are host-side checks. They do not establish sound quality or hardware behavior.
