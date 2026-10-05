# GroovePuter 0.9.14 — D0-F
# P0 SYNTH A PRESERVATION CONTRACT + MINIMAL PROVENANCE SEAM

Status:

    D0-E = GREEN
    D0-F = CONTRACT / SEAM RECONCILIATION

Authoritative base:

    feature/20260928-d0e-provenance-survival
    919d6db1b1951f94813958459ca68767268cb5ef

## 1. Scope

D0-F answers two questions only:

1. Which preservation claims are sufficient for the first generated Synth A
   P0 to prove idea-lineage CONTINUES?

2. What is the smallest production seam required to move existing
   owner-derived evidence across materialization and into Development session
   context without reverse analysis?

D0-F does not implement DevelopmentCoordinator, general lineage classification,
trajectory semantics, persistent genealogy, arbitrary-Material analysis, or
complete Genre certification.

## 2. P0 material scope

The first proof vertical is deliberately:

    P1R-generated
    Synth A
    one selected generated bar
    one-bar Development operation
    session-local provenance

P1R Synth A is a legitimate bass-domain vertical: strong-rhythm
materialization obtains a BassRhythmResult, consumes BassRhythmPlan onsets and
continuations, applies bass tonal behavior, and publishes that result as
Synth A.

Synth B remains out of this P0 because its semantic role may be Chord,
Melodic or ChordWithMelodicFill.

## 3. One-sided lineage theorem

P0 MUST NOT implement NEW_IDEA.

It establishes only a sufficient positive proof:

    ALL REQUIRED CLAIMS PASS
        → LINEAGE = CONTINUES

Any REQUIRED claim that is:

    FAIL
    UNKNOWN
    UNAVAILABLE

produces:

    LINEAGE = UNKNOWN

not:

    NEW_IDEA.

This asymmetry is normative.

Failure of one preservation projection is evidence that the sufficient P0
continuity proof no longer applies. It is not proof that musical identity is
gone.

## 4. P0 binding preconditions

Before musical preservation is evaluated, the source must be proven to be the
same generated Material origin.

Required technical preconditions:

    owner-derived generated Synth A origin exists
    exact Material binding is current
    selected bar is the bar named by that origin

Binding is not itself a musical preservation claim.

## 5. REQUIRED preservation claims

### P0-R1 — BAR EXTENT PRESERVATION

Meaning:

    candidate remains inside the same one-bar extent represented by the
    generated source bar.

Current P0 mechanical proof:

    candidate.lengthTicks == source.lengthTicks

This deliberately rejects GROW / 2-4-8 bar development from the first P0.

------------------------------------------------------------

### P0-R2 — BASS ONSET TOPOLOGY PRESERVATION

Meaning:

    the identity-bearing generated bass onset skeleton remains in the same
    physical event positions for this conservative first P0.

Owner provenance:

    BassRhythmPlan

Current candidate-side proof is intentionally stronger than the ultimate
musical requirement:

    same event count
    same corresponding startTick values

This permits a safe proof without reverse-inferring PhraseRhythmIdentity from
the candidate.

Future domain work may admit controlled DISPLACE/THIN while preserving lineage.
D0-F does not.

------------------------------------------------------------

### P0-R3 — TONAL PITCH-CLASS PROJECTION PRESERVATION

Meaning:

    at each preserved onset, the candidate retains the source pitch class.

Current P0 proof:

    candidate.note % 12 == source.note % 12
    for every corresponding preserved onset

Why this is sufficient in this narrow scope:

- the source was authoritatively P1R-generated under an explicit tonal frame;
- root/scale/progression provenance is retained from the generation owner;
- the candidate keeps the same pitch classes at the same attacks;
- therefore D0-F need not reverse-infer a new harmonic root from raw notes.

This allows octave/register movement while keeping the original tonal origin
applicable.

## 6. Derived applicability, not extra REQUIRED claims

If P0-R1/R2/R3 all PASS and owner provenance is bound, then the following
origin evidence remains usable for downstream consumers:

    rootPitchClass / scaleTypeValue
    ChordProgressionSource
    per-bar HarmonicRhythmPlan
    BassRhythmPlan origin

D0-F does NOT claim this is a complete H0 harmonic-equivalence proof.

In particular, lifetime across chord boundaries and voice leading are not
certified by this P0.

## 7. OPTIONAL / NOT_REQUIRED for lineage P0

These may matter to operation conformance, genre or later Development stages,
but they are not required for this first CONTINUES proof:

    bass contour                     OPTIONAL
    bass↔kick named relationship      OPTIONAL
    exact duration/lifetime           NOT_REQUIRED
    exact articulation                NOT_REQUIRED
    exact accent/slide/ghost          NOT_REQUIRED
    exact register                    NOT_REQUIRED
    velocity/probability              NOT_REQUIRED
    FX                                NOT_REQUIRED

This is what allows HOLD, CONNECT and octave revoicing to remain possible
continuations of the same generated idea.

## 8. Operation consequences

The contract is based on observed candidate facts, never operation name.

Expected first-P0 outcomes:

    exact repeat
        REQUIRED claims PASS
        LINEAGE CONTINUES
        STATE RELATION EXACT

    REVOICE / octave MOVE
        may change register
        if onset topology and pitch classes survive:
        LINEAGE CONTINUES
        STATE RELATION VARIATION

    HOLD
        may change lifetime
        REQUIRED claims may still PASS
        LINEAGE CONTINUES

    CONNECT
        may change slide/connection
        REQUIRED claims may still PASS
        LINEAGE CONTINUES

    DISPLACE
        onset topology differs
        sufficient P0 proof is lost
        LINEAGE UNKNOWN

    THIN
        onset topology differs
        LINEAGE UNKNOWN

    EXTEND
        operation capability is still unresolved independently;
        lineage preservation must not be used to certify the operation.

A REVOICE command with forceDisplaceTheOne is the canonical adversarial case:
the command name says REVOICE but the actual onset mutation prevents the P0
continuity proof.

## 9. STATE RELATION in P0

Once lineage CONTINUES has been proved:

    candidate byte-exact to reference
        → EXACT

    candidate non-exact
        → VARIATION

If continuity is not proved:

    relation remains UNKNOWN

P0 does not manufacture NEW_IDEA from failed preservation.

## 10. Evidence that must survive materialization

Common generated-phrase origin:

    phraseGenerationIdentity
    barCount
    rootPitchClass
    scaleTypeValue
    ChordProgressionSource

Per generated Synth A bar:

    exact Material binding
    phraseBarOrdinal
    BassRhythmPlan
    HarmonicRhythmPlan

The D0-E 296-byte research basis remains a valid upper bound. D0-F may become
smaller because replay keys and unrelated generation-program fields are not
required once compact owner witnesses are captured directly.

Generation trajectory id is not required for P0 lineage and must not become a
D0-C TrajectoryRole verdict.

## 11. Existing evidence already accessible

Before COMMIT, PreparedPhraseExecution already exposes:

    phraseGenerationIdentity
    rootPitchClass
    scaleTypeValue
    ChordProgressionSource
    PhraseHarmonicClockProjection

Therefore harmonic WHAT and harmonic WHEN need no new analysis layer.

## 12. Missing evidence-export seam

Strong-rhythm materialization computes:

    BassRhythmResult bass
    bass.plan : BassRhythmPlan

but StrongRhythmMigrationResult currently exports only:

    bassRhythmId

and GeneratedPhraseP1R::materializeOneBar() reduces the full result to bool.

Minimum future evidence export:

    StrongRhythmMigrationResult
        + resolved BassRhythmPlan

then:

    materializeOneBar(...)
        + bounded out evidence / result forwarding

No reverse reconstruction from the committed SynthPattern is required.

## 13. Newly discovered prerequisite — generated Material identity

D0-F found a lifecycle blocker earlier than provenance evaluation.

PreparationBasis::valid() requires:

    valid MaterialId
    + exact MaterialVersionToken

and developWorkingMaterial() fails closed when captureCurrentPreparationBasis()
cannot produce that valid basis.

However GeneratedPhraseSong::applyPreparedPersistent() currently writes:

    scene.synthABanks[...] = scratch.synthA

without assigning generated Synth A a MaterialId or updating its
MaterialSlotDescriptor.

Therefore a freshly generated Synth A bar cannot honestly enter the existing
Development NEXT lifecycle merely because semantic provenance exists.

D0-F explicitly rejects weakening PreparationBasis to address+version.

## 13.1 Descriptor-aware free-slot prerequisite

The current Phrase allocator checks:

    physical Pattern/Drum emptiness
    + Song references

but does not consult MaterialSlotDescriptor.

Under unified Song slots this matters: an accepted Melody may have no physical
SynthPattern payload while its slot descriptor still says Melody. Such a slot
is not semantically free merely because its Pattern bytes are empty.

Therefore identity closure must begin by making generated-Phrase safe-slot
selection descriptor-aware.

For generated Synth targets, the future allocator must require the canonical
free descriptor for BOTH synth voices in every destination slot:

    kind = Pattern
    id   = 0

This is stricter than merely "not Melody". It proves there is no pre-existing
Material identity to preserve when Phrase generation writes Synth A and Synth B.

This is prerequisite A0. It precedes assigning any new MaterialId.

Because A0 proves the previous descriptors are canonical defaults, generated
Phrase Undo does not need to grow by storing descriptor snapshots. The future
identity-closure patch may deterministically restore the affected descriptors
to their canonical default alongside the already-existing empty Pattern restore.

## 14. Minimum identity closure for the production seam

The future implementation should use the existing Material identity owner:

    PatternPagingService::allocateMaterialId()

not create a transient second ID namespace.

Because a generated Phrase can contain up to eight bars, the practical
implementation should reserve a bounded batch of MaterialIds with one durable
high-water update rather than perform up to eight independent SD metadata
transactions.

For the strict Synth A P0, only generated Synth A requires identity closure.

A later checkpoint may generalize the same closure to Synth B.

Allocated IDs may be consumed even if the subsequent COMMIT loses its target
or UndoOwner refuses publication. Monotonic opaque identity permits such gaps
and must never reuse them.

ID reservation must not be moved into PREPARE merely to avoid those gaps:
the canonical allocator mutates durable project high-water state, whereas
PREPARE must remain private and non-publishing.

## 15. Material-slot ownership rule

Identity closure is not semantic provenance.

It only makes the existing Material lifecycle able to name generated CURRENT.

The generated commit must establish:

    descriptor.kind = Pattern
    descriptor.id   = reserved canonical MaterialId
    exact Pattern bytes

inside the existing generation/Undo publication boundary.

A0 proves the pre-generation descriptor state is canonical default/free.
Therefore Undo may restore the generated Synth A descriptor deterministically
to:

    MaterialSlotDescriptor{}

without carrying another per-bar descriptor snapshot in the Undo payload.

Do not mint an ID inside the D0-F semantic sidecar.

## 16. Minimal generated-origin sidecar

After identity closure, use a separate bounded session evidence object rather
than expanding WorkingMaterialStorage or MaterialSlot.

Conceptual shape:

    GeneratedSynthAOrigin
        common:
            phraseGenerationIdentity
            barCount
            rootPitchClass
            scaleTypeValue
            ChordProgressionSource

        bars[<=8]:
            MaterialReference
            origin PatternVersion
            phraseBarOrdinal
            BassRhythmPlan
            HarmonicRhythmPlan

This object owns no Pattern/Melody payload.

It is immutable origin evidence.

## 17. Publication ordering

Correct transaction order:

    PREPARE
        resolve P1R semantic evidence
        preflight materialization
        build unpublished origin candidate
        NO durable MaterialId allocation

    PRE-COMMIT ID RESERVATION
        revalidate exact target
        reserve bounded canonical MaterialIds through the existing
        PatternPagingService identity owner
        perform the durable high-water update OUTSIDE the audio guard

    COMMIT
        materialize each bar
        capture resolved BassRhythmPlan
        write generated Patterns
        write Material descriptors/ids
        compute exact PatternVersion

    UndoOwner::commitPrepared SUCCESS
        ↓
    publish GeneratedSynthAOrigin session sidecar

If COMMIT fails:

    sidecar MUST NOT publish.

If a Legacy route is generated:

    P1R Synth A provenance sidecar MUST NOT claim support.

## 18. Entering Development session

On first D against generated Synth A:

1. captureCurrentPreparationBasis() resolves canonical MaterialReference +
   PatternVersion;

2. look up matching GeneratedSynthAOrigin bar;

3. privately project the Pattern to RuntimeSynthEventBuffer using the existing
   projection settings;

4. record versionForMelody(projected source) as session runtime binding;

5. copy only the selected bar's origin evidence into a transient
   Development-session basis;

6. evaluate candidate preservation relative to that source/basis.

PatternVersion and RuntimeVersion answer different questions and must not be
collapsed.

## 19. Manual edits after session start

D0-E's claim-local invalidation remains normative.

The D0-F preservation evaluator is not allowed to assume provenance continues
after an untyped edit.

Known typed edits may invalidate only dependent claims.

Unknown mutation:

    current applicability → UNKNOWN

never:

    NEW_IDEA.

## 20. RETURN and TRAJECTORY remain out of scope

Generated origin provenance is not a RETURN_TARGET.

P0 continuity proof does not establish DEVELOPMENT.

D0-F must not reinterpret:

    phraseGenerationIdentity
    PhraseEvolutionLaw
    BarFunction

as D0-C trajectory verdicts.

## 21. Production prerequisites identified by D0-F

PREREQUISITE A0 — DESCRIPTOR-AWARE PHRASE SLOT SAFETY

    generated Phrase allocation must not treat an existing Melody/identified
    Material slot as free merely because physical Pattern bytes are empty.

PREREQUISITE A1 — MATERIAL IDENTITY CLOSURE

    generated Synth A bars receive canonical MaterialIds
    under existing PatternPagingService ownership.

PREREQUISITE B — BASS EVIDENCE EXPORT

    resolved BassRhythmPlan survives the strong-rhythm result seam.

PREREQUISITE C — P1R BAR EVIDENCE FORWARDING

    materializeOneBar forwards the compact owner-derived plan.

PREREQUISITE D — SESSION ORIGIN PUBLICATION

    successful generated-Phrase COMMIT publishes immutable bounded origin
    evidence; failed/legacy commits do not.

Only after A0/A1/B/C/D may the actual D path consume the P0 preservation contract.

## 22. D0-F acceptance

D0-F is GREEN when:

1. the P0 REQUIRED claim set is sufficient and adversarially tested;
2. REVOICE/HOLD/CONNECT/octave MOVE can prove CONTINUES when they actually
   preserve the required facts;
3. DISPLACE/THIN and chromatic change lose the proof without becoming NEW_IDEA;
4. exact repeat is EXACT + CONTINUES;
5. operation names cannot force lineage;
6. P0 remains one-bar and generated-Synth-A-only;
7. the exact MaterialId lifecycle blocker is demonstrated;
8. the minimum owner-evidence extraction seam is explicit;
9. no production source changes during D0-F;
10. no PreparationBasis weakening, reverse analysis, or second identity
    namespace is introduced;
11. the existing descriptor-blind Phrase allocation gap is explicitly carried
    as a production prerequisite rather than hidden by the provenance sidecar.

After D0-F GREEN, the next production checkpoint should implement prerequisites
A-D in that order, with Material identity closure tested before semantic session
consumption is wired.