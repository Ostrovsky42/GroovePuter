# Product direction stage

This documentation-only stage establishes [`../../PLAN.md`](../../PLAN.md) as the canonical product direction and execution order.

## Purpose

- keep product identity separate from individual implementation PRs;
- promote `GENRE != FEEL != GENERATOR != TEXTURE` from a contributor rule to a product invariant;
- define `Phrase` as the missing level between bar and section;
- separate tactical stabilization, near-term delivery, strategic outcomes, and deferred ideas;
- prevent stage documents from becoming parallel roadmaps.

## Expected result

Contributors can answer four questions before starting a feature:

1. Which product outcome in `PLAN.md` does this serve?
2. Does it preserve the independence of GENRE, FEEL, GENERATOR, and TEXTURE?
3. Does it create, shape, develop, arrange, or safely output a Phrase?
4. Is it more important than the current ownership and transport work?

## Acceptance checklist

- [x] `PLAN.md` defines product identity and the Phrase hierarchy.
- [x] `PLAN.md` records current tactical work and the canonical delivery order.
- [x] `PLAN.md` keeps deferred keyboard generators and protocol features visible without admitting them to active scope.
- [x] `docs/stages/README.md` states that stage documents do not reorder the roadmap.
- [x] No firmware, runtime behavior, persistence format, or controls are changed.
