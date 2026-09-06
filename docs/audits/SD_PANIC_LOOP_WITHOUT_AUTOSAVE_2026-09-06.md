# SD panic loop without autosave — 2026-09-06

Continuation of [SD_MOUNT_UNAVAILABLE_2026-09-05.md](SD_MOUNT_UNAVAILABLE_2026-09-05.md)
and [CARDPUTER_RECOVERY_REVIEW_2026-09-05.md](CARDPUTER_RECOVERY_REVIEW_2026-09-05.md).
Session goal was to run `scripts/build_cardputer_recovery_diagnostics.sh`
(`DebugLevel=error`) to get the SD driver's real mount-failure reason. That
build was never reached — a live crash loop on the user's device took
priority, per the standing rule to stop an active failure before continuing
diagnosis.

## Live-keyboard MIDI: confirmed working, closed

Traced `/tmp/nosd-check.log` (captured last session) line by line against
source, not by re-running the earlier hysteresis theory. `dispatchedControl`
(`cardputer_usb_midi_transport.cpp:457`, incremented once per
`g_controlQueue.tryPop` in `drainControlEvents()`) climbed `1→7→12` across
real note-key presses (`s`, `g`), proving `PerformanceKeyboard::keyDown()` →
`QueuedUsbMidiSink::handleMusicalEvent()` → dispatch is intact end to end.

Four consecutive `f` presses immediately after that produced no increment at
all — traced to `WorkflowPages::allowsPerformanceKeyboard(page_index_)`
(`workflow_mode.h:315`), which gates `performance_keyboard_.keyDown()` in
`miniacid_display.cpp:625` to the PERFORM page only. The intervening `]` key
(`miniacid_display.cpp:630`) had navigated off PERFORM (only reachable there
because `keyDown()` returned false first, per the fallthrough at
`miniacid_display.cpp:628-632`). Not a bug: the `f` presses were on a page
where the live keyboard is deliberately inactive. No code change.

## Autosave scenes found and moved, move likely did not survive

`/media/gg/196F-3950/scenes/` held 8 autosave files
(`*.auto.json`, ~114 KB each — `deep-signal`, `EXIT`, `mamimpo`,
`quiet-meteor`, `quiet-signal`, `rusty-polaris`, `wild-harbor`,
`windy-canyon`), each capable of tripping `MiniAcid::init() →
loadSceneFromStorage()` (`miniacid_engine.cpp:2816`), which loads
`sceneStorage_->hasSceneAuto()` unconditionally, no user action required.
Moved them into `scenes/auto_backup_2026-09-06/` and called `sync`. The card
was then pulled from the reader while `nautilus` still held it open
(`fuser` showed `DeviceBusy` on unmount) — an unclean removal on FAT/exFAT can
roll back recent directory metadata. The very next boot log still showed
`loadSceneFromStorage: recovered autosave`, consistent with the move not
having survived. Not re-verified since; the card has not been reinserted into
a computer since that boot.

## New finding: the panic loop reproduces with NO autosave present

The next capture, after the user reported clearing the scenes autosave
another way (`запускаю карту без scenes`), confirms this is bigger than the
autosave-load spike documented so far:

- Every one of 21 boots in a 180 s window, scenes folder empty
  (`loadSceneFromStorage: Streaming parse failed, loading default scene`
  every time — no autosave in the picture at all).
- Free internal heap right after SD mount + SMF runtime init is **~700–900
  bytes** (`[SMF-INIT] ready freeInt=4384` → after UI creation
  `DRAM left: 708` / `908` / `924` / `928` across different boots), with SD
  physically present, independent of any scene content.
- `Reset Reason: 4` (SW panic) every time, and — the key new fact —
  `[BOOT] Previous stage retained: 100` on every one of these: the crash
  happens **after** `setup() complete`, during runtime, not during scene load
  or UI page creation as previously assumed. The earlier
  "autosave load OOMs at `[UI] Creating page 2/12`" account explains a crash
  *during setup*; it does not explain this loop, which starts from a clean
  setup every time and dies later.
- No panic backtrace or `Guru Meditation` line was captured in the monitor
  log across any of the 21 cycles, despite ESP-IDF's panic handler normally
  printing one to UART0 unconditionally. `scripts/monitor_window.sh` shows
  frequent `[MONITOR] waiting: ... could not open port` / reconnect churn in
  the same windows, timed suspiciously close to where a backtrace would be
  expected — the capture method itself may be dropping it during
  reconnection, rather than the firmware not producing one. Not established
  either way.

## What this changes about the picture so far

The prior framing ("autosave JSON load drives heap to ~1 KB, causing the
observed reboot loop") is not wrong, but it is now known to be incomplete: SD
presence alone drives the post-boot heap floor into the same danger zone
(under 1 KB) with zero scene content involved, and the actual crash observed
tonight happens later, in the running system, not in the scene-load path.
Whatever consumes memory during ordinary runtime with SD mounted needs
identifying — this was not reached tonight.

## Workaround, unchanged

Removing the SD card stops the loop, as before. This was reapplied tonight
to stop the active failure on the user's device; no source change was made
as a result.

## For tomorrow

1. Run `scripts/build_cardputer_recovery_diagnostics.sh` for the original
   goal (real SD driver mount-failure reason at `DebugLevel=error`) — not yet
   done.
2. Get one uninterrupted capture across a panic boundary (a raw serial tool
   without the monitor script's reconnect/window logic, or a longer settle
   delay before starting the window) specifically to catch whether a
   backtrace is being produced and dropped, or never produced.
3. Identify what runs during the main loop with SD mounted that keeps
   consuming heap after a clean, low-but-nonzero post-setup floor — the
   current `RetainedBootStage` mechanism only marks `setup()` milestones
   (`GroovePuter.ino`, stages up to 100) and goes dark for anything that
   happens afterward; it has no visibility into where in `loop()` a panic
   lands. Extending it to record a small number of runtime checkpoints
   (audio update / UI update / MIDI dispatch / SD-related polling) would
   turn "retained stage 100" into a useful signal instead of a dead end for
   this specific failure mode. Proposed, not implemented — needs its own
   RED/GREEN pass and hardware confirmation, not something to add
   unverified right before ending a session.
4. Re-verify the `scenes/auto_backup_2026-09-06/` move actually happened by
   reinserting the card into a computer with a clean unmount this time
   (`udisksctl unmount` completed, not a busy-device physical pull).
