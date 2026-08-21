# GroovePuter 0.9.9-D3 — Cardputer ADV Hardware Acceptance

## Purpose

Physically validate the 0.9.9-D3 Song LIVE ARRANGEMENT ownership contract on M5Stack Cardputer ADV after the exact candidate SHA is software-green.

This hardware stage is intentionally narrow. It does not change D3 production semantics, add diagnostics to the realtime path, or redefine the C/D1/D2 lifecycle. It validates that the finite-state ownership already covered by host/source tests remains correct under real audio, keyboard, transport, and USB-MIDI timing.

Hardware-validation branch creation point:

```text
D3 source branch: agent/20260821-03-0.9.9-d3-song-live-arrangement
D3 source SHA:    a4419e402b65f80ddffd2a349af798f58989e9d2
Hardware branch:  agent/20260821-04-0.9.9-d3-hardware-validation
```

`a4419e402b65f80ddffd2a349af798f58989e9d2` is a diagnostic checkpoint, not a flash/merge candidate, because its Cardputer ADV CI path is not fully green. Before executing the physical cases below, move this documentation commit onto the later immutable D3 SHA that passes the complete software matrix, including the separate fixed-DRAM check.

## Hardware list

Required:

- M5Stack Cardputer ADV / ESP32-S3FN8.
- USB-C data cable.
- Development computer with `arduino-cli`.
- Built-in Cardputer speaker or 3.5 mm headphones.

Recommended for the final MIDI/output smoke:

- Yamaha SEQTRAK.
- The already validated GroovePuter ↔ SEQTRAK USB-MIDI connection used by the project.

## Wiring

No new wiring is required for D3.

Cardputer ADV standalone path:

```text
USB-C -> power / flash / Serial
built-in keyboard -> Song editor commands
built-in ES8311 -> speaker/headphones
```

SEQTRAK smoke path uses the existing project USB-MIDI connection. D3 must not alter INTERNAL/MIDI/LAYER output ownership.

PORT.A is not used by this test. If anything remains attached there, preserve the project invariant:

```text
SDA = GPIO2
SCL = GPIO1
```

## Build / Flash steps

### 1. Freeze the exact candidate

Do not test a moving branch. Record the candidate first:

```bash
git rev-parse HEAD
```

The SHA used for firmware, CI evidence, and the hardware result log must be identical.

### 2. Required software gate on that SHA

Run focused D3 first:

```bash
bash tests/run_0_9_9_d3_tests.sh
```

Then build Cardputer ADV exactly as CI does:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
```

The fixed-DRAM command above is a separate mandatory PASS. A normal ADV compile does not imply it.

For the SEQTRAK MIDI-only build gate:

```bash
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Do not flash until the same immutable SHA is green for D3 focused, Undo R4-R7, C/D1/D2 cumulative, Core host, SDL, ADV normal, fixed DRAM, SEQTRAK MIDI-only, tonal, Synth persistence, sampler, and output ownership.

### 3. Flash current sources

```bash
bash scripts/upload.sh /dev/ttyACM0
```

Use the actual Cardputer port if it is not `/dev/ttyACM0`.

### 4. Serial monitor

```bash
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

Keep Serial open for the complete run. Record any reset reason, Guru Meditation, watchdog, stack-canary, heap-corruption, or USB-MIDI recovery message.

## Test preparation

Use the Song page and prepare deliberately recognizable material.

1. Enable Song mode.
2. Set a moderate tempo around 60-90 BPM so there is enough time to edit mid-row and Undo before the boundary.
3. Prepare Song slot A with at least four rows.
4. Give Synth A, Synth B, and Drums clearly distinguishable current-row patterns.
5. Prepare Song slot B with audibly different material from slot A.
6. Keep at least one later row different from the current row so forward/reverse direction is obvious.
7. Start playback.
8. On the Song page, press `P` whenever necessary to move the editor cursor to the current playhead row.

Useful Song controls used below:

```text
Left/Right     move Synth A -> Synth B -> Drums; crossing outer edge changes EDIT Song slot
Up/Down        move Song row
P              cursor -> playhead
Q..I           assign pattern 1..8 from current PAT:A/B context
B              change PAT:A/B assignment context only
Bksp / Tab     clear current Song cell
Ctrl+B         request PLAY Song slot A/B switch
Ctrl+R         request Song direction change
Ctrl+Z         canonical Undo
Space/action   project transport start/stop
```

When replacing a current cell, choose a `Q..I` pattern that is audibly different from the stored one. If needed, use `B` first to change the assignment bank context; `B` by itself must not mutate Song data.

## Expected behavior shared by all cases

During PLAY, a mutation that affects the current audible row has two truths until the row boundary:

```text
persistent Scene truth = newly committed Song
current audible truth  = captured old A/B/Drums snapshot
```

The old audible snapshot remains authoritative until the existing row boundary. At the boundary it is consumed exactly once and the committed Song truth becomes eligible for playback.

Terminal cleanup must obey the D3 slot lifetime rule:

```text
Armed/Ready
  -> Reading
  -> unpublish
  -> clear D3 payload + metadata
  -> Empty
```

A following writer must never observe an `Empty` slot while the previous D3 audible snapshot can still be read.

## D3-HW-01 — Clear current Synth A / Synth B / Drums

### Purpose

Validate that clearing a current audible Song reference does not cut the already audible material mid-row.

### Procedure

Run three independent subcases. Reload or restore the prepared Song between them.

#### A. Synth A

1. Start playback on a row with clearly audible Synth A.
2. Press `P` so the cursor follows the current row.
3. Move to the Synth A cell.
4. Near the middle of the row, press `Bksp` or `Tab`.
5. Keep listening through the row boundary.

#### B. Synth B

Repeat the same steps on the Synth B cell.

#### C. Drums

Repeat the same steps on the Drums cell.

### PASS

- The current audible row does not cut when the cell is cleared.
- The old material continues until the musical row boundary.
- The clear becomes effective only when committed Song truth is eligible at/after the boundary.
- Drums do not drop early through a separate microtiming path.
- No duplicate activation is heard.

### FAIL examples

- immediate Synth note cut;
- immediate drum dropout;
- one late hit from stale old material after the boundary;
- double-trigger or timing jump at the boundary.

## D3-HW-02 — Replace current Synth A / Synth B / Drums

### Purpose

Validate `old ref -> new ref` ownership, not only `old ref -> empty`.

### Procedure

For each of Synth A, Synth B, and Drums:

1. Start a recognizable current-row pattern.
2. Press `P` and select the active track cell.
3. Mid-row, press a different valid `Q..I` pattern key.
4. Listen before and across the row boundary.

### PASS

- The new pattern is visible as persistent Song truth immediately.
- The old pattern remains the only audible truth before the boundary.
- The new pattern does not leak into the middle of the current row.
- Boundary transition occurs once.

### FAIL examples

- mixed old/new notes inside one row;
- new kick/snare pattern appears before the boundary;
- old snapshot survives one row too long.

## D3-HW-03 — Undo before boundary

### Purpose

Validate canonical R4 Undo plus exact pending-revision invalidation.

### Procedure

1. Start playback on recognizable current-row material.
2. Press `P` and edit the current cell mid-row using either clear or replace.
3. Before the next row boundary, press `Ctrl+Z`.
4. Continue playback for at least two boundaries.
5. Repeat once for Synth A and once for Drums.

### PASS

- Undo restores the persistent Song cell.
- The current audible material remains continuous; there is no gap or forced restart.
- The canceled pending activation does not fire at the next boundary.
- No second Scene mutation is created by ACTIVATE.

### FAIL examples

- silence or click when Undo is pressed;
- canceled edit becomes audible at the next boundary;
- stale snapshot appears one boundary later.

## D3-HW-04 — Ctrl+B PLAY A/B boundary switch

### Purpose

Validate separation of EDIT Song slot from PLAY Song slot and boundary ownership of the explicit PLAY switch.

### Procedure

1. Make Song A and Song B audibly different.
2. Start playback from Song A.
3. Mid-row press `Ctrl+B`.
4. Observe the toast and keep listening through the boundary.
5. Do not move the edit slot during the first run.
6. Repeat while the editor is viewing the non-playing slot.

### PASS

- Toast reports `Play B -> NEXT ROW` or the equivalent pending state while playing.
- Song A remains audible for the rest of the current row.
- Song B becomes PLAY truth exactly once at the row boundary.
- Merely moving EDIT A/B never redirects audio.
- A second conflicting request while pending is BUSY/rejected rather than queued or newest-wins.

### FAIL examples

- immediate mid-row Song switch;
- EDIT slot movement changes audio;
- two switches occur from one request;
- second request overwrites the pending target.

## D3-HW-05 — Reverse boundary switch

### Purpose

Validate that the old private reverse queue is gone and reverse now uses the canonical Song mutation plus D3 boundary owner.

### Procedure

1. Use a Song of at least four clearly distinguishable rows.
2. Start forward playback away from the first/last row.
3. Mid-row press `Ctrl+R` once, shorter than the long-press threshold.
4. Listen through the boundary and observe the next row.
5. Repeat to return to forward direction.

### PASS

- Current row finishes normally.
- Direction change takes effect only on the row boundary.
- The next row follows the newly committed direction exactly once.
- No skipped row, repeated boundary, or extra audible restart occurs.
- No extra Scene revision is attributable to ACTIVATE.

### FAIL examples

- immediate jump to another row;
- one-row double advance;
- stale direction persists for an extra boundary;
- short press accidentally behaves like the long `Song: START` action.

## D3-HW-06 — STOP settlement

### Purpose

Validate terminal settlement when a D3 mutation is committed but its audible snapshot is still pending.

### Procedure

1. Start playback.
2. Mid-row clear or replace the current Synth A or Drums cell.
3. Before the boundary, stop transport.
4. Inspect the Song cell: committed persistent truth must remain.
5. Start transport again.
6. Repeat with a different replacement pattern.

### PASS

- STOP terminates the pending audible snapshot without reverting the committed Song mutation.
- Restart uses committed Song truth, not the stale pre-edit snapshot.
- There is no delayed activation from the previous transport run.
- No stuck note remains after STOP.

### FAIL examples

- restart briefly plays the old pattern;
- pending transition fires after restart;
- STOP silently reverts the Song cell.

## D3-HW-07 — A -> terminal -> B slot-lifetime race

### Purpose

Exercise the terminal cancellation lifetime rule under real playback. This is the physical counterpart to the regression that protects `Reading -> unpublish -> clear -> Empty` ordering.

### Variant 1: Undo terminal

Repeat at least 10 times:

1. `A`: mid-row clear the current Synth A cell.
2. `terminal`: before the boundary press `Ctrl+Z`.
3. Immediately create `B`: replace the current Synth A cell with a different `Q..I` pattern.
4. Listen through the next two boundaries.

### Variant 2: STOP terminal

Repeat at least 10 times:

1. `A`: mid-row replace the current Drums cell.
2. `terminal`: stop before the boundary.
3. Restart playback.
4. `B`: create another current-row Drums replacement.
5. Listen through the next two boundaries.

### PASS

Across all repetitions:

- B is never polluted by A's old audible snapshot;
- B is never spuriously rejected because the old slot was half-cleared;
- no stale A material appears after the terminal transition;
- no double activation, crash, watchdog, or stuck note occurs.

Any single stale-snapshot reproduction is a D3 release blocker.

## D3-HW-08 — 2-3 minute edit stress

### Purpose

Catch timing/lifetime failures that are too sparse for one-shot manual cases.

### Procedure

For 2-3 continuous minutes while playback runs, rotate through:

```text
current A clear
current A replace
current B clear/replace
current Drums clear/replace
Undo before boundary
future-row edit
EDIT A/B navigation
Ctrl+B PLAY switch
Ctrl+R direction switch
STOP / restart
```

Do not intentionally exceed the one-pending-operation contract. When BUSY is shown, treat it as expected backpressure and wait for the pending action to terminate.

### PASS

- no mid-row discontinuity;
- no stale snapshot after any terminal state;
- no stuck note;
- no timing discontinuity at the boundary;
- no Guru Meditation, watchdog, stack canary, or heap-corruption message;
- UI remains responsive;
- no progressive audio degradation.

## D3-HW-09 — SEQTRAK MIDI/output ownership smoke

### Purpose

Confirm D3 did not disturb the frozen 0.9.6 INTERNAL/MIDI/LAYER ownership or SEQTRAK timing.

### Procedure

1. Connect the already validated SEQTRAK MIDI path.
2. Use the normal SEQTRAK MIDI-only configuration.
3. Play Song A for several rows.
4. Perform one current-row replacement, one `Ctrl+B` switch, one `Ctrl+R` switch, and one STOP/restart.
5. Listen to both timing and note termination on SEQTRAK.

### PASS

- MIDI notes remain on the expected lanes/channels.
- No stuck note appears after boundary activation or STOP.
- No duplicated note burst occurs at a Song switch.
- Timing remains continuous across D3 boundaries.
- Output mode/ownership behavior is unchanged from the accepted 0.9.6 contract.

## Three defect classes to listen for

Every case above should be evaluated specifically for these classes:

1. **Mid-bar/row discontinuity** — cut, click, early new pattern, double trigger, or timing jump before the boundary.
2. **Stale snapshot after terminal state** — material from a canceled/settled D3 operation reappears after Undo, STOP, or a following writer.
3. **Stuck note / timing discontinuity** — internal or MIDI note never terminates, repeated note burst, missing clock interval, or boundary jitter obvious against the groove.

## Troubleshooting

### Current edit becomes audible immediately

Reject the build. The committed Song reference is being dereferenced before the D3 audible snapshot loses ownership.

### B action contains material from previous A action

Reject the build. This strongly indicates terminal cleanup published `Empty` before old payload/metadata lifetime ended.

### Ctrl+B changes audio immediately

Reject the build. The UI/runtime path bypassed `requestSongPlaybackSwitch()` or activated outside the row boundary.

### Ctrl+R jumps immediately

Reject the build unless the key was held long enough to invoke the distinct long-press `Song: START` behavior.

### Drums fail while synths pass

Inspect the drum/microtiming audible-pattern path separately. D3 ownership must cover Drums exactly like Synth A/B.

### BUSY appears on a second edit

This is expected while another audible activation is pending. D3 intentionally rejects conflicting work instead of queueing or replacing it.

### Serial logging is quiet

Normal production logging may not print every D3 transition. Silence is not a PASS by itself; use audible/UI behavior plus crash/reset telemetry. Do not enable expensive realtime logging for release acceptance unless diagnosing a reproduced failure.

## Acceptance checklist

Candidate identity:

- [ ] Hardware run uses one immutable SHA.
- [ ] D3 focused PASS on that SHA.
- [ ] Undo R4-R7 PASS on that SHA.
- [ ] C/D1/D2 cumulative PASS on that SHA.
- [ ] Core host PASS on that SHA.
- [ ] SDL PASS on that SHA.
- [ ] Cardputer ADV normal build PASS on that SHA.
- [ ] `check_cardputer_dram_budget.sh` separate PASS on that SHA.
- [ ] SEQTRAK MIDI-only build PASS on that SHA.
- [ ] tonal PASS on that SHA.
- [ ] Synth persistence PASS on that SHA.
- [ ] sampler PASS on that SHA.
- [ ] output ownership PASS on that SHA.

Physical Cardputer ADV:

- [ ] D3-HW-01 clear current Synth A PASS.
- [ ] D3-HW-01 clear current Synth B PASS.
- [ ] D3-HW-01 clear current Drums PASS.
- [ ] D3-HW-02 replace current Synth A PASS.
- [ ] D3-HW-02 replace current Synth B PASS.
- [ ] D3-HW-02 replace current Drums PASS.
- [ ] D3-HW-03 Undo before boundary PASS.
- [ ] D3-HW-04 Ctrl+B boundary switch PASS.
- [ ] D3-HW-05 reverse boundary switch PASS.
- [ ] D3-HW-06 STOP settlement PASS.
- [ ] D3-HW-07 A -> terminal -> B repeated PASS.
- [ ] D3-HW-08 2-3 minute edit stress PASS.
- [ ] D3-HW-09 SEQTRAK MIDI/output smoke PASS.
- [ ] no mid-row discontinuity heard.
- [ ] no stale snapshot after terminal state heard.
- [ ] no stuck note / timing discontinuity heard.
- [ ] no reset, Guru Meditation, watchdog, stack canary, or heap corruption.

## Acceptance rule

Hardware acceptance is binary for D3 release purposes:

- all mandatory software gates on one immutable SHA;
- all D3-HW-01 through D3-HW-08 Cardputer ADV cases PASS;
- D3-HW-09 PASS when validating the SEQTRAK release path;
- zero reproduction of the three defect classes.

Do not mark the D3 PR hardware-accepted from CI alone. Do not transfer results between different SHAs.