# Smart Play Restoration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to implement this plan task-by-task. Executor is Luna. Steps use checkbox (`- [ ]`) syntax. Do not delegate or implement the whole document in one patch.

**Goal:** Restore controller-independent Smart Play on the v0.9.19 recovery branch, with truthful harmonic context, safe MIDI ownership and a future HGS consumer boundary.

**Architecture:** One `PerformanceKeyboard` consumes normalized intents and a bounded read-only harmony snapshot. Legacy material and HGS produce that snapshot through separate adapters. Source lifecycle and existing transport/output owners retain authority.

**Tech Stack:** C++17, existing host assert tests, GCC/Clang sanitizers, SDL, Arduino ESP32-S3/Cardputer ADV without PSRAM.

**Spec:** [Implementation contract](../specs/2026-10-13-smart-play-hgs-contract.md); [HGS v2 design](../specs/2026-10-13-unified-harmonic-system-v2-design.md).

## Global Constraints

- All restrictions in the implementation contract apply to every task.
- Work in `/home/gg/.codex/worktrees/performance-smart-play`, starting code HEAD `6c7f971c`.
- Restore selected code by reviewed hunks; do not merge `experimental/performance-layer` wholesale.
- No performance FX, scenes, new internal live synth target, or renamed Fn+N/G/numeric shortcuts.
- No rebase, push, tag, release, flash or merge into main in this execution.
- All new audio/input reachable structures are fixed-capacity; release operations are source-scoped.
- Commit only paths owned by the completed task after its tests pass. Never stage all dirty files.
- New APIs below are proposed implementation interfaces; they are not claims about existing symbols.
- Red tests must fail on the behavior being fixed, not merely a broken test fixture or missing include.

## Review Focus

1. Same physical note released after role/harmony change releases its ORIGINAL output — SP-2/SP-3.
2. Same chord on the next event or Song loop still has distinct event identity — SP-1/SP-4/SP-5.
3. Repeated harmony updates under HOLD cannot postpone adoption forever — SP-4.
4. Genre idiom output may supersede generic harmonic metadata — SP-5 and HGS2-7c.
5. Shared MIDI channels, stale sessions and full queues must not kill unrelated owners — SP-2/SP-7.

## Read first / actual file ownership

| Existing file | What to inspect |
|---|---|
| `src/input/performance_keyboard.h/.cpp` | Held/scheduled owners, pulse boundaries, source pool construction |
| `src/input/performance_smart_play.h` | Pure interpreter; currently NOT wired into live dispatch |
| `src/input/performance_harmony_context.h` | Legacy P1R material/recipe validation only |
| `src/generation/roles/chord_progression.h` | Legacy quality semantics and stable numeric IDs |
| `src/input/performance_pulse.h` | NEXT_STEP clocked config and rate ratios |
| `src/midi/midi_input_{event,queue,dispatcher,settings}.h` | Source identity and old v1 configuration |
| `src/midi/external_note_queue.h` | Application handoff, overflow cleanup |
| `src/midi/project_transport_timeline.h` | Existing epoch/bar/phase authority |
| `src/ui/pages/perform_page.cpp/.h` | Five contexts, tools navigation and row limits |
| `src/ui/key_normalize.h`, `GroovePuter.ino` | Cardputer key normalization and dispatch order |
| `platform_sdl/sdl_main.cpp` | Equivalent desktop wiring |
| `src/generation/migration/strong_rhythm_migration.cpp` | `GenreIdiom::apply()` after tonal materialization |
| `src/dsp/miniacid_engine.h/.cpp` | Material publication/read access; no new DSP processing |

Experimental files are references, not replacement copies: `performance_intent.h`,
configuration methods in `performance_keyboard.*`, platform performance config store,
MIDI generation/cable/bend fixes. Their dependencies must pass the scope firewall.

---

### SP-0 — Freeze starting state and reproducible focused runner

**Files:** Create `tests/run_smart_play_tests.sh`, `docs/superpowers/reports/2026-10-13-smart-play-progress.md`.
Read `tests/run_host_tests.sh`, `tests/run_performance_closure_tests.sh`, `scripts/build.sh`.

**Interfaces:** Runner accepts zero or more exact target names: `model`, `quality`,
`legacy_context`, `arp`, `keyboard`, `external`; unknown names fail. No arguments runs all
registered targets. Later tasks append targets explicitly. Respect `CXX` and extra
sanitizer flags, keep output below `build/smart-play-tests/`, never treat missing files as skips.

- [x] Record full start/base/main/spec SHAs, `git status`, worktree list and initial source diff.
- [x] Compile existing tests with `-std=c++17 -Wall -Wextra -Werror -I.`. Copy exact source
  dependencies from `run_host_tests.sh`; `legacy_context` also needs chord progression
  and generation context implementation. First run existing tests without modifying them.
- [x] Add runner dispatch and run `bash tests/run_smart_play_tests.sh`. Expected all named
  binaries exit 0. Record baseline failures separately; do not weaken an assertion to get green.
- [x] Capture baseline ADV build once with `scripts/build.sh --warnings all`, flash/static
  RAM and toolchain/FQBN. If toolchain missing, report exact cause and continue pure host work.
- [ ] Commit runner and progress baseline: `test: establish smart play restoration baseline`.

### SP-1 — Shared quality and provider-neutral harmonic snapshot

**Files:** Create `src/generation/harmony/harmonic_types.h`, `harmonic_quality.h`, `harmonic_scale.h`,
`harmonic_snapshot.h`, `tests/test_harmonic_snapshot.cpp`,
`tools/harmony/build_bootstrap_tables.py`, bootstrap registry JSON and
`src/generation/harmony/generated/harmonic_vocabulary.h`.
Modify `src/generation/roles/chord_progression.h`, `src/generation/tonal/scale_catalog.h`,
`src/input/performance_smart_play.h`, `src/generation/rhythm/rhythm_types.h`,
`tests/test_performance_chord_quality.cpp`, focused runner.

**Consumes:** Existing ChordQuality/HarmonicEvent/ScaleType ABI and contract §3 bootstrap formulas.
**Produces:** The exact types in contract §3; `qualityDefinition(QualityId)` and
`scaleDefinition(ScaleId)` immutable lookup; explicit legacy quality/scale-to-registry adapters;
`bool validateHarmonySnapshot(const HarmonySnapshot&)`;
`bool sameHarmonyEvent(const HarmonyEventKey&, const HarmonyEventKey&)`.
`Ready` validation checks nonzero material/publication identity, positive duration, state root/bass and tonal/home tonic validity, matching formula/mask,
valid quality/scale IDs, masks below bit12 and preferred⊆allowed. A valid empty role mask remains valid.
Status-only invalid/no-harmony snapshots cannot emit notes. Initialize every field explicitly.

- [x] Write tests `all_quality_intervals`, `legacy_quality_parity`, `snapshot_validation`,
  `same_pitch_new_event`, `event_ordinal_exceeds_255`, `mask_empty_is_not_chromatic`.
  Check C major `{0,4,7}` versus C minor `{0,3,7}` and exact unchanged legacy enum numbers.
- [x] Run `bash tests/run_smart_play_tests.sh quality snapshot`; observe the intended red tests.
- [x] Implement bootstrap quality/scale registries and legacy adapters; record first10 quality
  IDs from contract and explicit mappings for all current scale IDs0..9. Add
  `tools/harmony/quality_registry.json` and `scale_registry.json` bootstrap sources, with
  `build_bootstrap_tables.py --out src/generation/harmony/generated/harmonic_vocabulary.h`.
  The later corpus compiler regenerates this same vocabulary owner; it does not create a
  second competing quality table. Test regeneration byte parity and unknown IDs.
  One formula owner supplies shared masks; legacy adapter reconstructs original ordered/compound
  intervals so ninth remains14 and bass THIRD/FIFTH/SEVENTH never become a sorted-mask index.
  Preserve legacy seed traces/intervals and ScaleType ABI; add sizeof/copyability assertions.
- [x] Reserve append-only domains here for SP-4's deterministic arp and later HGS2 use:
  HarmonicGenome=19, HarmonicCandidateTieBreak=20, HarmonicRoute=21,
  HarmonicModulation=22, HarmonicVerticalColor=23, PerformanceArp=24 (Count=25).
  First assert baseline old domains0..18 unchanged. No existing generator starts using
  these new domains in this task. Keep the legacy context adapter intact
  until SP-5; do not pretend the neutral snapshot is already published by the application.
- [x] Run `CXX=g++ bash tests/run_smart_play_tests.sh quality snapshot model legacy_context`,
  and the full focused suite with GCC. The `clang++` run is unavailable because that compiler
  is not installed in this environment; this is recorded in the progress report/ledger.
- [ ] Commit: `feat: define bounded harmonic snapshot and shared chord quality`.

### SP-2 — Neutral intents and source-owned note lifecycle

**Files:** Restore scoped portions of `src/input/performance_intent.h`; modify
`src/input/performance_keyboard.h/.cpp`, `src/midi/midi_input_event.h`,
`midi_input_queue.h`, `midi_input_dispatcher.h`, `external_note_queue.h`,
`src/platform/cardputer_usb_midi_transport.cpp`, and existing MIDI output retry owners
only where their failing tests require it. Create `tests/test_smart_play_input_ownership.cpp`.
Update existing MIDI queue/parser/dispatcher/transport tests and runner.

**Consumes:** Existing MIDI ingress, musical router and scoped cleanup.
**Produces:** `PerformanceInputIdentity` containing source/session/cable/channel/original
key; `PerformanceKeyboard::dispatchIntent(const PerformanceIntent&) -> bool`;
identity overloads of `externalNoteOn/Off`; scoped release entrypoints.
Use experimental identity definitions after checking enum compatibility; do not import
ChangeControl/Scene handling. Unknown intent is rejected without mutation.

- [x] Record the exact v0.9.19 source→dispatcher→queue→keyboard→output route, then identify
  which source/session fields are lost. Add tests at each such boundary, not just keyboard mocks.
- [x] Add assertions: USB 60 + Cardputer same pitch; one source disconnects and other stays;
  old generation NoteOff cannot release new NoteOn; cable 1 cannot release cable 0;
  sustain per source/channel; NoteOn velocity 0 acts as release; repeated NoteOn has bounded ownership.
- [x] Add flood tests: continuous Mod/PitchBend/Sustain-on does not increment recovery epoch; dropped release
  produces scoped recovery; pending neutral bend reaches output after capacity returns;
  stale generation pending bend cannot affect the reconnected device.
- [x] Distinguish CC123 release and CC120 sound cleanup while respecting shared channel
  ownership. Test actual emitted messages: no shared-channel wire all-sound-off for scoped cleanup.
- [x] Port only the behavior proved above. Output-note records retain original identity and
  emitted pitch/channel/target; NoteOff never recomputes the current interpretation.
  No all-owner panic on disconnect or harmless controller flood.
- [x] Run focused input/queue tests plus `bash tests/run_performance_closure_tests.sh` and
  `bash tests/run_midi_performance_route_binding_0_9_7_tests.sh`.
- [x] Commit coherent ingress and output fixes separately if necessary; each commit needs its
  own regression. Update progress with restored experimental hunks and rejected FX dependencies.

### SP-3 — Wire RAW/BASS/LEAD/CHORD into the one live performer

**Files:** Modify `src/input/performance_smart_play.h`, `performance_keyboard.h/.cpp`,
`performance_intent.h`; create `tests/test_smart_play_dispatch.cpp`; update model/external tests.

**Consumes:** SP-1 masks/quality and SP-2 identity. Existing chord performer and event router.
**Produces:** `setPerformanceRole(PerformanceRole)`, `setLeadMode(LeadMode)`, getters;
`setHarmonySnapshot(const GroovePuterHarmony::HarmonySnapshot&)`;
`clearPerformanceHarmonyContext()`; bounded voice-leading helper with nearest/up/down direction.
Use the existing `NotePlan`; extend status only append-only if persisted/exposed.

- [x] Add dispatch tests that capture actual router events, not just `interpretNote()` results:
  RAW pitches 0/11/12/64/95/96/127 exact; BASS root over C→Am→F→G; LEAD E→Fmaj7 keeps E;
  manual CHORD still calls the existing chord engine; forbidden pitch classes never appear.
- [x] Test nearest tie resolves down; up/down at range end returns no plan; zero allowed mask;
  SUS THIRD and triad SEVENTH refuse; m9 ninth is not THIRD; no-harmony scale fallback is bounded and visibly reported.
- [x] Pin state transitions from contract §6: selecting RAW stops performer transforms,
  enabling arp from RAW selects ARP; existing keys held across changes clean up once.
- [x] Wire both Cardputer note intents and external MIDI through the same interpreter/owner.
  Cardputer adapter first maps physical key to its existing root/scale/octave gesture;
  semantic core receives musical values and never a key matrix or USB endpoint.
- [x] Apply generated masks at the musical decision boundary, before scheduling. A role may
  choose a subset or nearest allowed note, never widen the owner's mask. CHORD uses existing
  bounded voicing then constrains its notes; if no valid voicing remains, return a visible refusal.
- [x] Run `bash tests/run_smart_play_tests.sh model dispatch ownership keyboard external`,
  including sanitizer build. Assert Pattern/Song data snapshots are byte/field-identical before
  and after role changes and live input; do not compare padded structs by memcmp.
- [x] Commit: `feat: route live smart roles through source-owned performer`.

### SP-4 — Complete Smart Arp musical and timing contracts

**Files:** Modify `performance_keyboard.h/.cpp`, `performance_pulse.h` only as needed;
expand `tests/test_performance_smart_arp.cpp`; create `tests/test_smart_arp_event_trace.cpp`.

**Consumes:** Snapshot, role mask and existing pulse service.
**Produces:** Existing `setArpPitchSource`, `setHarmonyChangePolicy` and pool method now
implement contract §6; generated output remains on the existing scheduled-event path.

- [x] Write trace fixtures with pitches/velocities, pulse ordinal, note-on/off and owner.
  Cover all four sources, all six rates, six contours including DOWNUP, 1..existing max octaves.
- [x] Assert FOLLOW phase survives pool 4→3; RESTART starts next pulse only; identical snapshot
  delivery does not restart; new same-chord event does. HOLD at every point of a cycle adopts
  latest of three updates exactly once at original cycle end. Context loss during HOLD clears it.
- [x] MEMORY test captures interval shape then releases all roots: empty output + HOLD ROOT;
  absent memory: empty + EMPTY MEMORY. No silent HELD fallback.
- [x] Assert pitch/velocity pairs stay paired when sorted; AS PLAYED held insertion order is
  preserved; HARMONY order is canonical. Empty pool, single note, clipped range and full pool
  never cause divide-by-zero, repeated NoteOff or infinite cycle.
- [x] Assert equal seed/transport/input traces produce identical RANDOM trace under different
  service polling frequencies; unrelated RNG calls do not affect it. Late pulses must follow
  existing bounded late-event policy rather than emit an unlimited catch-up burst.
- [x] Fix only failing pool/phase/identity code. Keep transport clock and STOP fallback intact.
  Old generated scheduled NoteOns become stale when source/epoch changes; scheduled NoteOff
  cleanup still executes for notes actually emitted.
- [x] Run `bash tests/run_smart_play_tests.sh arp trace keyboard external` and closure tests.
- [x] Commit: `fix: make smart arp sources and harmony transitions deterministic` (`9a8907c1`).

### SP-5 — Publish truthful legacy harmony to Cardputer and SDL

**Files:** Modify `src/input/performance_harmony_context.h`,
`src/state/generated_synth_a_origin.h`, `src/generation/migration/strong_rhythm_migration.cpp`
only for truthful provenance, `src/dsp/miniacid_engine.h/.cpp` read-side bridge,
`GroovePuter.ino`, `platform_sdl/sdl_main.cpp`.
Create `src/input/performance_harmony_bridge.h`, `tests/test_smart_play_harmony_bridge.cpp`;
expand `tests/test_performance_harmony_context.cpp`.

**Consumes:** Existing published transport, playing Song slot/row, Material identity, origin.
**Produces:** In `performance_harmony_bridge.h`:

```cpp
struct HarmonyPlaybackPosition {
  int16_t activePage, playbackSongSlot, songRow, activeSynthAPatternGlobal;
  GroovePuterMaterial::MaterialReference activeMaterial;
  GroovePuterMaterial::MaterialVersionToken activeMaterialVersion;
  uint32_t publicationGeneration, transportEpoch;
  uint64_t absoluteStep;
};
bool makeLegacyHarmonySnapshot(
    const GroovePuterMaterial::GeneratedSynthAOrigin& origin,
    const GroovePuterMaterial::GeneratedPhraseRecipe& recipe,
    const HarmonyPlaybackPosition& position,
    GroovePuterHarmony::HarmonySnapshot& out);
```

A shared `PerformanceHarmonyBridge::update(PerformanceKeyboard&, const HarmonySnapshot&)`
returns void and is used by both applications; duplicate keys are suppressed, invalid context
clears harmony-owned output. `activeMaterial` address/id must agree with position's active slot
and origin; exact material version must match. All position fields come from coherent read-side
publication. An origin records GENERIC/IDIOM/GRAPH authority, never a guess from current editor.
`absoluteStep%16` resolves in-bar position; store full epoch/ordinal event identity.

- [x] Extend existing adapter tests: wrong project/page/slot/material ID/version → Stale;
  song loop event key changes; snapshot for next queued material cannot appear early;
  repeated root/quality across bar boundary retains a full valid event duration.
- [x] Add fixture where `GenreIdiom::apply()` replaces generic pitch content: generic
  provenance must not produce Ready. Record all current generation paths in a coverage table.
- [x] Add an explicit harmonic-authority/provenance field to the existing origin candidate
  publication. Default Unknown; set only by the producer that actually determines final pitch
  content. No guessing harmony from Synth A notes or current GENRE editor selection.
- [x] Resolve snapshots on control-side playback updates before keyboard service/next pulse,
  using the existing coherent playback publication. Never read unsynchronized renderer fields.
  Feed both Cardputer and SDL through the same bridge.
- [x] Verify STOP makes HARMONY source unavailable when no stable committed context exists;
  HELD/MEMORY and manual roles remain playable. Replaced material/seek/project load cannot
  resurrect the previous snapshot. No graph calculations in this bridge.
- [x] Run `CXX=g++ bash tests/run_smart_play_tests.sh bridge legacy_context arp trace keyboard external` and SDL build.
- [x] Commit: `feat: publish validated material harmony to smart play`.

### SP-6 — Standalone UI, common configuration persistence and CC80/81

**Files:** Modify `src/ui/pages/perform_page.cpp/.h`, `src/ui/ui_view_continuity.h`
if row bounds require it, `performance_keyboard.h/.cpp`, `performance_intent.h`,
`GroovePuter.ino`, `platform_sdl/sdl_main.cpp`, `MANUAL.md`, `PLAN.md`.
Create `src/input/performance_keyboard_config.h`,
`src/platform/cardputer_performance_config_store.h/.cpp`,
`tests/test_smart_play_config.cpp`, `tests/test_smart_play_ui_contract.cpp`.
Update `tests/test_performance_instrument_ui_source_regressions.py`.

**Consumes:** SP-3 getters/setters; SP-4 prerequisite statuses.
**Produces:** `exportConfiguration()`, validated all-or-nothing `applyConfiguration(...)`,
`configChanged()`, `markConfigPersisted()`; semantic absolute `SetArpEnabled` and
`SetLatchEnabled` intents. One config owner for Cardputer, MIDI and load.

- [x] Add behavioral tests for default/missing/invalid/legacy-18-byte/current config records;
  roundtrip every musical field including rhythm/seed; invalid enum cannot partially apply.
  Saving twice without a change produces no write. Failed save leaves dirty state for retry.
- [x] UI tests cover each row's label, active condition and action: KEY/manual settings;
  CHORD voicing; ARP source/order/octaves/latch/follow; RHYTHM existing rates/gate/ratchet/
  Euclid; INPUT LIVE INPUT/ROLE/MODE/actual target/source/status.
  LEAD mode is visible only for LEAD; missing harmony/memory is visible. Use a bounded
  scrolling list with existing font/row height; never cram eight unreadable lines into 135 px.
- [x] Preserve existing Fn+N, Tab/tools, Shift+Tab, Alt+O, G and numeric routing with
  normalized UIEvent tests. Unmodified musical keys remain notes where v0.9.19 made them notes.
- [x] Extract config codec/store from the experimental Scene store without Scene arrays or
  dependencies. Preserve compatible old preference bytes; new version has explicit encoding
  and range checks. Persist on control loop after successful write only; never from renderer.
- [x] Add CC80/81 ONLY for PERFORM: 0..63 OFF, 64..127 ON, repeated ON idempotent,
  latch ON with arp OFF rejected visibly. A bounded latest-value coalescer preserves last
  state per selected source/channel; old session values discarded. CC74 etc remain unbound.
- [x] Run config/UI/ownership tests; run the existing UI source regression script and build SDL.
  Document: roles transform new live notes, material stays unchanged, output target determines
  where sound is heard. Document standalone steps for every source and role.
- [x] Commit: `feat: expose and persist standalone smart play controls`.

### SP-7 — Restoration software gate and hardware handoff

**Files:** Update progress, MANUAL/PLAN; create
`docs/superpowers/reports/2026-10-13-smart-play-hardware-checklist.md`.

**Consumes:** SP-0..SP-6 evidence. **Produces:** Software verdict and explicit unrun hardware rows.

- [x] Run focused GCC/Clang/sanitizer suite, then `bash scripts/ci/run_core.sh` to completion.
  Core invokes the host suite; do not redundantly rerun the whole host suite without a reason.
  An interrupted/timed-out suite is INCOMPLETE, not PASS. Record failing baseline separately.
- [x] Run `make -C platform_sdl clean all CXX=g++` and `scripts/build.sh --warnings all`.
  Record exact SHA + dirty diff hash, toolchain, FQBN, flash, static RAM and delta from SP-0.
- [x] Review `git diff a5f74f73 --stat` and all DSP/midi/UI entrypoint edits for scope leaks.
  Verify no Performance FX/Scene dispatch, no additional chord/arp clock, no new internal
  live routing. Preserve upstream changes unrelated to this feature.
- [x] Prepare physical tests with clear expected results: boot no controller; Fn+N; manual
  notes and four arp sources; HARMONY available/unavailable; musical rates; role changes
  with held notes; USB role boot; dual-source sustain; unplug/replug while arp; CC/bend flood;
  latency timestamps and audio-block diagnostics. Name receiver/output for each sound test.
- [x] Mark every physical row NOT_RUN until human evidence arrives. Record that idiom-origin
  harmony remains unavailable until HGS2-7c when applicable; this is a capability boundary.
- [x] Commit software report/checklist. Advance to the next HGS2 task in the handoff dependency order; this task alone does not mean
  the entire HGS/genre project is complete.

## Completion evidence format

```text
TASK=SP-n
START_SHA= full SHA
FINAL_SHA= full SHA (or SHA + dirty diff hash before commit)
FILES_CHANGED=
BEHAVIOR_PROVED=
COMMANDS_AND_EXIT_CODES=
SCOPE_FIREWALL=
MEMORY_DELTA=
KNOWN_LIMITATIONS=
HARDWARE_ACCEPTANCE=NOT_RUN/PASS/FAIL
NEXT_TASK=
```

For failed tests, record the first concrete failure and the smallest responsible task.
Do not import an entire experimental implementation to hide missing dependencies.
