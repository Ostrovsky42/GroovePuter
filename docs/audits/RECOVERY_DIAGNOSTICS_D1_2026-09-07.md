# Recovery diagnostics D1 — first allocation-failure capture (2026-09-07)

Protocol continuation of `docs/superpowers/plans/2026-09-06-cardputer-runtime-panic-localization.md`,
Task 3. Baseline was re-frozen specifically because
[MEMORY_R0_D0_01_2026-09-06.md](MEMORY_R0_D0_01_2026-09-06.md) found the prior R0 ELF did not
contain `cardputer_runtime_diagnostics` and could not produce group D. This run's baseline does.

## Correction to the raw capture, made by the operator during the run

The capture ran across two physically different SD states. For the first ~4 boot episodes the
SD card was inserted. After the fourth crash the display appeared stuck, and the operator
**physically removed the SD card** — this was a deliberate action, not spontaneous driver
failure. Everything from `[SD] mount result=0` onward in the raw log is CARD-ABSENT, not a new
SD mount defect. Do not fold the second half of this log into the existing "intermittent SD
mount" thread; it is a different, explained condition.

## Identity

```
RUN ID                  RECOVERY-D1-01
COMMIT                  de6b5a3dcd1a2a8e7f802c73b64f80314e2c246b
                         (HEAD of feature/20260904-02-0.9.10-pattern-phrase-p3-phrase-lifetime
                         at capture time; contains cardputer_runtime_diagnostics, +438 lines
                         over the c1f71c2b R0 baseline that D0 could not use)
ELF                      build/cardputer-recovery-diagnostics/GroovePuter.ino.elf
ELF SHA-256              87006cbd9dd82bbb74779862f7a321982e406b84afc4c4861a2abeb37b748732
STATIC DRAM               190968 B (budget 191488 B) — gate PASS
SERIAL PORT               /dev/ttyACM0
SD                        PRESENT for boots 1-4; PHYSICALLY REMOVED by operator before boot 5
                           ("я вытащил карту с ней не запускалось")
SCENES                    autosave present, recovered successfully on boots 1-4
USER INTERACTION           none during boots 1-4 (crashes are not input-triggered);
                           [KEY]/[DSP] START/STOP present only in boot 5 (no-SD, stable)
BOOT ORIGIN                capture started after a fresh flash + hard reset (RTS pin)
RESET REASON                1 (boot 1, power/upload reset), then 4 for every subsequent boot
                            (`[BOOT] Previous stage retained: 100` each time — the crash always
                            happens after setup fully completes, never during setup)
DURATION                    60 s window (scripted `--duration 60`); 5 boot episodes captured
RAW LOG                     logs/serial-recovery-diag-20260907-190148.log (37458 B)
CAPTURE TOOL                scripts/serial_monitor.py
```

## D1 result: reproducible allocation failure, three times, identical signature

Boots 1→2, 2→3, 3→4 (all SD-present) each end the same way. `setup()` reaches stage 100, then
the device resets with `Reset Reason: 4`. Each subsequent boot's `[RDIAG-PREV-ALLOC]` line
names the *previous* boot's first (and, per the recorder's latch semantics, only) allocation
failure:

```
[RDIAG-PREV-ALLOC] bytes=4096 caps=0x00001800 task=loop core=1 fn=0x3c11cd28
```

Identical across all three transitions — same size, same caps, same task, same function
pointer. `caps=0x1800` = `MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT`. `fn=0x3c11cd28` is not a
call site (the plan's own note: "function_name — имя функции аллокатора, а не обещанный call
site") — it is a pointer into `.flash.rodata`, and it resolves to the literal string
`"heap_caps_malloc"`. So this is the plain ESP-IDF allocator entry point failing a 4096-byte
`MALLOC_CAP_INTERNAL|MALLOC_CAP_DEFAULT` request from the `loop` task, on core 1.

The same `[RDIAG-PREV]` line carries the last memory snapshot for that boot:

```
internal8=400/244   internalDefault=400/244
```

400 bytes free, 244 largest contiguous, at the time of the last snapshot before the failure —
against a 4096-byte request. This is not a fragmentation story: nothing close to 4096 was ever
going to fit. It is a plain "loop-time request exceeds the standing runtime floor" failure, and
the floor itself (~400 B free internal heap after setup, matching the `DRAM left: 400` seen at
`[UI] Creating page 2` during setup in this same log) was already known from
`SD_PANIC_LOOP_WITHOUT_AUTOSAVE_2026-09-06.md` as the ~700-900 B post-setup floor; this capture
narrows it further to ~400 B and, for the first time, ties a specific failing request to it.

For comparison, the boot that mounted SD but then had it pulled (boot 4→5 in the raw log,
labelled `[RDIAG-PREV] boot=3` before the operator's removal) shows the exact same 4096-byte /
`heap_caps_malloc` / `loop` signature — so the mechanism does not depend on which specific boot
iteration it is; it reproduces on demand once the runtime floor is reached.

The current (5th, SD-absent) boot's own first-failure latch is different — `bytes=29512
caps=0x1800 fn=0x3c11ccdd` (`heap_caps_malloc_prefer`), matching the previously known ~29.5 KB
SD-mount-sized request from `RUNTIME_PANIC_LOCALIZED_2026-09-06.md` — but that boot did **not**
crash from it within the capture window; SD was absent so the mount path failed cleanly
(`mount result=0`) and the device continued running, taking user START/STOP input without
incident for the remainder of the 60 s window. This is a second, separate, already-partially-
understood code path (SD mount sizing) and should not be conflated with the loop-task 4096-byte
finding above.

## What this rules out (Task 1 hypothesis table)

- **Heap corruption**: `integrity=1` (OK) at every report, no OK→FAIL transition observed.
- **Stack overflow**: every task's `stackFree` at the last snapshot is healthy (loop=23284,
  audio=6972, smf=5256/5000/5192, midi=2572/2588/2444 bytes) — nowhere near exhausted.
- **A different assert/exception**: the diagnostic explicitly caught this via the allocation-
  failure hook, not a generic panic backtrace; `Reset Reason: 4` and `stage=100 retained` are
  consistent with the allocator failure itself precipitating the reset (the plan's Task 2 did
  not instrument what happens *after* a failed `heap_caps_malloc` call — most ESP-IDF/Arduino
  call sites that don't check the return value dereference the null pointer immediately after).

This satisfies the plan's Task 3 transition condition: a named mechanism (loop-task 4096 B
INTERNAL|DEFAULT allocation exceeding a ~400 B runtime floor) that is distinguished from every
competing hypothesis in the Task 1 table, reproduced identically three times.

## What is still unknown

`heap_caps_malloc` is the allocator entry, not the caller. No call site or backtrace was
captured — Task 2's instrument was scoped to record the allocator function's own name, per the
plan's explicit constraint against fabricating a call site from the return address. Grepping
`src/` for a literal 4096-byte allocation surfaced only `kSmfStreamCacheBytes = 4096` in
`src/midi/smf_stream.h` as a same-sized constant; it has not been confirmed as the caller, and
no other candidate has been ruled in or out. This is exactly Task 4's job ("Объяснить бюджет
одновременных выделений") — not yet attempted this run.

## Task 4 progress: candidate call site, not yet confirmed

Static reading of `GroovePuter.ino::loop()` and what it calls unconditionally every tick (no
user input observed in any of the three crashing boots — no `[KEY]` lines precede any of them):

- `M5Cardputer.update()`, `LedManager::update()`, `Encoder8::update()`, the 40 ms UI redraw
  gate, the 5 s `[PERF]` log, and the diagnostics sample/report calls were inspected; none
  allocate dynamically on the path read.
- `MiniAcidDisplay::update()` → `servicePersistence_()` runs every tick (`src/ui/miniacid_display.cpp:203,282`)
  and calls two things unconditionally:
  - `GroovePuterPlatform::serviceCardputerSmfRoutePersistence()` — gated behind
    `runtime.takeLoadRequest(...)`, which is only ever queued from the SMF player page's
    user-driven load path (`src/ui/pages/smf_player_page.cpp:198`). No SMF was requested in
    any crashing boot, so this is a no-op on this path. Ruled out.
  - `captureUiSession_()` → if the live UI-session snapshot differs from the stored one,
    `scheduleUiSessionSave_()` arms a save due 1000 ms later; once due, every tick calls
    `GroovePuterPlatform::saveCardputerUiSession()` (`src/platform/cardputer_ui_session.cpp:80`)
    until it returns `true` — and `ui_session_save_pending_` is only cleared inside that
    success branch, so a save that keeps failing keeps re-running on every single tick, not
    once. This writes through Arduino `Preferences` (NVS), not SD, but NVS operations on this
    SDK are precompiled (no source under `~/.arduino15` to read directly) and its internal
    page/buffer sizing was not independently confirmed to be uninvolved.

This is a plausible mechanism for a delayed, input-independent, escalating-with-uptime failure
(consistent with the crash landing tens of seconds in rather than immediately), but it is a
**candidate, not a confirmed cause** — the plan is explicit that speculation must not substitute
for a controlled experiment. No `[SESSION] saved` line appears anywhere in the captured log, so
whether the save ever succeeds, keeps retrying every tick, or was never armed at all is unknown
from this capture alone.

## Superseded: the actual mechanism was found, not the two candidates above

Both persistence-path candidates above were ruled out empirically, not by further reading. The
`saveCardputerUiSession()`/`autoSaveSceneRecovery()` hypotheses were wrong: RTC checkpoints placed
immediately before each call (`Phase::BeforeSessionSave`, `Phase::BeforeSceneAutosave`) never
fired as the last recorded phase across two separate crash captures, directly refuting both.

A chain of RTC-checkpoint bisections (each one a single additive instrument, rebuilt and reflashed
between rounds, per the plan's "exactly one experiment" discipline) narrowed the crash site:

1. `Phase::Ui` (already present, but only wired into the key-driven `drawUI()` — the 40 ms periodic
   redraw in `loop()` called `g_miniDisplay->update()` directly, bypassing it) was added to that
   periodic call site. Result: `phase=ui` was the last recorded phase, not `control` — the crash is
   inside `update()`, not the keyboard-handling or PERF/RDIAG blocks that surround it.
2. `Phase::BeforeSkinDraw` / `Phase::BeforePageDraw` bisected `update()`'s first half
   (`servicePersistence_`, `syncVisualStyle_`, `handlePaging_` — all already ruled out or trivial)
   from its draw pipeline. Result: `phase=before-skin-draw` — the crash is inside
   `skin_->drawBackground()` / `skin_->tick()`.
3. `tick()` is trivial (integer increment only). The one allocating path in `drawBackground()` is
   the lazy line-cache rebuild in `src/ui/cassette_skin.cpp` (`linePlain_`/`lineEven_`/`lineOdd_`,
   three `std::vector<uint16_t>` sized to the 240 px panel width). A `Phase::InSkinCacheRebuild`
   checkpoint placed at the top of that block confirmed it as the last phase before crash, across
   two more boots — this is the failing allocation.

**Root cause:** `CassetteSkin::drawBackground()` lazily (re)builds a ~1.4 KB three-buffer dither
cache the first time free DRAM has already collapsed to the ~400 B post-setup floor documented
above. `std::vector::assign`/`resize` on allocation failure aborts with no handling (this codebase
builds without exceptions), which is the actual `Reset Reason: 4` panic — not a corruption, not a
stack overflow, not the SD driver. Why the rebuild condition re-evaluates true again later in a
long-running session (theme and panel width are both provably immutable after construction) was
not fully explained; it did not block the fix, since a fixed-size cache removes the failure mode
regardless of trigger frequency.

## Fix, attempt 1 (reverted): eager fixed-size cache

Converting the three vectors to `std::array<uint16_t, Layout::SCREEN_W>` embeds their ~1.4 KB
inside `CassetteSkin` itself, paid in one shot when `MiniAcidDisplay`'s constructor does
`std::make_unique<CassetteSkin>(...)` — which happens immediately *before* that constructor's own
page-materialization call, previously the tightest point in the whole boot (documented elsewhere
as ~1400 B free right before `[UI] Creating page N`). Hardware validation of this attempt showed
the crash got **worse**: `Reset Reason: 4` with `[BOOT] Previous stage retained: 70` (crashing
*inside* the `MiniAcidDisplay` constructor, before page creation, never reaching stage 71) on
essentially every boot, at `internal8=4620/2292` — far more free memory than before, because the
new failure point moved earlier in the boot, not because anything was fixed. The fix didn't remove
the ~1.4 KB requirement, it just relocated the same cost onto a *tighter* margin. Reverted.

## Fix, attempt 2 (validated): keep the lazy vectors, guard the allocation itself

Reverted to the original lazy `std::vector` design (identical memory timing to the pre-fix code —
no new peak-memory requirement introduced anywhere), and added a pre-flight free-heap check in
`CassetteSkin::drawBackground()` before attempting the rebuild:

```cpp
const uint32_t freeNow = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT);
if (freeNow < neededBytes + kSafetyMarginBytes) needsRebuild = false;
```

If the rebuild is skipped, `lastBgColor_` is left untouched (retries on a later frame once heap
recovers) and the frame falls back to a flat single-color `fillRect` instead of the dithered cache
— no allocation, never crashes. This is a UI-only, purely-cosmetic degradation path, gated behind
`#if defined(ESP32) || defined(ESP_PLATFORM)` so the SDL/desktop build is untouched.

**Hardware validation** (`logs/serial-recovery-diag-fixed3-20260907-202443.log`, ELF SHA
`bbfea0b843f21901548dc2cdfcbef4e6febcffaa384899731ce2158e3595b807`, 180 s window, SD present): one
boot ran for 78,000+ checkpoints (multiple minutes) — previously every single capture crashed
within tens of seconds to low tens-of-thousands of checkpoints. A `[RDIAG-ALLOC] bytes=4096`
first-failure latch still recorded once (the guard correctly caught a real low-heap moment), but
critically the device **did not crash** at that point — checkpoints kept advancing and a
`[DSP] START command received` was processed afterward. The eventual reset that boot did hit came
much later, at `phase=idle` (unrelated to the skin cache) with `internal8=120/52` — heap had
gradually eroded from the usual ~400 B floor down to ~120 B over the long run. That is a distinct,
slower degradation (likely a genuine slow leak elsewhere, now exposed only because this crash no
longer masks it) and is **out of scope for this fix** — recorded here as a follow-up, not chased
further this session given host-hardware fatigue (SD card pulled/reseated, device fully dropping
off USB, multiple hung boots requiring manual resets).

## Result

The specific, previously 100%-reproducible `CassetteSkin` line-cache crash (the mechanism this
whole D1 investigation was chasing) is fixed and hardware-validated. Host suite
(`tests/run_host_tests.sh`) passes; both the diagnostic and production images build clean and pass
`scripts/check_cardputer_dram_budget.sh` (190968 B / 191488 B diagnostic, 190776 B / 191488 B
production — unchanged from before the fix, confirming no new static-DRAM cost). A slower,
unrelated heap-erosion pattern surfaced during the long validation run and remains open for a
future session.

## Second mechanism found the same evening: unguarded page construction

Manual navigation to the drum sequencer / synth pages reproduced a second, independent instance of
the identical root cause: `MiniAcidDisplay::getPage_()` lazily constructs `IPage` subclasses via
`std::make_unique` (`src/ui/miniacid_display.cpp`) with no allocation-failure handling, same as the
`CassetteSkin` cache was. Fixed the same way — refuse the switch rather than attempt an allocation
that could fail:

- Added `pageAllocationSize(int index)`, a table of *empirically observed* `createPage_()`
  allocation costs (the same `freeBefore - freeAfter` delta `createPage_()` already logs), pulled
  from every capture in this session (`grep -h "Page.*created SUCCESS" logs/*.log`, max per index).
  `sizeof(PageType)` was tried first and rejected: it only measures the object's own footprint, not
  memory its constructor separately heap-owns, and undercounted the real cost enough to let an
  unsafe allocation through anyway.
- `getPage_()` checks free DRAM against that cost (+ margin) before calling `createPage_()`; if
  insufficient, the page stays unconstructed and a `showToast("LOW MEMORY", ...)` fires. The
  existing render path in `update()` already handles a null `getPage_()` result (draws
  "PAGE INDEX INVALID") — no new fallback needed, just not tripping the allocation.

**Two hardware-found bugs in the fix itself, both corrected in this session:**

1. The guard checked `MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT`; `createPage_()`'s own
   "DRAM: N bytes free" print — and every other free-heap print in this file — uses
   `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`. The two report different totals on this target. The
   mismatch let a page through that the correct (8BIT) view already knew didn't fit, and it
   crashed anyway (`[UI] Creating page 5 (DRAM: 1424 bytes free)` immediately followed by
   `Reset Reason: 4`, repeatedly). Fixed by matching caps.
2. `MiniAcidDisplay`'s constructor restores the persisted session's last-active page via a
   **direct** `createPage_()` call, bypassing `getPage_()` (and therefore the guard) entirely. A
   session that was last on the drum page reproduced a hard boot-loop indistinguishable from the
   original SD-panic-loop symptom — `[UI] Creating page 5 (DRAM: 1424 bytes free)` on every single
   boot, at the maximum free DRAM this device ever has post-setup, immediately followed by
   `Reset Reason: 4`. Fixed by routing the constructor's initial page through `getPage_()` instead
   of calling `createPage_()` directly.

**Threshold tuning, hardware-validated across several rounds:** a flat `sizeof`-derived margin of
512 B was crash-free but blocked routine navigation to the synth pages almost unconditionally
(typical post-boot free DRAM, ~1088-1128 B, sits below the ~1036 B empirical cost + 512 B). Narrowed
to 70 B — inside the confirmed gap between an observed crash (free=1064 B) and an observed success
(free=1160 B) for the same page — restoring normal navigation without re-observing that crash.

**Confirmed fixed by hardware validation:** the drum-page boot-loop (constructor bypass) is gone —
`[UI] Page 5 creation skipped: low memory (free=1424 need=1528)` now prints on every boot instead of
crashing, across a full capture with the persisted session pointing at the drum page.

## Third mechanism, same evening, not fixed: this is systemic, not a single call site

With the drum-page boot-loop closed, normal use (navigating from the restored drum-page fallback
to the synth page) produced a *third* crash of the identical shape, in a *third* location:

```
[UI] Creating page 2 (DRAM: 1112 bytes free)
[UI] Page 2 created SUCCESS (size: 1032, DRAM left: 80)
...
Reset Reason: 4
[RDIAG-PREV-TASK] name=loop seq=10624 phase=before-page-draw core=1 stackFree=23252
[RDIAG-PREV-ALLOC] bytes=4096 caps=0x00001800 task=loop core=1 fn=0x3c11cde8
```

Page 2 construction itself succeeded this time — leaving only 80 B free — and the device crashed
moments later during `currentPage->tick()`/`draw()` (last checkpoint `Phase::BeforePageDraw`),
almost certainly a fourth unguarded allocation, this time inside `SynthSequencerPage`'s own render
path. Not investigated further this session.

**This is the actual finding of the evening, not any one of the three specific crash sites**: this
device's free internal DRAM after boot is a few hundred to ~1400 bytes, and essentially *any*
unguarded heap allocation anywhere in the UI/render path is a candidate for the exact same
`Reset Reason: 4` abort once it lands at the wrong moment. Three independent instances of the same
defect shape (`CassetteSkin`'s line cache, page construction, and now page rendering) were found in
a single evening of ordinary navigation, each only found because the previous one stopped masking
it. Auditing every lazy allocation in the page/draw hierarchy for the same failure mode, or — more
durably — increasing the actual free-DRAM floor (the ~30 KB SD-mount cost and other boot-time
consumers already flagged in `SD_PANIC_LOOP_WITHOUT_AUTOSAVE_2026-09-06.md`'s Task 4), is unstarted
and is the right next step, not chasing individual call sites one hardware cycle at a time.

## Aside: USB MIDI keyboard (nanoKEY2) — not a bug, a missing feature on this branch

Tested plugging a nanoKEY2 into the Cardputer during this session; its power LED never lit,
and `[MIDI-RX] ... rx=0/0/0/0` never moved. The operator confirmed the same cable/keyboard *did*
power up under a different firmware. Checked this branch's source
(`src/platform/cardputer_usb_midi_transport.{h,cpp}`): it includes Arduino-ESP32's `USBMIDI.h` and
calls `USB.begin()` — that is USB **device** mode (the Cardputer presents itself as a class-
compliant MIDI device to a host computer). There is no USB **host** stack anywhere in this branch
(`usb_host_install`, VBUS/5V enable, HCD — none of it greppable in `src/`), so there is no code
path that would ever power an external keyboard plugged into the Cardputer's port. This is not a
regression from tonight's fixes; the feature simply isn't implemented on this branch. Per the
session's opening transcript, USB MIDI host input (USB MIDI 1.0 + UART running-status
normalization) is being built on a separate worktree, `miniacid-midi-io-nanokey2` — that is almost
certainly the firmware the operator remembered powering the keyboard.

## Session close

Stopped here by user decision, given the hour and the number of hardware cycles already spent
tonight (multiple device hangs requiring manual reset, one full USB drop, several margin-tuning
round-trips). Two real, hardware-validated fixes are committed to the tree: the `CassetteSkin` line
cache guard and the page-construction guard (both call sites: lazy `getPage_()` and the
constructor's restore path). The systemic low-DRAM-floor problem, and the specific `before-page-draw`
crash it just exposed, are recorded above as the starting point for the next session.
