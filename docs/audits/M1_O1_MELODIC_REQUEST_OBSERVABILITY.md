# M1-O1 — melodic request observability

## Purpose

M1-O1 provides a focused, compile-time test probe for the exact
`MelodicMotifRequest` built by `migrateStrongRhythmMaterial()` immediately
before `realizeMelodicMotif()`.  It observes the already-derived request; it
does not reproduce selection, seed, bass, or protected-space logic.

## Exact ancestry

Base: `bb3984ccb74c674521b720d18d72c48b4238353b` (M1-T1F2).

This evidence closes O1 only.  It does not make an A2 acceptance decision and
does not begin M1 production wiring.

## Probe architecture

The only seam is guarded by `GROOVEPUTER_M1_TEST_PROBE` in
`strong_rhythm_migration.*`.  With the macro enabled, caller-owned
`StrongRhythmMelodicRequestProbe` receives one value-copy of the completed
`MelodicMotifRequest` directly before realization.  With the macro undefined,
the declaration, storage, setter, and copy site are preprocessor-eliminated.

Neither `StrongRhythmMigrationResult` nor `MelodicMotifRequest` gained a
field, and normal builds expose no getter or diagnostic API.

## Functional proof

The focused test captures and replays production requests for:

| Fixture | Production selection | Result |
| --- | --- | --- |
| P1 | Electro/base/address 19; Melodic, SparseCall, Mirror, 4 bars | PASS |
| P2 | Acid/base/address 0; Melodic, RepeatedCell, Pivot, 4 bars | PASS |
| P3 | Electro/base/address 7; Melodic, DelayedAnswer, CallResponse, 4 bars | PASS |

The preserved P3 anchor remains exact.  For each capture, four local request
copies replay through `realizeMelodicMotif()` with only `barOrdinal` changed to
0, 1, 2, or 3; field-wise checks reject any other meaningful change.

This establishes a legitimate future A2 experiment.  It deliberately does not
classify SparseCall space or RepeatedCell bar variation.

## Non-interference and normal build proof

Focused CI compiled the same three migration fixtures with the probe disabled
and enabled, then compared field-wise observable migration status/IDs, drum
material, Synth A, and Synth B.  Outputs were identical.

Normal host and Cardputer binaries contain no
`StrongRhythmMelodicRequestProbe` or
`setStrongRhythmMelodicRequestProbe` symbol; the probe-enabled test binary
contains the setter symbol.  This verifies normal-build preprocessor
elimination.

`StrongRhythmMigrationResult` and `MelodicMotifRequest` normal ABI layout is
unchanged because neither type was modified.  The normal Cardputer ADV build
passed the repository fixed DRAM-budget gate.  The guarded code is absent from
that binary, so its probe-attributable DRAM delta is zero.

## CI evidence

Candidate SHA: `36d99c3885d349dce912f8a14efe115d2e51ba18`.

- [O1 observability candidate run](https://github.com/Ostrovsky42/GroovePuter/actions/runs/32958055616): PASS
  - focused GCC, Clang, ASan/UBSan, probe ON/OFF, and host symbol gate: PASS
  - normal SDL and SEQTRAK MIDI-only: PASS
  - normal Cardputer ADV, fixed DRAM budget, and Cardputer symbol absence: PASS
- [Core regressions run](https://github.com/Ostrovsky42/GroovePuter/actions/runs/32958055610): PASS
  - host tests, SDL, Cardputer ADV/fixed DRAM, and SEQTRAK MIDI-only: PASS

## Decision

**DECISION A — OBSERVABILITY READY.** The exact production request is now
available to focused tests without a normal-build semantic, ABI, runtime-state,
or probe-symbol delta.

## Next checkpoint and hard stop

Resume M1-A2 from this evidence to classify P1/P2/P3 using captured production
requests.  Do not implement M1, change `kProfiles`, or start M1L here.
