# GroovePuter 0.9.10 — UI Constitution V1 — U1C Evidence

Status: **HOST/CI CLOSED; HARDWARE VISUAL + MEMORY/REALTIME DELTA PENDING**

Branch: `feature/20260906-04-0.9.10-ui-constitution-v1`

Verified implementation candidate:

`cdf993905bc8ba0f92ebc5b5c959e1c098aa2d66`

Authoritative UI Constitution CI run:

`34047889572`

## Scope

U1C closes one coherent-frame ownership gap only.

Before U1C, global chrome derived status from live `MiniAcid` state **after** the active page had already rendered. Even with U1A typed page identity and U1B authoritative PAT/PHR source truth, body and global chrome could therefore observe different runtime instants inside one UI frame.

U1C introduces one bounded read projection:

```text
typed UI location
      +
authoritative runtime state
      |
      v
UiStatusSnapshot   <= 16 B
      |
      +---- page draw happens
      |
      v
status chrome renders captured snapshot
```

This is not a Scene snapshot, Pattern/Phrase copy, renderer cache or new musical owner.

## RED evidence

U1C RED was published on the UI Constitution branch before production changes.

GitHub Actions run:

`34047529245`

The focused `u1c-coherent-frame-status` job failed for the intended reason:

```text
AssertionError: ui_common has no explicit status snapshot capture API
```

The harness was otherwise valid; the failure represented the actual missing architectural boundary.

## GREEN implementation

### `src/ui/ui_common.h`

The status path is now split explicitly into:

```cpp
UiStatusSnapshot captureUiStatusSnapshot(MiniAcid& mini_acid,
                                         UiStatusContext context);

void drawStatusChrome(IGfx& gfx,
                      const UiStatusSnapshot& status);
```

The compatibility `drawLiveMixLockBadge` hook also consumes only a captured snapshot.

### `src/ui/ui_common.cpp`

Existing status derivation semantics were preserved and exposed through `captureUiStatusSnapshot(...)`.

`drawStatusChrome(...)` no longer receives or reads `MiniAcid`. It formats/caches/renders only the supplied bounded snapshot.

No U1B source/transport logic was changed:

- Synth A/B PAT/PHR still derives from `MiniAcid::currentSequencedSource(0/1)`;
- Song/SMF remain independent transport owners;
- Pattern address remains Pattern-only.

### `src/ui/miniacid_display.cpp`

The shell now derives typed context and captures status once before active-page drawing:

```text
UiLocation -> UiStatusContext
        |
        v
capture UiStatusSnapshot
        |
        v
currentPage->draw(...)
        |
        v
draw global status from captured snapshot
```

Thus global status cannot perform a second live engine read after body draw.

## Resource-law defense

U1C deliberately does **not** snapshot musical material.

It does not add:

- `currentPhraseBuffer` copies in `MiniAcidDisplay`;
- Pattern/Phrase event arrays in the shell;
- full-screen framebuffer dependencies;
- `CardputerDisplay` dependencies;
- heap allocation or page-retained cache state.

The existing compile-time bound remains:

```text
sizeof(UiStatusSnapshot) <= 16
```

This is consistent with the UI Constitution Resource Law: capability/state truth is projected into a bounded view without making page or renderer residency an owner of musical state.

## Preservation / exact-head verification

On exact candidate

`cdf993905bc8ba0f92ebc5b5c959e1c098aa2d66`

GitHub Actions run

`34047889572`

finished with all four jobs successful:

```text
u1a-semantic-location          SUCCESS
u1b-source-transport-truth     SUCCESS
u1c-coherent-frame-status      SUCCESS
preserve-p3-u1-semantics       SUCCESS
```

The inherited preservation job executed:

```text
bash tests/run_pattern_phrase_p3_u1_tests.sh
```

and passed.

A stale U1A static anchor was encountered on the preceding implementation commit: it still required the pre-U1C live-engine render call. U1A's behavioral/typed-location tests were already passing. The anchor was updated to the stronger invariant:

```text
typed context participates in capture
render receives captured snapshot only
```

No production change was made to satisfy that stale anchor.

## Diff-size / preservation check

Comparison from the pre-U1C source baseline `c1cf2b8242bd06dce62132208b5fda217ca42aca` to verified candidate `cdf993905bc8ba0f92ebc5b5c959e1c098aa2d66` showed only:

```text
src/ui/miniacid_display.cpp                 4 changed lines
src/ui/ui_common.cpp                       24 changed lines
src/ui/ui_common.h                         18 changed lines
tests/test_ui_status_chrome_source_regressions.py 13 changed lines
```

No page implementation, input mapping, Pattern/Phrase runtime, theme, geometry or renderer backend was changed by U1C.

## What U1C does not prove

U1C makes the **global status** coherent with one bounded capture point. It does not yet make every page body consume one immutable frame-wide musical projection. Existing pages may still read their authoritative owners during draw.

Therefore this checkpoint does **not** claim a full Scene/frame snapshot architecture.

It also does not claim Cardputer ADV heap, construction-peak, transfer-bandwidth or audio-underrun improvement. Those remain memory/hardware measurements.

## Next admissible boundary

Do not jump directly to visual redesign or tile rendering.

The next UI Constitution slice should be chosen from the remaining ownership gaps and must preserve:

- current screen compositions;
- current workflows and shortcuts;
- Pattern/Phrase runtime semantics;
- page-recreation/resource neutrality;
- no duplicate musical state.

A shell/pixel-owner migration is admissible only with a preservation test proving the final visible hierarchy is retained while duplicate header ownership is removed.
