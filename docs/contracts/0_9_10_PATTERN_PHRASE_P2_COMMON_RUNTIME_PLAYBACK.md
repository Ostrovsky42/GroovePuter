# 0.9.10 PATTERN / PHRASE P2 — Common Runtime Playback

## Checkpoint

`0.9.10-PATTERN-PHRASE-P2` moves Synth A/B PATTERN playback onto the P1C immutable `PhraseRuntime::RuntimeSynthEventBuffer` representation and establishes one authoritative runtime lifetime decision for internal synth and PatternPlayer MIDI.

P2 is runtime execution only. It does not add a PHRASE source, PHRASE persistence, MAKE PHRASE, GRID behavior, LENGTH behavior, or new musician-facing controls.

## Mandatory base

```text
dev_0.9.10
fa552763d34e0172ceed1d07913743165d9a5867
```

This base already contains:

- PATTERN/PHRASE P1C runtime-event foundation;
- Performance Instrument V1 integration;
- frozen GF2-C2 Gate B measurement closure.

Gate B merge provenance:

```text
fa552763d34e0172ceed1d07913743165d9a5867
├─ 4feac178513102f28d4d1aab5a3bba679f0e6348
└─ c646c2c06ff15d6838de2146f98511a64564f64e
```

## Goal

After P2 the authoritative Synth A/B PATTERN execution path is:

```text
persistent / already-published PATTERN material
        ↓ control-side fixed projection
PhraseRuntime::RuntimeSynthEventBuffer
        ↓
one runtime playback/lifetime owner
        ├─ internal synth
        └─ PatternPlayer MIDI
```

There must not be one lifetime decision for the internal voice and another independent lifetime decision for MIDI.

## Runtime ownership

P2 introduces one bounded, heap-free runtime playback state per Synth voice. It consumes immutable runtime events and owns only runtime execution state:

- selected immutable source;
- next event cursor;
- currently active note identity;
- absolute release deadline;
- deterministic release-before-new-onset ordering;
- hard-barrier clearing.

It does not own persistent Pattern data, Song data, generation policy, Undo, UI, or transport.

## Publication rule

The accepted P1/P1C publication rule is binding:

- control-originated mutations project/refresh immutable runtime data outside AudioTask;
- exact AudioTask boundaries may select or release already-prepared bounded state only;
- AudioTask must not copy `SynthPattern`/`SynthStep`, allocate, wait, or build a projection;
- normal Song traversal selects a prepared resident Pattern projection by metadata;
- `NO PATTERN` selects canonical immutable silence;
- the existing quantized-generation pending activation owner remains the only pending owner and may carry already-prepared old-audible runtime material.

No generic second publication system is allowed.

## PATTERN compatibility

P2 preserves the accepted PATTERN musical model:

```text
physical Pattern             16 steps
bar                          384 ticks
step                          24 ticks
GRID 8/16/32                 synth scheduler no-op
legacy TIE                   compatibility input only
probability / ghost          same RNG decision points
swing / microtiming          same trigger ordering
RETRIG                        same event order and count
```

P2 may remove the previously characterized internal-vs-MIDI lifetime asymmetry only by making both backends consume the same runtime lifetime/barrier decision. It must not otherwise redesign PATTERN semantics.

## Boundary and lifetime rules

For current PATTERN playback:

- ordinary physical bar wrap is not itself a release barrier;
- a runtime event whose accepted lifetime continues past 384 ticks may remain active across that wrap (legacy TIE compatibility);
- a new accepted monophonic onset terminates the prior active lifetime before the new NoteOn;
- natural runtime duration expiry releases once;
- Stop, Pause, Mute, explicit Pattern/source replacement, Song physical source transition, page/source identity mismatch, cancel/replace generation transition, and other existing hard lifecycle barriers release fail-closed;
- a hard barrier is one logical decision consumed by both internal synth and MIDI;
- no duplicate NoteOff and no stuck active state are allowed.

P2 does not expose arbitrary new cross-bar note authoring. Explicit PHRASE start+duration comes later and will feed this same owner.

## GF2 firewall

GF2-C2 Gate B is frozen.

P2 must not modify:

- the four authoritative Gate B snapshots;
- Gate B methodology;
- Genre/Recipe semantics;
- DENSITY;
- FEEL;
- phrase-law semantics;
- secondaryRole;
- DEPTH;
- GRID;
- GF2 materialization semantics.

Required result:

```text
GF2 PRODUCTION SEMANTIC DELTA = NONE
```

Existing quantized-generation ownership may be extended only with derived prepared runtime-event data needed to preserve already-accepted audible publication semantics. No generation decision, recipe, material, density, feel, role, or arbitration rule may change.

## Performance firewall

Performance Instrument V1 remains independent. P2 must not change Performance keyboard routing, PERFORM UI, CHORD/ARP/RHYTHM behavior, external-poly policy, or Performance MIDI ownership.

## Explicit non-goals

P2 does not implement:

- PHRASE persistence/storage;
- PATTERN xor PHRASE source switching UI;
- MAKE PHRASE;
- LENGTH 1/2/4/8 semantics;
- GRID snap/zoom semantics;
- new Scene schema;
- new Undo owner;
- new scheduler or audio task;
- new MIDI queue/transport owner;
- new generation or GF2 policy;
- arbitrary musician-authored cross-bar PHRASE notes.

## Acceptance

Focused P2 evidence must prove:

1. P1C runtime-event ABI and projector remain unchanged unless a demonstrated compatibility bug requires an explicit reviewed correction.
2. Runtime playback state is fixed-size, heap-free, deterministic, and backend-neutral.
3. Same-note ordinary PATTERN playback preserves onset order, velocity, accent, slide, probability, ghost, timing and RETRIG behavior.
4. Legacy TIE can still extend an accepted gate over an ordinary 384-tick wrap.
5. Terminating onset produces release-before-new-onset.
6. Natural expiry produces exactly one release.
7. Hard barrier produces exactly one logical release and clears runtime state.
8. Internal Synth A/B and PatternPlayer MIDI consume the same start/release decision sequence.
9. `NO PATTERN` cannot retain stale runtime material.
10. Song BAR_START performs selection/release only; it does not project mutable Pattern bytes.
11. Existing pending-generation publication remains single-owner and old-audible until its accepted activation boundary.
12. P0 characterization is updated only where the old internal/MIDI divergence is intentionally replaced by common lifetime ownership; all unrelated P0 behavior stays invariant.
13. P1C focused gate remains GREEN.
14. Core HOST/SDL/CARDPUTER_ADV/FIXED_DRAM/SEQTRAK validation is GREEN on the final exact P2 head.
15. GF2 exact-head validation remains GREEN with `GF2 PRODUCTION SEMANTIC DELTA = NONE`.

## Stop boundary

P2 stops when common PATTERN runtime playback and lifetime ownership are exact-head proven.

Do not start PHRASE storage, MAKE PHRASE, PATTERN xor PHRASE switching, LENGTH, GRID editor semantics, or the next musician-facing checkpoint inside P2.
