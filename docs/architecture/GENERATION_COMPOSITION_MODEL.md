# Generation Composition Model

**Status:** normative architecture gate for post-Stage-6 generation work  
**Audit base:** `agent/20260809-01-groove-vocabulary-stage6-bar-evolution` @ `ee99c5cda41a8e55ea6eba7a1105249fffe0621b`  
**Scope:** define how Genre, Groove Vocabulary, phrase identity, future VoiceRole/Pitch/Motif layers, Feel, bounded Mood bias, articulation, physical binding and Scene materialization are allowed to compose.  
**Non-goal:** this document does not add a Mood subsystem, melodic generator, Texture subsystem, new Genre IDs, Scene fields or production routing.

---

## 1. Core rule

Two rules are normative for all new generation code:

> **Independent responsibility != independent randomization.**

and:

> **A downstream layer may choose only inside the legal space left by upstream decisions.**

The architecture exists to increase the number of distinct legal musical solutions without increasing global entropy until phrase identity disappears.

The old mental model:

```text
Genre -> generator -> events
```

is replaced by a composition pipeline:

```text
User / Song intent
        ↓
Genre constraints
        ↓
RhythmArchetype selection
        ↓
PhraseRhythmIdentity
        ↓
Bar / phrase development
        ↓
VoiceRole assignment
        ↓
Pitch / Motif strategy
        ↓
Motif / harmonic development
        ↓
Feel interpretation
        ↓
Articulation
        ↓
Physical synth/drum binding
        ↓
Scene materialization
```

`Mood` is not another event generator. It is a bounded bias that may influence explicitly permitted dimensions inside this pipeline.

The currently removed 0.9 `TEXTURE` UI/runtime model is **not** restored by this document. A future sound-character/texture renderer, if one is ever introduced, must remain downstream of musical event identity and may affect sound/rendering only.

---

## 2. Existing foundations this model preserves

The current Groove Vocabulary stack already establishes useful boundaries:

- `RhythmArchetype` owns rhythmic grammar, lane relationships, protected space, density and mutation policy;
- `RhythmPhraseRealizer` creates fixed-capacity `RhythmPhrasePlan` values and `PhraseRhythmIdentity`;
- `BarEvolution` is a transient planning core and owns no Scene/page/bank/Song/PhraseCore destination;
- Stage 4 materialization requires explicit caller-owned bindings instead of assuming `Synth A == Bass`;
- Stage 5 keeps production migration explicit and conservative;
- Scene remains the source of truth after generated material is committed;
- Atlas direct runtime application still exists as a legacy/exact-preset path, but the target architecture treats Atlas primarily as offline evidence/compiler input.

This document composes those boundaries; it does not replace them with a new central generator object.

---

## 3. Do not create a musical God object

Conceptually, a phrase can have several identities:

```text
Phrase
  ├─ RhythmIdentity
  ├─ MotifIdentity
  ├─ HarmonicIdentity
  └─ DevelopmentIntent
```

These are related, but they MUST NOT be collapsed prematurely into one large mutable `PhraseIdentity` struct.

Current rule:

```text
PhraseRhythmIdentity stays independent and stable.
```

Future stages may introduce `MotifIdentity` or harmonic identity as separate fixed-capacity values. A common aggregate is allowed only if a real cross-layer API requires it and ownership remains explicit.

---

# 4. Authority Matrix

The verbs below are normative:

- **OWNS** — may create or directly mutate this dimension before materialization;
- **CONSTRAINS** — may narrow legal choices but does not choose concrete events by itself;
- **BIASES** — may change weights inside an already legal set;
- **SUGGESTS** — advisory preference that may be ignored when stronger rules conflict;
- **FORBIDDEN** — cross-layer mutation that must be rejected by architecture/tests.

| Layer | OWNS | May CONSTRAIN / BIAS | FORBIDDEN |
|---|---|---|---|
| User / Song intent | requested operation, selected source/destination, explicit high-level intent | Genre, phrase length, requested transformation | hidden replacement of already materialized user edits |
| Genre | style corridor, compatible archetype/role/strategy sets, weighting | BPM corridor, register/density/timing compatibility | emitting concrete notes/hits; choosing synth TYPE as a side effect |
| RhythmArchetype | rhythmic legal topology: lanes, anchors, protected space, relationships, density/mutation contracts | compatible Feel/role strategies | pitch choice, synth TYPE, Scene destination |
| PhraseRhythmIdentity | stable selected rhythmic core for a phrase | downstream rhythm-preserving transforms | pitch, timbre, physical binding, storage |
| BarEvolution / rhythm development | legal bar-function development of an existing rhythm identity | add/drop/repeat budgets allowed by archetype | page/bank/Song/PhraseCore ownership; illegal anchor removal |
| VoiceRole | semantic musical function and role-specific constraints | compatible pitch/motif/articulation strategies | assuming a physical Synth A/B destination; inventing rhythm onset topology |
| Pitch / Motif strategy | pitch classes, intervals, contour, motif event content on supplied legal onset/gate sites | harmonic/register/motif constraints | creating new rhythmic onset sites unless an upstream rhythm contract explicitly permits it |
| Motif / phrase development | reuse/answer/sequence/compress/expand decisions over motif identity | phrase development intensity | moving drum/backbeat structure; synth-engine choice |
| Feel | timing interpretation, velocity/expression corridors, swing/push/pull on eligible events | role-specific expression | pitch changes; required-anchor removal; motif rewrite |
| Mood (future bounded bias) | no event topology ownership | legal register, pitch-class budget, repetition/novelty, density, tension, energy weights | direct masks/onsets, synth TYPE, bypassing Genre/archetype/identity constraints |
| Articulation | accent/slide/gate/expression semantics allowed by role/engine capability | capability-aware rendering choices | pitch grammar, phrase topology |
| Physical binder | maps semantic roles/articulation to explicit hardware/engine destinations | capability fallback | inventing notes/onsets; changing motif/rhythm identity |
| Scene materializer | transactional commit to explicit caller-owned destinations | serialization-compatible projection | re-running upstream generation implicitly during redraw/load |
| Scene after materialization | persisted/user-edited concrete truth | none implicitly | being silently overwritten because Genre/Mood/UI state changed |
| Future Texture/sound character | sound/rendering character only | compatible timbre/FX weights | rhythm, motif, pitch or phrase identity mutation |

### Hard ownership examples

These are architecture failures:

```text
PitchStrategy created onset        FAIL
Feel changed pitch                 FAIL
Mood moved backbeat                FAIL
Texture changed motif              FAIL
Binder invented onset              FAIL
Genre directly emitted event       FAIL
UI redraw regenerated material     FAIL
Scene load re-projected Genre TYPE FAIL
```

---

# 5. Decision ordering

Decision order and precedence are related but not identical.

The normal creation path is:

```text
1. Resolve explicit User / Song intent.
2. Resolve Genre corridor.
3. Select a compatible RhythmArchetype.
4. Establish/reuse PhraseRhythmIdentity.
5. Select/apply a legal bar trajectory or phrase-development intent.
6. Assign semantic VoiceRole(s).
7. Select compatible Pitch/Motif strategy from upstream decisions.
8. Establish/reuse MotifIdentity when that subsystem exists.
9. Develop motif/harmony while preserving upstream rhythm identity.
10. Apply Feel interpretation to eligible event expression.
11. Resolve articulation semantics.
12. Bind semantic roles to explicit physical engines/tracks.
13. Materialize transactionally into explicit Scene destinations.
14. Hand authority to the materialized Scene/user state.
```

No downstream step may widen an upstream legal set.

Example:

```text
Genre permits archetypes {A, B, C}
selected archetype B permits bass relations {Respond, FillGaps}
selected VoiceRole permits pitch strategies {Pedal, ChordTone, Stepwise}
Mood::Dark biases {Pedal, ChordTone}
RNG selects ChordTone
```

Mood may change weights inside `{Pedal, ChordTone, Stepwise}`. It may not add `AcidContour` if upstream compatibility excluded it.

---

# 6. Constraint precedence

When constraints conflict during generation, resolve in this order:

```text
0. explicit user-owned materialized Scene state (after commit)
1. hard safety / representation / musical invariants
2. stable phrase identities already established or explicitly reused
3. selected archetype grammar and protected-space contracts
4. explicit User / Song generation intent
5. Genre corridor and compatibility constraints
6. semantic VoiceRole constraints
7. bounded Mood biases
8. soft/weighted preferences
9. deterministic random choice
```

A lower-precedence layer may never repair a conflict by mutating a higher-precedence decision outside its authority.

If the legal set becomes empty, return an explicit failure/fallback status at the current layer. Do not silently broaden the search space.

---

# 7. Compatibility model

Do **not** build one global NxN compatibility matrix.

Only adjacent or directly dependent layers define compatibility:

```text
Genre <-> RhythmArchetype
RhythmArchetype <-> BarTrajectory / Feel compatibility
Genre <-> VoiceRole set
VoiceRole <-> PitchStrategy
VoiceRole <-> Articulation
PitchStrategy <-> Harmonic strategy
Feel <-> RhythmArchetype / Role eligibility
Articulation <-> Engine capability
VoiceRole <-> Physical binding
```

Each edge should be representable as a small allow-list, weighted list, capability mask or bounded predicate.

### Compatibility rules

1. Compatibility narrows choices; it does not materialize events.
2. A compatibility table must have one clear owner.
3. Missing compatibility is not equivalent to universal compatibility.
4. Fallback must be explicit and deterministic.
5. Compatibility data must be testable without Scene/UI state.
6. Genre aliases or visible UI labels must not become hidden physical binding rules.

---

# 8. Composition RNG contract

Separate RNG domains are required, but domain separation alone is insufficient.

Normative rule:

> **A downstream seed must be derived from the stable identifiers of upstream decisions that semantically affect it.**

It MUST NOT derive from mutable concrete event bytes when a stable semantic identifier exists, and MUST NOT consume a shared global stream whose call order couples unrelated layers.

Conceptual derivation:

```text
root GenerationContext
  + domain
  + selected Genre/recipe identity when relevant
  + selected RhythmArchetypeId
  + PhraseRhythmIdentity discriminator / phrase coordinate
  + selected VoiceRole
  + selected strategy id
  + phrase/development coordinate
  -> local deterministic RNG
```

Examples:

```text
Rhythm archetype selection seed
    depends on Genre + generation context

BarEvolution seed
    depends on selected RhythmArchetype + trajectory/phrase coordinate

Bass pitch seed
    depends on PhraseRhythmIdentity + Bass VoiceRole + PitchStrategy

Motif development seed
    depends on MotifIdentity + development coordinate

Feel seed
    depends on already chosen event identity + Feel profile
```

Changing Feel must not reshuffle pitch. Changing synth engine binding must not regenerate rhythm. Changing Mood may alter only domains explicitly declared Mood-sensitive.

### Required RNG regression shape

For each new domain, test:

```text
same upstream decisions + same seed -> same result
change unrelated downstream dimension -> upstream result unchanged
change one allowed upstream decision -> dependent domain may change
call-order permutation -> independent domains unchanged
```

---

# 9. Mood contract

Mood is intentionally deferred as an implementation subsystem, but its authority is fixed now.

Mood is a **bounded bias profile**, not a generator.

Allowed future Mood dimensions include:

```text
pitch/register bias
pitch-class budget bias
motif reuse / novelty bias
ornament-density bias
harmonic-tension bias
energy/development bias
```

Example legal `Dark` influence:

```text
register center     -1 relative step
authorized pitch-class budget lower
motif reuse weight  +20%
ornament weight     -15%
harmonic tension    +10%
```

These are weights/corridor shifts, not direct event writes.

Forbidden Mood behavior:

```text
kickMask = ...
move backbeat
insert bass onset
select TB303 TYPE
rewrite existing Scene events
```

Mood changes after materialization affect only a future explicit generation operation unless a separately designed non-destructive live-performance parameter is involved.

---

# 10. Scene and user authority

Generation ends at materialization.

Before materialization:

```text
generation plans are transient candidates
```

After successful materialization:

```text
Scene concrete patterns + persisted patches + user edits are authoritative
```

Therefore:

- UI redraw must never regenerate material;
- changing Genre metadata must not silently rewrite events or synth TYPE;
- changing future Mood must not silently rewrite events;
- Scene load must restore saved patches/events without Genre projection;
- repeated generation requires an explicit user/Song generation action;
- a generator may return provenance/diagnostics, but provenance is not authority over later user edits.

This extends the existing rule that restored Scene synth state wins over Genre metadata.

---

# 11. Cross-layer leakage tests

Every new generation layer must add negative ownership tests, not only success tests.

Minimum source/runtime assertions:

```text
Genre code contains no direct pattern-event placement in the new path.
Pitch/Motif code cannot increase the supplied onset set.
Feel cannot alter note/pitch fields or structural onset masks.
Mood code cannot access Scene mutation or physical synth TYPE APIs.
Articulation cannot change pitch/onset topology.
Physical binder cannot create events absent from the semantic plan.
BarEvolution cannot choose Scene/page/bank/Song/PhraseCore destinations.
Materializer requires explicit destinations/bindings from its caller.
Scene load/redraw does not call generation.
```

Where a layer intentionally changes topology, the permission must be explicit in the upstream contract and named in the API. No generic `mutate()` escape hatch is allowed.

---

# 12. Atlas relationship

Atlas is evidence for the legal spaces and weights used by this model; it is not a privileged runtime owner.

Atlas-derived knowledge may populate:

```text
Genre <-> archetype weights
RhythmArchetype lane/relationship candidates
BarTrajectory candidates
VoiceRole relationship tendencies
Pitch/Motif strategy weights
Feel compatibility corridors
Mood bias priors
validation/fingerprint corridors
```

Atlas may not bypass the composition pipeline by writing arbitrary runtime events except through the explicitly preserved exact-preset compatibility path.

The extraction/coverage contract is defined separately in:

```text
docs/architecture/ATLAS_VOCABULARY_EXTRACTION.md
```

---

# 13. Stage boundary and revised roadmap

This architecture gate changes the post-Stage-6 order.

```text
Stage 6 core
    BarEvolution + existing Groove Vocabulary foundation

Stage 6A
    Generation Composition Model
    authority / decision order / precedence / compatibility / RNG / leakage tests

Stage 6B
    Atlas Orthogonal Extraction Model + coverage audit

Stage 6.1
    harden BarEvolution review findings before any production multi-bar caller

Stage 7
    Groove Vocabulary expansion from curated Atlas-derived generalized grammars
    target: approximately 30–40 first, then evidence-gated growth toward 50–70

Stage 8
    VoiceRole runtime + Bass Generator v2
    separate bass rhythm relationship and bass pitch/contour vocabularies

Stage 9
    Phrase / Motif Vocabulary
    motif memory, contour, harmonic targets, 2–4 bar development, anti-ringtone tests

Stage 10
    Mood / Vibe bounded bias profiles

Stage 11
    weak-genre rehabilitation + new/hybrid genres primarily through data/compatibility
```

Stage numbering after 6 is now governed by this document rather than the older provisional roadmap.

---

# 14. Acceptance gate for future implementation PRs

A future generation PR is architecture-compliant only if all applicable answers are explicit:

```text
Who owns the new decision?
Which upstream decisions constrain it?
Which downstream layers consume it?
What is the stable RNG domain and dependency key?
Which compatibility edge authorizes it?
What happens when the legal set is empty?
Can it mutate an existing Phrase identity?
Can it mutate a materialized Scene? (normally NO)
Which cross-layer leakage test proves the boundary?
Which Atlas evidence supports the weights/grammar, if Atlas-derived?
```

If these cannot be answered without saying "the generator decides", the responsibility is still too broad.

---

# 15. Completion criterion for Stage 6A

Stage 6A is complete when this document is accepted as normative and later code reviews can reject cross-layer ownership violations mechanically.

No production feature is required for Stage 6A itself.
