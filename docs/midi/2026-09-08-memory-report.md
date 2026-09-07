# Memory report: where DRAM goes, what nanoKEY2 Host would cost, why it isn't in yet

Answers, in order: what we measured tonight, where the bytes go today, what a
USB Host integration would need, and why M1-M3 of
[the memory plan](../superpowers/plans/2026-09-07-midi-memory.md) aren't done.
Everything below is either a hardware measurement made this session (cited
with its log) or explicitly marked as an estimate. Commit: `8c275100`.

## 1. The hard limit

ESP32-S3, no PSRAM: **327,680 B** total DRAM (`Maximum is 327680 bytes` —
Arduino's own linker report). Of that, this build's global/static variables
already claim:

```
190,872 B fixed (build_cardputer_memory_baseline.sh, normal/runtime)
190,776 B fixed (normal/product)
```

That's **58%** of all DRAM, gone before `setup()` runs a single line — audio
tables, the scene buffers, the drum patterns, USB class buffers, the engine
itself (`g_miniAcidInstance` alone is 16,888 B; `g_mainScene` and
`s_tempLoadScene` are 26,048 B each). This is this project's own DRAM gate,
already near its ceiling — reducing it further means shrinking those
structures, not a memory-management fix.

That leaves a **nominal ~136,712–136,904 B** of heap+stack (Arduino's "for
local variables" figure). What actually happens to it is the real question.

## 2. Where the nominal heap actually goes (boot-stage measurement)

Captured tonight with `[MEM-*]` per-boot-stage snapshots
(`scripts/build_cardputer_memory_baseline.sh normal runtime`, fixed by this
commit — the script never defined `GROOVEPUTER_RUNTIME_DIAGNOSTICS`, so these
snapshots were silently no-ops before). `INTERNAL|8BIT` free, one full cold
boot with an SD card present:

| Boundary | Free (B) | Cost of this step | Owner |
|---|---:|---:|---|
| after M5 + I2S | 137,972 | — | hardware bring-up |
| after display init | 72,432 | **-65,540** | display driver/framebuffer |
| after AudioTask create | 63,352 | -9,080 | audio task + stack |
| after 2× TempoDelay buffers | 44,648 | -18,704 | DSP delay lines (8.6 KB × 2 + overhead) |
| after SD mount | 14,504 | **-30,144** | SD/FatFS |
| after SMF runtime begin (old, eager) | 4,728–5,076 | -9,428–9,776 | SMF player task/queues |
| after USB MIDI device sink | 4,620–4,688 | ~0 to -108 | existing USB-device sink |
| after `MiniAcid::init()` (scene load) | ~2,000 | ~-2,600 | engine + scene parse |
| after sample scan | ~2,000 | ~0 | (no samples present in this scene) |
| after first UI page construction | ~1,300–1,900 | -400 to -700 | one `IPage` |
| **`setup()` complete (old)** | **~200–400** | | **this is the floor everything else lands on** |

Five owners — display, SD, SMF, DSP buffers, AudioTask — account for
**~132,900 B** of the ~137,000 B nominal heap before the UI ever draws a
frame. What's left for the *rest* of the app (every page, every cache, every
feature added from here on) is whatever's left over: **a few hundred bytes**,
which is exactly the floor this evening's crash investigation
(`docs/audits/RECOVERY_DIAGNOSTICS_D1_2026-09-07.md`, ported to this branch
in this commit) found on the sibling P3 branch, then reproduced here
byte-for-byte (`fn=heap_caps_malloc`, `bytes=4096`, same crash shape).

## 3. What this commit recovered: SMF was already lazy, boot just didn't use it

`cardputer_smf_player_registry.cpp` already has a `LazyCardputerSmfPlayer`
wrapper with `ensureStarted()` built specifically to defer the ~9 KB SMF
task/queue cost until the player is actually used — flagged as an open M2
item in the plan. `GroovePuter.ino`'s `setup()` called
`beginCardputerSmfPlayerService()` directly, forcing that cost on *every*
boot regardless of whether MIDI Player is ever opened. Removing that one call
(this commit) was enough to more than reproduce the SMF line's savings
downstream:

| | Before (crash-fixed, SMF eager) | After (SMF lazy) |
|---|---:|---:|
| Free at first UI page creation | ~1,000–1,900 B | **~8,800–10,964 B** |
| Drum page (needs ~1,528 B + margin) | blocked by the memory guard every time | succeeds every time (~7,300–7,400 B left after) |
| Synth pages (need ~1,036 B + margin) | intermittent (right at the threshold) | reliable |

Net measured gain: **roughly +7,000–7,400 B** of usable headroom at the point
that matters most (UI construction), for one line removed. This is a real,
hardware-confirmed number, not an estimate.

**The cost didn't disappear, it moved.** When the user actually opened MIDI
Player after ~80 s of page navigation, `CardputerSmfPlayerService::begin()`'s
own `xTaskCreatePinnedToCore(..., kPlayerTaskStack=6144, ...)` failed —
*not* from insufficient total free memory (6,592–6,704 B was free) but from
fragmentation: the largest contiguous `INTERNAL` block had dropped to 5,108 B,
smaller than the 6,144 B stack the task needs in one piece. This is
`cardputer_smf_player.cpp`'s own pre-existing, already-graceful failure path
(`"[SMF-INIT] task creation failed"` → `SmfPlayerState::Error`, no crash) —
not a regression from this fix. It's the honest trade this lazy pattern
makes: SMF goes from "always resident, always ~9 KB gone" to "usually
available, occasionally needs a retry after heavy navigation fragments the
heap." User's call tonight was to keep it lazy.

## 4. What nanoKEY2 Host mode would cost — honest answer: not fully measured

Two separate things get conflated here, and neither one is the number the
question wants:

- **The standalone `nanokey2_h0` probe's own free-heap** (`docs/midi/2026-09-07-midi-io-evidence.md`,
  observations like `ESP.getFreeHeap()=342684`–`347340`) is *not* a Host-mode
  cost. That probe is a bare sketch — no MiniAcid engine, no scene, no
  display skin, no SD-backed anything. Its static footprint is 23,784 B (7%
  of DRAM) versus this product's 190,872 B (58%). Its absolute free-heap
  number tells you almost nothing about what Host mode would cost *inside*
  GroovePuter.
- **The actual Host *delta*** — free-heap immediately before `usb_host_install()`
  versus immediately after install + client + device-open + one transfer,
  captured in the *same* boot — is exactly M1 in the plan, and it has not
  been captured. The one existing data point
  (`docs/midi/2026-09-07-midi-io-evidence.md`, "M1 Host memory probe") shows
  *no* observable growth between `HOST-INSTALLED` and `HOST-FIRST-NOTE`
  within that probe, and a `+2,924 B` (not a loss) difference across one
  attach/detach cycle — but it's missing the one point that matters, a
  pre-`usb_host_install()` baseline in the same boot. Until that exists, "how
  much does Host cost" has no measured answer, only bounds implied by
  general ESP-IDF `usb_host` component behavior (client control structures,
  one device's descriptors, one transfer buffer) — commonly a few KB, but
  stating a number here without measuring it in this binary would be exactly
  the kind of guess this investigation has repeatedly found wrong tonight
  (see the page-size table in `src/ui/miniacid_display.cpp`: `sizeof()`
  guesses undercounted real costs twice before empirical numbers replaced
  them).

## 5. What headroom would actually be needed

The plan's own admission rule (`docs/superpowers/plans/2026-09-07-midi-memory.md`,
M2):

```
freeBefore(c) >= peakAdditional(c) + reserve(c)
reserve(c) = max(16384 B, 2 × measured peak transient allocation)
```

Take the *best* free-DRAM moment this session ever measured — right after
the SMF-lazy fix, at UI construction: **~8,800–10,964 B**. That's already
*below* the plan's own minimum `reserve` of 16,384 B, before a single byte of
Host stack is added. So even with tonight's ~7 KB win banked, this device
does not yet clear its own written-down admission bar for a feature that
needs to coexist with everything else — the shortfall is on the order of
**at least 5,000–8,000 B more**, and that's assuming Host mode's own peak
cost turns out to be small; if M1 measures it at say 3–5 KB, the shortfall
is closer to 10,000–13,000 B before Host mode could be admitted by this
plan's own rule.

Where that could plausibly come from, roughly in order of how entangled/risky
each is (none of this is started):

1. **SD, deferred the same way SMF was** — SD mount is the single largest
   remaining boot-time cost (~30 KB) but, unlike SMF, `MiniAcid::init()`
   calls `initializeStorage()` directly for scene loading — SD isn't an
   optional feature here, it's how the current scene gets loaded at all.
   Deferring it means restructuring scene load, not removing one call.
2. **The two `TempoDelay` buffers (~18.7 KB)** — currently eager "to close a
   fragmentation window before SD/SMF," per their own comment; whether that
   guard is still needed once SD is also lazy is untested.
2. **The display driver's 65,540 B** — the single largest line item of all,
   and the least likely to be reducible without changing what the display
   library or its framebuffer strategy is.
3. Auditing further per-page/per-draw allocations the way `CassetteSkin` and
   page construction were tonight — bounded, incremental, but each one is
   another hardware round-trip to find and confirm, and there is at least
   one more known unguarded site left open (`SynthSequencerPage`'s own
   `tick()`/`draw()` crashed once tonight at `DRAM left: 248`, undiagnosed).

## 6. Why it isn't in yet — the milestone chain, plainly

- **M0 (measure the product's own budget):** done this session, both here
  and on the sibling P3 branch. Result: the device runs on a few-hundred-byte
  floor by the time it's interactive, for reasons unrelated to MIDI at all.
- **M1 (measure Host's actual incremental cost in this binary):** not done.
  The isolated probe works and receives real MIDI data qualitatively, but
  its own free-heap number is not a usable estimate for the product context,
  and the one in-probe attempt at a Host-cost delta is missing its
  pre-install baseline point.
- **M2 (make the budget actually work: free enough, then gate the
  transition):** not done, and per §5, even after tonight's real ~7 KB win,
  current headroom is still under the plan's own stated minimum reserve
  before Host's cost is even added.
- **M3 (hardware acceptance of the integrated product):** blocked behind M1
  and M2; not reachable yet.

Tonight's work is a real, measured step on M0 (and partway into M2, via the
SMF fix) — not a completed budget. The honest ballpark for "how far off are
we": **on the order of 10–15 KB more headroom** would be needed at the
riskiest moments, on top of what SMF-laziness already recovered, before this
plan's own admission rule would let a USB Host feature in without becoming
the seventh crash mechanism found this way.
