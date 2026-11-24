# GroovePuter Memory Architecture Lab (MAO)

Status: PLANNED — documentation only. No experiment has passed; no measurements are claimed.
Base: main @ 8bfc5b8143fe816d2b42972d625206f277dc4a45 (2026-11-24).
Branch: research/20261124-memory-architecture-lab.

## Mission

Determine the usable internal SRAM envelope of Cardputer ADV (ESP32-S3, no PSRAM assumed; verify hardware), recover memory without removing supported features, and derive an evidence-backed Memory Contract v2. Preserve autonomous audio and external MIDI/SEQTRAK behavior. Keep the MIDI Workstation implementation independent; port only validated changes as small reviewable PRs.

Historical context (reported M0, not reproduced here): v0.9.37 / 68506ac8, FS1B CDC default: .data 44,828 B, .bss 143,328 B, static DRAM 188,156 B, project cap 191,488 B, headroom 3,332 B, flash 1,812,746 B. The historical commit was not resolvable via remote API at planning time; never treat these numbers as the current baseline.

## Rules

- Baseline first, one variable per experiment, paired builds with same source, compiler, flags, core/IDF versions and hardware. Pin board, FQBN, SDK, dependency versions and build commands.
- No production source changes in planning commit. No merging, weakening CI, removing features, changing public behavior or release/version numbers as part of exploration.
- Keep all existing supported profiles and host tests; identify actual commands from repository, not invented ones. Record full SHA and dirty state for each run.
- Distinguish compile/host evidence from real-device evidence. Hardware tests are manual until performed; never claim PASS from host tests. Never flash hardware without operator approval.
- Measure gross and net savings in bytes, including instrumentation cost. Static DRAM is not a proxy for free internal SRAM. Cache changes may expose heap regions rather than extend linkable static DRAM; region capabilities and linker placement matter.
- Do not use universal allocator overhead assumptions, claim malloc-to-static increases free memory automatically, assume all IRAM trades 1:1 with DRAM, or add overlapping capability totals.
- Every experiment needs hypothesis, reproduction, metrics, negative tests, rollback and decision (PASS / FAIL / BLOCKED / INCONCLUSIVE).
- Prefer fixed-size structures and explicit ownership over custom allocator complexity. Never trade deterministic realtime timing for unverified savings.

## MAO-0 — Baseline and inventory (no firmware behavior change)

1. Discover current build/CI and memory guardrails; record actual limit, .data/.bss/IRAM/flash definitions and profile flags.
2. Reproduce exact source SHA across Cardputer ADV product, FS1B, CDC variants, MIDI-only/SEQTRAK if supported; include SDL/host regression results separately.
3. Save ELF, map, section totals, SDK config or precompiled SDK headers and linker scripts. Resolve toolchain nm/size/objdump paths; run size -A, nm -S --size-sort --radix=d, objdump -h; use idf.py size-files/size-components only if actual build supports them. Account for COMMON, alignment, padding and other sections; avoid double counting.
4. Rank largest 30 mutable/static symbols and IRAM functions, grouped by module. For each: bytes, section, owner, lifetime, mutability, alignment, DMA/ISR constraints, concurrency, source location, candidate action.
5. Identify dynamic allocation call sites, task stack allocations, driver buffers, library allocations, startup order; map project/transport/genre/UI/storage/audio/MIDI lifetimes and PREPARE/COMMIT/ACTIVATE interactions.
6. Record actual ICache/DCache configuration, IRAM placements, memory regions and enabled features; confirm PSRAM status and chip revision on hardware later.
7. Add baseline report, build manifest, symbol inventory CSV, lifetime inventory and candidate ranking. If hardware unavailable, mark runtime baseline BLOCKED and continue only with offline analysis.

## MAO-1 — Runtime telemetry and memory contract baseline

Build an opt-in diagnostic profile; instrumentation must not allocate or log in realtime, use bounded counters/ring buffers and flush outside critical paths. Capture boot checkpoints, steady state, before/after each workload, and failure paths. Track:

- internal byte-addressable free/min/largest: heap_caps_get_{free_size,minimum_free_size,largest_free_block}(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT).
- internal DMA free/min/largest using MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA (and 8BIT where required by actual consumer). Capability pools overlap; do not sum.
- heap_caps_get_info: free blocks, allocated blocks, total free/allocated; largest/free ratio is a diagnostic, not a universal fragmentation metric.
- Per-task configured stack and high-water mark (verify API units against pinned IDF; ESP-IDF normally returns bytes).
- Heap allocation/free/failure counters and caller attribution via supported hooks/heap tracing in separate diagnostic runs. Include C++ new, STL growth, libraries and realloc; hook code must be ISR-safe, no blocking, no recursion, no allocation.
- Runtime heap low-watermark is lifetime/per-region based; collect scenario minima with sampled checkpoints or supported local monitoring and repeat cold boot. Note per-region minima may not be simultaneous.
- Timing: MIDI clock interval/error/jitter p50/p95/p99/max; missed deadlines; event losses and note ownership; audio underruns, render time, UI frame time, storage latency, reset/wdt/boot failures. Use external MIDI capture where possible and record timestamp resolution.

## MAO-2 — Immutable definitions and static data

Hypothesis: mutable duplicates and eager copies waste DRAM.
- Inventory genre variants, lookup tables, fonts, immutable presets, MIDI CC/device profiles, scales/chords, sample metadata.
- Move verified read-only objects to Flash via const/constexpr and flash-safe layout; avoid dynamic initialization and unnecessary pointer-heavy tables; verify placement in ELF, not declarations alone.
- Separate immutable definitions from active mutable instances and user overrides. Audit cache-off access paths, alignment, hot working set and serialization compatibility.
- Remove redundant copies or pack representations only with proof of bounds, semantics, compatibility and measured CPU overhead. No feature removal.
- Compare symbol/section deltas and deterministic genre output for fixed seeds, MIDI golden traces, project load/save round trips, audio and UI performance.
- Rollback: one focused revert per data family.

## MAO-3 — Shared workspace / lifetime model

Hypothesis: mutually exclusive scratch operations can share one bounded arena (static or boot-allocated, choose by evidence).
- Build overlap matrix across genre generation, DEVELOP, import/export, project parsing, UI transitions, background tasks, cancellation, USB reconnect and playback.
- Only combine buffers proven non-overlapping by ownership and lifetime. Size = maximum concurrent requirement + alignment/metadata, not merely largest screen. Compare static union, typed workspace, preallocated bounded pool; prefer simplest safe implementation.
- Define acquire/release, bounded capacity, alignment, typed object lifetime, failure handling and debug owner/generation IDs. RAII for release on early exit; no blocking or arena acquisition in realtime. Handle reentry explicitly; refuse or defer conflicting work without partial mutation.
- Never overlay live project state, transport, note ownership, active playback queues, DMA buffers, ISR state or pending COMMIT data with scratch memory.
- Test canaries, bounds, failed acquisition, cancellation, reentry, stale references, concurrent audio/MIDI, repeated operations; host ASan/UBSan where supported.
- Rollback: retain previous allocation strategy behind experiment-only build option or revert isolated commit.

## MAO-4 — Realtime allocation, DMA and FreeRTOS

- Establish zero application-controlled dynamic allocation in MIDI clock, note dispatch, sequencer tick, audio render, performance control paths. Audit implicit allocation from std::string/vector, callbacks, logs, formatting. reserve alone is not proof; enforce capacity.
- Allocate long-lived DMA buffers at driver initialization before optional large allocations if supported by dependency order; validate correct caps, alignment, descriptor constraints, ownership and reconnect lifecycle. Do not assume all DMA allocations are contiguous or interchangeable.
- Compare startup allocation ordering, boot-reserved pools and static data only if fragmentation or allocation failure evidence justifies it. Moving malloc to static may reduce overhead but may increase reserved residency; compare total free and peak live memory.
- Audit stack peaks across deepest call chains, error paths, SD/USB, simultaneous playback and UI. Tune incrementally with conservative margin and stack overflow checks; high-water mark alone cannot prove safety.
- xTaskCreateStatic only where deterministic lifecycle benefits outweigh static DRAM cost; not automatic RAM savings. Preserve scheduling, affinity, priority, task teardown and restart.
- USB reconnect and filesystem/library allocations may be allowed outside realtime, bounded and verified. No blanket zero malloc after setup claim.
- Flash writes/cache-off: audit full IRAM-safe call graph and data residency, ISR flags, synchronization and flash/SD distinction. No flash erase/write in critical callback. Test persistence during playback and interruption recovery; defer or pause only according to existing user-facing contract.

## MAO-5 — Cache and IRAM laboratory

Start with actual cache config from MAO-0. Candidate pairs (KiB I/D): 32/64, 32/32, 32/16, 16/32, 16/16, only if supported by pinned target SDK and toolchain. Theoretical maximum difference is 64 KiB from 32/64 to 16/16; not a prediction for this device.
- Verify SDK Kconfig, linker scripts, boot behavior and freed region capabilities. Test DCache first while ICache fixed; then ICache separately. Cache-line sizes only as independent experiment.
- Arduino prebuilt libraries may require Arduino-ESP32 Library Builder or Arduino-as-IDF component; do not assume sketch macros override SDK settings. Pin and archive reproducible experimental SDK; no production build migration as prerequisite.
- Compare released SRAM/heap, largest DMA block, actual placement, boot and long workloads, performance and jitter; reject incompatible profiles. Smaller cache can degrade UI, Flash accesses and synthesis.
- Rank IRAM symbols; examine SDK flags for optional IRAM placement. Only move proven noncritical code and only if entire call graph is safe. Preserve cache-off and interrupt correctness; verify ELF placement and timings.
- If build customization cannot be reproduced or board tests unavailable, mark BLOCKED. Keep production SDK unchanged.

## Acceptance workloads (same fixtures for baseline and candidate)

W0 cold boot and init (at least 5 boots per candidate); W1 idle and playback (internal, external MIDI, and INT+MIDI where supported); W2 maximum supported track/polyphony/event load, tempo changes, transport, TIE and Note Off; W3 GENRE/DEVELOP repeated with fixed seeds and switching UI while playing; W4 FORM/song/scene transitions, macros and bank operations; W5 project save/load, import/export, SD failure, flash persistence and cancellation; W6 repeated USB/UART MIDI connect/disconnect/reconnect, panic and active notes; W7 combined worst supported scenario; W8 endurance at least 60 minutes for finalists, with repeated workload cycles and post-quiescence snapshots. At least 3 independent runs per workload/config; archive exact fixtures, duration, hardware, raw logs and failures. Keep SD backup and known-good firmware/restore path before device tests.

Provisional gates (not production changes):
- Existing CI, host tests and golden MIDI traces pass unchanged; no new feature loss, corrupt project, stuck notes, crash, watchdog or supported-event loss.
- Zero application realtime allocations after warm-up (including cold/rare paths tested separately); zero failed allocations in supported workflows.
- Internal 8-bit heap minimum target >=30 KiB, provisional warning 15–30 KiB, candidate rejected below 15 KiB until a justified contract revision. Baseline below floor is a finding, not silently waived.
- Largest DMA block must meet largest required allocation including alignment/driver constraints and measured safety margin; define per profile from inventory, not arbitrary fixed number.
- No leaks: compare same quiescent state after warm-up, distinguish persistent caches from monotonic growth, investigate any unexplained delta.
- No audio underruns or missed supported MIDI deadlines. Set numeric timing thresholds after baseline before testing candidates; compare p99 and max using same instrumentation and repeat runs. No regression outside documented baseline variance/measurement error; no masking of outliers.
- No stack overflow or unexplained low stack margin; establish per-task margin before reductions.
- Keep current static memory gate until MAO-6; a candidate crossing it can be evaluated only in explicitly isolated lab build, never by weakening production CI. Report actual linker feasibility.
- For any claimed gain show before/after absolute bytes, timing cost, complexity, confidence and profile-specific differences.

## MAO-6 — Decision and production transfer

Review all candidate results: accepted/rejected/blocked, measured benefit, risk, maintenance cost, portability and feature parity. Produce Memory Contract v2 per supported profile: static budget, internal heap floor/target, DMA largest block, stack margins, realtime allocation contract, timing limits, observability and hardware acceptance protocol.
Increase Static DRAM cap only if linker regions and measured worst-case runtime capacity justify it, with explicit review; no arbitrary 200+ KiB target. Keep total memory and section limits distinct. Test on hardware before production promotion. Integrate each validated change separately against current main/MIDI Workstation as applicable, not merge lab wholesale. Archive rejected experiments and reproducible results. Extract reusable analysis tooling only after proven useful.

## Expected artifacts

Under docs/research/memory-architecture-lab/ (planned, not yet created): BASELINE.md, BUILD_MANIFEST.md, SYMBOL_INVENTORY.csv, LIFETIME_MATRIX.md, WORKLOADS.md, RESULTS.md, MEMORY_CONTRACT_V2.md; raw evidence can be stored as CI artifacts with checksums and linked manifests rather than committing large binaries. Diagnostic scripts belong in a dedicated lab directory consistent with repo conventions discovered in MAO-0. Every report states source SHA, tool versions, build profile, hardware status and measurement limitations.

## First execution goal

Complete MAO-0 offline, inventory and rank candidates, define telemetry protocol and hardware blockers. Do not implement a speculative arena, change SDK or increase limits before reviewing inventory. Commit report separately. Then progress MAO-1 before runtime-dependent decisions; offline MAO-2 investigations may proceed without hardware, but production acceptance remains blocked.

## Primary references

Use documentation matching pinned SDK version, not latest blindly:
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/mem_alloc.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/performance/ram-usage.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/kconfig-reference.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/freertos_idf.html
- https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/heap_debug.html
- https://github.com/espressif/esp32-arduino-lib-builder
