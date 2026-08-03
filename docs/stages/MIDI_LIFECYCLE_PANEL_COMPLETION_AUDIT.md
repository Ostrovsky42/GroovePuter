# MIDI lifecycle and SMF panel completion audit

## Purpose

This document is the acceptance ledger for the stacked completion branch:

```text
base: feature/midi-lifecycle-panel-performance
head: feature/midi-lifecycle-panel-performance-completion
canonical product plan: PR #36 / PLAN.md
```

It does not expand the product scope of PR #35. It converts the stale PR checklist into verifiable implementation and acceptance evidence.

## Scope boundary

Included:

1. ownership-safe MIDI Continue TX;
2. capability-gated Song Position Pointer behavior;
3. immediate ownership-safe SMF track mute;
4. SMF panel session state across page eviction;
5. removal of routine synchronous UI logging from performance-sensitive paths;
6. cached MIDI directory enumeration outside drawing;
7. bounded partial SMF Player redraw;
8. host, SDL, Cardputer-Adv and direct Cardputer-Adv -> SEQTRAK acceptance.

Excluded:

- README compatibility tables;
- remote SEQTRAK CC23/CC24 mute;
- BLE MIDI;
- Wave 1 status chrome, scene revision, key-mode indication, root motion and cadence operators;
- a new navigation system, UI framework, MIDI dispatcher or transport task.

## Audit of inherited PR #35 head

The inherited PR body lists ten remaining tasks but marks none complete. Commit count is not accepted as evidence.

| Area | Inherited state | Required completion evidence |
|---|---|---|
| Continue TX | API, event type and concrete encoding exist; dispatcher path was added | host regression plus dispatcher source check; hardware observation where target supports Continue |
| SPP | API, encoding and capability helpers exist | queued single-owner dispatch, seek integration and explicit capability suppression for the conservative SEQTRAK profile |
| Track identity | scheduled SMF event carries `trackIndex` | producer, queue and dispatcher checks agree on the same bounded index |
| Queued mute | muted NoteOn rejection exists close to dispatch | regression proves NoteOff remains cleanup-critical |
| Immediate mute | mailbox and bounded ownership types exist | active notes of only the selected SMF track are released by `MidiDispatchTask`; Pattern/PERFORM ownership remains intact |
| Session state | `smf_player_session_state.h` exists | `SmfPlayerPage` restores and publishes path, selection, scroll and panel visibility across page destruction/recreation |
| Directory cache | `smf_directory_cache.h` exists | draw and row resolution perform no `SD.open`, `openNextFile`, mount retry or directory traversal |
| Partial redraw | `smf_player_dirty.h` exists | page uses bounded dirty regions; a progress/overlay update does not call full `LayoutManager::clearContent` |
| Logging | routine browser and transition logs remain | normal navigation/rendering contains no synchronous Serial output; failure and opt-in diagnostics remain available |
| Validation | no complete evidence set | host, SDL and Cardputer-Adv build references plus direct hardware checklist |

## Completion-branch implementation status

This table records wiring, not acceptance. A row is accepted only when the gates below pass on the same final head.

| Area | Current implementation |
|---|---|
| Continue / resume SPP | Persisted device profile selects conservative SEQTRAK or class-compliant GM capabilities. GM resume serializes `F2` before `FB`; SEQTRAK suppresses unvalidated `F2/FB` and uses the validated `FA` fallback. |
| Active seek SPP | A bounded latest-wins SPP mailbox uses the established SMF SPSC lane. PROJECT seek computes the target from SMF tick/division, schedules it from the current audio block anchor, and `MidiDispatchTask` owns the physical `F2` write. |
| Immediate mute | Consumer-side bounded ownership emits scoped NoteOff events for only the muted track. `tryPop()` only preflights capacity; ownership is committed by `MidiDispatchTask` after a successful USB write, so late, muted or backpressured NoteOn drops cannot create false owners. |
| Session state | Path, selection, browser scroll, inspector scroll and panel visibility are restored across cached-page navigation and page eviction. |
| Directory rendering | Drawing uses the existing seven-row `browserRows_` cache. SD traversal is excluded from `drawHeader`, `drawContent`, `drawFooter` and their draw helpers. The unused second cache abstraction was removed. |
| Partial redraw | The active MIDI Player intercepts content clearing. Re-entry and non-player views receive a safe full frame; now-playing updates erase only changed rows and the animated progress/wave row. |
| Routine logging | Page-local browser UART output is suppressed without disabling USB/SMF failure diagnostics in platform tasks. |
| Temporary patch artifacts | Trusted patch jobs restore the canonical workflow before committing implementation. The final diff must contain no temporary apply job. |

## Implementation checkpoints

```text
b89a9280a238c6119950beb30f1f3af6904ea1fc
feat(midi): schedule SPP for active PROJECT seek

f5a23653ed157db31edd7541ceebaa987ef46b5b
fix(midi): commit track ownership after USB success
```

Repository inspection confirms:

- capability-gated active-seek SPP publication in `CardputerSmfPlayerService`;
- bounded mailbox scheduling at an audio block anchor;
- `ScheduledSmfMidiEventType::SongPositionPointer` dispatch through `UsbMidiOutput`;
- physical output remaining inside `MidiDispatchTask`;
- bounded track ownership committed only after successful physical dispatch;
- ownership commit failure falling back to scoped SMF cleanup;
- restoration of the original `core-regressions.yml` after each patch job;
- no temporary active-seek or ownership workflow in the resulting head.

Bot-authored implementation commits can receive an `action_required` run with no jobs. Such status is not accepted as evidence. This ordinary follow-up commit exists solely to run host, SDL and Cardputer-Adv gates against the complete implementation on one clean head.

## Additional invariants recovered from PLAN.md

The following were not explicit in the original PR #35 checklist and are now mandatory acceptance conditions:

- `MidiDispatchTask` remains the only TinyUSB writer.
- Scheduled MIDI remains sample/deadline based.
- Source changes invalidate stale queued events and release only ownership belonging to the previous source.
- Stop, seek, mute, route change, source change, disconnect, panic and page eviction leave zero stuck notes.
- No UI drawing occurs inside `AudioGuard`.
- No SD/storage traversal occurs in `drawHeader`, `drawContent`, `drawFooter` or any function called from them.
- No dynamic allocation is introduced on audio or MIDI hot paths.
- Existing internal synth and drum audio remains available while MIDI output is enabled.
- Scene/project codecs remain unchanged by this completion branch.

## Completion gates

## 2026-08-03 USB/SD DRAM recovery

### Root cause

The current branch and `origin/main` use the same USB lifecycle and SD startup
order, but the merged branch added 8056 bytes of global DRAM:

```text
origin/main MIDI-only globals: 186832 bytes
failing branch globals:        194888 bytes
delta:                           8056 bytes
```

Late `USB.begin()` then lacked enough internal memory to create TinyUSB state.
Moving USB startup earlier only transferred the failure to the SD browser. The
failure was therefore a shared DRAM budget regression, not an SD protocol bug,
SEQTRAK bandwidth problem or misplaced `USB.begin()` call.

### Contracts

- Keep USB initialization at the established main lifecycle point.
- `MidiDispatchTask` remains the only physical TinyUSB writer.
- Do not reduce scheduled SMF capacity, stream cache, NoteOff priority or track
  ownership to buy memory.
- Project Import and MIDI Player share one filesystem owner.
- Browser capacity limits resident memory, not reachable files.
- Filesystem traversal remains outside audio and MIDI realtime tasks.
- Dirty rendering may reserve only the actual 240x135 Cardputer surface.
- MIDI visual pulses may be lossy and bounded; musical events may not.
- MIDI-only global DRAM must remain at or below 191488 bytes (187 KiB).

### Implementation

```text
MidiFileManager:       3676 -> 804 bytes
Cardputer display:     1572 -> 748 bytes
SMF player object:    15648 -> 15264 bytes
Total globals:       194888 -> 190808 bytes
Recovered:                       4080 bytes
```

The browser keeps eight entries resident for a maximum seven-row display. It
counts the whole directory and reloads a FAT-order window when scrolling. This
removes the former 48-entry visibility ceiling while keeping directories before
files. The SMF visual queue changed from 64 to 16 events; it affects only the
wave animation and retains a dropped-event counter.

`scripts/build_seqtrak_midi_only.sh` runs an ELF section check after compile and
fails when `.dram0.data + .dram0.bss` exceeds 191488 bytes.

### Automated evidence

```text
tests/run_host_tests.sh:              PASS
make -C platform_sdl all CXX=g++:     PASS
scripts/build.sh --warnings all:      PASS
normal globals:                       190864 bytes
MIDI-only globals:                    190808 bytes
```

Local hardware candidate:

```text
path:   build/cardputer-adv-seqtrak-midi-only/GroovePuter.ino.bin
sha256: e6cc36d82f4f48da82155664d33463e4d15924649a84826980c0e5612ce6a3a0
```

Hardware acceptance remains required. On MIDI Player the first status line must
show `C0 U1` after boot, files beyond the eighth directory entry must remain
reachable, and playback must advance `OK` without leaving `Q` permanently
backlogged.

### Gate A — Source and host tests

Required:

```text
bash tests/run_host_tests.sh
```

The host suite must include focused regressions for:

- Start versus Continue lifecycle selection;
- capability-gated SPP suppression and 14-bit encoding;
- active-seek SPP latest-wins and generation invalidation;
- track identity preservation;
- queued muted NoteOn rejection;
- ownership commit only after successful dispatch;
- immediate release of only the muted track;
- session restore after page recreation;
- zero SD traversal from draw paths;
- bounded dirty-region invalidation;
- single TinyUSB writer and no hot-path allocation.

### Gate B — SDL build and smoke

Required evidence:

- SDL target builds from a clean checkout;
- MIDI Player page can be opened repeatedly;
- browser/player/inspector/performance views retain state after navigation;
- progress updates do not flash or clear unrelated regions;
- keyboard navigation remains unchanged.

### Gate C — Cardputer-Adv build

Required build profile:

```text
ESP32-S3FN8
PSRAM disabled
PartitionScheme=huge_app
Arduino / M5Unified / M5Cardputer
```

The build report records flash/RAM deltas and confirms no scene codec change.

### Gate D — Direct Cardputer-Adv -> SEQTRAK hardware acceptance

Hardware:

- M5Stack Cardputer-Adv;
- Yamaha SEQTRAK;
- direct USB-C data connection;
- known-good `.mid` files containing multiple tracks and sustained notes.

Required observations:

- Clock/Start/Stop behavior remains unchanged;
- pause/resume emits the capability-approved lifecycle behavior;
- unsupported SPP/Continue claims are not presented as validated;
- muting a sustaining track silences it immediately;
- another SMF track sharing channel/note ownership remains correct;
- Pattern and PERFORM notes are not silenced by SMF mute;
- queued future NoteOn events from a muted track do not sound;
- unmute resumes only future events;
- stop, seek, route change, clock-source change, disconnect, reconnect and panic leave no stuck notes;
- page navigation and browser activity do not disturb timing;
- internal GroovePuter synths and drums remain audible;
- no reset, watchdog, heap collapse or sustained audio-underrun regression.

## Ready-for-test rule

The branch is announced as ready for one combined hardware test only when:

1. all implementation rows above are wired, not merely declared;
2. temporary patch workflows are absent from the final diff;
3. host, SDL and Cardputer-Adv gates are green on the same head SHA;
4. the exact firmware artifact/head SHA is recorded;
5. only Gate D remains for the user to execute on physical hardware.

The PR is ready for merge only after Gate D results are recorded and any failures are resolved.
