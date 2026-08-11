# P2 + P3 Hardware Audition

**Branch:** `agent/20260811-19-p2-p3-hardware-audition`  
**Exact Stage15 base:** `fc42763e7798866e61895bf1b8d62339ec59e0a7`

## Purpose

This is a temporary hardware-audition integration branch. It is not the production admission path for P2/P3 and must not replace the independently reviewed capability PRs.

The branch imports the corrected source contracts from:

- P2 #237 current corrected head `39423a7a66ed1576624d5624341fc7fa7398f353`;
- P3 #238 current corrected head `eb697d9916f4faa64b50428c0a705d7c2948f0ce`.

The audition layer answers one question: can the two capabilities compose and remain musically distinguishable on Cardputer hardware without introducing a second rhythm or harmonic owner?

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

## Cardputer entry and safety contract

Exact global entry chord:

```text
Ctrl+Alt+O   enter P2/P3 AUDITION
Esc          exit and restore temporary state
```

`Alt+H` remains the existing help screen. Plain `O`, `Ctrl+O` and `Alt+O` do not enter audition.

While audition is active, normal Cardputer HID/word input is consumed by the harness so accidental editing/saving cannot leak through the ordinary UI. The existing Synth B page remains the visual owner; a long-lived toast identifies the active audition/test instead of adding another production page.

The harness snapshots only state it touches in RAM:

- Synth B bank-0 slots 1..4 used by the fixtures;
- active song data used temporarily by fixture 4;
- current Synth B bank/pattern selection;
- song mode/position/playback slot;
- Synth B mute state;
- track volumes;
- previous UI page / previous play state.

It does not call scene persistence or mark the Scene revision dirty. `Esc` restores the snapshot before returning to the previous page.

## Hardware controls

After `Ctrl+Alt+O`:

```text
Ctrl+1  RETRIGGER
Ctrl+2  DENSE_RETRIGGER
Ctrl+3  CROSS_BAR_HOLD
Ctrl+4  MULTI_BAR_NS
Esc     restore + exit
```

Selecting a fixture stops the previous fixture, installs deterministic Synth B probe material and starts playback from the beginning automatically. No extra Play key is required.

## Deterministic fixtures

### 1. RETRIGGER

```text
N---S---N---S---
source 0  0   1   1
```

Expected: four audible attacks, two pitch/source identities. `S` repeats the same pitch as its preceding source; it must not advance to a new pitch.

### 2. DENSE_RETRIGGER

```text
NSSSSSSSSSSSSSSS
```

Expected: sixteen audible attacks of the same pitch, one harmonic source advance.

### 3. CROSS_BAR_HOLD

```text
bar 1: ------------NCCC
bar 2: CCCC R-----------
```

`C` is explicit continuation and `R` explicit release. The bar boundary itself creates neither a new onset nor a new harmonic source.

For this fixture the hardware renderer deliberately uses one repeating 16-step Synth B pattern rather than Song rows. This is required because current Stage15 Song row selection emits `AllNotesOff` at a row boundary, which would invalidate the hold test. The first audible onset occurs late in the first bar, then the existing `SynthPattern` TIE semantics must carry it through step 15 -> step 0.

### 4. MULTI_BAR_NS

Four bars containing N/S actions. Bars 2 and 4 deliberately begin with `S`; those attacks map to the source identity established in the preceding bar through explicit incoming-source composition.

This fixture may use four temporary Song rows because every tested row boundary begins with an audible N or S attack; the current Song `AllNotesOff` before row selection therefore does not masquerade as a continuation result.

## Hardware-rendering rules

- deterministic probe pitches make source advancement obvious;
- `N` changes to the pitch assigned to the new global source ordinal;
- `S` attacks again using the existing source pitch;
- continuation renders through existing `SynthPattern` TIE (`note = -2`);
- release remains silence after the final TIE;
- Synth B is isolated by temporary track-volume state;
- no P1 chord-quality/polyphony is needed;
- no H6 progression/rhythm candidate is admitted.

## Acceptance checklist

```text
ENTRY
[ ] Ctrl+Alt+O enters; toast says P23 AUDITION CTRL+1..4
[ ] Alt+H still opens normal help outside audition

CTRL+1 RETRIGGER
[ ] 4 attacks
[ ] pitch pattern is A A B B, not A B C D

CTRL+2 DENSE
[ ] 16 attacks
[ ] all attacks use one pitch/source

CTRL+3 CROSS-BAR HOLD
[ ] onset near end of bar
[ ] note remains held through 15 -> 0
[ ] no fresh attack exactly at bar boundary
[ ] release occurs after the explicit continuation span

CTRL+4 MULTI-BAR N/S
[ ] four-bar loop plays
[ ] bar-start S repeats the preceding source pitch
[ ] N advances to a new deterministic source pitch

EXIT
[ ] Esc stops audition
[ ] previous UI page/pattern/song/volumes are restored
[ ] normal controls work again
```

## Scope boundary

This PR remains a temporary hardware audition harness. It must not become the production persistence/UI owner for P2/P3. Successful hardware listening may justify later production integration, but H6 R2 vocabulary admission remains separate.
