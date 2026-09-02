# GroovePuter 0.9.9-D2 — Live Phrase Arrangement

## Purpose

Allow Phrase-page `G` generation while transport is running without changing the currently audible bar mid-bar.

D2 keeps the accepted 0.9.9-C activation owner and the D1 Pattern/Phrase liveness rules. It does not add another scheduler, queue, Undo owner, dirty-state model, or persistence format.

Exact base: `dev_0.9.9 @ 7c413348955837f2ca45c9c8d86b5275ba636f0c` (merged D1).

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3.
- Yamaha SEQTRAK is recommended for the final MIDI timing smoke but is not required for host ownership tests.
- Normal GroovePuter audio output/headphones for boundary listening.

## Wiring

No wiring changes.

Use the normal Cardputer ADV GroovePuter setup. If SEQTRAK is connected, retain the existing USB-MIDI/SEQTRAK routing. D2 adds no GPIO, I2C, SPI, audio, or MIDI electrical dependency.

## Transaction contract

```text
Phrase G
  |
  v
PREPARE (control path, bounded 1/2/4/8 bars)
  |
  v
one canonical COMMIT
  - allocated Pattern bytes
  - Song references
  - single-bar Song-row timing
  - one Undo receipt
  - one Scene revision
  |
  v
existing 0.9.9-C pending owner
  - current bar remains old audible truth
  - second intent => BUSY
  |
  v
BAR_START
  - ACTIVATE runtime Song destination
  - no generation
  - no Scene write
  - no allocation
  - no filesystem
  - no second Undo/revision
```

The BAR_START pending hook executes before normal Song-row advancement. This ordering is required because D2 commits `feel.patternBars = 1`; activating first prevents an intermediate next-row selection and prevents the first generated row from lasting an extra bar.

## Undo and STOP

- `Ctrl+Z` before BAR_START restores the generated Song/Pattern persistent mutation and cancels only its matching pending activation.
- If the generated material is already audible, D2 refuses a mid-bar restore. Stop transport first, then use the retained receipt.
- `STOP` settles an already committed Phrase destination immediately so the next START uses committed truth.
- Pending activation itself is runtime-only and is never serialized.

## Memory contract

- Persistent Pattern/Song data uses the existing Scene storage.
- Undo uses the existing fixed `UndoOwner` payload; no second history store exists.
- Pending publication still uses the accepted C two-slot owner.
- D2 adds only a small fixed metadata array indexed by those two slots.
- Musical PREPARE staging is bounded to eight bars and exists only on the control path; no allocation occurs at BAR_START.

The exact Cardputer ADV fixed-DRAM result from CI is authoritative for acceptance.

## Build / Flash

Focused cumulative contract:

```bash
bash tests/run_0_9_9_d2_tests.sh
```

Then validate the same exact SHA with the normal release matrix:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash only the exact software-green D2 SHA for hardware smoke.

## Expected behavior

1. With transport stopped, Phrase `G` behaves as before: generated material is committed and selected immediately.
2. During PLAY, Phrase `G` prepares and commits persistent truth but the current audible bar does not change.
3. UI reports `GEN -> NEXT BAR` while activation is pending.
4. At the next BAR_START, generated Song destination becomes audible with no intermediate Song row and no extra first bar.
5. A second generation intent while pending is rejected by the existing C BUSY policy.
6. `Ctrl+Z` before the boundary restores the generated persistent mutation and cancels activation.
7. `STOP` before the boundary settles the committed Phrase destination immediately.
8. Save serializes committed Scene truth only; reboot/load does not restore a pending activation.

## Troubleshooting

- **Phrase still says `STOP PLAYBACK FOR PHRASE`:** old Phrase-page guard is still present; D2 UI integration failed.
- **First generated row lasts two bars:** BAR_START hook is running after `advanceSongBar_()` or Song bar phase was reset after advancement.
- **A natural next Song row flashes/plays before generated material:** activation ordering is wrong; pending hook must execute first at the same BAR_START.
- **Second `G` replaces pending work:** C BUSY/reject ownership was bypassed; D2 must not add newest-wins or a hidden queue.
- **Undo changes audible material mid-bar after activation:** generated-Phrase Undo safety gate failed; stop transport before restoring an already audible generated target.
- **Save contains pending fields:** runtime activation leaked into Scene persistence; reject the build.
- **Fixed DRAM regresses unexpectedly:** inspect D2 fixed metadata size and compiler map; do not compensate with an unbounded/heap pending queue.

## Acceptance checklist

Software, one exact SHA:

- [ ] `bash tests/run_0_9_9_d2_tests.sh` PASS
- [ ] Core host regressions PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV normal build PASS
- [ ] fixed DRAM policy PASS
- [ ] Cardputer ADV SEQTRAK MIDI-only build PASS
- [ ] D1 Pattern/Phrase liveness gates PASS
- [ ] C bounded activation gates PASS
- [ ] one persistent COMMIT / one Scene revision for Phrase `G`
- [ ] no direct `markSceneMutated()` in D2 generation path
- [ ] no second pending queue/scheduler/clock
- [ ] BAR_START ACTIVATE occurs before normal Song-row advancement
- [ ] BAR_START has no generation/allocation/filesystem/persistent mutation
- [ ] pending state is not serialized

Physical Cardputer ADV, same SHA:

- [ ] stopped Phrase `G` still works
- [ ] PLAY Phrase `G` leaves current bar unchanged
- [ ] generated material starts exactly at next BAR_START
- [ ] first generated Song row lasts exactly one bar
- [ ] second `G` while pending reports BUSY and does not replace work
- [ ] `Ctrl+Z` before boundary cancels pending and restores Song/Pattern state
- [ ] `STOP` before boundary settles committed generated destination
- [ ] with SEQTRAK connected, no stuck notes or transport discontinuity
- [ ] no reboot / Guru Meditation / stack canary / watchdog

CI proves software contracts only. It does not claim the physical checklist above.
