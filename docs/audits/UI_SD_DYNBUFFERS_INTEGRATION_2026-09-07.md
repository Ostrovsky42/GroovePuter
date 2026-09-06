# UI Constitution + FS1 dynamic FatFs integration

Date: 2026-09-07

This document records the narrow integration of the accepted SD runtime-residency mechanism into:

`feature/20260906-04-0.9.10-ui-constitution-v1`

It is deliberately **not** a merge of the SD diagnostic workstream.

## Source provenance

Source workstream:

`feature/20260906-05-0.9.10-sd-runtime-residency`

FS1-B mechanism commit:

`0cb4a701646053a9ba84cdc390d9ba1ae52e33b4`

Final source-workstream hardware evidence head:

`c408376a82d9e2ca07004e987726cac59bf1bf4f`

Exact ESP-IDF source:

`858a988d`

Accepted configuration delta:

`CONFIG_FATFS_USE_DYN_BUFFERS=1`

Product file-handle contract remains:

`max_files = 5`

## Source hardware evidence

FS1-B source branch measured:

- SD mount residency recovered: **24028 B free**;
- contiguous recovery: **18944 B largest-block capacity**;
- five simultaneous handles: **5/5 open, 5/5 writable**;
- at five simultaneous handles: free **11668 B**, largest **7668 B**;
- after close: free **37680 B**, largest **26612 B**;
- real-use soak: **205 s**, 59 key presses/navigation/SMF playback;
- **0 panics**;
- **0 audio underruns**;
- free memory minimum during soak: **21656 B**;
- largest-block minimum during soak: **13300 B**.

These numbers are evidence for the source SD workstream, not an automatic acceptance result for the combined UI branch. The combined branch must be measured on Cardputer ADV because UI/Phrase/Undo code has changed since the SD candidate was characterized.

The source workstream also corrected an earlier assumption: one open Arduino `File` costs roughly 5.2 KiB in this configuration. The dynamic FatFs change did not create that cost; it removed the permanent mount-time reservation and lets file-related residency follow real file lifetime.

## Integration boundary

The UI branch carries only the reproducible build mechanism:

- `scripts/build_fatfs_dynbuffers_candidate.sh`
- `scripts/build_cardputer_dynbuffers.sh`
- `scripts/upload_cardputer_dynbuffers.sh`

No SD diagnostic runtime code, handle census instrumentation, panic tracing, or memory-phase instrumentation is merged into the product UI line.

The shared Arduino/M5Stack installation is never modified. `build_cardputer_dynbuffers.sh` creates a disposable SDK overlay, replaces only overlay `lib/libfatfs.a`, then builds through the normal Arduino platform recipe with `compiler.sdk.path` pointed at that overlay.

The build fails unless its link map proves:

1. the overlay candidate `libfatfs.a` was linked;
2. the stock SDK `libfatfs.a` was not also linked.

This preserves the source-workstream rule against mixing stock and candidate archive objects inside the linker group.

## Raw evidence caveat

The SD workstream records that its raw `*.log` captures were **not committed** because repository `.gitignore` excludes them. The audit/contracts and exact numeric observations are in Git; raw captures remain external/local evidence and should not be claimed as repository artifacts.

## Combined hardware acceptance target

The first combined UI+FS1 Cardputer run should verify both domains together:

- boot with SD inserted;
- no panic/reboot loop;
- no audio underruns;
- truthful PAT/PHR chrome and shell ownership;
- Phrase overview, GRID cursor, derived selection, duration edit, delete and runtime Undo;
- page navigation/PERFORM while audio is running;
- SMF playback while navigating the new UI;
- free/largest snapshots at idle, after SMF activity, first Phrase entry and Phrase mutations;
- loop-task stack HWM during Phrase mutation/Undo if diagnostics are available.

Do not require byte-identical memory numbers from the source SD branch: the combined UI candidate has additional runtime/UI state. The acceptance question is whether the large recovered margin remains and whether no new downward trend, panic, underrun or critical allocation cliff appears.
