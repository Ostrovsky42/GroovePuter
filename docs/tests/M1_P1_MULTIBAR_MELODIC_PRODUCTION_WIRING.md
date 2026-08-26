# M1-P1 — multi-bar melodic production wiring

## Purpose and ancestry

Base: `e67182fcf67acf8aa573a7b419585fb962c11f39` (M1-A2).

M1-P1 fixes the pre-M1 address leak only on the explicit four-bar phrase path:
one composition resolution is frozen, then four bars are materialized with
explicit ordinals. Ordinary one-bar migration remains compatible.

## Ownership and identity

`strong_rhythm_migration.*` owns `StrongRhythmFrozenSelection`. It contains
the existing `GenerationCompositionResult`, route, selection generation
identity, realization generation identity, and an explicit logical phrase
identity. It is bounded, caller-owned, and ephemeral.

For ordinary wrappers, the compatibility selection still uses the historical
pattern address identity. For M1 phrase materialization,
`phraseGenerationIdentity` is explicit and `patternAddress` is validated only
as physical storage. All role requests inherit the frozen realization
generation identity; `phraseBarOrdinal` is the sole varying musical-time
coordinate.

The Song B four-bar path resolves once using its logical Song-B identity,
then materializes bar ordinals 0..3 through the frozen selection. It retains
the existing bank/song/transport owner.

## Four-bar results

Focused production test uses the A2 authoritative fixtures.

| Contract | Result |
| --- | --- |
| T1 storage-address invariance | PASS: ranges 40..43 and 120..123 are field-wise equal |
| T2 one selection | PASS: one frozen composition supplies all four bars |
| P1 SparseCall physical space | PASS: Synth B note counts `1,0,1,0` |
| P2 physical differentiation | PASS: non-empty and empty bars are distinct |
| P3 CallResponse physical | PASS: Synth B note counts `2,2,2,2` |
| C control | PASS: one ordinary bar generated once then copied: `1,1,1,1` |
| W phrase | PASS: frozen selection materialized: `1,0,1,0` |

The P1 test pre-fills empty-destination bars with a note sentinel; normal
tonal materialization replaces them with empty Synth patterns. No mute or
post-clear policy was added.

## Failure atomicity and exclusions

Before M1 Song B four-bar materialization, the bridge snapshots the four
fixed Bank B candidates. On a materialization failure they are restored before
Song B publish. No heap allocation or persistent phrase cache is introduced.

No cross-bar sustain (M2), harmonic crossing (M3), phrase-length policy (M4),
composition policy, `kProfiles`, or melodic-role policy is changed.

## Verification

`tests/run_0_9_9_m1_p1_tests.sh` runs GCC repeat determinism, Clang parity,
and ASan/UBSan. It field-compares meaningful SynthStep fields for range
invariance and validates the revised physical corpus.

Normal host/SDL/Cardputer/DRAM/SEQTRAK gates remain required before a final
software-green decision; they are not hardware listening evidence.

## Next checkpoint and hard stop

After all normal regression gates are green, M1-P1 may authorize M1L. This
checkpoint does not perform listening, add transport behavior, or begin M2/M3/M4.
