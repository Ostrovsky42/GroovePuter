# GroovePuter 0.9.14 — D0-D1 Claim Strength Repair

Status:

    D0-C = GREEN
    D0-D = GREEN / OWNERSHIP AUDIT COMPLETE
    D0-D1 = CLAIM-STRENGTH REPAIR

## Frozen finding

The runtime fact behind the previous D0-C claim named MetricAnchorTheOne is only:

    exists RuntimeSynthEvent with startTick == 0

That is primary-downbeat onset presence.

It is not authoritative evidence of:

- metric hierarchy;
- "The One" as a Funk structural concept;
- Funk pocket;
- Funk identity;
- complete Funk genre validity;
- lineage continuity;
- development or return.

## Repair

Old D0-C capability name:

    MetricAnchorTheOne

New capability name:

    PrimaryDownbeatOnsetPresence

The 0.9.14 semantic adapter now owns a narrow observable helper:

    hasPrimaryDownbeatOnset(buffer)

which answers only whether any runtime event starts at tick zero.

Legacy 0.9.13 hasEventOnTheOne(), theOnePreserved and requireTheOne remain
untouched. D0-D1 does not revalidate them as 0.9.14 semantic authority.

## Critical correction

D0-C previously promoted:

    FunkSoul/request requires "The One"
    + source has startTick 0
    + candidate loses startTick 0

directly into:

    GenreStatus::Violation

D0-D established that the provider is not strong enough for that complete
semantic verdict. D0-D1 removes that promotion.

The observable may later be consumed by a properly owned genre/preservation
contract, but it cannot itself write GENRE, LINEAGE or TRAJECTORY.

## Adversarial witnesses

A. Source and candidate both have a tick-zero onset:

    capability = AVAILABLE
    genre       = UNKNOWN unless independently supplied
    lineage     = UNKNOWN unless independently supplied
    trajectory  = UNKNOWN unless independently supplied

B. Source has tick-zero onset, candidate loses it:

    observable loss is mechanically distinguishable
    but complete GENRE VIOLATION is not inferred

C. Both lack tick-zero onset:

    absence carries no invented negative Funk meaning

D. Unrelated candidate has tick-zero onset:

    presence does not certify same idea, variation, return, development
    or Funk validity

## Production delta

Only:

    src/dsp/development_semantics.h
    src/dsp/development_semantic_adapter.h

may differ from the frozen D0-C head for production source.

No generator, Material, Phrase, storage, persistence or legacy-development
behavior changes in D0-D1.

## Acceptance

D0-D1 is GREEN when:

1. the capability name states no more than the provider proves;
2. D0-C regression gate remains GREEN;
3. all four downbeat-strength witnesses pass;
4. source firewall prevents downbeat evidence becoming genre, lineage or
   trajectory authority;
5. legacy 0.9.13 behavior remains untouched;
6. no unrelated pattern-editor repair is included.

Hard stop after D0-D1. D0-E is a separate checkpoint.