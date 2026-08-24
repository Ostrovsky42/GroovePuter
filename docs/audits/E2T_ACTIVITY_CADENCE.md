# 0.9.9-E2t Rhythm Evolution Activity / Cadence

Status: **DRAFT CHECKPOINT — DO NOT MERGE**

Exact base: `d94014f9549869d7b4964c7cd6277b69dd34bcf2` (E0a).

## Purpose

Introduce one pure bounded policy that answers only:

> When may rhythm evolution be attempted?

E2t does not decide mutation depth, mutation kind, lifecycle action, candidate choice, scheduling, transport ownership, or persistence.

The two independent axes remain:

- `EvolutionActivity` = **how often** an attempt is permitted;
- `RealizationLevel` (`P1/P2/P3`) = **how far** a realization may change.

The cadence result vocabulary is intentionally only:

- `Hold`
- `Attempt`

No `KEEP`, `REVERT`, or `EVOLVE` lifecycle meaning is introduced.

## Existing policy audit

At the exact E0a base there is no canonical evolution Activity type.

Existing nearby concepts have different ownership:

- `GenreSettings` stores generative mode, recipe/morph and rhythm selection state; it has no evolution Activity field.
- `PhraseEvolutionLawId` describes phrase-shape planning (`Loop`, `RepeatReply`, `DevelopReturn`, `SparseDrift`), not cadence frequency.
- `RealizationLevel` (`P1Canonical`, `P2Variation`, `P3Transformation`) drives mutation/realization depth and budget, not timing.
- `generationAttemptOrdinal` remains reroll/retry identity and is not an E2t input.
- E0a `phraseBarOrdinal` and `evolutionOrdinal` remain the explicit temporal coordinates.

E2t therefore adds a separate transient `EvolutionActivity` policy rather than reusing any UI or generation symbol.

## Policy

`EvolutionActivity` is bounded:

| Activity | Eligible segment boundaries |
|---|---:|
| `Off` | 0 / 4 deterministic buckets |
| `Low` | 1 / 4 deterministic buckets |
| `Medium` | 2 / 4 deterministic buckets |
| `High` | 4 / 4 deterministic buckets |

A cadence decision is legal only at the start of a four-bar E0a evolution segment.

The policy fails closed to `Hold` when:

- activity is invalid;
- `phraseBarOrdinal` is not a four-bar boundary;
- `phraseBarOrdinal / 4 != evolutionOrdinal`.

For `Low` and `Medium`, the bucket is deterministic from:

- `GenerationContext.projectSeed`;
- `GenerationContext.phraseOrdinal`;
- `evolutionOrdinal`.

It does not read or accept:

- `patternAddress`;
- Song transport-local position;
- `generationAttemptOrdinal`.

## 8-bar interpretation

E0a remains authoritative:

```text
physical bars       0 1 2 3 | 4 5 6 7
evolutionOrdinal    0 0 0 0 | 1 1 1 1
cadence boundary    ^       | ^
```

Thus 8 bars remain **4+4**, not one eight-bar cadence trajectory.

`High` produces `Attempt` only at physical bars `0` and `4`; every other physical bar is `Hold`.

## Determinism contract

Required:

```text
same EvolutionActivity
same phraseBarOrdinal
same evolutionOrdinal
same GenerationContext.projectSeed
same GenerationContext.phraseOrdinal
=> bit-identical Hold/Attempt decision
```

The focused golden masks over evolution ordinals `0..15` for:

```text
projectSeed   = 0x0E2C0A9E
phraseOrdinal = 17
```

are:

```text
Off     = 0x0000
Low     = 0xA401
Medium  = 0xA6D1
High    = 0xFFFF
```

Changing only the stable generation context changes the deterministic cadence stream:

```text
projectSeed + 1 -> Low = 0x8420
phraseOrdinal + 1 -> Low = 0x1040
```

These are cadence decisions only. No realization or mutation is executed by E2t.

## Hardware list

Focused E2t policy test:

- no hardware required;
- Linux/macOS host with C++17 compiler and Python 3.

Regression build coverage additionally compiles the existing Cardputer ADV target in CI, but E2t adds no hardware runtime behavior.

## Wiring

None.

E2t is a pure host-testable generation policy and has no GPIO, MIDI, I2C, SPI, display, or transport wiring.

## Build / flash steps

Focused host contract:

```bash
bash tests/run_0_9_9_e2t_tests.sh
```

Full host regressions:

```bash
bash tests/run_host_tests.sh
```

Cardputer ADV compile-only validation:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
```

No flash is required for E2t acceptance because the policy is not wired into runtime execution in this checkpoint.

## Expected behavior

Focused test output includes:

```text
E2T_ACTIVITY OFF=0000 LOW=A401 MEDIUM=A6D1 HIGH=FFFF
E2T_8_BAR 0..3=segment0 4..7=segment1 decision-points={0,4}
0.9.9-E2t activity/cadence: PASS
```

The source regression must also print:

```text
0.9.9-E2t source regressions: PASS
```

## Troubleshooting

If the golden masks change, inspect only the cadence seed/context or bucket policy. Do not compensate by changing `RealizationLevel`, mutation budgets, Pattern addresses, Song transport state, or retry ordinals.

If an 8-bar test permits attempts at bars other than `0` and `4`, the E0a 4+4 interpretation has been broken.

If the policy needs `patternAddress`, `songBarIndex`, or `generationAttemptOrdinal`, reject that wiring: those coordinates have different ownership.

If a future caller needs lifecycle semantics (`KEEP/REVERT/EVOLVE`), add them in a later lifecycle checkpoint rather than extending `EvolutionCadenceDecision` here.

## Acceptance checklist

- [ ] Branch starts exactly at `d94014f9549869d7b4964c7cd6277b69dd34bcf2`.
- [ ] `EvolutionActivity` is bounded to `Off/Low/Medium/High`.
- [ ] Result vocabulary is only `Hold/Attempt`.
- [ ] `RealizationLevel` is absent from the cadence API.
- [ ] `patternAddress` is absent from the cadence API.
- [ ] Song transport-local position is absent from the cadence API.
- [ ] `generationAttemptOrdinal` is absent from the cadence API.
- [ ] Invalid/non-boundary/inconsistent temporal input fails closed to `Hold`.
- [ ] 8 bars preserve E0a 4+4 decision points at bars `0` and `4`.
- [ ] Same Activity + temporal coordinates + generation context is bit-identical.
- [ ] No scheduler, transport owner, mutation executor, lifecycle state, candidate chooser, persistence, or UI is added.
- [ ] Focused E2t tests pass.
- [ ] Full host regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV compile and fixed-DRAM budget checks pass.
- [ ] Draft PR remains unmerged.
