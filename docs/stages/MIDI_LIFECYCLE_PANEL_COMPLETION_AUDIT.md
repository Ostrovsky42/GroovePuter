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

### Gate A — Source and host tests

Required:

```text
bash tests/run_host_tests.sh
```

The host suite must include focused regressions for:

- Start versus Continue lifecycle selection;
- capability-gated SPP suppression and 14-bit encoding;
- track identity preservation;
- queued muted NoteOn rejection;
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
