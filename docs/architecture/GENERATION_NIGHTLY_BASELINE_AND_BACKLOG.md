# Generation Nightly Baseline and Backlog Audit

Status: normative startup gate for autonomous Stage 7C–13 work

Date: 2026-08-10

## Baseline

The implementation loop must treat `dev_0.9_test` as the production source of truth for all pre-existing application behavior.

At the time this document was added:

```text
dev_0.9_test
0a2fe15696eb7c8f0bdcc1a986aed3ca342dd948

roadmap branch
agent/20260810-03-stage7-13-generation-vocabulary-roadmap
```

The roadmap branch is not behind `dev_0.9_test`; the current `dev_0.9_test` head is already an ancestor of the Groove Vocabulary stack. Do not create a meaningless merge commit when `git merge-base`/`git rev-list` proves there are no missing devtest commits.

Before starting every new production stage, fetch origin and repeat the ancestry check. If `dev_0.9_test` has advanced, integrate the new production changes first, resolve conflicts by preserving the newer devtest user behavior unless the current stage explicitly replaces it, run the relevant regressions, then freeze the new stage base SHA.

## Mandatory pre-Stage-7C backlog audit

Do not assume that an old open PR is missing from production and do not cherry-pick old branches blindly.

For every open/draft PR or previously planned feature that overlaps the current code:

1. inspect the current `dev_0.9_test` implementation;
2. inspect the current Groove Vocabulary stack implementation;
3. classify the item as one of:
   - `ALREADY_PRESENT`;
   - `SUPERSEDED`;
   - `HARDWARE_PENDING`;
   - `VALID_INDEPENDENT_BACKLOG`;
   - `BLOCKS_STAGE_7_13`;
4. only port code when current production behavior proves the function is actually absent;
5. preserve newer fixes and ownership contracts instead of resurrecting stale implementations.

## Known items requiring explicit classification

### FEEL pattern length — PR #183

The branch restores the existing `Scene::feel.patternBars` control as `1B / 2B / 4B / 8B` in `GENERATE -> FEEL` and leaves physical Cardputer acceptance pending.

Before Stage 12, determine whether this behavior is already present on the actual working base. If absent and still valid, integrate it through a narrow current-base PR. Do not invent a second phrase-length owner.

### Audio runtime foundation — PR #180 follow-ups

The runtime-foundation work explicitly deferred:

- R3 unified note ownership registry;
- R4 DSP musical regression suite;
- event/performance looper capture.

Audit these against current `dev_0.9_test` before Stage 8–12.

`Unified note ownership` is potentially relevant to Stage 10/11 held notes and cross-bar NoteOff correctness. If current production already has an equivalent owner, reuse it. If not, implement the minimum required ownership boundary before introducing long notes across bars.

The DSP musical regression suite is independent unless a new generation stage changes DSP semantics. Do not fold a broad DSP rewrite into Stage 7–13.

The event/performance looper is not required by Stage 7–13 unless current Phrase/Song integration proves otherwise. Keep it independent.

### Dead Genre timbre projection — PR #191

This old cleanup branch diverges from the current Groove Vocabulary stack. Do not merge/cherry-pick it mechanically.

Audit current `GenreSceneView` / Genre ownership. If the dead synth projection still exists with no production caller, remove it in a narrow current-base cleanup PR and lock the ownership with a source regression. If it is already gone or superseded, classify it accordingly.

### AY articulation — PR #142

Old branch based on `dev_0.9`, not the current `dev_0.9_test` stack.

Audit the actual AY implementation for:

- velocity zero silence;
- bounded accent;
- legato slide;
- gate envelope hold/release;
- truthful parameter labels;
- reset of transient articulation state.

Do not port the old branch if the current engine already satisfies these contracts. AY work does not belong inside Stage 7–13 unless a current defect blocks generated-note correctness.

### SID articulation — PR #139

Old branch based on `dev_0.9`.

Audit current SID behavior for:

- click-safe attack/release;
- velocity zero silence;
- accent;
- legato slide;
- DC safety;
- truthful labels.

Again: classify first; only create a current-base fix if a real current defect remains.

### Stage 7A Batch 1 candidates

Do not lose the unresolved status of:

```text
HARD_02 staggered_machine
HARD_05 cross_cycle
HARD_04 break_halfstep
HARD_09 rock_push
HARD_03 halfback_control
```

Search repository evidence and hardware notes for actual user verdicts. `halfback_control` was intentionally a control candidate. Missing evidence means `HARDWARE_PENDING`, never inferred acceptance.

### Known inherited PA_EN source regression

The Groove Vocabulary stack's focused gates pass while aggregate Core regressions currently reach an inherited Cardputer ADV `PA_EN` source assertion.

Treat this as a stale/inherited baseline assertion only after verifying the current hardware profile and test still disagree in the same way. If the assertion is objectively stale and can be corrected without changing hardware semantics, fix it in a narrow baseline-maintenance PR so the full host suite becomes truthful. Do not hide or skip real hardware regressions.

## Source-level unfinished-code sweep

Before Stage 7C and again before final Stage 13 completion, inspect production sources for:

```text
TODO
FIXME
HACK
XXX
stub
not implemented
no-op
pending
deferred
temporary
compatibility-only
legacy fallback
unreachable
```

A marker alone is not a bug. For each hit, determine whether it is:

- valid documentation/comment;
- deliberate compatibility boundary;
- dead code;
- unfinished user-visible function;
- test-only/audition-only code;
- technical debt relevant to Stage 7–13.

Create an explicit backlog table with owner, classification, stage relevance and action. Do not perform unrelated cleanup merely because a marker exists.

## UI/functionality smoke inventory

Before expanding generation, preserve all working `dev_0.9_test` workflows and explicitly smoke-test the areas most likely to be affected by generation/state changes:

- GENERATE -> GENRE;
- GENERATE -> FEEL;
- Pattern NOTE ENTRY;
- page/bank/slot navigation;
- Song mode and pattern length;
- Phrase Core A/B/C/D;
- Save / reboot / Load;
- Project Reset;
- Synth A/B TYPE and parameters;
- drum editing and mute/solo;
- MIDI Hub routing/channel persistence;
- MIDI Player/browser navigation;
- GP MASTER / SEQ MASTER behavior;
- 96 PPQN clock and NoteOff/AllNotesOff safety;
- SEQTRAK MIDI-only build;
- SDL build;
- Cardputer ADV normal + fixed DRAM.

If the current code contains another user-visible function that is implemented but unreachable, partially wired, or permanently stuck behind a stale compatibility path, add it to the backlog before Stage 13. Fix it only in the appropriate narrow PR.

## Merge and base discipline

Every Stage 7C–13 implementation PR must state:

```text
current dev_0.9_test SHA
stage base SHA
merge-base result
whether devtest advanced during the stage
which independent backlog items were deliberately excluded
```

If `dev_0.9_test` advances while a stage is being implemented, re-evaluate rather than blindly rebasing a finished musical corpus. Any change that affects deterministic generation IDs/RNG, Scene persistence, input handling, MIDI timing or hardware behavior invalidates the relevant review/hardware assumptions and requires the corresponding gates again.

## Completion rule

Stage 13 is not the only completion target. The autonomous loop must end with two outputs:

1. Stage 7C–13 implementation status;
2. a reconciled `dev_0.9_test` backlog showing every discovered dangling function as `DONE`, `SUPERSEDED`, `HARDWARE_PENDING`, or intentionally `DEFERRED` with a reason.

Never report a dangling function as completed solely because an old PR exists.