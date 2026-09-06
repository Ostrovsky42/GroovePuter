# 0.9.10 — UI Constitution V1 — U1D Evidence

Checkpoint: `U1D — renderer residency must not own musician continuity`

## Verified production candidate

Exact implementation SHA:

`191f0c1dd68e49f5469c53a733f5a72bc9ac26ab`

Commit:

`refactor(ui): decouple view continuity from renderer residency`

The evidence document may be committed later than the implementation candidate. The implementation SHA above is the verification pin for the production change.

## RED

RED head:

`1ac8c2fae1f1689c5489777e0ca142fe5a1fbfdb`

Workflow run:

`34049373496`

Focused job:

`u1d-renderer-residency-continuity`

The characterization checks first proved that the existing renderer is evictable and that the Cardputer threshold remained `freeDRAM < 16384`. The RED then failed because no residency-independent runtime continuity owner existed:

`AssertionError: U1D needs a fixed-size runtime view-continuity value outside page residency`

No production code had been changed for that RED.

## GREEN architecture

U1D adds one fixed-size runtime-only value:

`UI::UiViewContinuityState`

It is owned by `MiniAcidDisplay` and is statically constrained to `<= 16 bytes`.

The shell captures page view continuity immediately before an evictable page object is destroyed and restores it immediately after recreation.

The page base exposes bounded virtual capture/restore hooks, so the shell does not depend on `dynamic_cast` or concrete page types.

Covered high-level continuity:

- Synth A/B local representation: NOTES / KNOBS / MORE;
- PERFORM tools-layer visibility;
- PERFORM KEY / CHORD / ARP / RHYTHM context;
- PERFORM remembered row for each tool context;
- FEEL focused row;
- FEEL local preset cursor;
- GENRE focused row.

## Deliberate exclusions

U1D does not change:

- the `<16384` Cardputer eviction threshold;
- current/previous page retention policy;
- `UiSessionState` persistence layout or reboot semantics;
- musical state ownership;
- Pattern/Phrase event buffers;
- shortcuts, labels, layout, themes or draw order.

Pattern-editor cursor/selection state is real residency-sensitive debt but is not restored here. It is material-sensitive and requires a separate validity contract over the edit target before stale coordinates can be safely restored.

Likewise, unapplied GENRE draft choices are not treated as generic view continuity because their validity depends on the active Scene/revision.

## Exact-head verification

Workflow:

`0.9.10 UI Constitution V1`

Run:

`34049957110`

Head SHA:

`191f0c1dd68e49f5469c53a733f5a72bc9ac26ab`

Results:

- `u1d-renderer-residency-continuity` — PASS;
- `u1a-semantic-location` — PASS;
- `u1b-source-transport-truth` — PASS;
- `u1c-coherent-frame-status` — PASS;
- `preserve-p3-u1-semantics` — PASS.

The inherited preservation job ran `tests/run_pattern_phrase_p3_u1_tests.sh` successfully on the same SHA.

A separate existing Undo Safe Editing workflow also completed successfully on the same implementation SHA.

## U1D decision

U1D is GREEN on the pinned production candidate.

Renderer residency may now vary with memory pressure without resetting the covered high-level musician view position. The next UI Constitution checkpoint must remain an ownership/geometry correction, not a visual redesign.
