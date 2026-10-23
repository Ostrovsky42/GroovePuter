# MEMORY-PRUNE-A Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement task-by-task. Independent TinyUSB and Voice audits use dispatching-parallel-agents; no shared production edits.

**Goal:** Remove optional Cardputer ADV runtime allocations while preserving the accepted musical and persistence paths.

**Architecture:** One board capability header selects optional Sampler/Tape/Waveform ownership at compile time. Desktop keeps these capabilities. Scene metadata remains decodable and round-trippable without a sample registry. TempoDelay and TinyUSB are independent opt-in experiments.

**Tech Stack:** C++17/Arduino ESP32-S3, SDL2, existing dynamic-FatFs builds and ELF/serial memory instrumentation.

**Spec:** User MEMORY-PRUNE-A request, source PR #494 HEAD verified af4d51072e99b7cd7497c332bfc6da8877294c26.

## Global Constraints

- Separate optimization branch; never change PR #494 directly or merge without permission.
- Do not raise DRAM gate (191488 bytes).
- Preserve HGS/Harmony/Genre, Melody A/B 8B/Song/FORM, TAKE/DEVELOP/BREAK, MIDI/SEQTRAK/SMF/GRAB, synthesized drums/main DSP, Undo and transactional persistence.
- No credits for already-disabled Tape buffers. Keep TempoDelay in product until musical tradeoff is assessed. Keep active Voice/Formant.
- All unsupported hardware checks and unavailable heap observations are OPEN, never inferred from ELF or SDL.

## Review Focus

- Old sampled projects load without dereferencing removed components; stable sample references survive re-save.
- Removed pages cannot be reached by shortcuts or restored sessions.
- No allocations or waveform copies in AudioTask for removed features; diagnostics retain peak/test-tone observations.
- A/B delay bypass is explicitly experimental and does not alter normal musical output.
- TinyUSB class flags must affect linked SDK objects and preserve MIDI/CDC/recovery, not just sketch headers.

### Task 1: Baselines and capabilities
Files: src/platform/cardputer_capabilities.h, tests/test_cardputer_prune_compat.cpp, scripts/experiments/*, docs/testing/memory-prune-a/*.
- [x] Capture baseline product/runtime ELF .data/.bss/flash and current device heap/largest-block samples; normal and MIDI profiles use distinct output directories.
- [x] Test board capability defaults and explicit A/B overrides.
- [x] Commit reproducible plan and baseline provenance.

### Task 2: Sampler exclusion
Files: GroovePuter.ino, src/dsp/miniacid_engine.{h,cpp}, src/sampler/*.cpp, src/input/internal_synth_output.cpp, src/output/output_mode_runtime.h, src/ui/miniacid_display.cpp, src/ui/pages/sampler_page.cpp, sample_scene_persistence.cpp.
- [x] Add failing legacy ID/stable-ref roundtrip proof without SampleIndex.
- [x] Gate registry, pool, loader, page, engine ownership, render/trigger and preload paths. Preserve scene metadata and synthesized drum triggers.
- [x] Build sampler-only A/B with other capabilities retained; record ELF deltas and heap/largest OPEN unless actually flashed.
- [x] Run compatibility and sampler host/SDL tests; commit coherent sampler cut.

### Task 3: Tape exclusion
Files: engine, src/dsp/tape_*.cpp, src/ui/pages/tape_page.cpp.
- [x] Prove tape buffers were already disabled; measure only removed remaining objects/control/render dependencies.
- [x] Gate runtime ownership/control and page implementations; retain legacy scene Tape fields.
- [x] Build incremental A/B and verify synthesized audio/persistence unaffected; commit.

### Task 4: Waveform storage
Files: engine waveform ownership/render capture, UI waveform overlay, GroovePuter acceptance diagnostics.
- [x] Remove optional double-buffer storage on Cardputer, retain audio peak and test tone diagnostics without sample history.
- [x] Prevent disabled overlay activation; validate source/target tests and incremental ELF comparison; commit.

### Task 5: Independent experiments/audits
- [x] TempoDelay: opt-in compile-time bypass (no two delay allocations), retain normal default; compare ELF/runtime allocation, document loss of echoes and hardware listening OPEN if not performed.
- [x] TinyUSB: separate reproducible local candidate archive/core experiment; never mutate installed SDK; document actual class-buffer provenance and required rebuild.
- [x] Voice/Formant: document active UI/Song/MIDI/render paths and exact measured object sizes; keep required functions.

### Task 6: Verification and delivery
- [x] Host suite and focused HGS/SmartPlay/8B persistence tests, SDL build and focused visible checks, Cardputer ADV + MIDI build, unchanged DRAM gate.
- [x] Flash stable intermediate after successful build; capture heap/largest and diagnostic integrity/audio. Melody A+B/generation/Save-reset-load/synth/drums/USB MIDI require actual observations; mark OPEN otherwise.
- [x] Review diff and linked symbols, memory table (static exact; dynamic measured or OPEN), compatibility/risk notes. Push separate branch and Draft PR; no merge.

## Execution ledger

- Native implementation authorized by the explicit MEMORY-PRUNE-A brief; independent read-only TinyUSB/Voice audits and whole-branch review delegated under dispatching-parallel-agents/executing-plans.
- Ruling: group production capability guards into one coherent commit after independent intermediate A/B images, rather than expose half-configured UI/engine APIs between commits.
- Ruling: keep FormantSynth and TempoDelay. TinyUSB full SDK rebuild remains OPEN; archive-removal experiment is deliberately non-flashable and isolated.
- Ruling: fix existing sampler-reference transaction/lifecycle bug because disabled Sampler otherwise relies on that metadata for all stable-reference projects. RED→GREEN legacy/stable/failed-load/New proofs added.
- Verification complete: full host exit 0, normal and pruned SDL, pruned compatibility ASan/UBSan, HGS/Smart Play, Cardputer ADV and MIDI builds, user hardware acceptance and physical USB MIDI. Pending physical MIDI-only/external devices and full TinyUSB SDK rebuild remain explicitly OPEN.
- All static and serial stage observations are in docs/testing/memory-prune-a/measurements.json; remaining gates explicitly OPEN in README.
