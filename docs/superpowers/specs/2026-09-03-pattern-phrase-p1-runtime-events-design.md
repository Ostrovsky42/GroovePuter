# PATTERN / PHRASE P1 Runtime Events Design

Status: **APPROVED FOR IMPLEMENTATION**

## Base

P1 is stacked on the frozen P0 characterization head:

```text
c02d9ae04fcd43b5e3ced11e5aa50e850e26b4e6
```

P0 itself is based on post-SAM2695 `main`:

```text
6694876edff654bc0e14cafd3181c7ff2ff5060e
```

## Goal

Introduce a fixed-capacity, immutable-at-consumption synth playback event representation and a pure legacy `SynthPattern` projector. P1 must create no audible/runtime behavior change: `MiniAcid` continues consuming `SynthPattern` directly.

The representation is the future common seam for mutually-exclusive Song-fragment sources:

```text
PATTERN -- read-only projection --\
                              RuntimeSynthEventBuffer
PHRASE  -- owned events -------/
```

P1 implements only the PATTERN projection side and the shared value types. PHRASE storage is not introduced yet.

## Explicit non-goals

P1 does **not** modify:

- `MiniAcid::processSequencerEvents()` or gate countdown execution;
- Song source ownership;
- `PhraseCore::PhraseBank` persistence;
- `StorageMode::OwnedEvents` storage;
- Scene schema/version;
- Undo/Redo;
- `AudioMutationGate` publication;
- MAKE PHRASE;
- Phrase UI/editor;
- GRID semantics;
- drums;
- Synth C, FM/DX;
- MIDI routing or the USB/DIN tee.

## Runtime value model

Namespace: `PhraseRuntime`.

Constants:

```cpp
kTicksPerBar = 384
kSubticksPerTick = 16
kMaxPhraseBars = 8
kMaxSynthEvents = 128
```

`RuntimeSynthEvent` is fixed-capacity and trivially copyable:

```cpp
struct RuntimeSynthEvent {
  uint16_t startTick;
  uint16_t durationSubticks;
  uint8_t note;
  uint8_t velocity;
  uint8_t probability;
  uint8_t flags;
  uint8_t fx;
  uint8_t fxParam;
};
```

`flags` carries only the existing Pattern articulation bits needed by future playback: accent, slide, ghost. No owner, pointer, source address, physical Pattern index, Undo token, or backend identity is stored in an event.

`RuntimeSynthEventBuffer` contains `RuntimeSynthEvent events[128]`, `count`, and `lengthTicks`. It is caller-owned, trivially copyable and has no heap allocation. P1 freezes `sizeof(RuntimeSynthEvent) == 10` and `sizeof(RuntimeSynthEventBuffer) == 1284` on the host ABI used by CI.

`startTick` is an integer 96-PPQN phrase coordinate. `durationSubticks` uses 1/16 tick precision. At 8 bars, the full phrase is 3072 ticks / 49152 subticks, within `uint16_t`.

## Pattern projection API

```cpp
struct PatternProjectionSettings {
  uint8_t synthIndex;
  uint8_t swingPercent;
  bool swingEnabled;
  float gateLengthRatio;
};

enum class PatternProjectionStatus : uint8_t {
  Ready,
  InvalidSynthIndex,
};

PatternProjectionStatus projectPatternToRuntimeEvents(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination);
```

The function builds a local candidate and assigns `destination` only on success. Invalid `synthIndex` leaves caller state untouched.

## Trigger-time compatibility

P1 must mirror the existing MiniAcid Pattern trigger search, not replace it with an idealized scheduler.

For every `barTick` from 0 to 383:

```text
nominalStep = barTick / 24
scan sIdx = nominalStep - 1 .. nominalStep + 1
s = (sIdx + 16) % 16
nominalT = s * 24
trigger = (nominalT + swing + timing + 384) % 384
```

Only a step found by this same scan becomes a token. This deliberately preserves current behavior for existing timing values, including the scheduler's bounded neighbor search.

Swing is clamped to the current 50..75 percent range. Odd steps receive the same integer delay calculation as MiniAcid when `swingEnabled` is true. `gridSteps` is not an input and must not appear in the projector.

Projected note events are ordered by actual trigger time, not physical step index.

## Gate-duration compatibility

The existing gate policy is converted from samples-per-step into musical time without changing its formula:

```text
gate = gateLengthRatio
if gate < 0.1 -> 0.5
voice multiplier: A = 0.85, B = 1.05
A minimum effective gate = 0.15
B maximum effective gate = 0.98
baseDurationTicks = 24 * effectiveGate
baseDurationSubticks = round(baseDurationTicks * 16)
```

The fixed-point representation is bounded to at least one subtick.

The conversion is representation-only in P1. MiniAcid still uses its existing sample countdown.

## Legacy TIE projection

`SynthStep::note == -2` remains an input compatibility sentinel only. It never becomes a `RuntimeSynthEvent`.

For each projected onset, the projector walks subsequent trigger tokens in chronological order, wrapping once into the next Pattern cycle. A TIE extends the event by one base gate duration only when it occurs while that event is still active. A subsequent note onset terminates the previous monophonic lifetime at that onset if the previous duration would overlap it.

This makes the P0 crossing fixture explicit data:

```text
step15 note timing +23 -> startTick 383
step0 TIE timing +1    -> next-cycle absolute tick 385
result                  -> one event whose end is > tick 384
```

The event is still a projected legacy PATTERN event, not authoritative PHRASE storage.

## Ownership and publication

P1 adds no global/singleton runtime owner. Buffers are caller-owned values. No pointer into Scene/Pattern storage is retained. No mutation occurs in the audio callback because P1 is not wired into the callback at all.

A later PR will prepare buffers off the audio path and publish them atomically. That later publication must use the existing `AudioMutationGate` contract and must not create a second MIDI owner.

## Testing contract

Focused P1 tests must prove:

- exact ABI/memory bounds;
- simple onset projection;
- Synth A/B gate scaling;
- P0 swing + microtiming wrap to tick 11;
- P0 negative microtiming wrap to tick 361;
- P0 cross-boundary legacy TIE represented as one event crossing tick 384;
- expired TIE does not revive a note;
- a following onset clips previous monophonic lifetime;
- invalid synth index leaves destination unchanged;
- no heap, Scene persistence, Undo, AudioMutationGate or GRID dependency in P1 files;
- GCC deterministic repeat, Clang parity, ASan and UBSan.

Normal repository SDL/Cardputer/fixed-DRAM/SEQTRAK regressions remain mandatory before P1 is declared GREEN.
