# GroovePuter 0.9.9-D3 — Song LIVE ARRANGEMENT

## Purpose

Allow persistent Song editing while transport is running without letting newly committed Song references leak into the currently audible row before the musical row boundary.

Exact base: `dev_0.9.9 @ 677ee9c61fe1e0e0f502d084d972c8d90823452f` (merged D2).

D3 extends the existing 0.9.9-C bounded two-slot activation owner. It does **not** add a Song scheduler, a third activation slot, a new Undo receipt, a Scene snapshot, or persisted pending state.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- Yamaha SEQTRAK is recommended for final MIDI/output smoke but is not required for host tests.
- Headphones or normal GroovePuter audio output for audible-boundary checks.

## Wiring

No wiring changes.

Use the existing GroovePuter Cardputer ADV setup. D3 adds no GPIO, I2C, SPI, audio or MIDI electrical dependency.

## Ownership contract

```text
Song edit
  |
  v
PREPARE Song after-state + determine audible impact
  |
  +-- non-playing / future row / non-playing Song slot
  |      -> one canonical Song COMMIT immediately
  |
  +-- current audible row / reverse
         -> capture old audible Pattern snapshot
         -> reserve existing C pending slot
         -> one canonical Song COMMIT immediately
         -> persistent truth is NEW
         -> audible truth remains OLD
         -> existing row boundary
         -> ACTIVATE committed truth
```

`COMMIT != ACTIVATE` applies to ordinary Song editing exactly as it already applies to generation.

While a Song mutation is pending, playback must not obtain current-row material by dereferencing newly committed Song references. The captured old Synth A / Synth B / Drums snapshot is authoritative audible truth until ACTIVATE.

## Frozen audible cases

### 1. Clear current audible cell

```text
before persistent: SynthA = 12
after COMMIT:      SynthA = -1
audible before boundary: old Pattern 12
after boundary: committed Song truth may become audible
```

There must be no mid-row note cut caused by dereferencing `-1`.

### 2. Replace current audible cell

```text
before persistent: SynthA = 12
after COMMIT:      SynthA = 27
audible before boundary: old Pattern 12
after boundary: committed Pattern 27 when that row becomes active
```

The same contract applies independently to Synth B and Drums.

### 3. Edit future row or non-playing Song slot

No pending snapshot is required. The mutation uses the existing R4 Song Undo owner and commits immediately.

If another audible activation is already pending, D3 preserves the existing 0.9.9-C BUSY/reject policy instead of changing revision underneath that pending action.

### 4. Undo before boundary

For a committed `12 -> -1` or `12 -> 27` edit, Undo before ACTIVATE restores the R4 Song receipt, cancels the exact matching pending revision and leaves the old audible Pattern continuous.

Undo after the committed change is already audible is refused during PLAY when it would rewrite the current audible row mid-row.

## EDIT A/B vs PLAY A/B

- `EDIT:A/B` is persistent editor selection.
- During PLAY, changing EDIT slot does not redirect audible playback.
- `Ctrl+B` requests a PLAY A/B switch through the same bounded C owner.
- PLAY slot switches become runtime truth only on the row boundary.
- Stopped behavior remains immediate.

## Reverse

The old private `songReverseTogglePending_` mechanism is retired.

Ctrl+R retains its existing short/long-press UI semantics, but persistent reverse changes now use the canonical Song `UndoOwner` and the same D3 row-boundary activation owner.

## Memory / realtime contract

- Existing C `PendingGeneration g_slots[2]` remains the only pending publication storage.
- D3 adds only two small fixed `SongActivationMetadata` records indexed by those slots.
- No heap allocation occurs in pending metadata or ACTIVATE.
- ACTIVATE does not generate, serialize, touch filesystem, publish Undo, or increment Scene revision.
- Pending state is never serialized.

## Build / Flash

Focused cumulative tests:

```bash
bash tests/run_0_9_9_d3_tests.sh
```

Before hardware acceptance, validate the same exact SHA with the normal release matrix, including Core, SDL, Cardputer ADV fixed DRAM and SEQTRAK MIDI-only builds.

Flash only the exact software-green SHA used by CI.

## Expected behavior

1. Editing a future row during PLAY updates persistent Song data immediately and does not disturb audio.
2. Clearing or replacing the current Synth A, Synth B or Drum cell does not change the current audible row.
3. The old captured material remains authoritative until the row boundary.
4. At the boundary, pending state is consumed once and committed Song truth becomes eligible for playback.
5. A second conflicting intent while pending reports BUSY; it does not replace the pending action.
6. Undo before boundary restores persistent state and invalidates matching pending activation without an audible glitch.
7. Selecting EDIT A/B while playing does not change PLAY A/B.
8. Ctrl+B changes PLAY A/B only at the row boundary.
9. STOP settles committed D3 truth; reboot/load never resurrects pending state.

## Troubleshooting

- **Clear current Synth cell cuts immediately:** `activeSynthPattern()` is reading the committed Song ref before the pending snapshot. Reject the build.
- **Replace 12 -> 27 is heard immediately:** pending snapshot is being treated as fallback instead of authoritative audible truth.
- **Drums disappear immediately after clear:** check both `activeDrumPattern()` and the sequencer microtiming path for old-snapshot ownership.
- **Changing EDIT A/B changes audio immediately:** `setActiveSongSlot()` still owns PLAY slot during transport.
- **Ctrl+B changes Song immediately:** the UI bypassed `requestSongPlaybackSwitch()`.
- **Reverse creates an extra revision:** legacy queued reverse/`markSceneMutated()` is still active.
- **Undo before boundary produces a gap:** pending activation was removed after, rather than before, restoring the Song receipt.
- **Pending comes back after load:** runtime metadata leaked into persistence.

## Acceptance checklist

Software, one exact SHA:

- [ ] `bash tests/run_0_9_9_d3_tests.sh` PASS
- [ ] D2 cumulative suite remains PASS
- [ ] Core host regressions PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV normal build PASS
- [ ] fixed DRAM policy PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] one Song action = one canonical Undo receipt + one Scene revision
- [ ] no direct `markSceneMutated()` in D3 Song owner
- [ ] no private reverse pending state
- [ ] no second scheduler/queue/history
- [ ] current Synth A clear keeps old audible snapshot until boundary
- [ ] current Synth A replace keeps old audible snapshot until boundary
- [ ] current Synth B clear/replace has the same behavior
- [ ] current Drums clear/replace has the same behavior
- [ ] future-row/non-playing-slot edit creates no pending snapshot
- [ ] Undo before boundary cancels exact pending revision
- [ ] EDIT slot does not redirect PLAY slot during PLAY
- [ ] Ctrl+B PLAY switch is boundary-owned
- [ ] ACTIVATE is runtime-only and allocation/filesystem free
- [ ] pending metadata is not serialized

Physical Cardputer ADV, same SHA:

- [ ] loop a row containing Synth A and clear its current cell mid-row: no immediate cut
- [ ] repeat for Synth B
- [ ] repeat for Drums: no immediate rhythm dropout
- [ ] replace current Synth/Drum refs mid-row: old material continues until boundary
- [ ] edit a future row: current audio is unchanged and no BUSY unless another pending action exists
- [ ] Undo a current-row edit before boundary: no audible discontinuity and edit is restored
- [ ] switch EDIT A/B while playing: PLAY slot remains unchanged
- [ ] Ctrl+B switches PLAY slot exactly once at row boundary
- [ ] reverse transition occurs at row boundary without extra Scene revision
- [ ] STOP with pending D3 work settles committed truth
- [ ] no stuck notes, Guru Meditation, stack canary or watchdog

CI proves software contracts only; physical acceptance must be performed on the flashed exact SHA.
