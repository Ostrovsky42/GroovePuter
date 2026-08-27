# 0.9.9 PHRASE-C2-C0 — Cross-Bar Melodic Boundary Topology

## Purpose

Characterize the frozen P1R production-reachable post-admission melodic boundary topology without changing production musical semantics.

## Frozen P1R base

`016bcd6ba514b3a57f8803c63c869f1b2a8953a7`

P1R decision: `DECISION A — PRODUCTION PHRASE EXECUTION READY`.

## Question

Does the existing production corpus contain a natural pure-melodic boundary with semantic occupancy at step 15, an initial gap in the next bar, and a later admitted onset of the same melodic voice, without adding duration, articulation, hybrid arbitration, or vocabulary policy?

## Non-goals

No lifetime producer, Continue/Release policy, gate change, transport/MIDI/internal-synth runtime change, Song/Bank publication, new melodic vocabulary, hardware listening, or production probe.

## Production firewall

`src/` must remain byte-identical to the frozen P1R base. The focused runner executes:

```sh
git diff --exit-code 016bcd6ba514b3a57f8803c63c869f1b2a8953a7 -- src/
```

## Corpus construction

The authoritative observation path is test-side deterministic replay of the same public production owners used by strong-rhythm materialization:

1. exact frozen phrase selection;
2. production rhythm realization;
3. BassRhythm admission;
4. ChordRhythm admission;
5. MelodicMotif admission;
6. MelodicPitchIntent admission;
7. hybrid-only monophonic chord masking when applicable;
8. full `preparePhraseExecution()` plus `materializePreparedPhraseBar()` replay for representative witnesses.

Raw melodic rhythm names or vocabulary masks are never used as the final classifier.

## Production dependency inventory

Topology dependencies are the selected composition/profile, rhythm archetype/family, exact phrase length admission, frozen selection and realization generation coordinates, realized kick topology, role-specific protected space, BassRhythm result, ChordRhythm result, semantic Synth-B role, MelodicMotif result, bar/vocabulary coordinate, sparse-bar legality, and MelodicPitchIntent result.

Current production tonal profiles allow only `MelodicRhythmOperationId::Preserve`; contour and motif pitch operations do not move onset/continuation coordinates. This is part of the source-level sufficiency argument and is validated by focused replay.

## Exhaustiveness argument

Final corpus bounds and any reduction are recorded after the characterization pass. No unreachable decision is permitted from a random/bounded seed search. A positive Class A decision requires only one reproducible production-reachable witness, but frequency tables are reported against their explicit corpus denominator.

## Signature definition

Boundary signatures are post-observation deduplication keys. They include the topology-driving production role/status and admitted masks plus phrase/boundary coordinates. They are not presented as a pre-realization shortcut.

## Signature sufficiency proof

Pending final corpus pass.

## Signature collision validation

Pending final corpus pass.

## Observation schema

Each retained representative records generation settings/identity, requested/effective phrase length, archetype, progression/source period, temporal coordinates, semantic role, melodic rhythm/motif/status, admitted onset/continuation masks, final occupancy, step-15 bits, incoming step-0 bit/first onset/empty state, and informational harmonic ranges. Hybrid records additionally retain chord/melodic occupancy and preemption order.

## Semantic occupancy vs physical gate

Semantic step occupancy is the C2-C0 classifier. Physical gate lifetime is characterized separately and is not allowed to change the semantic class.

## Pure melodic voice ownership

Bootstrap Class A requires `SemanticSynthBRole::Melodic` on both sides of the ordinary intra-phrase boundary. Hybrid Synth B is classified separately.

## Class A overview

Pending final corpus pass.

## A-onset

Pending final corpus pass.

## A-continuation

Source dependency audit currently shows no production tonal rhythm operation capable of extending the existing continuation vocabulary to step 15. Final status is frozen only after replay validation.

## A-overlap

Pending final corpus pass. Onset and continuation overlap is never silently collapsed.

## Class B

Pending final corpus pass.

## Class H

Pending final corpus pass.

## Negative controls

N0: incoming same-voice onset at step 0.

N1: incoming `ValidButEmpty`.

N2: phrase end / loop wrap, never bootstrap continuation.

N3: outgoing semantic occupancy ends at or before step 14; crossing would require pre-boundary extension.

## Known M1L hardware fixtures

The existing sparse `{2}/empty/...` fixture is expected to map to N1 at empty incoming bars. The existing call-style `{6,14}` fixture is expected to map to B/N3 when no admitted continuation reaches step 15. These are hypotheses to map against frozen production behavior, not evidence used to manufacture Class A.

## Raw occurrence counts

Pending final corpus pass.

## Unique topology counts

Pending final corpus pass.

## Frequency by profile

Pending final corpus pass.

## Frequency by phrase length

Pending final corpus pass.

## Production-default reachability

Pending final corpus pass.

## Intra-segment distribution

Pending final corpus pass.

## 3->4 evolution seam distribution

Pending final corpus pass.

## Representative witnesses

Pending final corpus pass.

## Physical gate characterization

Pending representative Class A replay. C2-C0 records current gate behavior but does not alter gate multipliers, NoteOff scheduling, AllNotesOff, transport, MIDI, or internal synth lifetime.

## Optional H1-F1/H2 witness

Pending informational search. Absence does not affect the semantic decision.

## Limitations

No hardware listening is performed in C2-C0. Non-default generation-attempt coverage and any frequency denominator are stated explicitly in the final report rather than implied to be universal.

## Validation

Required focused gates: frozen `src/` firewall, GCC, deterministic repeat, Clang, ASan, UBSan, corpus/signature/class-count determinism, collision validation, and P1R compatibility where applicable.

## Provenance

Branch: `research/20260827-05-0.9.9-phrase-c2-c0-boundary-topology`

Frozen base: `016bcd6ba514b3a57f8803c63c869f1b2a8953a7`

## Hardware

Host characterization only. No Cardputer ADV, SEQTRAK, external display, MIDI cable, or audio output is required for this checkpoint.

## Wiring

None.

## Build / run

```sh
bash tests/run_0_9_9_phrase_c2_c0_tests.sh
```

## Expected behavior

The runner prints a deterministic post-admission corpus report and one final research decision while leaving `src/` unchanged.

## Troubleshooting

A raw rhythm with step 15 is not evidence. Verify the admitted masks after Bass/Chord/protected-space handling and the final semantic Synth-B role. A negative A result is a characterization result, not automatically a test failure.

## Acceptance checklist

- exact frozen P1R base;
- separate research branch/worktree lineage;
- zero `src/` delta;
- post-admission production observation;
- pure vs hybrid role separation;
- step-15 onset/continuation/overlap separated;
- incoming step 0, later onset, ValidButEmpty and phrase end explicit;
- raw occurrences and unique signatures reported;
- profile/length/default-path and intra/seam distributions reported;
- source dependency and collision sufficiency evidence recorded;
- representative full P1R replay;
- physical gate observed, never changed;
- no producer/runtime/hardware changes;
- deterministic compiler/sanitizer gates terminal.

## Decision

Not frozen yet. C2 producer remains unauthorized until this document records terminal focused evidence and an exact C2-C0 SHA.
