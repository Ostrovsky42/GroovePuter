# P2 Common Runtime Playback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Route existing Synth A/B PATTERN execution through P1C immutable runtime events and one backend-neutral lifetime owner while preserving accepted PATTERN behavior and producing no GF2 semantic delta.

**Architecture:** `SynthPattern` remains persistent musical authority. Existing control-side mutation/page/generation owners prepare fixed `PhraseRuntime::RuntimeSynthEventBuffer` values; AudioTask selects prepared values only. A small `RuntimeSynthPlaybackState` owns monophonic start/release state and emits one logical transition consumed by both internal synth and PatternPlayer MIDI.

**Tech Stack:** C++20 host tests / C++17-compatible firmware code, ESP32-S3/Cardputer ADV, SDL host harness, GitHub Actions, existing P1C projector, existing AudioMutationGate/AudioGuard and quantized-generation publication owner.

**Spec:** `docs/contracts/0_9_10_PATTERN_PHRASE_P2_COMMON_RUNTIME_PLAYBACK.md`

## Global Constraints

- Base is `dev_0.9.10 @ fa552763d34e0172ceed1d07913743165d9a5867` unless authoritative dev advances; never reconstruct from historical P1C/Gate-B parents.
- `GF2 PRODUCTION SEMANTIC DELTA = NONE`.
- Do not change Gate B snapshots/methodology, Genre/Recipe, DENSITY, FEEL, phrase-law, secondaryRole, DEPTH, GRID, or GF2 materialization semantics.
- Do not change Performance Instrument V1 behavior.
- No PHRASE source/storage, MAKE PHRASE, LENGTH, GRID editor behavior, Scene schema, second Undo owner, second scheduler, second AudioTask, or second MIDI owner.
- AudioTask may select already-prepared runtime buffers and advance fixed runtime state; it may not project/copy `SynthPattern` or allocate/wait.
- Existing Pattern probability/ghost RNG order, timing, RETRIG ordering, velocity/accent/slide behavior, and MIDI publication order remain invariant unless an acceptance test explicitly identifies the old internal/MIDI lifetime divergence being removed.
- Ordinary bar wrap is not a hard release barrier; explicit accepted source/lifecycle transitions are.

---

### Task 1: Add the backend-neutral lifetime state

**Files:**
- Create: `src/phrase/runtime_synth_playback.h`
- Create: `src/phrase/runtime_synth_playback.cpp`
- Create: `tests/test_pattern_phrase_p2_runtime_playback.cpp`
- Create: `tests/run_pattern_phrase_p2_tests.sh`
- Create: `.github/workflows/0_9_10_pattern_phrase_p2_common_runtime_playback.yml`

**Interfaces:**
- Consumes: `PhraseRuntime::RuntimeSynthEvent` from `src/phrase/runtime_synth_events.h`.
- Produces:

```cpp
namespace PhraseRuntime {

enum class RuntimeSynthPlaybackActionType : uint8_t {
  Release = 0,
  Start,
  Retrigger,
};

struct RuntimeSynthPlaybackAction {
  RuntimeSynthPlaybackActionType type = RuntimeSynthPlaybackActionType::Release;
  RuntimeSynthEvent event{};
};

struct RuntimeSynthPlaybackActions {
  RuntimeSynthPlaybackAction values[2]{};
  uint8_t count = 0;
};

class RuntimeSynthPlaybackState {
 public:
  RuntimeSynthPlaybackActions acceptOnset(const RuntimeSynthEvent& event,
                                           uint32_t absoluteStartSubtick);
  RuntimeSynthPlaybackActions acceptRetrigger(const RuntimeSynthEvent& event);
  RuntimeSynthPlaybackActions releaseDue(uint32_t absoluteSubtick);
  RuntimeSynthPlaybackActions hardBarrier();
  bool active() const;
  uint8_t activeNote() const;
  uint32_t releaseAtSubtick() const;
};

}  // namespace PhraseRuntime
```

- [ ] **Step 1: Write RED lifetime tests** covering first onset, replacement onset release-before-start, exact expiry, no duplicate expiry, retrigger without deadline extension, ordinary 384-tick wrap with no barrier, and hard barrier exactly once.
- [ ] **Step 2: Run RED in GitHub Actions.** Expected: compile failure because `src/phrase/runtime_synth_playback.h` is absent.
- [ ] **Step 3: Implement fixed state only.** Store active flag, active event, and 32-bit absolute release subtick. `acceptOnset()` returns `[Release, Start]` when replacing an active note, otherwise `[Start]`; release deadline is `absoluteStartSubtick + max(1,durationSubticks)`. `acceptRetrigger()` returns one Retrigger only while active and never changes deadline. `releaseDue()` returns one Release when `absoluteSubtick >= deadline`; `hardBarrier()` returns one Release only when active. No heap/container/callback ownership.
- [ ] **Step 4: Run focused test GREEN** with GCC, repeat determinism, Clang when available, ASan and UBSan.
- [ ] **Step 5: Commit** `feat: add common runtime synth lifetime owner`.

### Task 2: Add fixed prepared PATTERN runtime banks

**Files:**
- Modify: `src/dsp/miniacid_engine.h`
- Modify: `src/dsp/miniacid_engine.cpp`
- Extend: `tests/test_pattern_phrase_p2_runtime_playback.cpp`
- Create: `tests/test_pattern_phrase_p2_source_contract.py`

**Interfaces:**
- Consumes: `projectPatternToRuntimeEvents(...)` and `RuntimeSynthEventBuffer`.
- Produces engine helpers:

```cpp
void rebuildPatternRuntimeEventBank(int pageIndex);
void refreshPatternRuntimeEvents(int synthIndex, int bankIndex,
                                 int patternIndex, int pageIndex);
const PhraseRuntime::RuntimeSynthEventBuffer& activePatternRuntimeEvents(
    int synthIndex) const;
```

Fixed state is two voices × all 16 resident physical pattern addresses plus one canonical empty buffer per voice. No vector/heap is permitted.

- [ ] **Step 1: RED** populate all resident addresses with distinguishable material and prove every lookup returns the correct already-projected buffer; `NO PATTERN` must select canonical empty.
- [ ] **Step 2: RED source contract** rejects projection/build calls from `processSequencerEvents()` and `generateAudioBuffer()`.
- [ ] **Step 3: Implement control-side rebuild/refresh** using current accepted swing/gate settings and page metadata. Invalid identity must leave the last valid entry unchanged.
- [ ] **Step 4: GREEN** prove lookup is selection-only and cross-voice isolated.
- [ ] **Step 5: Commit** `feat: own prepared Pattern runtime event bank`.

### Task 3: Settle existing Pattern mutations into the derived bank

**Files:**
- Modify only existing Pattern mutation/page settlement callers that already own bounded AudioGuard/AudioMutationGate commits.
- Extend: `tests/test_pattern_phrase_p2_source_contract.py`
- Extend: `tests/test_pattern_phrase_p2_runtime_playback.cpp`

**Interfaces:** Uses `refreshPatternRuntimeEvents(...)` / `rebuildPatternRuntimeEventBank(...)`; creates no publication owner.

- [ ] **Step 1: RED** prove Scene mutation does not change audible prepared bytes until the existing commit settles them, then only the targeted resident entry changes.
- [ ] **Step 2: Implement one refresh inside the existing bounded mutation settlement**, covering note, accent, slide, timing, velocity, probability, ghost, FX, paste/rotate/clear via their common commit path rather than per-command hooks.
- [ ] **Step 3: RED/GREEN page test** requires projection bank settlement before publishing new page identity; failed page load keeps old identity + old bank.
- [ ] **Step 4: Commit** `feat: settle Pattern runtime projections with existing owners`.

### Task 4: Preserve the existing quantized-generation audible overlay

**Files:**
- Modify: existing quantized-generation pending activation value/owner only.
- Modify: existing generation caller settlement only where required to refresh derived resident runtime buffers.
- Extend P2 runtime/source-contract tests.

**Interfaces:** Existing pending owner gains prepared old-audible `RuntimeSynthEventBuffer` values. No second pending slot.

- [ ] **Step 1: RED** persistent generation commit changes Scene bytes while playback stays on old prepared runtime material until current accepted BAR_START activation.
- [ ] **Step 2: Extend the existing pending value** with already-projected old-audible Synth A/B buffers prepared before publication.
- [ ] **Step 3: Switch runtime source resolution** to pending prepared buffer while pending, otherwise resident buffer.
- [ ] **Step 4: GREEN** cover full generation, Synth-A-only, Synth-B-only, cancel/replace, target/revision mismatch and Undo-before-boundary with no stale overlay.
- [ ] **Step 5: Commit** `feat: preserve pending generation runtime projection`.

### Task 5: Make runtime events authoritative for Pattern onset material

**Files:**
- Modify: `src/dsp/miniacid_engine.cpp`
- Extend P2 runtime/source-contract tests.

**Interfaces:** `activePatternRuntimeEvents(synth)` supplies immutable event material; existing PPQN scheduler locates matching start ticks without reading `SynthPattern` event bytes.

- [ ] **Step 1: RED source contract** forbids `activeSynthPattern(...).steps[...]` material reads in Synth onset/timing/retrigger paths.
- [ ] **Step 2: RED runtime parity** covers note, timing, velocity, accent, slide, probability, ghost and RETRIG/fxParam against frozen P0 traces.
- [ ] **Step 3: Replace event-material reads only.** Keep transport tick equations, RNG decision order, LEDs and existing output routing unchanged.
- [ ] **Step 4: GREEN P2 + P1C + unrelated P0 cases.**
- [ ] **Step 5: Commit** `feat: drive Pattern onsets from runtime events`.

### Task 6: Replace split gate countdown with the single lifetime owner

**Files:**
- Modify: `src/dsp/miniacid_engine.h`
- Modify: `src/dsp/miniacid_engine.cpp`
- Extend P2 and P0 runtime tests.

**Interfaces:** Each Synth owns one `PhraseRuntime::RuntimeSynthPlaybackState`. A helper consumes each returned action in one place:
- `Start`: internal `startNote(...)` then publish PatternPlayer NoteOn in frozen order.
- `Retrigger`: internal `startNote(...)` then publish PatternPlayer NoteOn without changing release deadline.
- `Release`: internal `release()` and PatternPlayer NoteOff from the same action.

- [ ] **Step 1: RED** prove natural expiry and replacement onset produce identical logical action sequences for internal/MIDI, with release-before-new-onset.
- [ ] **Step 2: RED** legacy TIE projected duration remains active across ordinary tick 384 wrap.
- [ ] **Step 3: Integrate one playback state per Synth** and remove `gateCountdownA_` / `gateCountdownB_` as Pattern lifetime owners. Live Performance ownership remains independent and unchanged.
- [ ] **Step 4: GREEN** prove exactly one release on expiry and no stuck state.
- [ ] **Step 5: Commit** `feat: unify Pattern synth lifetime ownership`.

### Task 7: Converge hard lifecycle barriers

**Files:**
- Modify only current existing Stop/Pause/Mute/source-transition/Song-selection barrier call sites in `MiniAcid`.
- Extend P2/P0 tests.

**Interfaces:** Every Pattern hard barrier calls the same runtime-state `hardBarrier()` action consumer before/with existing transport cleanup; ordinary bar wrap does not.

- [ ] **Step 1: RED** Song physical source transition must release the internal voice and MIDI from one decision, replacing the old P0 asymmetric expectation.
- [ ] **Step 2: RED** Stop, Pause, Mute, explicit Pattern/source replacement, `NO PATTERN`, page/source identity mismatch and generation cancel/replace each release once; repeated barrier is idempotent.
- [ ] **Step 3: Implement barrier convergence** without adding a second transport or cleanup queue.
- [ ] **Step 4: GREEN** update only the intentionally obsolete P0 divergence assertion; all GRID/timing/TIE/unrelated P0 cases remain unchanged.
- [ ] **Step 5: Commit** `feat: converge Pattern lifetime barriers`.

### Task 8: Exact-head firewalls and release proof

**Files:**
- Complete: `tests/test_pattern_phrase_p2_source_contract.py`
- Complete: `tests/run_pattern_phrase_p2_tests.sh`
- Complete: `.github/workflows/0_9_10_pattern_phrase_p2_common_runtime_playback.yml`
- Update P2 contract with exact evidence only after final runs.

- [ ] **Step 1: Run focused P2 gate** including deterministic GCC/Clang, ASan, UBSan, source firewalls and `git diff --check`.
- [ ] **Step 2: Run P1C gate unchanged.**
- [ ] **Step 3: Run Core HOST/SDL/CARDPUTER_ADV/FIXED_DRAM/SEQTRAK exact-head validation.**
- [ ] **Step 4: Run GF2 exact-head validation and Gate-B frozen-artifact comparison.** Required statement: `GF2 PRODUCTION SEMANTIC DELTA = NONE`.
- [ ] **Step 5: Inspect final diff from `fa552763...`** for accidental Genre/Recipe/DENSITY/FEEL/phrase-law/secondaryRole/DEPTH/GRID/Gate-B/Performance changes.
- [ ] **Step 6: Keep PR Draft until every required exact-head run is terminal GREEN.** Do not merge in P2 implementation checkpoint unless separately instructed.
