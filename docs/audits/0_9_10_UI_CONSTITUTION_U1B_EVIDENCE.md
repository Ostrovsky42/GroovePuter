# GroovePuter 0.9.10 — UI Constitution U1B Evidence

Status: host/CI semantic-truth checkpoint evidence. Hardware visual/memory acceptance remains separate.

Branch:

`feature/20260906-04-0.9.10-ui-constitution-v1`

This document records historical RED/GREEN evidence. SHA values below are provenance for those observations, not a permanently valid merge pin. The authoritative closure condition is a fresh successful UI Constitution workflow on the actual branch HEAD.

## Scope

U1B separates two semantic facts that were previously conflated in one `UiStatusSource` value:

1. per-target sequenced source — `PATTERN` or `PHRASE` for Synth A/B;
2. transport/playback owner — normal cycle, Song, or SMF.

The UI remains a projection of engine truth. U1B does not create a new musical source owner, does not change Pattern/Phrase playback, does not change renderer geometry, framebuffer strategy, page residency, navigation, themes, Undo, or audio lifetime semantics.

## RED

Historical RED candidate:

`675e47ea2cb7c53a89f49c002cc73cffe874de51`

Workflow run:

`34046665757`

Observed result:

- `u1a-semantic-location`: GREEN;
- `u1b-source-transport-truth`: RED;
- inherited P3-U1 preservation continued independently.

The U1B gate failed because the required independent typed routing axes did not yet exist and `ui_common.cpp` did not yet project authoritative `MiniAcid::currentSequencedSource(0/1)` into status chrome. This was the intended semantic RED, not a harness-only failure.

## GREEN implementation

Historical implementation candidate:

`cf3893a78726f457504743f69aec9eae7cc2c39c`

Workflow run:

`34047157048`

Observed result on that exact candidate:

- `u1a-semantic-location`: SUCCESS;
- `u1b-source-transport-truth`: SUCCESS;
- `preserve-p3-u1-semantics`: SUCCESS.

## Resulting authority model

`UiStatusSource` is removed from the status model.

The replacement is:

```text
UiSequencedSource
    NotApplicable
    Pattern
    Phrase

UiTransportOwner
    Cycle
    Song
    Smf
```

Both values are encoded in `UiStatusRouting`, which remains exactly one byte. `UiStatusSnapshot` remains bounded to at most 16 bytes.

For Synth A/B, sequenced-source truth is projected from:

```text
MiniAcid::currentSequencedSource(0)
MiniAcid::currentSequencedSource(1)
```

Song mode changes transport ownership only. SMF state changes transport ownership only. Neither is permitted to fabricate PAT/PHR source truth.

## Important counterexamples

The model explicitly permits and tests combinations such as:

```text
Synth A + PHRASE + normal cycle
Synth A + PHRASE + SONG transport
Synth A + PHRASE + SMF transport owner
```

This is intentional. `PHR` answers which retained sequenced source belongs to the target; `SONG`/`SMF` answers who owns playback/transport context. One value no longer overwrites the other.

Pattern address is emitted only when the target's `UiSequencedSource` is actually `Pattern`. A Phrase-backed Synth does not borrow a Pattern address merely because Pattern material also exists in the engine.

## Preservation

U1B intentionally does not change:

- Synth `NOTES / KNOBS / MORE` product structure;
- Pattern/Phrase source-selection ownership;
- Phrase event representation or cross-bar lifetime;
- page order or workflow shortcuts;
- PERFORM behaviour;
- renderer/backend/framebuffer policy;
- page residency or eviction policy;
- DRAM ceiling or stack sizes.

The inherited P3-U1 gate succeeding on the GREEN implementation candidate is the host evidence that the semantic status migration did not break the existing Pattern/Phrase UX/runtime slice.

## Memory / renderer status

U1B adds no persistent routing object beyond the one-byte replacement already enforced by `static_assert`. This is not hardware DRAM evidence and does not supersede the R0 memory workstream.

The UI Constitution Resource Law remains authoritative:

- full framebuffer is an implementation detail, not a semantic API;
- renderer lifetime is independent from musical subsystem lifetime;
- retained cost and construction peak require separate hardware evidence;
- host/SDL GREEN does not mean hardware-ready.

## Closure rule

Do not treat the historical GREEN SHA in this file as the final branch verification pin after later documentation or CI changes. U1B is host/CI-closed only when the current branch HEAD has fresh SUCCESS for:

1. `u1a-semantic-location`;
2. `u1b-source-transport-truth`;
3. `preserve-p3-u1-semantics`.

Hardware visual, physical 1:1 readability, Cardputer interaction, DRAM and render-transfer evidence remain pending for later checkpoints.
