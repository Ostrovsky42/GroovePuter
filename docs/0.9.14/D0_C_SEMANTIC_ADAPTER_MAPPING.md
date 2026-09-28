# GroovePuter 0.9.14 — D0-C Semantic Adapter Mapping

Status:

    D0-A = REVISE / CLOSED
    D0-B = RED / CONTRACT FROZEN
    D0-C = GREEN

## Purpose

D0-C introduces the smallest production representation capable of carrying the
independent semantic facts proven necessary by D0-B.

It does not redesign transformations, Material lifecycle, publication policy,
genre profiles, Phrase execution, or generator ownership.

## Boundary

The new layer is deliberately split into:

    development_semantics.h
        bounded semantic fact carrier

    development_semantic_adapter.h
        one-way mapping from explicit claim summaries + legacy evidence
        into independent facts

The adapter MUST NOT import as authority:

    IdeaClassification
    GenreResult
    DevelopmentDisposition
    evaluateDisposition()

Those remain legacy 0.9.13 behavior until a later migration checkpoint.

## Represented facts

    LINEAGE
        UNKNOWN / CONTINUES / NEW_IDEA

    STATE RELATION(reference)
        UNKNOWN / NOT_APPLICABLE / EXACT / VARIATION

    TRAJECTORY ROLE
        UNKNOWN / NONE / REPEAT / DEVELOPMENT / BREAK / RETURN

    GENRE STATUS
        UNKNOWN / ALLOWED / VIOLATION

    OPERATION CONFORMANCE
        UNKNOWN / HONORED / VIOLATED

    CAPABILITY(claim/domain)
        UNKNOWN / AVAILABLE / UNAVAILABLE

No publication result exists in the carrier.

## Adapter rules

1. Positive lineage/genre/operation facts require explicit required-claim
   summaries. Mutation count cannot create them.

2. State relation always names its reference. Exactness may be observed directly.
   Non-exact VARIATION requires explicit preservation PASS.

3. Failed/unknown preservation does not manufacture a positive state relation.

4. Trajectory is accepted only with explicit trajectory context availability.

5. Legacy evidence may contribute only narrowly proven facts:

   - promised contour failure -> OPERATION VIOLATED;
   - EXTEND without tonal-root authority -> CAPABILITY UNAVAILABLE;
   - primary-downbeat onset presence -> capability AVAILABLE.

   D0-D1 supersedes the earlier Funk/The One shortcut: tick-zero onset
   presence/loss does not by itself write GENRE.

6. GENRE ALLOWED or VIOLATION requires an explicit complete genre-requirements
   result from the proper owner. One local observable cannot certify or reject
   the complete genre verdict by itself.

7. UNKNOWN remains local.

## Canonical witnesses

D0-C must represent:

    A -> A
        EXACT to PREDECESSOR
        CONTINUES
        REPEAT

    A -> B -> A
        EXACT to RETURN_TARGET
        CONTINUES
        RETURN

    A -> B -> A'
        VARIATION to SOURCE
        VARIATION to PREDECESSOR
        VARIATION to RETURN_TARGET
        CONTINUES
        RETURN

The same local A -> A' change must also be representable as:

    trajectory = NONE

or, with established temporal/formal context:

    trajectory = DEVELOPMENT

without changing its state relation.

## D0-C negative contract

D0-C must not:

- modify `musical_development.h`;
- modify Material lineage/storage/publication owners;
- add similarity percentages or DevelopmentDistance;
- add IdeaFingerprint;
- add AdmissibilityResult;
- interpret PhraseEvolutionLaw or BarFunction as the new semantic authority;
- infer NEW_IDEA from pitch+onset distance;
- infer GENRE ALLOWED from one passing local claim;
- make CAPABILITY UNAVAILABLE imply VIOLATION.

## Acceptance

D0-C is GREEN when:

1. only the two new semantic headers constitute production delta over D0-B;
2. repeat vs return is representable for byte-identical material;
3. transformed return supports SOURCE/PREDECESSOR/RETURN_TARGET simultaneously;
4. isolated variation vs developmental variation are distinguishable by context;
5. B7/B8/B9/B10 separation passes;
6. UNKNOWN remains claim-local;
7. the adapter never reads legacy classification/disposition authority;
8. no publication policy is added;
9. no scalar/global shortcut is introduced.

After D0-C GREEN, the next checkpoint is not generator redesign. It is a bounded
owner-mapping decision: determine which existing owner supplies each required
claim and which claims remain evidence gaps.
