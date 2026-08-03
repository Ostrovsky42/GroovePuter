# Stage documents

This directory contains implementation specifications, acceptance procedures, and historical stage records.

The canonical product direction, priority order, strategic outcomes, and deferred backlog are maintained in [`../../PLAN.md`](../../PLAN.md).

Protocol and hardware contracts are maintained in [`../reference/`](../reference/). Hardware acceptance records are maintained in [`../tests/`](../tests/).

Stage documents must:

- map their scope to one item in `PLAN.md`;
- preserve the runtime and architecture invariants defined there;
- describe testable implementation and hardware acceptance;
- link to canonical protocol references instead of restating partial contracts;
- record newly discovered follow-up work in `PLAN.md` instead of silently expanding the active stage.

A stage document does not establish a competing roadmap or reorder work on its own.
