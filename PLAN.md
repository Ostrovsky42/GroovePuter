# GroovePuter product direction and execution plan

**Status:** canonical product direction and priority order  
**Last reviewed:** 2026-09-01  
**Current release base:** `dev_0.9.9 @ 0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d`  
**Active release line:** `0.9.10-GF2 — MUSICAL CAPACITY / GENRE / RECIPE / PHRASE`

This file is the single source of truth for **why GroovePuter exists, what must be built next, how musical capability is admitted, how success is measured, and what is deliberately deferred**.

Implementation documents under `docs/stages/`, `docs/contracts/`, `docs/research/`, and `docs/audits/` are subordinate specifications, evidence, acceptance procedures, and historical records. They must map to an item in this file and must not silently reorder the roadmap.

`README.md` describes capabilities that are actually available on the release branch. `MANUAL.md` describes current user-visible behavior. Neither file may redefine product priorities independently of this plan.

---

## Product thesis

GroovePuter is a **portable time-feel instrument for capturing, generating, transforming, developing, and arranging musical phrases**.

It remains a self-contained groovebox. External instruments are output targets, not the definition of the product. Yamaha SEQTRAK remains the first deeply tested external target, but GroovePuter must remain useful without it.

The central promise is:

> Capture or generate a musical phrase, reshape how it feels in time, derive recognizably related material, and develop it into an arrangement.

The intended product path is:

```text
play / generate / extract
        -> Phrase
        -> shape FEEL
        -> develop identity
        -> Section
        -> Song
        -> internal playback or external output
```

External transfer is one output of this path. It must not displace the standalone instrument, interface clarity, or musical-development work.

---

## Canonical musical model

The previous four-axis rule remains valid but is now refined by the production Recipe and Phrase work.

```text
GENRE != RECIPE != RHYTHM != FEEL != PHRASE/EVOLUTION != TEXTURE
```

These names do not imply six equal storage objects or six UI knobs. They identify separate musical responsibilities.

| Concept | Responsibility | Must not silently become |
|---|---|---|
| **GENRE** | Musical language, priors, prohibitions, characteristic relationships, valid corridors | A synth preset, a FEEL preset, or a collection of arbitrary weighted IDs |
| **RECIPE** | Vocabulary and strategy inside a Genre; a way of speaking within the same language | A second Genre catalog or duplicate ownership hierarchy |
| **RHYTHM** | Canonical rhythmic topology and compatibility | Microtiming, timbre, or phrase-length ownership |
| **FEEL** | Swing, microtiming, push/pull, gate, timebase, performance accent interpretation | Note choice, motif identity, harmonic motion, phrase function, or role relationship |
| **PHRASE / EVOLUTION** | Identity across time, deliberate invariance, controlled transformation, boundary function, return/closure/continuation | Bar-index randomness or a generic variation amount |
| **TEXTURE** | Synth engine, kit, processing, drive, space, degradation | Notes, rhythm structure, motif identity, or phrase boundaries |

Changing one responsibility must preserve the others unless the user explicitly chooses a destructive operation such as reinterpretation or regeneration.

### Genre and Recipe hierarchy

`GENRE` and `RECIPE` are related but not interchangeable:

```text
GENRE
  language / priors / constraints / prohibitions

RECIPE
  vocabulary / strategy / emphasis inside that language
```

The authoritative `(Genre, Recipe)` membership catalog must have one production owner. UI lists, persistence adapters, reports, and tests may query that owner but must not maintain competing membership arrays.

---

## Five musical admission filters

Every new musical capability, Genre claim, Recipe, Phrase law, and user-facing musical control must pass these filters.

### 1. Remove timbre — what remains?

Mentally or mechanically replace all sound sources with a neutral click or neutral instrument. If two supposed structural identities become indistinguishable, their difference belongs primarily to production/Texture, not to separate musical structure.

A new Genre must survive this test through rhythm, harmonic rhythm, bass/drum relation, phrase development, role interaction, or another demonstrated structural law.

### 2. Negative capacity matters

A musical language is partly defined by what it **does not permit**.

Examples of admissible constraints include:

```text
do not move harmony here
keep this motif invariant
keep the pedal while upper harmony changes
do not fill this protected metric position
do not let every role evolve at once
do not resolve yet
```

A capability model that records only positive choices is incomplete.

### 3. Recognizability usually lives in structural relationships

When evaluating Genre/Recipe identity, prioritize evidence from:

```text
rhythmic topology
harmonic rhythm
bass <-> drum relationship
phrase development
role interaction
```

Timbre, effect choice, and instrument model may support recognition but do not by themselves justify a structural Genre distinction.

### 4. Musical activity exists on several time levels

Do not concentrate all development on the bar ordinal.

Relevant scopes include:

```text
step -> beat -> bar -> multi-bar figure -> motif -> phrase -> period -> section
```

These are musical time scopes, not a mandate to create a class or runtime owner for each level.

### 5. Would a musician make this decision?

A user-facing musical control must correspond to a decision a musician could plausibly describe while playing, arranging, or composing.

Good examples:

```text
TAKE 03
REPLY
HOLD
DEVELOP
RETURN
```

Bad examples unless they are clearly technical/debug controls:

```text
MORPH 0.43
TRAJECTORY 7
```

Engineering parameters may exist internally, but engineering vocabulary must not be presented as musical agency.

---

## 0.9.9 production foundation — implemented

The Phrase level is no longer a missing architectural object. The canonical `dev_0.9.9` line landed the bounded Phrase execution and product workflow required for 0.9.10 musical work.

Existing foundation includes, in production form where applicable:

```text
explicit Phrase requests: 1 / 2 / 4 / 8 bars
Phrase temporal coordinates
PhraseSemanticResult
PhraseHarmonicTimeline
ChordProgressionSource / phrase-global harmonic WHAT
HarmonicRhythm / harmonic WHEN separation
phrase-wide harmonic clock projection
bounded random-access Phrase materialization
bounded PREPARE / COMMIT generation
Song publication / placement
PHRASE product page
PHRASE CORE workspace
Song -> sounding physical-pattern follow/edit flow
Cardputer ADV / SDL / SEQTRAK build and regression gates
```

The remaining 0.9.10 problem is therefore **not to create Phrase infrastructure again**.

The remaining problem is:

> Make declared musical concepts intentionally executable, preserve musical identity across development, and prove causality between a musical decision and the generated result.

### Phrase memory rule

The existing bounded embedded discipline remains authoritative:

- no unbounded resident N-bar material arrays;
- no dynamic allocation in audio/MIDI hot paths;
- new fixed-capacity semantic carriers require `sizeof`, instance count, RAM, and flash evidence;
- physical storage and semantic musical identity remain separate;
- Pattern/Song destination addresses must not become musical identity coordinates.

---

## Entity boundaries

| Entity | Responsibility |
|---|---|
| **Step** | One position on a timing grid |
| **Bar** | Meter-relative time unit |
| **Pattern** | Events owned by one track or generator, usually cyclic |
| **Phrase** | Bounded, musically related material spanning one or more bars and roles |
| **Section** | Arrangement role and combination of Phrases, such as intro, main, variation, break, or outro |
| **Scene** | Persisted/runtime snapshot according to the actual codec |
| **Song** | Ordered musical playback structure using current Song rows while higher form evolves |

`Scene` and `Section` must not be synonyms. Runtime snapshot state and musical form remain separate concerns.

The time-scope hierarchy used for musical reasoning does not require one storage entity per level.

---

## Capability maturity model

A declared enum value, catalog entry, or random possible output is **not** automatically an implemented musical capability.

For every GF2 dimension, evidence should classify the strongest demonstrated state:

```text
DECLARED
    -> SELECTABLE
    -> PROPAGATED
    -> CONSUMED
    -> EXECUTION-CAPABLE
    -> CAUSALLY PROVEN
```

Definitions:

- **DECLARED** — vocabulary or data exists.
- **SELECTABLE** — an authoritative owner can intentionally choose it.
- **PROPAGATED** — the selected intent reaches the relevant downstream boundary without substitution or loss.
- **CONSUMED** — a production consumer reads the intent.
- **EXECUTION-CAPABLE** — the consumer can intentionally change musical output according to the intent.
- **CAUSALLY PROVEN** — controlled evidence shows that changing this intent, while holding relevant inputs fixed, causes the claimed musical difference.

A behavior that merely occurs somewhere in the corpus is not a capability until intentional selection/execution is demonstrated.

### Gap classifications

GF2 audits should use these result classes where applicable:

```text
REPRESENTABLE
PARTIALLY REPRESENTABLE
DECLARED BUT NOT EXECUTED
EXECUTABLE ONLY ACCIDENTALLY
NOT REPRESENTABLE
UNKNOWN
```

No new runtime semantic owner may be introduced merely because a cleaner abstraction is possible. A new owner requires evidence that an important musician decision cannot be represented cleanly through existing authoritative owners without duplication or semantic conflict.

---

## Product acceptance metrics

### Interface trust

- **Unlabelled silent alphabetic keypresses:** `0` across the documented page/input test matrix.
- **Persistent mutation visibility:** `100%` of inventoried persistent mutation gateways set the dirty state where the current codec defines persistence.
- **Dirty false positives:** `0` for navigation, focus, held live notes, transient playheads, and other runtime-only actions.
- **Status correctness:** visible source/state/clock/output/key-mode tokens agree with their actual state owners in observable host tests.
- **Status redraw:** no full-page redraw caused solely by a step tick or status update.

### Time to usable music

- **Time to first groove:** no more than `90 seconds` from boot for a user familiar with the key sheet, using only the device.
- **Return to last saved state:** one load operation and no ambiguous unsaved-state loss.
- **Mode prediction:** the user can predict whether an alphabetic key means `NOTE`, `CMD`, `LOCAL`, or `LOCK` before pressing it on every tested page.

### Musical identity and development

The previous "fourth bar must differ" metric is superseded by causal musical-law acceptance.

For enabled 0.9.10 laws:

- **Neutral-timbre recognizability:** the claimed structural behavior remains observable when timbre is removed or normalized.
- **Motif identity:** a protected motif remains recognizably the same under allowed transformations and remains unchanged when a law requires invariance.
- **Phrase causality:** switching a Phrase law or transformation intent while holding seed/profile/input material fixed produces the specifically claimed structural difference.
- **Boundary causality:** boundary-conditioned behavior occurs because of phrase position/function, not merely because an RNG branch happened to fire.
- **Role invariance:** when one role is declared stable while another develops, the stable role remains within its protected contract.
- **Legacy identity:** old scenes without new fields preserve existing generated output until a new feature is explicitly enabled, where practical to test.

### Realtime safety

- no sustained increase in audio underruns during the standard hardware acceptance run compared with its recorded baseline;
- no monotonic heap loss during navigation, generation, playback, save/load, and MIDI activity;
- stuck notes: `0` after stop, mute, route change, source change, page eviction, disconnect, or panic in the acceptance matrix;
- every new retained semantic carrier has an explicit fixed-capacity memory budget.

---

## Interaction principles

### State must be visible before it is editable

The user must be able to see the effective state relevant to the current workflow. UI state must derive from existing owners and must not create duplicate transport, input, routing, persistence, musical-policy, or Phrase owners.

### Routine work must be direct

Common musical actions stay close to the existing workflow. Detailed settings may remain behind existing pages or modifiers; common actions must not require another multi-screen subsystem.

### Honest status

GroovePuter must distinguish:

```text
requested intent
accepted semantic decision
execution result
physical playback state
```

The UI must not claim a musical law executed if only a selector changed, and must not claim an external device recorded data when GroovePuter only knows transmission completed.

### Separate playback intentions

Where relevant, preserve the distinction:

```text
AUDITION LOOP
SEND ONCE
PERFORM
```

### No new navigation system

New GF2 capabilities must enter the existing GENRE / FEEL / PHRASE / SONG / editor workflow rather than create another independent navigation model.

### Musical controls before engineering controls

Product-facing controls must name musician decisions. Raw numeric weights, internal trajectory IDs, RNG salts, reachability states, and census dimensions belong to diagnostics/research unless a separate musician-readable semantic is proven.

---

## Runtime and architecture invariants

These constraints override feature convenience:

- Cardputer ADV target remains ESP32-S3FN8 with PSRAM disabled and `PartitionScheme=huge_app` unless a separately approved hardware decision changes it.
- `MidiDispatchTask` remains the single TinyUSB writer.
- Audio, pattern, live, SMF, Phrase audition, and Phrase send share one explicit ownership model.
- No parallel MIDI scheduler or second transport state machine.
- No dynamic allocation on audio or MIDI hot paths.
- No string formatting in `AudioTask`.
- No UI drawing under an audio lock.
- No scene scan, pattern scan, structural hash, serialization, or storage traversal in the frame loop.
- Scheduled MIDI remains sample/deadline based; routine UI work must not perturb timing.
- Source switches invalidate stale queued events and release only notes owned by the previous source.
- Stop, seek, mute, route change, disconnect, and page transitions must not leave active notes.
- Scene/project codecs remain backward compatible through actual codec conventions and missing-field defaults.
- UI work prefers bounded dirty regions over repeated full-screen rendering.
- Existing standalone audio behavior remains available while MIDI output is enabled.
- No general framework rewrite, new UI engine, new persistence system, new transport task, new MIDI dispatcher, or second Phrase architecture.
- No full musical-variant graph may be materialized at runtime; runtime generation remains bounded/local.
- New semantic owners require demonstrated representational need, not abstraction preference.

---

## Mandatory discovery and branch discipline

Before each production GF2 checkpoint:

```text
git fetch origin
resolve exact authoritative base SHA
compare planned scope against actual source owners
run/replay the relevant semantic census or focused characterization
```

Every checkpoint must record:

- exact base/head SHA;
- relevant files, types, functions, state owners, tests, and hardware gates;
- which existing mechanisms are reused;
- production semantic delta;
- persistence delta;
- RAM/flash impact where applicable;
- realtime risks;
- whether musical policy changes;
- capability maturity before and after the checkpoint;
- explicit scope firewall.

Research checkpoints may remain unmerged evidence branches. Production checkpoints must not silently inherit experimental research ancestry unless that ancestry is explicitly frozen and admitted.

When multiple independent investigations can run in parallel, their findings may be parallel; production owner mutations must remain ordered and reviewable.

---

# Canonical delivery order

## 0.9.10-GF2 — MUSICAL EXECUTION FOUNDATION

Release objective:

> Turn existing musical vocabulary into intentional, causally provable execution while preserving embedded/runtime ownership discipline.

The release is **not** measured by the number of new Genres or Recipes. It is measured by whether GroovePuter can be asked to make a named musical decision and can prove that the resulting structural change came from that decision.

### S0 — GF2 BASELINE / CATALOG OWNERSHIP / REMOTE CONVERGENCE

Current production candidate: `GF2-0R` (Draft PR #412).

Required outcome:

- one authoritative `(Genre, Recipe)` membership owner;
- exact released `0.9.9` semantic parity before new musical policy;
- remote authoritative branch/PR lineage for all subsequent GF2 production work;
- recover any valid local-only I0/C2-V0 work if independently verifiable; otherwise reproduce it from the exact accepted base rather than guessing from memory;
- classify old GF2/PHRASE research branches as `AUTHORITATIVE`, `REFERENCE ONLY`, or `SUPERSEDED`.

Exit gate:

```text
one exact GF2 production base
no unknown local-only production dependency
Recipe membership ownership singular
semantic delta NONE for catalog convergence
```

### S1 — GF2-I0 / SEMANTIC REACHABILITY BASELINE

Build/recover a machine-readable capacity map for at least:

```text
RHYTHM COMPATIBILITY
FEEL
BASS
CHORD
CHORD PROGRESSION
MELODIC
MOTIF
PHRASE / EVOLUTION
SECONDARY ROLE
GENERATION CORRIDOR
TONAL POLICY
DRUM POLICY
HARMONIC RHYTHM
PHRASE LENGTH
DEPTH
```

Trace each dimension through:

```text
DECLARED -> SELECTABLE -> PROPAGATED -> CONSUMED -> EXECUTION-CAPABLE
```

Include negative capacity and blockers. This checkpoint must not expand musical policy.

Exit gate: reproducible static/reachability report that becomes the pre-I1 baseline.

### S2 — GF2-I1 / TEMPO AND GENERATION-CORRIDOR ARBITRATION

Resolve conflicting or ambiguous tempo/corridor ownership without turning engineering timing into a new musical axis.

Required questions:

- which owner sets requested/suggested/effective BPM;
- what happens when profile/corridor/runtime values disagree;
- whether a musical law expressed in bars produces perceptually unreasonable rates at extreme tempo;
- whether density/grid corridor values are actually consumed or merely declared.

Keep musical meter authoritative. Seconds/event-rate may be used as perceptual guardrails, not as a replacement for musical time.

Exit gate: one documented tempo/corridor arbitration path and no contradictory effective tempo claims.

### S3 — GF2-I2 / FEEL CONTRACT

Freeze FEEL as performance/timing interpretation.

Allowed FEEL responsibility may include:

```text
timebase
grid interpretation
swing
microtiming
push / pull
gate
velocity / articulation interpretation
```

FEEL must not silently own:

```text
bass <-> kick topology
harmonic rhythm
motif transformation
phrase function
boundary behavior
role relationship
```

Exit gate: causal tests show FEEL changes performance interpretation without replacing musical identity.

### S4 — GF2-C2-V1 / MUSICAL-LAW CONFORMANCE HARNESS

This gate precedes major Phrase-law expansion.

Build deterministic neutral-timbre fixtures that can compare law OFF vs law ON while holding seed/profile/input material fixed.

Minimum fixture families:

```text
exact motif repeat
motif transposition / sequence
related second phrase with changed ending
protected ostinato / invariant layer
pedal bass under moving harmony
intentional non-root bass
bass <-> kick lock
bass <-> kick avoidance
phrase-boundary-conditioned change
one role changes while another remains stable
```

Each fixture records both structural assertions and a bounded listening/review artifact where practical.

Exit gate: the harness can distinguish accidental corpus occurrence from intentional causal execution.

### S5 — GF2-I3A / MOTIF IDENTITY AND DELIBERATE INVARIANCE

Goal: the engine must know **what is being preserved** before it is allowed to develop it.

Initial transformation scope should remain small. Candidate proven operations may include:

```text
REPEAT
TRANSPOSE
SEQUENCE
REGISTER SHIFT
```

The critical new capability is protected identity/invariance:

```text
this musical material is intentionally stable for this scope
```

Ostinato is treated first as evidence for deliberate invariance, not as a request for an unrelated new owner.

Exit gate:

- repeat can preserve exact identity;
- permitted transformation remains recognizably related;
- protected material cannot be accidentally replaced by generic mutation;
- neutral-timbre conformance passes.

### S6 — GF2-I3B / PHRASE FUNCTION AND BOUNDARY EXECUTION

Existing `PhraseEvolutionLawId` vocabulary must move beyond planning metadata.

Current production vocabulary includes concepts such as:

```text
Loop
RepeatReply
DevelopReturn
SparseDrift
```

Do not expand this enum merely to mirror theory books. First make existing laws causally meaningful.

The implementation should be able to express functional behavior equivalent to:

```text
STATE
REPEAT / CONFIRM
DEVELOP
PREPARE
ANSWER / CLOSE
RETURN
```

using the smallest proven set of existing/new operations.

Candidate operations may include changed ending, fragmentation, sequence, hold/defer, and restore/return, but operation names are hypotheses until conformance evidence justifies production vocabulary.

Exit gate: existing Phrase laws produce structurally distinguishable multi-bar behavior under neutral timbre and fixed-input causal tests.

### S7 — GF2-I4 / MUSICAL CONSUMERS AND BASS CAPACITY

Audit which existing owner actually executes each meaningful law before adding fields.

Required capability questions include:

```text
who enforces DO NOT CHANGE?
who advances or suppresses harmony?
who owns intentional non-root bass motion?
who owns pedal bass?
who preserves/restores motif identity?
who changes one role while another remains stable?
```

Bass work must distinguish rhythmic behavior from pitch/voice-leading behavior. A rich BassRhythm vocabulary does not prove intentional bass pitch capacity.

Minimum target evidence:

- root-only behavior can be intentionally escaped where musically allowed;
- pedal bass is selectable and causally executed;
- non-root bass is not merely random scale wandering;
- existing BASS/TONAL owners remain authoritative unless a proven gap requires otherwise.

Exit gate: no important bass capability is reported as implemented merely because the resulting notes can occur accidentally.

### S8 — GF2-I5 / DEPTH AS MUSICAL ROLE HIERARCHY

DEPTH must pass the musician-decision test.

Do not define DEPTH as "more layers" or "more events" alone.

Target musical interpretation:

```text
foreground
support
background
handoff
```

Use existing `CompositionSecondaryRole` and Phrase context where sufficient. Do not create a new hierarchy owner without evidence.

Exit gate: under neutral timbre, a reviewer/test can identify which role is foreground and whether an intentional handoff occurred.

### S9 — GF2-C2 / GATE B ROLE-RELATION CAPACITY

Only after I3/I4/I5 execution exists, test whether existing owners can express intentional role relationships.

Required relationship fixtures include a bounded subset of:

```text
BASS <-> DRUMS lock / avoid / gap-fill
BASS <-> MELODY contrary or oblique relationship
MELODY <-> SECONDARY call / answer
rhythmic interlock / hocket
complementary density
one-role-stable / one-role-developing
boundary handoff
```

Decision question:

> Can existing BASS / DRUM / MELODIC / PHRASE / SECONDARY ROLE owners express the musician decision through shared context without duplicated authority?

If YES: do not add a generic relationship owner.

If NO: record a concrete representational conflict before authorizing a new owner.

Cross-bar lifetime C2/R1 evidence may be reused only where it supports a proven musical law; do not revive lifetime infrastructure merely because an old research branch exists.

Exit gate: explicit Decision A/B on role-relation representability.

### S10 — GF2-G1 / ACTUAL CAPACITY INTERPRETATION

Repeat the semantic census after S2-S9 and compare:

```text
EXPECTED MUSICAL CAPACITY
vs
ACTUAL EXECUTION CAPACITY
```

Every claimed capability must have a maturity state and evidence.

Exit gate: capacity-gap matrix identifies what is truly missing rather than what merely lacks vocabulary.

### S11 — GF2-R2 / RECIPE AND GENRE PROMOTION — CONDITIONAL

Do not expand catalogs merely because research found more musical concepts.

Recipe promotion may combine already proven capabilities into meaningful strategies inside an existing Genre.

Genre promotion requires structural evidence that survives timbre removal.

If two candidate Genres collapse to the same structural behavior under neutral timbre, prefer differentiation through Recipe, FEEL, or TEXTURE rather than duplicating Genre policy.

Exit gate: every promoted Genre/Recipe has explicit structural evidence and one authoritative catalog membership path.

This sprint may be skipped for 0.9.10 if execution capacity is improved but catalog expansion is not yet justified.

### S12 — GF2-P1 / PRODUCT AND HARDWARE FREEZE

Expose only causally proven, musician-readable decisions.

Do not expose raw semantic-census states, internal IDs, arbitrary trajectory numbers, or research-only vocabulary as musical UI controls.

Required release gates:

```text
focused GF2 conformance
host regressions
SDL build
Cardputer ADV build
fixed DRAM budget
SEQTRAK MIDI-only build
neutral-timbre musical-law corpus
real hardware generation / Phrase workflow
stuck-note / lifecycle acceptance where affected
persistence compatibility where affected
```

Release success criterion:

> We can name a musical decision, request it from the engine, and demonstrate that the resulting structural behavior appeared because of that decision.

---

## 0.9.10 scope firewall

The following are **not required** to call 0.9.10 successful unless Gate B proves they are necessary for the minimal causal execution foundation:

```text
generic RelationshipPolicy owner
full counterpoint system
period / section runtime domain rewrite
large motif-transformation vocabulary
large Genre expansion
large Recipe expansion
advanced form generator
new navigation architecture
new synth/effect engines for feature count
```

A smaller truthful musical language is preferred over a larger catalog of labels that the engine cannot intentionally execute.

---

## 0.9.11+ — expanded musical language

After the 0.9.10 causal-execution foundation is frozen, later releases may investigate:

- richer role relations and counterpoint where proven necessary;
- larger motif-transform vocabulary;
- question/answer and period-level structure beyond the minimal Phrase laws;
- advanced ostinato relationships;
- richer transition and boundary vocabulary;
- section-level form and long-range development;
- additional Recipe/Genre promotion supported by corpus evidence;
- deeper Phrase derivation/history;
- further capture/extraction workflows;
- additional tested external device profiles;
- performance generators such as chord mode, arpeggiator, Note Repeat, Rhythm Gate, and Strum when they integrate with the existing Phrase ownership model.

---

## Historical roadmap convergence

The pre-0.9.9 Wave roadmap remains useful historical intent, but several items are now implemented or superseded by more precise production architecture.

| Previous item | Current disposition |
|---|---|
| "Missing musical object: Phrase" | **IMPLEMENTED FOUNDATION** in the 0.9.9 Phrase line |
| Wave 1 common status / interaction trust | **IMPLEMENTED OR MOVED TO MAINTENANCE**; no longer the primary product lane |
| Wave 1 Root Motion spine | **SUPERSEDED** by Stage15 tonal ownership plus PHRASE H1/W1/H2 progression/harmonic-clock architecture |
| Wave 1 Phrase cadence operators | **SUPERSEDED** by GF2-I3 motif identity + Phrase function/causal execution |
| Wave 2 Phrase foundation | **SUBSTANTIALLY IMPLEMENTED** by bounded Phrase execution, Song publication, PHRASE/PHRASE CORE product workflow |
| Wave 3 Phrase development | **PROMOTED TO CURRENT WORK** as 0.9.10-GF2 musical execution |
| MIDI lifecycle / external output | **MAINTENANCE / OUTPUT LANE**; important but no longer the canonical musical-development priority |

Historical stage documents remain evidence. They must not be interpreted as an active parallel roadmap when this file assigns a newer disposition.

---

## Deferred backlog

These ideas remain valid but must not interrupt the canonical order without a new evidence-based priority decision.

### Deeper Phrase tools

- named Phrase library beyond the current product workflow;
- search/tags/project-level Phrase catalog;
- non-destructive transform history;
- dedicated Phrase-internal editor beyond existing Pattern/Drum/Phrase Core tools;
- probability/articulation/ratchet/microtiming editing beyond current controls.

### Future performance generators

- chord mode and chord voicing;
- arpeggiator;
- Note Repeat;
- Rhythm Gate;
- Strum.

Each should produce, transform, or perform material through the existing ownership model rather than introduce another isolated keyboard/runtime system.

### External targets and protocol depth

- additional tested device profiles beyond SEQTRAK;
- per-target capability declarations;
- deeper custom per-track SMF routing;
- Program Change, CC, Pitch Bend, Aftertouch, carefully scoped SysEx;
- BLE-MIDI.

External protocol depth remains subordinate to the standalone musical instrument.

---

## Explicit non-goals

- No general UI framework rewrite.
- No second roadmap parallel to this file.
- No parallel MIDI dispatcher, timer task, or Song renderer.
- No conversion of GroovePuter into a SEQTRAK-only peripheral.
- No attempt to compete with a desktop DAW through feature count.
- No new synth, effect, generator, Genre, Recipe, or semantic owner merely to expand a list.
- No generic variation knob presented as a substitute for musical identity/development.
- No claim that an enum, profile weight, or random corpus occurrence proves musical execution.
- No architecture redesign solely because a cleaner abstraction is possible.

---

## Feature admission rule

A proposed feature enters the active release only when it does at least one of the following:

1. fixes a P0/P1 reliability, trust, ownership, or embedded-safety defect;
2. makes current state or effective control behavior materially more predictable;
3. creates causally provable musical direction above one bar without parallel architecture;
4. creates, captures, extracts, plays, shapes, develops, arranges, or safely outputs a Phrase;
5. makes GENRE / RECIPE / RHYTHM / FEEL / PHRASE / TEXTURE responsibilities more truthful and independently controllable;
6. removes an identified negative-capacity blocker for a common musician decision;
7. removes a repeated workflow step without adding another navigation system.

It must also be more important than unfinished items earlier in the canonical delivery order. Otherwise it remains deferred.

---

## Definition of done for roadmap stages

A stage is complete only when:

- its exact base/head SHA and ownership boundary are recorded;
- numerical/structural acceptance targets have a reproducible procedure;
- musical claims have capability-maturity evidence appropriate to the claim;
- causal musical claims include controlled comparison where practical;
- behavior is covered by host/source regression tests where practical;
- SDL and Cardputer ADV build gates pass where affected;
- SEQTRAK MIDI-only remains build-safe where affected;
- new structures document `sizeof`, instance count, total RAM, and flash delta;
- hot paths remain allocation bounded;
- MIDI/audio timing and active-note cleanup are validated where affected;
- direct hardware acceptance is documented for hardware-dependent behavior;
- user-visible status is honest about requested vs accepted vs executed state;
- old scenes load with safe defaults and unchanged legacy sound until new behavior is explicitly enabled, where applicable;
- `README.md`, `MANUAL.md`, and the relevant stage/contract document agree with shipped behavior and this plan;
- deferred ideas discovered during implementation are recorded here rather than entering active scope silently;
- no new semantic owner was introduced without explicit representational-gap evidence.

For 0.9.10 musical stages, an additional rule applies:

> A musical capability is not DONE merely because the output can occur. It is DONE only to the maturity level actually demonstrated, and product UI must not claim a stronger level than the evidence supports.
