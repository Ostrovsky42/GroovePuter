# GroovePuter 0.9.14 — D0-E
# SEMANTIC PROVENANCE SURVIVAL + CLAIM-LOCAL INVALIDATION + MINIMAL P0 BASIS

Status:

    D0-C  = GREEN
    D0-D  = GREEN / OWNERSHIP AUDIT COMPLETE
    D0-D1 = GREEN / CLAIM-STRENGTH REPAIR CLOSED
    D0-E  = RESEARCH CONTRACT

Base:

    feature/20260928-d0d1-claim-strength-repair
    5696567ca7d540c8683115aa5a5e20e2f9c48596

## 1. Primary architectural finding

The main 0.9.14 gap is not lack of musical knowledge inside generation.

It is survival of authoritative evidence across materialization:

    SEMANTIC GENERATION PLAN
        rich owner-derived evidence
            ↓
    MATERIALIZATION
            ↓
    PATTERN / MELODY
        notes and physical event data survive
        most semantic causes do not

Therefore the default 0.9.14 strategy MUST NOT be:

    raw note buffer
        → reverse infer lost semantic ownership

When provenance does not exist, unsupported semantic claims remain UNKNOWN.

## 2. Three strategies considered

### A. Full persistent provenance per Material

Advantage:
    semantic evidence can survive save/reload and page changes.

Problems for P0:
- changes Scene/page persistence contracts;
- adds per-slot storage and DRAM pressure;
- risks turning MaterialSlot into a second musical owner;
- demands invalidation/Undo/persistence semantics before P0 proves the idea.

Decision:
    NOT SELECTED FOR P0.

### B. Pure transient Development basis

Advantage:
    smallest lifecycle change.

Problem:
    if generation evidence is not captured before PreparedPhraseExecution dies,
    the transient session starts from already-semantic-poor Pattern/Melody.

Decision:
    INSUFFICIENT BY ITSELF.

### C. Reverse analysis of arbitrary Material

Examples:
    infer harmonic root from notes
    infer original rhythm identity from onset masks
    infer phrase role from one current buffer

D0-D established that these are not restorations of lost provenance. They would
create a new analytical layer with different epistemic authority.

Decision:
    REJECTED AS DEFAULT P0 STRATEGY.
    Arbitrary material keeps unsupported claims UNKNOWN.

## 3. Selected P0 strategy — hybrid

D0-E selects a bounded hybrid for the first production vertical:

    OWNER-DERIVED GENERATED ORIGIN
            +
    CLAIM-LOCAL CURRENT APPLICABILITY
            +
    TRANSIENT DEVELOPMENT SESSION BASIS

Properties:

1. Origin provenance is immutable historical evidence.
2. Current applicability is mutable and claim-local.
3. Material remains the sole owner of musical payload/publication.
4. The provenance sidecar is evidence, not another Material representation.
5. P0 is session-scoped and does not require cold-boot persistence.
6. Arbitrary material without provenance remains supported only where runtime
   observables are sufficient.

## 4. Origin facts vs current applicability

This separation is normative.

Example:

    generated origin:
        tonal root = C
        progression source = X
        bass rhythm plan = Y

User changes one pitch.

The system MUST NOT say:

    origin root was deleted

and MUST NOT say:

    current tonal preservation failed

without an authoritative evaluator.

Instead:

    ORIGIN ROOT = still known historical fact
    CURRENT TONAL APPLICABILITY = UNKNOWN

Likewise a moved onset does not erase which BassRhythmPlan generated the source;
it invalidates the claim that CURRENT is still proven by that plan.

## 5. Invalidation is not failure

Claim invalidation means:

    evidence no longer safely applies to CURRENT

and therefore normally transitions to:

    UNKNOWN

not:

    FAIL

FAIL requires an owner that can authoritatively evaluate the claim and prove
the invariant is violated.

## 6. Existing extraction seam

`PreparedPhraseExecution` remains available through P1R PREPARE and COMMIT.

It already carries common origin evidence including:

- phraseGenerationIdentity;
- realization GenerationContext;
- selected rhythm archetype/composition;
- realization level and structural density intent;
- rootPitchClass and scaleTypeValue;
- ChordProgressionSource;
- harmonic clock/timeline;
- phrase generation programme;
- phrase coordinates.

`materializePreparedPhraseBar()` internally also resolves compact per-bar owners
such as BassRhythmPlan.

Current `GeneratedPhraseP1R::materializeOneBar()` keeps only the materialization
status, so the resolved BassRhythmPlan is not exported after the call.

Therefore the smallest future production seam is:

    expose/capture compact owner-derived per-bar witness during COMMIT

not:

    analyze the committed Pattern afterwards.

D0-E does NOT implement that seam.

## 7. Candidate P0 material scope

The first vertical is deliberately narrow:

    GENERATED P1R
    SYNTH A
    SESSION / RESIDENT DEVELOPMENT CONTEXT

Rationale:

- Synth A is currently materialized from the authoritative BassRhythmPlan;
- BassRhythmPlan is <= 8 bytes;
- root/scale/progression provenance already exists;
- harmonic WHEN is available as a compact HarmonicRhythmPlan <= 8 bytes;
- this vertical can prove provenance survival without solving Synth B's
  Chord/Melodic/Hybrid role ambiguity;
- it avoids inventing a generic arbitrary-Melody analyzer.

This is a proof vertical, not a claim that Development is ultimately bass-only.

## 8. Candidate generated-origin basis

The following is a research shape, not a frozen production struct.

Common per generated phrase:

    phraseGenerationIdentity
    realization GenerationContext
    rhythmArchetypeId
    structuralDensityTarget
    RealizationLevel
    effective phrase bars
    generation trajectory id
    rootPitchClass
    scaleTypeValue
    ChordProgressionSource

Per materialized bar:

    MaterialReference
    origin MaterialVersionToken
    phraseBarOrdinal
    BassRhythmPlan
    HarmonicRhythmPlan

Important:

    generation trajectory id

is origin programme provenance only. It is NOT the D0-C semantic
`TrajectoryRole::Development/Return` verdict.

## 9. Why compact witnesses beat full generation-plan persistence

Existing budgets:

    PhraseRhythmIdentity       <= 256 bytes
    ChordProgressionSource     <= 16 bytes
    BassRhythmPlan             <= 8 bytes
    HarmonicRhythmPlan         <= 8 bytes
    MaterialVersionToken       = 8 bytes

A full PreparedPhraseExecution or full PhraseRhythmIdentity per Material slot
is unnecessary for P0.

Where the owner is deterministic, a replay key may re-enter the same owner and
derive the same semantic witness. This is authoritative derivation, not reverse
analysis of notes.

However replay across firmware/code changes is NOT assumed by D0-E. P0 is
session/release scoped.

## 10. Address-space / DRAM consequence

The project has up to:

    256 global pattern addresses
    × 2 synth voices
    = 512 synth Material addresses

while one resident Scene page contains:

    16 slots × 2 synth voices = 32 resident slots.

Therefore an always-resident global semantic table is disproportionately costly:

    16 bytes × 512 = 8 KiB
    32 bytes × 512 = 16 KiB

before richer evidence is included.

P0 should instead retain evidence only for the active/generated session or other
bounded working set.

## 11. Binding provenance to the correct Material

Every bar-local origin witness must be fail-closed bound to:

    MaterialReference
        address + stable MaterialId

and:

    origin MaterialVersionToken

`MaterialVersionToken` is exact byte/field state evidence only. It does NOT
become lineage or musical identity.

Use:

    reference
        → proves this sidecar names the intended Material identity

    version
        → proves whether CURRENT still equals a known material state

## 11.1 Pattern origin vs Runtime projection binding

A Pattern's MaterialVersionToken does not fully identify its projected Runtime
buffer.

`projectPatternToRuntimeEvents()` additionally consumes:

    synthIndex
    swingPercent
    swingEnabled
    gateLengthRatio

Therefore P0 must not assume:

    same PatternVersion
        → same RuntimeSynthEventBuffer

When Development acquires a Pattern as Working Melody, the session should bind
both levels:

    ORIGIN MATERIAL BINDING
        MaterialReference
        Pattern MaterialVersionToken

    SESSION RUNTIME BINDING
        versionForMelody(projected working buffer)

This does not require persistence.

The repository also already provides:

    projectPatternToRuntimeEventsWithSourceSteps()

which exposes the physical Pattern step that produced each projected onset.
This is authoritative projection metadata and may be retained transiently when
a later P0 evaluator needs logical-step ownership. It is not reverse analysis.

Changing swing/gate context may change physical start/duration while leaving
the logical Pattern topology intact. Such changes therefore invalidate only
claims that depend on physical pacing/lifetime, not the immutable origin
rhythm/archetype evidence by themselves.

## 12. Typed mutation vs stale-version fallback

The preferred path is:

    known mutation owner
        → classify mechanical edit effects
        → invalidate only dependent semantic claims
        → update exact version

If CURRENT's version changes without a known provenance transition:

    fail closed
        → all origin-dependent CURRENT applicability = UNKNOWN

Do NOT infer which semantic claims survived from a generic similarity score.

Exact return to the retained origin version may safely restore origin-backed
applicability because the musical bytes/fields are again exactly the materialized
origin state.

## 13. Mutation-effect classes

D0-E does not freeze UI commands as semantic operations.

Instead it uses mechanically observable edit effects:

    PITCH_CONTENT
    ONSET_TOPOLOGY
    DURATION
    ARTICULATION
    METRIC_TRANSFORM
    PHRASE_LENGTH
    FX_ONLY
    VELOCITY_ONLY
    UNKNOWN_MUTATION

This is deliberately below musical semantics.

## 14. Claim-local invalidation matrix

| Edit effect | Claims that become UNKNOWN | Claims not invalidated solely by this effect |
| --- | --- | --- |
| pitch content | tonal-context applicability; harmonic-source applicability; pitch contour | bass rhythm topology; bass↔drum timing; phrase extent |
| onset topology | bass rhythm topology; bass↔drum relation; harmonic attack timing | stored tonal origin; articulation lifetime if otherwise unchanged |
| duration | articulation/lifetime | onset topology; tonal origin; harmonic attack positions |
| articulation | articulation/lifetime/connection | onset topology; tonal origin |
| metric transform / rotate | rhythm topology relative to metric anchors; bass↔drum relation; harmonic timing | tonal WHAT origin |
| phrase length | phrase extent/formal scope | existing event-level pitch/onset evidence where still in range |
| FX only | FX-specific evidence only | P0 structural claims |
| velocity only | expression-specific evidence only | P0 structural claims |
| unknown/compound mutation | all origin-dependent current semantic claims | immutable origin facts only |

These are invalidation rules, not musical PASS/FAIL rules.

## 15. Mapping to actual edit owners

RuntimePhraseEdit already gives useful mechanical boundaries:

    transposeEvent       → pitch content
    resizeEventByGrid    → duration
    deleteEvent          → onset + pitch-content removal
    joinNextEvent        → onset removal + duration
    insertSnapped        → onset + pitch-content addition
    setLengthBars        → phrase length

PatternEdit provides:

    adjustNote/Octave
    clearStep
    accent
    slide
    FX
    rotate

Some Pattern commits use arbitrary lambdas such as Paste. These MUST use the
`UNKNOWN_MUTATION` fallback unless their mechanical effect is explicitly
classified at the mutation owner.

## 16. Command name is not enough

`adjustNote()` can cross the Pattern sentinel values for silence/TIE and may
therefore affect onset existence as well as pitch.

Consequently future invalidation should classify the actual mechanical
before/after effect at the mutation boundary, not blindly trust the UI command
name.

This is still deterministic mechanical diff, not musical reverse analysis.

## 17. Undo constraint

The canonical Undo payload is fixed at 1536 bytes and existing Tier-1 receipts
already approach that ceiling.

D0-E therefore rejects adding a large semantic snapshot to every Undo receipt.

A future implementation should prefer:

- a compact validity/version receipt where budget permits;
- exact-origin re-arming when bytes return to origin;
- otherwise fail-closed UNKNOWN after a restore whose previous applicability
  cannot be reconstructed safely.

D0-E tests report the current RuntimePhraseUndo payload/slack but do not change
the Undo contract.

## 18. Transient DevelopmentSession basis

P0 does NOT require a persistent lineage graph.

A transient session needs conceptually:

    LINEAGE_ANCHOR
    PREDECESSOR
    CURRENT
    explicit RETURN_TARGET
    generated-origin evidence reference
    current claim applicability

For the first P0, RETURN_TARGET may explicitly designate the existing lineage
anchor snapshot. It may alias that retained snapshot; it does not need another
full-size musical buffer merely to be semantically explicit.

Persistence of RETURN_TARGET across cold boot is NOT a P0 requirement.

## 19. P0 lifecycle scope

Required for the proof vertical:

- provenance survives semantic generation → materialization;
- provenance remains usable while the bounded development session is alive;
- known manual edits invalidate claims locally;
- unknown mutation paths fail closed;
- RETURN can name an explicit transient target.

Not required yet:

- cold-boot provenance restoration;
- persistent genealogy;
- every project slot carrying semantic metadata;
- arbitrary Material harmonic analysis;
- development continuing after provenance was lost.

After provenance is unavailable, D0-C capability/claims must honestly become
UNKNOWN/UNAVAILABLE.

## 20. Candidate P0 evidence set

For generated P1R Synth A, the repository can support these origin witnesses:

    BASS RHYTHM TOPOLOGY
        BassRhythmPlan.onsets / continuations

    BASS↔KICK RELATION ORIGIN
        BassRhythmPlan.kickRelationship

    TONAL CONTEXT ORIGIN
        rootPitchClass / scaleTypeValue

    HARMONIC WHAT
        ChordProgressionSource

    HARMONIC WHEN
        HarmonicRhythmPlan per bar

    PHRASE COORDINATE
        phraseGenerationIdentity / phraseBarOrdinal / phrase length

    EXACT MATERIAL BINDING
        MaterialReference + MaterialVersionToken

    ARTICULATION/LIFETIME OBSERVABLES
        current Runtime/Pattern representation where present

D0-E does NOT declare which of these are REQUIRED for LINEAGE.

That preservation contract belongs to the next semantic reconciliation
checkpoint.

## 21. Representation gaps remaining after D0-E

Still absent:

- complete Lineage evaluator;
- complete semantic Trajectory evaluator;
- persistent RETURN_TARGET;
- complete positive Genre certification;
- arbitrary-Material harmonic owner;
- voice-leading owner;
- generic DnB hierarchy owner;
- Funk pocket owner;
- nontrivial cross-bar melodic lifetime producer.

D0-E does not attempt to repair them.

## 22. Source firewall

D0-E must not introduce production:

    MaterialSemanticProvenance
    LineageEvaluator
    DevelopmentCoordinator
    IdeaFingerprint
    DevelopmentDistance
    reverse harmonic inference

and must not modify:

    MaterialSlot ownership
    WorkingMaterialStorage ownership
    persistence schema
    generator algorithms

## 23. D0-E decision

The repository supports a credible P0 without reverse analysis if 0.9.14
preserves a small amount of owner-derived evidence at the materialization seam.

Selected architectural direction:

    COMPACT GENERATED ORIGIN WITNESS
            +
    CLAIM-LOCAL APPLICABILITY
            +
    TRANSIENT DEVELOPMENT SESSION

rather than:

    FULL PERSISTENT PROVENANCE PER SLOT

or:

    REVERSE ANALYSIS OF ARBITRARY MATERIAL.

## 24. Acceptance gate

D0-E is GREEN when:

1. no production source differs from the D0-D1 base;
2. source evidence confirms rich PREPARE provenance and poor committed Material
   provenance;
3. a bounded test-local generated-origin basis fits comfortably as session data;
4. claim-local invalidation handles pitch/onset/duration/articulation/metric/
   length/expression/unknown mutation effects without global invalidation;
5. invalidation produces UNKNOWN, not FAIL;
6. exact Material version is used only as a stale-binding guard;
7. arbitrary Material is not reverse-analysed to manufacture authority;
8. P0 scope and non-goals are explicit;
9. D0-C/D0-D1 remain GREEN;
10. no persistent lineage graph or new Material owner is introduced.

After D0-E GREEN, the next checkpoint may define the actual P0 preservation
contract and the smallest production provenance extraction seam.