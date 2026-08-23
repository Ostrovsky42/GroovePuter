# 0.9.9-P1c — Song Effective Pattern Ref Boundary

## Purpose

P1c separates canonical Song Pattern references from the Pattern reference used
for current Song playback/selection. The checkpoint adds one MiniAcid resolver
boundary so a future PatternLease-backed audition can substitute an already
owned physical Pattern without mutating the Song model or adding another
allocator.

## Contract

- **Persistent Pattern ref** is canonical Song model state.
- **Effective Pattern ref** is a playback/selection view for the current Song
  cell.
- The effective ref is resolved once, as a global Pattern ref, **before**
  PAGE/BANK/SLOT decomposition.
- `songPatternIndexForTrack()` and `applySongPositionSelection()` consume the
  same resolver. Song display-reflection helpers use the same boundary as well.
- The future audition identity is the existing tuple
  **Song playback slot + Song row/position + SongTrack**. This isolates cells
  that happen to share the same persistent Pattern ref and does not require a
  new global Song-cell ID.
- The Voice lane is not physical Pattern storage and remains outside this
  resolver.
- Rehearsal/pause sentinels and public Song ref getters remain canonical model
  state, not effective audition state.

## Ownership boundaries

P1c does not allocate, copy, reclaim, clear, persist, or transfer Pattern
backing. PatternLease allocation/lifecycle remains owned by
`PatternLeaseOwner`, including discard and persistent-transfer ordering.

P1c does not change Undo ownership or scene persistence. Ending an effective
view simply exposes the unchanged persistent Song ref again.

The runtime regression uses one compile-time test seam. Production builds have
no P1c override state at this checkpoint; the production resolver therefore
returns the canonical Song ref unchanged.

## Out of scope

- Pattern Picker UI and shortcuts
- RELATED generation
- generation/evolution semantics
- new Pattern allocator or GC
- PatternLease capacity/shape changes
- canonical Song mutation for preview
- standalone preview / `songMode=false` behavior
- Picker KEEP transaction changes

## Hardware list

N/A. P1c is a host-testable MiniAcid selection boundary and does not introduce
hardware semantics.

## Wiring

N/A.

## Build / validation

Focused and cumulative P1 contracts:

```sh
bash tests/run_0_9_9_p1c_tests.sh
```

Full host regression suite:

```sh
bash tests/run_host_tests.sh
```

SDL production build:

```sh
make -C platform_sdl clean all CXX=g++
```

Existing Cardputer ADV and SEQTRAK MIDI-only compile gates remain the firmware
validation path; P1c makes no hardware-runtime claim without an actual run.

## Expected behavior

Without an active override, Song playback, display-reflection, and current
PAGE/BANK/SLOT selection are unchanged. In the host-only override seam, the
selected Song cell reads material from the effective global Pattern ref and
reflects that same ref in bank/slot state while `songPatternAtSlot()` continues
to report the original persistent ref.

Two Song rows that share one persistent Pattern ref remain isolated: an
override keyed to one row does not affect the other row.

## Troubleshooting

- If material changes but displayed bank/slot does not, check that
  `applySongPositionSelection()` resolves before decomposition.
- If display helpers show the persistent ref during the host override, check
  that they consume the same resolver rather than reading the Song directly.
- If another row changes because it shares the same persistent Pattern, the
  override identity is too weak; it must include playback slot, row, and track.
- Any allocation/reclaim/lease cleanup in MiniAcid is a scope violation for
  P1c; that ownership belongs to `PatternLeaseOwner`.

## Acceptance checklist

- [ ] One canonical `effectivePatternRef` boundary exists in MiniAcid.
- [ ] Resolution precedes PAGE/BANK/SLOT decomposition.
- [ ] `songPatternIndexForTrack()` uses that boundary.
- [ ] `applySongPositionSelection()` uses that same boundary.
- [ ] Song display reflection uses the same effective global ref.
- [ ] No-override behavior remains unchanged.
- [ ] Override material and reflected bank/slot agree.
- [ ] Canonical Song refs remain unchanged.
- [ ] Shared persistent refs do not leak an override across Song rows/tracks.
- [ ] Synth A, Synth B, and Drums are covered.
- [ ] No allocator, GC, Undo owner, or persistence owner is added.
- [ ] No P4I Picker/standalone-preview behavior is changed.
- [ ] Focused, full-host, SDL, and existing firmware compile gates are green.
