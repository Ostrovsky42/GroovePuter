# 0.9.10 P1 Runtime Projection Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Insert a fixed immutable PATTERN runtime projection between mutable Scene PATTERN storage and Synth A/B playback without changing accepted PATTERN behavior.

**Architecture:** Scene `SynthPattern` remains the only persistent PATTERN authority. Each Synth owns a fixed page-resident bank of 16 immutable projections (2 banks × 8 slots), while AudioTask only selects already-prepared entries through existing Pattern/Song metadata. The existing quantized-generation pending slot also carries the old-audible projected material, so exact `BAR_START` activation only releases/selects prepared state.

**Tech Stack:** C++17, ESP32-S3/Cardputer ADV, SDL host harness, existing `AudioMutationGate`/`AudioGuard`, existing quantized-generation slot publication, Python source-contract tests, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-03-pattern-phrase-p1-runtime-projection-design.md`

## Global Constraints

- P0 characterization is the mandatory regression oracle.
- PATTERN remains 16 physical steps, 24 ticks per step, 384 ticks per bar.
- `gridSteps` remains a Synth scheduler no-op in P1.
- Scene `SynthPattern` remains the only persistent PATTERN authority.
- AudioTask never copies `SynthPattern`/`SynthStep` material and never builds a projection.
- AudioTask may select prepared projection data and release the existing generation overlay only.
- `NO PATTERN` means canonical prepared silence, never stale material.
- Projection storage is compile-time fixed; no heap container is allowed.
- No second queue, scheduler, audio task, page cache, mutation owner or Undo owner.
- PLAY generation remains outside long `AudioGuard`; only bounded projection settlement may use the existing guard.
- Cardputer ADV fixed-DRAM acceptance records the exact P1 memory delta.
- PHRASE persistence, MAKE PHRASE, GRID behavior, cross-bar lifetime and UI source switching remain outside P1.

---

# Human-readable result

## What the firmware looks like immediately after P1

To the musician, it should look **unchanged**.

The same project should play the same notes, timing, probability, ghost notes,
retriggers, Song boundaries and legacy TIE behavior. P1 deliberately adds no
menu, no new editor and no new musical control.

The internal change is this:

```text
BEFORE
Scene SynthPattern
      |
      +--> AudioTask reads mutable Pattern event bytes

AFTER P1
Scene SynthPattern
      |
      +--> control-side fixed immutable projection
                    |
                    +--> AudioTask reads projection event bytes only
```

This removes mutable Scene PATTERN material from the real-time event-material
path without changing the scheduler.

## What we are ultimately building toward

P1 is the foundation for the later musician-facing PATTERN → PHRASE line:

```text
                    SYNTH A / SYNTH B
                           |
                    exactly one source
                    /              \
               PATTERN            PHRASE
               1 bar              1/2/4/8 bars
               16 cells           explicit notes
                    \              /
                     one runtime playback path
                              |
                    internal synth + MIDI
```

The intended final firmware behavior is:

1. **PATTERN stays the fast groove tool.** One bar, 16 physical steps, immediate editing, old projects still valid.
2. **GRID 8/16/32 becomes resolution.** It controls snap/zoom/editor granularity; changing GRID does not move existing notes and is not phrase length.
3. **LENGTH 1/2/4/8 becomes duration.** Multi-bar structure belongs to PHRASE.
4. **MAKE PHRASE is one-way.** Current PATTERN becomes initial PHRASE material; there is no live back-sync.
5. **PATTERN xor PHRASE.** A Synth voice has exactly one authoritative musical source, never a hidden merge.
6. **Long notes become explicit.** PHRASE notes own `start + duration`, so sustaining across a bar is normal rather than encoded as PATTERN TIE behavior.
7. **One scheduler/output path remains.** PATTERN and PHRASE project into runtime material consumed by the same internal-synth and MIDI path.
8. **Undo remains singular.** PHRASE becomes another payload kind in the accepted global owner, not another stack.
9. **UI exposes musician decisions.** PATTERN/PHRASE, GRID, LENGTH and MAKE PHRASE are visible; projection slots and mutation mechanics are not.

That product destination is the reason P1 exists.

---

# P1 file map

**Create**

- `src/dsp/pattern_runtime_projection.h` — fixed runtime values and pure projection functions.
- `tests/test_pattern_phrase_p1_projection.cpp` — pure value/mapping tests.
- `tests/test_pattern_phrase_p1_runtime.cpp` — engine-level runtime characterization.
- `tests/test_pattern_phrase_p1_source_contract.py` — architectural prohibitions.
- `tests/run_pattern_phrase_p1_projection.sh` — focused deterministic host runner.
- `.github/workflows/0_9_10_pattern_phrase_p1_projection.yml` — exact-head P1 CI.

**Modify**

- `src/dsp/miniacid_engine.h`
- `src/dsp/miniacid_engine.cpp`
- `src/generation/migration/quantized_generation_undo_owner_impl.h`
- `src/ui/pages/pattern_edit_page.h`
- `src/ui/pages/pattern_edit_page.cpp`
- `src/ui/pages/genre_page.cpp`
- `src/ui/miniacid_display.cpp`

Do not redesign Pattern paging or drums in P1.

---

### Task 0: Freeze and inherit P0

**Files:** existing P0 runner/tests from PR #426.

**Produces:** an implementation base containing the accepted P0 characterization unchanged.

- [ ] Verify the final accepted PR #426 HEAD and terminal focused CI.
- [ ] Merge/freeze test-only P0 before P1 production commits, then update P1 from the resulting `main` without recreating the branch.
- [ ] Run:

```bash
tests/run_pattern_phrase_p0_characterization.sh
```

Expected final line:

```text
PATTERN/PHRASE P0 characterization: PASS
```

No P1 production code belongs in this prerequisite commit.

---

### Task 1: Define the pure projection value

**Files:**
- Create: `src/dsp/pattern_runtime_projection.h`
- Create: `tests/test_pattern_phrase_p1_projection.cpp`

**Produces:**

```cpp
#pragma once

#include <cstdint>
#include <type_traits>

#include "../../scenes.h"

enum class PatternRuntimeSourceKind : uint8_t {
  Empty = 0,
  Pattern,
};

struct PatternRuntimeStep {
  int8_t note = -1;
  bool slide = false;
  bool accent = false;
  bool ghost = false;
  uint8_t velocity = 100;
  int8_t timing = 0;
  uint8_t fx = 0;
  uint8_t fxParam = 0;
  uint8_t probability = 100;
};

struct PatternRuntimeSource {
  PatternRuntimeSourceKind kind = PatternRuntimeSourceKind::Empty;
  int8_t voice = -1;
  int8_t page = -1;
  int8_t bank = -1;
  int8_t slot = -1;
};

struct PatternRuntimeProjection {
  static constexpr int kSteps = SynthPattern::kSteps;
  PatternRuntimeSource source{};
  PatternRuntimeStep steps[kSteps]{};
};

static constexpr int kPatternRuntimeEntriesPerVoice =
    kBankCount * Bank<SynthPattern>::kPatterns;

inline int patternRuntimeIndex(int bank, int slot) {
  if (bank < 0 || bank >= kBankCount) return -1;
  if (slot < 0 || slot >= Bank<SynthPattern>::kPatterns) return -1;
  return bank * Bank<SynthPattern>::kPatterns + slot;
}

inline PatternRuntimeProjection makeEmptyPatternRuntimeProjection(int voice) {
  PatternRuntimeProjection projection{};
  projection.source.kind = PatternRuntimeSourceKind::Empty;
  projection.source.voice = static_cast<int8_t>(voice);
  for (int i = 0; i < PatternRuntimeProjection::kSteps; ++i) {
    projection.steps[i].note = -1;
  }
  return projection;
}

inline PatternRuntimeProjection projectSynthPattern(
    const SynthPattern& pattern,
    int voice,
    int page,
    int bank,
    int slot) {
  PatternRuntimeProjection projection{};
  projection.source.kind = PatternRuntimeSourceKind::Pattern;
  projection.source.voice = static_cast<int8_t>(voice);
  projection.source.page = static_cast<int8_t>(page);
  projection.source.bank = static_cast<int8_t>(bank);
  projection.source.slot = static_cast<int8_t>(slot);
  for (int i = 0; i < PatternRuntimeProjection::kSteps; ++i) {
    const SynthStep& src = pattern.steps[i];
    PatternRuntimeStep& dst = projection.steps[i];
    dst.note = src.note;
    dst.slide = src.slide;
    dst.accent = src.accent;
    dst.ghost = src.ghost;
    dst.velocity = src.velocity;
    dst.timing = src.timing;
    dst.fx = src.fx;
    dst.fxParam = src.fxParam;
    dst.probability = src.probability;
  }
  return projection;
}

static_assert(std::is_trivially_copyable<PatternRuntimeStep>::value,
              "runtime Pattern step must stay trivially copyable");
static_assert(std::is_trivially_copyable<PatternRuntimeProjection>::value,
              "runtime Pattern projection must stay trivially copyable");
```

- [ ] Write RED tests proving all nine current playback fields copy exactly and remain independent after later source mutation.
- [ ] Test mapping `0/0→0`, `0/7→7`, `1/0→8`, `1/7→15`; invalid input returns `-1`.
- [ ] Test canonical empty projection contains no playable note.
- [ ] Implement the header exactly as a fixed value abstraction.
- [ ] Run the isolated test GREEN.
- [ ] Commit:

```bash
git add src/dsp/pattern_runtime_projection.h tests/test_pattern_phrase_p1_projection.cpp
git commit -m "feat: add fixed Pattern runtime projection"
```

---

### Task 2: Add the resident projection bank to MiniAcid

**Files:**
- Modify: `src/dsp/miniacid_engine.h`
- Modify: `src/dsp/miniacid_engine.cpp`
- Create: `tests/test_pattern_phrase_p1_runtime.cpp`

**Produces:**

```cpp
void rebuildPatternRuntimeProjectionBank(int pageIndex);
void refreshPatternRuntimeProjection(int voiceIndex,
                                     int bankIndex,
                                     int patternIndex,
                                     int pageIndex);
const PatternRuntimeProjection& activePatternRuntimeProjection(
    int voiceIndex) const;
const PatternRuntimeProjection& residentPatternRuntimeProjection(
    int voiceIndex,
    int bankIndex,
    int patternIndex) const;
```

Fixed engine state:

```cpp
PatternRuntimeProjection patternRuntimeBank_[NUM_303_VOICES]
                                            [kPatternRuntimeEntriesPerVoice]{};
PatternRuntimeProjection emptyPatternRuntime_[NUM_303_VOICES]{};
int8_t patternRuntimePage_ = -1;
```

- [ ] RED: populate all 16 physical addresses for both Synths with distinct notes; rebuild; assert all 32 projections carry correct source identity and material.
- [ ] Implement full-bank rebuild for control-side/init/page settlement only.
- [ ] Implement validated single-entry refresh; invalid identity leaves last valid entry untouched.
- [ ] Initialize one canonical empty projection per voice.
- [ ] GREEN: prove no heap allocation and no cross-voice contamination.
- [ ] Commit:

```bash
git add src/dsp/miniacid_engine.h src/dsp/miniacid_engine.cpp tests/test_pattern_phrase_p1_runtime.cpp
git commit -m "feat: own resident Pattern runtime bank"
```

---

### Task 3: Settle manual PATTERN edits into the runtime bank

**Files:**
- Modify: `src/ui/pages/pattern_edit_page.h`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Create: `tests/test_pattern_phrase_p1_source_contract.py`

The existing `commitPatternMutation()` already has one bounded persistent COMMIT inside `audio_guard_`. Extend that same apply lambda:

```cpp
const auto apply = [&]() {
  GroovePuterUndo::restoreSynthPatternUndo(manager, prepared);
  mini_acid_.refreshPatternRuntimeProjection(
      voice_index_,
      manager.getCurrentBankIndex(voice_index_ + 1),
      manager.getCurrentSynthPatternIndex(voice_index_),
      mini_acid_.currentPageIndex());
};
```

- [ ] RED: edit one step and prove Scene changes while old projection remains until COMMIT.
- [ ] Add the refresh to the existing bounded apply lambda; add no revision/Undo publication for derived state.
- [ ] Prove paste, rotate, clear, note entry, accent, slide, FX and octave converge through the same owner rather than adding per-command projection code.
- [ ] GREEN: edited entry updates; other 15 addresses and other voice remain unchanged.
- [ ] Commit:

```bash
git add src/ui/pages/pattern_edit_page.h tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "feat: settle Pattern edits into runtime projection"
```

---

### Task 4: Resolve audible source from metadata only

**Files:**
- Modify: `src/dsp/miniacid_engine.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`

`activePatternRuntimeProjection()` preserves current resolution order:

```text
valid existing pending-generation projection
    else
existing Song/Pattern source metadata
    -> no PATTERN: canonical empty
    -> PATTERN: fixed resident bank entry
```

- [ ] RED: PATTERN-mode bank/slot selection changes audible projection without rebuild.
- [ ] RED: Song row with `-1` after an audible row returns canonical empty, never stale material.
- [ ] Implement metadata-only bank lookup; do not call `getSynthPattern()` for event material.
- [ ] GREEN: selection changes are constant-time and copy-free.
- [ ] Commit:

```bash
git add src/dsp/miniacid_engine.cpp tests/test_pattern_phrase_p1_runtime.cpp
git commit -m "feat: select prepared Pattern runtime sources"
```

---

### Task 5: Make projection authoritative for Synth playback

**Files:**
- Modify: `src/dsp/miniacid_engine.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

Replace event-material reads in exactly these paths:

```text
processSequencerEvents()  timing
triggerSynthStep_()       note/flags/velocity/fx/probability
generateAudioBuffer()     Synth A/B retrigger material
refreshSynthCaches()      audible material cache
```

- [ ] RED source-contract: reject `activeSynthPattern(` and direct Scene Synth PATTERN material reads in those audio sections.
- [ ] RED runtime cases: normal note, `-2` TIE, ghost, probability, velocity, accent, slide, timing and RETRIG/fxParam.
- [ ] Replace only event-material source; preserve scheduler equations, RNG calls, gate countdown, LED order and MIDI publication order.
- [ ] Run P1 focused tests plus unchanged P0 characterization GREEN.
- [ ] Commit:

```bash
git add src/dsp/miniacid_engine.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "feat: route Synth playback through runtime projection"
```

---

### Task 6: Prove Song BAR_START is selection-only

**Files:**
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

Expected path:

```text
BAR_START
  -> existing Song owner advances row
  -> existing physical source metadata changes
  -> activePatternRuntimeProjection() selects prepared entry/empty
  -> step 0 plays from that projection
```

- [ ] Test two Song rows with different step-0 notes at the exact existing boundary.
- [ ] Test `PATTERN→NO PATTERN` and `NO PATTERN→PATTERN` boundaries.
- [ ] Source-contract reject `projectSynthPattern`, `refreshPatternRuntimeProjection` and `rebuildPatternRuntimeProjectionBank` from audio boundary code.
- [ ] Source-contract reject `SynthPattern`/`SynthStep` material copy in the same path.
- [ ] Run GREEN. No production change is expected; if RED requires a new Song cursor or prefetch owner, stop and revise the design instead of implementing it.
- [ ] Commit:

```bash
git add tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "test: prove projection-only Song boundary selection"
```

---

### Task 7: Project the existing quantized-generation audible overlay

**Files:**
- Modify: `src/generation/migration/quantized_generation_undo_owner_impl.h`
- Modify: `src/dsp/miniacid_engine.cpp`
- Modify: `src/ui/pages/genre_page.cpp`
- Modify: `src/ui/pages/pattern_edit_page.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

**Produces:**

```cpp
const PatternRuntimeProjection* pendingAudibleSynthProjection(
    const MiniAcid& engine,
    int voice);
```

Extend the existing `PendingGeneration` activation value with prepared old-audible Synth projections. Fill them before the existing `armActivationSlot()` release publication. Do not create another pending owner.

Required ordering:

```text
prepare new candidate
capture old audible PATTERN
project old audible PATTERN into existing activation slot
arm existing activation slot
commit new persistent PATTERN
short guarded refresh of resident committed projection
continue PLAY through old pending projection
BAR_START releases existing activation
fall through to resident new projection
```

- [ ] RED: full generation remains old-audible after persistent COMMIT and switches to new resident projection exactly at BAR_START.
- [ ] RED: Synth-A-only and Synth-B-only generation preserve untouched voice isolation.
- [ ] RED: target-change, revision-change, explicit cancel and Undo-before-boundary cannot leave stale overlay selected.
- [ ] Fill pending projection from already captured old `SynthPattern`; AudioTask never recaptures Scene material.
- [ ] In `GenrePage`, keep `regenerateWithQuantizedCommit()` outside long `withAudioGuard`; after successful return, use one short existing guard to refresh both resident Synth entries.
- [ ] In `PatternEditPage`, after successful `regenerateSynthWithQuantizedCommit()`, use one short existing guard to refresh only the generated voice.
- [ ] After accepted generation Undo/Redo mutates persistent Synth PATTERN bytes, refresh matching resident entry/entries inside the caller's bounded existing guard.
- [ ] Switch audio pending lookup from `pendingAudibleSynthPattern()` to `pendingAudibleSynthProjection()`.
- [ ] Run P1, P0 and existing PLAY-generation guard regressions GREEN.
- [ ] Commit:

```bash
git add src/generation/migration/quantized_generation_undo_owner_impl.h src/dsp/miniacid_engine.cpp src/ui/pages/genre_page.cpp src/ui/pages/pattern_edit_page.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "feat: project pending generation audible material"
```

---

### Task 8: Settle page identity and projection bank together

**Files:**
- Modify: `src/ui/miniacid_display.cpp`
- Extend: `tests/test_pattern_phrase_p1_runtime.cpp`
- Extend: `tests/test_pattern_phrase_p1_source_contract.py`

`handlePaging_()` already owns page load inside `withAudioGuard`. Successful settlement order becomes:

```text
load/create target Scene resident page
-> rebuildPatternRuntimeProjectionBank(target)
-> setCurrentPage(target)
-> finish existing success result
```

- [ ] RED: same bank/slot contains different notes on page A/B; never observe page-B identity with page-A projection.
- [ ] RED: failed load preserves old page identity and old projection bank.
- [ ] Add rebuild before page identity publication inside the same existing guard.
- [ ] Do not add a second page cache or prefetch scheduler.
- [ ] Run GREEN.
- [ ] Commit:

```bash
git add src/ui/miniacid_display.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/test_pattern_phrase_p1_source_contract.py
git commit -m "feat: settle Pattern page projection atomically"
```

---

### Task 9: Add one deterministic P1 runner

**Files:**
- Complete: `tests/test_pattern_phrase_p1_source_contract.py`
- Complete: `tests/test_pattern_phrase_p1_projection.cpp`
- Complete: `tests/test_pattern_phrase_p1_runtime.cpp`
- Create: `tests/run_pattern_phrase_p1_projection.sh`

The runner should be concrete and self-contained:

```bash
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDL_DIR="${ROOT_DIR}/platform_sdl"
BUILD_DIR="${ROOT_DIR}/build/pattern-phrase-p1"
mkdir -p "${BUILD_DIR}"

cd "${ROOT_DIR}"
tests/run_pattern_phrase_p0_characterization.sh
python3 tests/test_pattern_phrase_p1_source_contract.py

PURE_BIN="${BUILD_DIR}/pattern_phrase_p1_projection"
g++ -std=c++17 -O1 -I"${ROOT_DIR}" -I"${SDL_DIR}" \
  -include bits/stdc++.h -include "${SDL_DIR}/arduino_compat.h" \
  tests/test_pattern_phrase_p1_projection.cpp -o "${PURE_BIN}"
"${PURE_BIN}"

DEFAULT_SOURCES="$(make -C "${SDL_DIR}" -pn 2>/dev/null | sed -n 's/^SOURCES := //p' | head -n 1)"
if [[ -z "${DEFAULT_SOURCES}" ]]; then
  echo "P1: failed to read platform_sdl SOURCES" >&2
  exit 1
fi
RUNTIME_SOURCES=" ${DEFAULT_SOURCES} "
RUNTIME_SOURCES="${RUNTIME_SOURCES// sdl_main.cpp / }"
RUNTIME_SOURCES="${RUNTIME_SOURCES} ../tests/test_pattern_phrase_p1_runtime.cpp ../src/midi/usb_midi_output.cpp"

SDL_CFLAGS="$(sdl2-config --cflags 2>/dev/null || true)"
SDL_LIBS="$(sdl2-config --libs 2>/dev/null || true)"
SDL_GFX_CFLAGS="$(pkg-config --cflags SDL2_gfx 2>/dev/null || true)"
SDL_GFX_LIBS="$(pkg-config --libs SDL2_gfx 2>/dev/null || true)"
if [[ -z "${SDL_CFLAGS}" ]]; then
  SDL_CFLAGS="-I/usr/include/SDL2 -D_THREAD_SAFE"
  SDL_LIBS="-lSDL2"
fi
if [[ -z "${SDL_GFX_CFLAGS}" ]]; then
  SDL_GFX_LIBS="-lSDL2_gfx"
fi

BASE_FLAGS="-std=c++17 -I.. -I. -include bits/stdc++.h -include arduino_compat.h -DUSE_RETRO_THEME -DUSE_AMBER_THEME"

build_and_run() {
  local cxx="$1"
  local output="$2"
  local extra_flags="$3"
  local log="$4"
  (
    cd "${SDL_DIR}"
    ${cxx} ${BASE_FLAGS} ${extra_flags} ${SDL_CFLAGS} ${SDL_GFX_CFLAGS} \
      ${RUNTIME_SOURCES} ${SDL_LIBS} ${SDL_GFX_LIBS} -o "${output}"
    "${output}"
  ) > "${log}"
}

GCC_BIN="${BUILD_DIR}/pattern_phrase_p1_gcc"
GCC_LOG1="${BUILD_DIR}/gcc-run1.txt"
GCC_LOG2="${BUILD_DIR}/gcc-run2.txt"
build_and_run g++ "${GCC_BIN}" "-O1" "${GCC_LOG1}"
"${GCC_BIN}" > "${GCC_LOG2}"
diff -u "${GCC_LOG1}" "${GCC_LOG2}"
cat "${GCC_LOG1}"

if command -v clang++ >/dev/null 2>&1; then
  build_and_run clang++ "${BUILD_DIR}/pattern_phrase_p1_clang" "-O1" "${BUILD_DIR}/clang.txt"
  diff -u "${GCC_LOG1}" "${BUILD_DIR}/clang.txt"
fi

build_and_run g++ "${BUILD_DIR}/pattern_phrase_p1_asan" \
  "-O1 -g -fsanitize=address -fno-omit-frame-pointer" \
  "${BUILD_DIR}/asan.txt"
diff -u "${GCC_LOG1}" "${BUILD_DIR}/asan.txt"

build_and_run g++ "${BUILD_DIR}/pattern_phrase_p1_ubsan" \
  "-O1 -g -fsanitize=undefined -fno-omit-frame-pointer" \
  "${BUILD_DIR}/ubsan.txt"
diff -u "${GCC_LOG1}" "${BUILD_DIR}/ubsan.txt"

echo "PATTERN/PHRASE P1 runtime projection: PASS"
```

- [ ] RED source-contract before production rewiring.
- [ ] Make GCC deterministic repeat GREEN.
- [ ] Make Clang parity GREEN when available.
- [ ] Make ASan and UBSan outputs identical to accepted GCC fixture.
- [ ] Commit:

```bash
git add tests/test_pattern_phrase_p1_source_contract.py tests/test_pattern_phrase_p1_projection.cpp tests/test_pattern_phrase_p1_runtime.cpp tests/run_pattern_phrase_p1_projection.sh
git commit -m "test: add P1 runtime projection acceptance"
```

---

### Task 10: CI, DRAM evidence and cleanup

**Files:**
- Create: `.github/workflows/0_9_10_pattern_phrase_p1_projection.yml`
- Update: `docs/superpowers/specs/2026-09-03-pattern-phrase-p1-runtime-projection-design.md` only with factual implementation evidence.
- Update: this plan only when TDD proves a documented interface needs correction.

- [ ] Add exact-head workflow running `tests/run_pattern_phrase_p1_projection.sh`.
- [ ] Run final matrix:

```text
P0 focused characterization     PASS
P1 focused projection runner    PASS
host Core tests                 PASS
SDL build                       PASS
Cardputer ADV build             PASS
Cardputer ADV fixed-DRAM gate   PASS
SEQTRAK MIDI-only build         PASS
```

- [ ] Record:

```text
sizeof(PatternRuntimeStep)
sizeof(PatternRuntimeProjection)
resident bytes per voice
resident bytes total
pending-generation projection delta
final fixed internal-DRAM delta
```

- [ ] Final source review confirms no audio-side mutable PATTERN event-material reads, no runtime heap projection, no new owner, no duplicated Song/page mapper.
- [ ] Remove temporary logs, dead helpers and duplicated tests.
- [ ] Keep PR draft until exact-head focused CI is terminal GREEN.
- [ ] Commit:

```bash
git add .github/workflows/0_9_10_pattern_phrase_p1_projection.yml docs/superpowers/specs/2026-09-03-pattern-phrase-p1-runtime-projection-design.md docs/superpowers/plans/2026-09-03-pattern-phrase-p1-runtime-projection.md
git commit -m "ci: gate P1 immutable runtime projection"
```

---

# Review checkpoints

## Review A — projection exists, playback still legacy

```text
fixed projection types        YES
resident bank                 YES
manual refresh                YES
playback still legacy         YES
behavior delta                NONE
```

## Review B — projection owns normal PATTERN event material

```text
normal timing/trigger/retrig   projection
Song BAR_START                 prepared selection only
NO PATTERN                     canonical empty
P0                             GREEN
```

## Review C — generation/page settlement complete

```text
pending generation old side   prepared projection
BAR_START activation           release/select only
page settlement                projection bank + page identity together
fixed DRAM                     accepted
full matrix                    GREEN
```

Only Review C completes P1.

---

# Explicitly not part of P1

```text
PhraseEvent runtime execution
MAKE PHRASE
PATTERN/PHRASE toggle
GRID snap/zoom
LENGTH 1/2/4/8 editor
cross-bar gate lifetime changes
new MIDI lifetime owner
Phrase persistence migration
Phrase Undo payload
new UI source badge/tab
```

These become later checkpoints after P1 proves the runtime seam.

---

# Definition of done

A musician should not need to relearn anything after P1 and should not hear a designed musical difference.

An engineer should be able to state:

```text
Scene owns PATTERN persistence.
Control-side code projects PATTERN material.
AudioTask consumes immutable projected event material.
Song selects already-prepared material at exact boundaries.
Generation releases/selects already-prepared material at exact boundaries.
NO PATTERN is explicit silence.
There is still one scheduler, one generation pending owner and one Undo owner.
```

That is the required foundation before PHRASE becomes a real alternative musical source.
