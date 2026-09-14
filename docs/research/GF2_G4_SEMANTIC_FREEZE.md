# GF2 G4 — MUSICAL SEMANTICS FREEZE

## Status

`SEMANTIC_FREEZE_DECISION / DOCUMENTATION_ONLY`

This document stops further G4 production expansion and records which G4 results are durable inputs to the next musical architecture phase, which claims are superseded, which evidence remains historical, and which proven production deltas still exist only as donor material.

This checkpoint does **not** create `MUSIC-R1`, does not change production, and does not declare any active 0.9.12 branch canonical.

---

# 1. Repository topology at freeze time

The freeze branch was initially created from C8 only as a convenient documentation base:

```text
research/20260914-g4-semantic-freeze
  initial parent:
  6897a2434c17c42d4da369f5ea39dce98050c2af
  feature/20260913-c8-final-build-memory
```

That C8 point is **not** the newest product lineage and must not be treated as the future MUSIC-R1 root.

The inspected descendant line is:

```text
d09dba481dedfea3411eef6f91a71dd1e9198a4b
integration/20260913-0.9.11-full-convergence-r1
        |
        v
6897a2434c17c42d4da369f5ea39dce98050c2af
feature/20260913-c8-final-build-memory
        |
        | 28 commits
        v
077e0ce10d5ad90ce30479990f49924252d448d7
fix/20260914-c9-midi-user-closure-r1
        |
        | 10 commits
        v
860b10afd60eb1a90c1be62bb5baba529c79fdf5
fix/20260914-c9a-final-stabilization
        |
        | 9 commits, current inspected FS1 diagnostic line
        v
0da06437da79e5f756f1b994a567ad05cd758d0d
feature/20260914-fs1-memory-census-m1-m2
```

Important interpretation:

- `077e0ce1...` adds the later C9 MIDI/user closure work.
- `860b10af...` contains later C9A product stabilization, including the Melody audible fail-closed/source-toggle work.
- `0da06437...` is a 0.9.12 FS1 diagnostic descendant. Relative to `860b10af...`, the inspected delta is workflow/test instrumentation rather than a new musical production authority change.
- therefore there is currently **no basis to name an exact MUSIC-R1 root**.

The next music production branch must start only after the active Material Closure line has produced one accepted exact root.

The freeze branch itself is intentionally documentary and is not a candidate production lineage.

---

# 2. Why G4 stops here

The useful G4 question was:

> Can the generator produce genuinely different musical ideas inside one genre while preserving genre identity?

G4 gradually split into two activities:

1. finding real ownership/admission/authority defects;
2. trying to reduce whole genre identities to compact predicates.

The first activity produced durable results. The second exposed a failure mode: an implementation proxy can pass every test while still being a poor model of the music.

Therefore the target is no longer:

```text
all owners -> PROVEN genre contract
```

`REVIEW_REQUIRED` and `UNKNOWN` are legitimate outcomes when the musical statement is not yet good enough.

The next architecture target is:

```text
GENRE BOUNDARIES
        |
        v
MUSICAL IDEA
        |
        +-------------------+
        |                   |
        v                   v
VARIATION                NEW IDEA
        |
        v
PHRASE DEVELOPMENT
        |
        v
MATERIAL
```

---

# 3. Canonical G4 results already present in the active descendant line

## 3.1 DnB bass vocabulary compatibility

Durable musical result:

- DnB rhythm ownership remains structurally Breakbeat-oriented;
- its bass choice space must be compatible with that rhythm domain;
- `HalfTimePocket` is a legitimate bass organization against faster drum activity.

Integrated mechanism:

```text
kBassBreakbeat
DrumAndBass / BASE -> kBassBreakbeat
```

Classification:

```text
CANONICAL_INTEGRATED
```

This is a useful cross-timescale reference case for MUSIC-R1.

## 3.2 Phrase-law truthfulness

Durable result:

- Phrase is the causal owner of multi-bar development;
- a selected non-Loop law is truthful only when an admitted trajectory exists;
- otherwise semantics normalize to `Loop` rather than advertising development that cannot occur.

Classification:

```text
CANONICAL_INTEGRATED
```

Scope limit:

This proves semantic truthfulness, not rich phrase development. A truthful `Loop` fallback is still musically shallow if every bar is effectively the same.

## 3.3 Dub Techno structural ownership

Durable formulation:

```text
TECHNO SKELETON + DUB RELATIONSHIP = DUB TECHNO IDEA
```

The integrated admission correction prevents Dub Techno from being justified only by timbre/FX.

Classification:

```text
CANONICAL_INTEGRATED
```

This is the main reference case where relationship/negative space matters more than a single event mask.

## 3.4 House structural ownership cleanup

Durable result:

- House ownership cannot be established by label or weight;
- the known over-broad `FunkHouseBridge` admission was removed from House BASE without deleting that vocabulary from legitimate owners.

Classification:

```text
CANONICAL_INTEGRATED
```

---

# 4. Canonical donor still missing from the active descendant line

## 4.1 Acid articulation authority restoration

Canonical authority branch:

```text
feature/20260911-01-g4-cc1a-p1-acid-articulation-authority
```

Canonical report commit:

```text
6bae461aa1593ef3ef27b537dac7c482976d125c
```

Production-fix commit:

```text
fe2697cf55323c735f794c0251e17d817ca3c0cd
```

Durable musical statement:

> Within each shipped Acid owner, the authoritative bass idea space preserves a genuine choice between independent re-articulation and connected/extended motion.

Production mechanism:

```text
kBassAcid =
  KickLock        70
  OffbeatPush     90
  RollingDrive   110
  SyncopatedHook  75
  SustainAndDrop 100
```

with Acid BASE / Chicago Jack / Rolling Acid bound to that Acid-local vocabulary.

Ratified status from the authority line:

```text
AUTHORITY_RATIFIED
CAPABILITY_REVIEW_REQUIRED
```

That distinction is preserved. The production authority repair is trusted donor material; the abstract genre contract is not automatically promoted to global truth.

At the currently inspected active 0.9.12 descendant `0da06437...`, `generation_profile.cpp` still contains `kBassDrive`, `kBassMachine`, `kBassBreakbeat`, etc., but no `kBassAcid` table. Therefore Acid P1 remains:

```text
CANONICAL_DONOR_NOT_INTEGRATED
```

Do not mechanically merge the historical Acid branch. Re-apply the smallest proven mechanism against the eventual Material Closure root and re-prove it there.

---

# 5. Canonical methods, historical numbers

## 5.1 I6 ownership/reachability method

Durable methodology:

- measure owner -> admitted archetype edges explicitly;
- prove reachability rather than infer it from declarations;
- never convert `UNKNOWN` into valid;
- detect structural collisions from normalized structure rather than labels;
- keep historical evidence distinct from current freshness.

The ratified G4 line had the following stable dimensions:

```text
profiles = 33
archetypes = 24
effective_owner_edges = 122
roots = 4224
materializations = 12672
reachability = 122 / 122
false_owner = 0
admission_orphan = 0
structural_collision = 0
```

Classification:

```text
CANONICAL_METHOD
HISTORICAL_NUMERIC_EVIDENCE
```

These numbers must be recomputed on the future MUSIC-R1 root. They are not eternal constants.

## 5.2 Materialized corpus / timbre stripping

Durable method from C0/C0R:

- compare materialized structure, not selected labels;
- distinguish attack topology from occupancy/lifetime;
- conceptually strip timbre when testing identity;
- do not call P-level realization differences new musical ideas by default.

Classification:

```text
CANONICAL_METHOD
HISTORICAL_CORPUS
```

---

# 6. Superseded semantic claims

The first CC1 Acid/Techno/Funk predicates remain useful as historical experiments, but they are not semantic authority for MUSIC-R1.

## 6.1 Acid proxy — superseded

Historical predicate:

```text
short gate
+ held gate
+ Kick -> Bass Respond
+ max offset <= 3 sixteenth-note steps
```

Why it failed as a portable musical contract:

- LaneGrammar metadata was not the final authoritative downstream articulation semantics;
- short+held was incorrectly treated as a per-pattern requirement;
- Kick->Bass Respond was not a universal Acid identity law;
- `<=3` encoded grid implementation detail.

Replacement lesson:

```text
prove authoritative two-sided articulation capability in owner-space
```

Classification:

```text
SUPERSEDED_AS_GENRE_CONTRACT
HISTORICAL_AS_FAILURE_WITNESS
```

## 6.2 Techno proxy — superseded as final contract

Historical predicate:

```text
NO_HARMONIC_MOTION
progression in {StaticModal, PedalDrone}
exactly one harmonic event
```

Why it is insufficient:

- “Techno does not move harmony” is too absolute;
- current progression IDs are implementation witnesses, not a musical definition;
- a one-bar event count does not establish harmonic role over musical time.

Retained MUSIC-R1 research question:

> Is the Techno base not progression-led, even when some harmonic motion exists?

This belongs to harmonic-role / harmonic-time analysis, not an enum whitelist.

Classification:

```text
SUPERSEDED_AS_FINAL_CONTRACT
FUTURE_HARMONIC_ROLE_CASE
```

## 6.3 Funk/Soul proxy — superseded as final contract

Historical predicate:

```text
Kick lane contains step 0
```

Why it is insufficient:

- The One is not equivalent to one kick onset;
- metric gravity, syncopation, pocket and return are relational phenomena;
- a pattern can contain kick step 0 without being organized around The One.

Retained question:

> Does the groove cycle preserve The One as metric gravity / return point?

Classification:

```text
SUPERSEDED_AS_FINAL_CONTRACT
FUTURE_ADVERSARIAL_EXAM
```

Funk is not a prerequisite for closing G4.

---

# 7. Branch/result classification

| Line | Meaning | Freeze classification |
|---|---|---|
| C0 / C0R materialized corpus | empirical structure / click-collapse discovery | `HISTORICAL_EVIDENCE + CANONICAL_METHOD` |
| contract-candidate ranking | exploration of possible predicates | `HISTORICAL_RESEARCH` |
| DnB compatibility | real owner-space defect/fix | `CANONICAL_INTEGRATED` |
| I3 phrase truthfulness | selected law must have causal realization | `CANONICAL_INTEGRATED` |
| I4 Dub Techno ownership | techno skeleton + dub relationship | `CANONICAL_INTEGRATED` |
| I5 House ownership | remove over-broad admission | `CANONICAL_INTEGRATED` |
| I6 global census | ownership/reachability method | `CANONICAL_METHOD`; numbers historical per SHA |
| CC1 Acid/Techno/Funk predicates | proxy-contract experiment | `SUPERSEDED_SEMANTICS` |
| CC1A Acid structural minimum | discovery of authority collapse | `CANONICAL_DIAGNOSIS` |
| CC1A-P1 Acid authority | real production correction | `CANONICAL_DONOR_NOT_INTEGRATED` |
| C3/C3A ratification machinery | evidence composition/provenance | `HISTORICAL_VERIFICATION_INFRASTRUCTURE` |
| C9/C9A | later product stabilization, not a new G4 semantic authority | `FOUNDATION_LINEAGE` |
| 0.9.12 FS1 | active Material Closure diagnostics | `ACTIVE_FOUNDATION_WORK`, not MUSIC-R1 root |

No branch name alone establishes authority. Exact commits, production diffs, reports and retained tests are the evidence.

---

# 8. Target model for MUSIC-R1

The next musical line is not `G4-I7`.

Its conceptual model is:

```text
GENRE BOUNDARIES
invariants / prohibitions / relationships
        |
        v
MUSICAL IDEA
coherent structural choice
        |
        +-------------------+
        |                   |
        v                   v
VARIATION                NEW IDEA
preserve anchors          change structural statement
        |
        v
PHRASE DEVELOPMENT
1 -> 2 -> 4 -> 8 bars
        |
        v
MATERIAL
```

First try to express idea identity and anchors through the existing `GenerationCompositionResult`.

A new runtime `MusicalIdea` owner is forbidden until evidence shows that the current composition result cannot truthfully represent the required semantics.

---

# 9. MUSIC-R1 completion contract

The musical architecture line is closed only when the following are demonstrated on one exact accepted descendant of the final Material Closure root.

## A. Genre boundary

```text
false_owner = 0
admission_orphan = 0
structural_collision = 0
```

Representative structural mechanisms are proven. Exhaustive `all genres -> PROVEN` is not required.

## B. Idea

Each reference genre can produce multiple structurally different ideas.

A `NEW IDEA` cannot be established solely by:

- timbre;
- instrument;
- velocity;
- transposition;
- FEEL;
- one-event mutation;
- weighting changes.

The difference must survive the relevant timbre-stripped structural comparison.

## C. Variation

P1/P2/P3 or successor semantics change realization while preserving identifiable idea anchors.

Variation must not silently reroll a different idea.

## D. Phrase development

2/4/8-bar material develops one frozen idea.

Forbidden failure mode:

```text
bar 0 = idea A
bar 1 = unrelated reroll
bar 2 = unrelated reroll
bar 3 = unrelated reroll
```

## E. Temporal depth

Meaningful activity/development exists across more than one time layer:

```text
step
beat
bar
2-bar
4-bar
8-bar
```

## F. Harmonic time

Where harmony matters, harmonic rhythm is evaluated in physical/musical time as well as bar counts.

The same `{0,8}` bar-domain representation cannot be assumed musically equivalent at radically different BPM.

## G. Perceptual/adversarial validation

Use both:

- rendered/listening evidence;
- timbre-stripped structural evidence.

Neither listening alone nor declarative labels alone establish correctness.

## H. Architecture constraints

Do not introduce:

- runtime `ReferenceGenre` hacks;
- one special production branch per genre;
- a generic genre-rule DSL;
- an ML genre classifier;
- a continuous engineering-facing `IDEA`/`MORPH` knob;
- duplicate sequencer/Phrase runtime ownership.

## I. Integration

All accepted music production changes live on one exact descendant of the accepted Material Closure root with exact-head verification.

---

# 10. Reference genres for MUSIC-R1

Use a small orthogonal set:

```text
Acid
  articulation / lifetime capability

DnB
  cross-timescale drum <-> bass relation

Dub Techno
  skeleton + relationship / negative space

Techno
  harmonic role / not progression-led
```

Funk/Soul is retained as a later adversarial exam for metric gravity/pocket.

---

# 11. Required execution order

## Phase 0 — finish Material Closure

Do not start MUSIC-R1 production from the semantic-freeze branch, C8, C9, C9A, or an active FS1 diagnostic head merely because it is newest.

First obtain one exact accepted Material Closure root.

## Phase 1 — donor reconciliation

Against that root:

1. inventory which canonical G4 production mechanisms are physically present;
2. re-apply only missing proven donors;
3. Acid P1 is the known missing donor at freeze time;
4. rerun current ownership/reachability and retained runtime gates;
5. do not mechanically merge historical G4 branches.

## Phase 2 — idea / variation baseline

Build a fresh corpus for Acid, DnB, Dub Techno and Techno and answer:

```text
what is one idea's structural fingerprint?
which fields are anchors?
which changes are variation?
which changes constitute a new idea?
```

Do not add a new runtime owner before this evidence exists.

## Phase 3 — phrase development

Prove causal 2/4/8-bar development of frozen ideas and measure activity distribution across time layers.

## Phase 4 — harmonic-role / harmonic-time case

Use Techno to test “not progression-led” rather than resurrecting `NO_HARMONIC_MOTION`.

## Phase 5 — perceptual calibration

Only after structural correctness:

- listening comparison;
- timbre-stripped comparison;
- weight calibration;
- genre-specific tuning.

Weights calibrate probability. They do not establish ownership.

---

# 12. Explicit non-goals of this freeze

This checkpoint does not:

- change `src/**`;
- change tests or workflow semantics;
- merge Acid P1;
- reopen I6 historical freshness;
- promote a `REVIEW_REQUIRED` contract;
- claim Techno or Funk are fully modeled;
- create MUSIC-R1 production code;
- declare C8/C9/C9A/FS1 canonical merely because they are later commits.

---

# 13. Decision

G4 stops as a research/authority phase.

Its durable lessons are:

```text
OWNERSHIP BEFORE WEIGHTING

materialized structure before labels

prohibitions and relationships before genre adjectives

authoritative downstream behavior before upstream metadata

variation != new idea

phrase development != per-bar reroll
```

The next musical problem is:

> Define and prove the boundary between Genre, Musical Idea, Variation and Phrase Development on top of one accepted Material Closure root.
