# P2 + P3 Hardware Audition

**Branch:** `agent/20260811-19-p2-p3-hardware-audition`  
**Exact Stage15 base:** `fc42763e7798866e61895bf1b8d62339ec59e0a7`

## Purpose

This is a temporary hardware-audition integration branch. It is not the production admission path for P2/P3 and must not replace the independently reviewed capability PRs.

The branch imports the corrected source contracts from:

- P2 #237 current corrected head `39423a7a66ed1576624d5624341fc7fa7398f353`;
- P3 #238 current corrected head `eb697d9916f4faa64b50428c0a705d7c2948f0ce`.

The audition layer answers one question: can the two capabilities compose without introducing a second rhythm or harmonic owner before we expose deterministic fixtures on Cardputer hardware?

## Composition ownership

```text
P2 64-step phrase timeline
  owns/validates:
    audible onset topology
    continuation topology
    release topology
        |
        v
P3 per-bar N/S identity
  resolves only:
    N = source advance
    S = same-source retrigger
        |
        v
Audition bar plan
  audibleOnsets
  continuation/release masks
  global harmonic source ordinal per audible onset
```

P2 is the sole gate-topology validator in the composed path. P3 is invoked with N/S masks and explicit incoming-source availability only; continuation/release masks are not independently reinterpreted by P3 at a bar boundary.

This avoids two incorrect models:

1. treating every audible onset as a new harmonic event;
2. requiring a one-bar P3 validator to rediscover cross-bar gate state already owned by P2.

## Deterministic fixtures

### 1. RETRIGGER

```text
N---S---N---S---
source 0  0   1   1
```

Expected: four audible attacks, two harmonic source advances.

### 2. DENSE_RETRIGGER

```text
NSSSSSSSSSSSSSSS
```

Expected: sixteen audible attacks, one harmonic source advance.

### 3. CROSS_BAR_HOLD

```text
bar 1: ------------NCCC
bar 2: CCCC R-----------
```

`C` is explicit continuation and `R` explicit release. The bar boundary itself creates neither a new onset nor a new harmonic source.

### 4. MULTI_BAR_NS

Four bars containing N/S actions. Bars 2 and 4 deliberately begin with `S`; those attacks must map to the source identity established in the preceding bar through explicit incoming-source composition.

## Current branch stage

Implemented now:

- corrected P2/P3 capability sources imported from the current PR heads;
- four deterministic fixture definitions;
- P2 whole-phrase gate validation;
- P3 per-bar source-identity validation;
- phrase-global source ordinal mapping;
- GCC/Clang/ASAN/UBSAN host matrix;
- Cardputer ADV normal + MIDI-only compile gates.

Still intentionally separate from this composition core:

- Cardputer key binding / temporary audition UI;
- writing fixtures into temporary Synth B/song slots;
- hardware listening acceptance;
- any H6 R2 vocabulary or production routing.

## Hardware binding target

The next binding layer may materialize the four plans into temporary Synth B patterns/song rows for audition. It must preserve these rules:

- `harmonicEventOnsets = N`, never `N | S`;
- `audibleOnsets = N | S`;
- continuation steps must not become new attacks;
- bar boundaries must not reset source identity or invent holds;
- only temporary audition state may be modified; no Scene persistence contract changes;
- no H6 progression/rhythm candidate admission.

The audition branch may use deterministic notes to make source changes audible. Chord-quality/polyphony from P1 is not required to validate P2/P3 timing semantics.
