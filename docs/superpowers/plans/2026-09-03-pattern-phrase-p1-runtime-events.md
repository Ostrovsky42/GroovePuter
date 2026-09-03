# PATTERN / PHRASE P1 Runtime Events Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a behavior-neutral fixed-capacity runtime synth-event representation plus a pure legacy Pattern projector.

**Architecture:** `SynthPattern` remains authoritative PATTERN storage. A new `PhraseRuntime` module projects Pattern steps into caller-owned immutable playback values with explicit start/duration. MiniAcid does not consume the new buffer in P1, so no audio, Song, MIDI, Scene or Undo behavior changes.

**Tech Stack:** C++17-compatible firmware code, host C++ tests, Python source guard, GCC, Clang, ASan, UBSan, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-09-03-pattern-phrase-p1-runtime-events-design.md`

## Global Constraints

- Exact stacked base is `c02d9ae04fcd43b5e3ced11e5aa50e850e26b4e6`.
- 96 PPQN, 384 ticks per bar, maximum future Phrase length 8 bars.
- No audible/runtime behavior change in P1.
- No change to `MiniAcid`, Song, Scene persistence, PhraseBank, Undo, AudioMutationGate, MIDI routing, GRID semantics or UI.
- No heap allocation and no retained pointers/physical Pattern addresses.
- `RuntimeSynthEvent` is 10 bytes; `RuntimeSynthEventBuffer` is 1284 bytes on the focused host ABI.

---

### Task 1: RED focused contract

**Files:**
- Create: `tests/test_pattern_phrase_p1_runtime_events.cpp`
- Create: `tests/test_pattern_phrase_p1_source_contract.py`
- Create: `tests/run_pattern_phrase_p1_runtime_events.sh`
- Create: `.github/workflows/0_9_10_pattern_phrase_p1_runtime_events.yml`
- Create: `docs/tests/PATTERN_PHRASE_P1_RUNTIME_EVENTS.md`

**Interfaces:**
- Consumes: frozen P0 Pattern behavior.
- Produces: executable requirements for `PhraseRuntime::RuntimeSynthEvent`, `RuntimeSynthEventBuffer`, `PatternProjectionSettings`, `PatternProjectionStatus`, and `projectPatternToRuntimeEvents(...)`.

- [ ] **Step 1: write the failing C++ contract**

The test includes the not-yet-existing `src/phrase/runtime_synth_events.h`, constructs canonical `SynthPattern` fixtures and asserts:

```cpp
static_assert(sizeof(PhraseRuntime::RuntimeSynthEvent) == 10);
static_assert(sizeof(PhraseRuntime::RuntimeSynthEventBuffer) == 1284);

PhraseRuntime::RuntimeSynthEventBuffer out{};
auto status = PhraseRuntime::projectPatternToRuntimeEvents(pattern, settings, out);
assert(status == PhraseRuntime::PatternProjectionStatus::Ready);
assert(out.lengthTicks == 384);
```

Cases must cover simple onset, A/B gate scaling, tick 11 and 361 wraps, cross-bar TIE, expired TIE, next-onset clipping and failure atomicity.

- [ ] **Step 2: write the source guard**

The guard must require P1 production changes to stay inside `src/phrase/runtime_synth_events.h/.cpp` and reject `gridSteps`, `AudioMutationGate`, `UndoOwner`, dynamic allocation (`new`, `malloc`, `std::vector`) and MiniAcid edits.

- [ ] **Step 3: run RED**

Run:

```sh
bash tests/run_pattern_phrase_p1_runtime_events.sh
```

Expected: compile failure because `src/phrase/runtime_synth_events.h` does not exist. The failure is the required TDD RED and must be recorded before Task 2.

- [ ] **Step 4: commit RED**

```sh
git add tests .github/workflows/0_9_10_pattern_phrase_p1_runtime_events.yml docs/tests/PATTERN_PHRASE_P1_RUNTIME_EVENTS.md
git commit -m "test(0.9.10): define P1 runtime event projection contract"
```

### Task 2: GREEN fixed-capacity runtime projection

**Files:**
- Create: `src/phrase/runtime_synth_events.h`
- Create: `src/phrase/runtime_synth_events.cpp`
- Test: `tests/test_pattern_phrase_p1_runtime_events.cpp`

**Interfaces:**
- Produces:

```cpp
namespace PhraseRuntime {
constexpr uint16_t kTicksPerBar = 384;
constexpr uint16_t kSubticksPerTick = 16;
constexpr uint8_t kMaxPhraseBars = 8;
constexpr uint16_t kMaxSynthEvents = 128;

enum RuntimeSynthEventFlag : uint8_t {
  kEventAccent = 1u << 0u,
  kEventSlide = 1u << 1u,
  kEventGhost = 1u << 2u,
};

struct RuntimeSynthEvent {
  uint16_t startTick;
  uint16_t durationSubticks;
  uint8_t note;
  uint8_t velocity;
  uint8_t probability;
  uint8_t flags;
  uint8_t fx;
  uint8_t fxParam;
};

struct RuntimeSynthEventBuffer {
  RuntimeSynthEvent events[kMaxSynthEvents];
  uint16_t count;
  uint16_t lengthTicks;
};

struct PatternProjectionSettings {
  uint8_t synthIndex;
  uint8_t swingPercent;
  bool swingEnabled;
  float gateLengthRatio;
};

enum class PatternProjectionStatus : uint8_t { Ready = 0, InvalidSynthIndex };

PatternProjectionStatus projectPatternToRuntimeEvents(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination);
}
```

- [ ] **Step 1: implement the value types and ABI assertions**

Use only fixed arrays/scalars and `std::is_trivially_copyable`. Do not add a global buffer.

- [ ] **Step 2: implement exact legacy trigger scanning**

For `barTick=0..383`, reproduce MiniAcid's nominal/previous/next step scan and modulo expression. Clamp swing to 50..75 and apply it only when `swingEnabled` is true.

- [ ] **Step 3: implement gate conversion**

Convert the existing A/B `gateLengthRatio` formula to 1/16-tick fixed point with `std::lround`, minimum one subtick.

- [ ] **Step 4: implement TIE folding and monophonic clipping**

Create no event for `note == -2`. For each onset, inspect subsequent tokens through one wrapped cycle. Extend only if the TIE occurs while active; clip at the next onset.

- [ ] **Step 5: run focused GREEN**

```sh
bash tests/run_pattern_phrase_p1_runtime_events.sh
```

Expected output ends with:

```text
PATTERN/PHRASE P1 runtime events: OK
PATTERN/PHRASE P1 focused gate: PASS
```

- [ ] **Step 6: commit GREEN**

```sh
git add src/phrase/runtime_synth_events.h src/phrase/runtime_synth_events.cpp
git commit -m "feat(0.9.10): add immutable Pattern runtime event projection"
```

### Task 3: Regression and review gate

**Files:**
- No new production files.

**Interfaces:**
- Consumes: Task 2 exact head.
- Produces: evidence that P1 is behavior-neutral and reviewable independently from P0.

- [ ] **Step 1: run focused sanitizer/compiler matrix**

```sh
bash tests/run_pattern_phrase_p1_runtime_events.sh
```

Require GCC deterministic repeat, Clang parity, ASan and UBSan all GREEN.

- [ ] **Step 2: verify diff firewall**

```sh
git diff --name-only c02d9ae04fcd43b5e3ced11e5aa50e850e26b4e6...HEAD
```

Production paths must be exactly:

```text
src/phrase/runtime_synth_events.h
src/phrase/runtime_synth_events.cpp
```

plus P1 tests/docs/workflow/spec/plan.

- [ ] **Step 3: run repository CI**

Require normal host/Core, SDL, Cardputer ADV, fixed DRAM and SEQTRAK MIDI-only jobs to pass at exact P1 head.

- [ ] **Step 4: open stacked Draft PR**

Base: `research/20260903-01-0.9.10-pattern-phrase-p0-characterization`.

Head: `feature/20260903-02-0.9.10-pattern-phrase-p1-runtime-events`.

PR must state explicitly: representation only; MiniAcid still consumes legacy Pattern; no Scene/Undo/UI/MIDI change.
