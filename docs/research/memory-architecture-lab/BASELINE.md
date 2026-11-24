# MAO-0: partial baseline and findings

Status: **BLOCKED / incomplete**, 2026-11-24. Research checkout started clean at `e3f16d57898182a8b68946ac7828d487b0973c5b`. Its sole difference from `8bfc5b8143fe816d2b42972d625206f277dc4a45` is this lab's planning README (verify `git diff --stat 8bfc5b8143fe816d2b42972d625206f277dc4a45 e3f16d57898182a8b68946ac7828d487b0973c5b`). Firmware source identity is therefore established, but build identity embeds the commit and an exact-head build was **not** performed. Values below are CI measurements of the parent, not a binary-identical build of the research head.

## Evidence recovered from existing CI

Run [Memory 44440879526](https://github.com/Ostrovsky42/GroovePuter/actions/runs/44440879526), [Cardputer ADV 44440879568](https://github.com/Ostrovsky42/GroovePuter/actions/runs/44440879568), [MIDI Targets 44440879520](https://github.com/Ostrovsky42/GroovePuter/actions/runs/44440879520), exact parent SHA, 2026-11-19. Measurements are compiler-reported program bytes and `.dram0.data + .dram0.bss`, not runtime free memory. Existing gate is 191,488 B, explicitly provisional, not a proven hardware safety boundary.

| profile | data B | bss B | gate sum B | headroom B | program B | evidence |
|---|---:|---:|---:|---:|---:|---|
| stock CDC | 44,748 | 142,976 | 187,724 | 3,764 | 1,739,138 | Memory normal/product, job 135281123255 |
| stock MIDI-only | 44,828 | 143,328 | 188,156 | 3,332 | 1,787,750 | Memory midi-only/product, job 135281123173; SEQTRAK job 135281122857 |
| FS1B CDC | 44,748 | 142,984 | 187,732 | 3,756 | 1,763,514 | ADV job 135281123935 |
| FS1B MIDI-only | NOT MEASURED | NOT MEASURED | NOT MEASURED | NOT MEASURED | NOT MEASURED | no matching CI product artifact found |

Stock ELF SHA-256: CDC `2836bf1f7a928277a39ba61a1fd6629246e94710331c023987d2138b17e42b16`; MIDI-only `ac51b1c3821247efd6ff856e42bf1075d69c21406bc645cec38cc22cf493cb59`. Builds used generated source identity in a copied tree. For stock CDC BSS, the report gives symbol interval coverage 142,755 B, uncovered 221 B, raw alias overlap 828 B: never sum raw symbol sizes as section totals. COMMON, padding, other memory sections and IRAM require the full map and section report. The same CI reported success for Core host and SDL on the parent; this is not local validation of this head.

`SYMBOL_INVENTORY.csv` is the 40 largest named DRAM entries printed in stock CDC CI, descending; exact parent build only. It is not the requested full per-symbol ownership audit, nor a top-30 IRAM inventory. Source-level constraints and provisional candidates are below and in `LIFETIME_MATRIX.md`; untouched third-party objects remain unattributed beyond the CI log. Preserve distinction between estimated savings and measured symbol size.

## Candidate triage — investigation only

| rank | candidate / evidence | theoretical bound, not measured saving | risk / effort | required proof |
|---|---|---|---|---|
| 1 | `g_phraseAuditionScratch`, 8,220 B static; examine bounded lifetime | at most 8,220 B gross if all storage is replaced, actual net unknown | high / high | overlap and reentry analysis; pending ACTIVATE and playback invariants; host, MIDI, hardware timing |
| 2 | `s_tempLoadScene`, 26,376 B static; transaction staging | at most 26,376 B gross, heap allocation would shift rather than guarantee a net gain | very high / high | atomic load, backup/error rollback, playback overlap, largest contiguous heap and timing |
| 3 | immutable sound/lookup data, including sine 4,096 B and SP12 arrays 8,000 B combined in parent ELF | up to represented bytes only after proving runtime initialization and exact placement; net unknown | medium–high / medium | waveform/output equivalence, flash cache-off safety, CPU and exact ELF deltas |
| 4 | `g_slots` 3,712 B and other pending-generation structures | unknown; full removal unsafe | very high / high | prove live ownership through commit/activate, undo and audio synchronization |
| 5 | framebuffer 64,800 B heap request (source), display strategy | up to 64,800 B gross only if replaced, net unknown | high / high | resolution/color/UX parity, UI latency and allocation failure/recovery |
| 6 | task stack reserves loop 32,768, audio 8,192, lazy SMF 6,144, dispatch static 4,096 B | unknown; no safe decrease without hardware high-water + worst-case call paths | high / medium | stack units, guard/overflow checks and cold/error/load paths |
| 7 | TinyUSB MSC/DFU/NCM buffers 14,608 B in parent ELF | up to 14,608 B gross, not an approved change | very high / high | isolated reproducible SDK experiment; prove no supported functionality or recovery removed, USB enumeration and reconnect. Production SDK unchanged |
| 8 | cache / IRAM | unknown; actual cache config and linker regions unavailable | very high / high | pinned SDK config, link maps and capabilities, timing and physical stability; SDK changes only in future isolated lab |

Do not add these bounds: overlap, residency shifts, alignment, metadata and replacement storage invalidate such a sum. `g_mainScene` and `g_miniAcidInstance` are live application state, not inherently waste. `g_smfPlayer` is 11,504 B static in the parent ELF despite lazy runtime initialization: deferment does not imply all object storage is freed. Likewise task stack and display numbers are requested capacities, not measured free-space deltas.

## Key limitations

No physical Cardputer ADV was available: chip revision, physical PSRAM, internal/DMA heap minima, largest blocks, stack watermarks, audio and MIDI timing, actual DMA consumption, cache behavior and endurance are **BLOCKED**. No exact-head product ELF, map, SDK headers/linker scripts, `size -A`, `nm` or `objdump`; IRAM and cache configuration unverified. Runtime and CI artifact protocols appear in `BUILD_MANIFEST.md` and `WORKLOADS.md`. No optimization, firmware functionality, production SDK configuration, gate or tests were changed.
