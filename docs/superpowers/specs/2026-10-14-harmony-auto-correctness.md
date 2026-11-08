# Harmony AUTO correctness brief

User approved the prior AUTO design and delegated ordering. Implement GRAPH
correctness before automatic selection; context delivery to PERFORM ARP is the
next separate step. Source review:
/tmp/genre-review-5e98/docs/superpowers/reports/2026-10-14-genre-regression-review.md.

## Contract

- Genre policy masks, budgets and explicit prohibitions remain hard constraints.
- Fix premature RETURN search failure without increasing the 128-node pool or
  four-edge depth. After truncation, Ready guarantees a legal exact-home path
  optimal within retained graph, not a globally optimal omitted path. A truncated
  search without a goal remains CapacityExceeded; a complete unreachable search
  remains ReturnUnreachable. Publication is atomic on failure.
- Offline/runtime corpus tokens share versioned meaning for roots, quality,
  extensions and slash bass. Exact-model hits must be demonstrated by vectors.
- Absolute scale masks are not transposed twice. Functional and diatonic scores
  reward their corresponding relation tags.
- Bounded candidates must represent allowed rich qualities on new roots without
  enlarging the 32-candidate cap or bypassing runtime projection capabilities.
- Defer publishing drums until all GRAPH projection checks pass.
- AUTO is a real selection mode, not a renamed refusal. Existing saved Legacy0
  and Graph1 meanings and default Legacy output stay intact. Explicit AUTO may
  choose GRAPH for supported genre/recipe/scale and Legacy otherwise; the UI
  shows effective source/reason. No silent expanded mapping or harmonic-minor
  support. Planner failures must not leave partial material publications.
- No Character/Mobility controls, DSP, task-stack, DMA, or ARP-context changes.
- Keep the experimental branch designation. Software tests are not listening
  acceptance; report remaining typed route failures and hardware limits.

## Acceptance

Reproduce each reported bug before changing production code. Run targeted HGS
and Smart Play tests, Legacy replay and the provided default fingerprints as
available, real UI AUTO interactions, strict C++17 checks and final Cardputer
FS1B build/static budget. Perform focused SDL feedback checks when feasible.
