# GroovePuter 0.9.9-C — Bounded Musical Activation

## Purpose

Separate **persistent truth** from **audible truth** after the accepted 0.9.9-B mutation ownership work.

Canonical lifecycle:

```text
STOP:
PREPARE -> COMMIT -> ACTIVATE NOW

PLAY:
PREPARE -> COMMIT -> PendingActivation -> BAR_START -> ACTIVATE
```

Base: `dev_0.9.9` @ `56a5381c2bb6d7ca83ff9668ce0c216dbb386368`.

C changes when committed generation becomes audible. It does **not** replace musical generators, add Song/Phrase live arrangement, add a second clock, or integrate 0.9.10.

## Hardware list

Software/build acceptance:

- M5Stack Cardputer ADV / ESP32-S3.
- Yamaha SEQTRAK for the MIDI-only firmware target.

Physical smoke after software acceptance:

- Cardputer ADV with the normal GroovePuter audio setup.
- SEQTRAK connected through the existing GroovePuter MIDI setup used by the project.

## Wiring

No wiring changes are introduced by C. Use the already accepted GroovePuter/Cardputer ADV and SEQTRAK connection used by the current release branch.

C adds no I2C, GPIO, USB, display, or audio hardware dependency.

## Ownership contract

### Persistent COMMIT

- PREPARE completes before any persistent write.
- COMMIT writes the new Scene material immediately through the canonical 0.9.8 `UndoOwner`.
- One accepted generation COMMIT creates one Scene revision and one Undo receipt.
- Full generation commits Pattern material, Genre/Feel, mode and BPM as persistent truth.
- ACTIVATE never creates another revision or Undo receipt.

### PendingActivation

- The existing two fixed generation publication slots are reused; no heap queue is added.
- During PLAY one slot can hold the prepared candidate while the other carries the old audible snapshot across COMMIT.
- `Armed` means the old audible overlay is published but COMMIT has not completed yet.
- `Ready` means COMMIT completed and the activation carries the exact committed Scene revision.
- BAR_START may claim only `Ready`; it never waits or spins on `Armed`.
- While any activation is pending, another generation intent is **rejected as Busy**. C does not implement hidden queueing or replacement.

### ACTIVATE

- Audio continues reading the old Pattern/Genre/swing snapshot until BAR_START.
- BAR_START validates target identity and the committed revision.
- Valid pending activation releases the old audible overlay and publishes deferred runtime mode/BPM.
- Stale target or stale revision drops the pending activation. It never redirects old Pattern material onto another target.
- After a drop, runtime mode/BPM converge to the current committed Scene truth; this is runtime settlement, not a new persistent mutation.
- No filesystem, JSON, persistence, generation, allocation, second scheduler, revision, or Undo publication is allowed at BAR_START.

### Cancellation and Undo

- Cancelling pending activation does not roll back committed Scene material.
- Undo restores committed persistent material through `UndoOwner` and then invalidates a pending activation only when its `committedRevision` is the exact revision being undone.
- Large B1 quantized-generation and compact B2 fallback-generation receipts remain distinct.
- STOP discards pending audible activation and settles runtime to the committed Scene state because no later BAR_START will perform normal activation.
- successful Load, New, reset/boot lifecycle discard runtime pending state.
- pending activation is not serialized and cannot resurrect after reboot/load.

### Generator compatibility

- Plain unmodified Pattern `G` remains the B1 quantized synth-generation entry.
- Legacy/fallback Pattern `G` keeps the B2 compiled Genre/Reggae/mode-generator semantics.
- Both routes now use the same bounded audible-activation owner during PLAY.
- Lo-Fi IDs/recipes and realized-material compatibility are unchanged.

## Build / Flash

Focused C gate:

```bash
mkdir -p build/host-tests

g++ -std=c++17 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_generation_activation_0_9_9.cpp \
  -o build/host-tests/test_generation_activation_0_9_9
build/host-tests/test_generation_activation_0_9_9

python3 tests/test_generation_activation_0_9_9_source.py

bash tests/run_undo_0_9_8_r2_tests.sh
bash tests/run_undo_0_9_8_r3_tests.sh
```

Normal software acceptance:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash the exact accepted C SHA using the repository's normal Cardputer ADV build/flash procedure. Do not use a different rebuild SHA for physical acceptance.

## Expected behavior

### STOP

Pressing generation while stopped commits the new material and it is immediately the runtime/audible state. No pending activation remains.

### PLAY

Pressing generation while playing:

1. prepares the new material;
2. commits it as persistent Scene truth immediately;
3. keeps the old material audible for the remainder of the current bar;
4. shows the existing next-bar pending UX;
5. activates the committed material at the next valid BAR_START.

A second generation request before that boundary is rejected as Busy and must not create another revision or receipt.

Undo before the boundary restores the previous committed Pattern/state and cancels only the pending activation associated with the undone revision. The cancelled material must not reappear at the next BAR_START.

Changing the target before the boundary makes the pending target stale. BAR_START drops it instead of applying it to the new target.

Save while pending serializes the already committed Scene only. Pending runtime state is not saved.

## Troubleshooting

- **New Pattern is audible immediately during PLAY:** an audio path is reading committed Scene material without consulting the pending audible overlay.
- **Old Pattern is applied to another selected slot:** material overlay lost exact target validation.
- **Second `G` queues another change:** admission policy regressed; `acquireWriteLease()` must reject while `g_publishedSlot` is active.
- **Undo restores material but it changes again at BAR_START:** matching pending activation was not invalidated by committed revision.
- **Save while pending restores the wrong BPM after reboot:** runtime old BPM overwrote committed `SceneManager` BPM during save synchronization.
- **STOP then restart uses old BPM/mode with new Pattern:** pending was dropped without settling runtime to committed Scene truth.
- **BAR_START causes filesystem/JSON work or generation:** ACTIVATE ownership widened incorrectly.
- **Fixed DRAM increases unexpectedly:** do not add another resident Scene/history/queue; C must reuse the existing two fixed publication slots and existing Undo owner.
- **Lo-Fi sounds or identity changes:** C must not change generation recipes or IDs; run 0.9.9-A compatibility gates.

## Physical hardware smoke

On one exact accepted SHA:

1. STOP: generate and confirm the new Pattern is audible immediately.
2. PLAY: generate mid-bar and confirm the old Pattern continues until the next bar boundary.
3. Confirm the new Pattern enters once, exactly on BAR_START.
4. Press generation again before BAR_START; confirm Busy/no second scheduled change.
5. Generate during PLAY, then Undo before BAR_START; confirm old committed material remains and nothing reappears at the boundary.
6. Generate during PLAY, switch Pattern/target before BAR_START; confirm no wrong-target activation.
7. Generate during PLAY, Save, reboot/load; confirm committed project state survives but no pending activation resurrects.
8. Generate during PLAY, then STOP before BAR_START; confirm restart uses committed material and committed runtime BPM/mode.
9. Repeat with plain quantized `G` and fallback/modified `G`; both must respect the boundary while retaining their distinct musical result families.
10. With SEQTRAK MIDI-only, repeat PLAY activation and confirm clean timing/NoteOff with no stuck notes.
11. Run a 30-minute soak with generation, Busy rejection, Undo, target switches, STOP/start and Save/load cycles.

## Acceptance checklist

- [ ] branch starts from B2 final `56a5381c...`
- [ ] STOP is PREPARE -> COMMIT -> ACTIVATE NOW
- [ ] PLAY is PREPARE -> COMMIT -> PendingActivation -> BAR_START -> ACTIVATE
- [ ] COMMIT and ACTIVATE are separate code paths
- [ ] one accepted COMMIT = one Scene revision
- [ ] one accepted COMMIT = one Undo receipt
- [ ] ACTIVATE creates no revision
- [ ] ACTIVATE creates no Undo receipt
- [ ] existing two fixed slots are reused
- [ ] no heap/unbounded pending queue
- [ ] repeated generation while pending is explicit Busy/reject
- [ ] pending carries exact target identity
- [ ] pending carries committed Scene revision
- [ ] BAR_START validates target identity
- [ ] BAR_START validates committed revision
- [ ] stale target drops pending
- [ ] stale revision drops pending
- [ ] old Pattern material is never redirected to a new target
- [ ] stale/drop runtime settles to current committed Scene truth
- [ ] no wait/spin in AudioTask
- [ ] no second scheduler/clock
- [ ] no filesystem/persistence at BAR_START
- [ ] no generation at BAR_START
- [ ] pending is not serialized
- [ ] Save while pending preserves committed material/mode/BPM
- [ ] successful Load removes pending
- [ ] New removes pending
- [ ] boot/reset lifecycle removes pending
- [ ] STOP removes pending and settles runtime
- [ ] cancelling pending does not roll back committed Scene
- [ ] Undo invalidates only matching pending revision
- [ ] plain quantized G musical generator unchanged
- [ ] fallback G musical generator unchanged
- [ ] both G routes share the C activation owner
- [ ] B1 generation-owner tests PASS
- [ ] B2 Pattern generation-owner tests PASS
- [ ] 0.9.8 R2/R3 tests PASS
- [ ] 0.9.9-A / Lo-Fi compatibility PASS
- [ ] full host suite PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV build PASS
- [ ] fixed DRAM gate PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] physical Cardputer smoke PASS
- [ ] physical SEQTRAK smoke PASS
- [ ] 30-minute soak PASS
- [ ] no 0.9.10 integration included
- [ ] no Song/Phrase 0.9.9-D ownership included
