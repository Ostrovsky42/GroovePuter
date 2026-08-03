# Stage documents

This directory contains implementation specifications, acceptance procedures, and historical stage records.

The canonical product direction, priority order, strategic outcomes, and deferred backlog are maintained in [`../../PLAN.md`](../../PLAN.md).

Verified protocol and external-device contracts are maintained separately under [`../reference/`](../reference/). Stage documents should link to those references rather than restating a partial or differently named protocol model.

Stage documents must:

- map their scope to one item in `PLAN.md`;
- preserve the runtime and architecture invariants defined there;
- use the relevant protocol/device reference as the baseline;
- describe testable implementation and hardware acceptance;
- record newly discovered follow-up work in `PLAN.md` instead of silently expanding the active stage.

A stage document does not establish a competing roadmap or reorder work on its own.
