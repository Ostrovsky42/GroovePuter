# H0-R1 Step A evidence and prepared-state blocker

Base: `4f1304916389c5af315faa24127555c062a32fa9`.
Branch: `feature/20261006-h0-r1-harmonic-rhythm`.
Worktree: `/tmp/grooveputer-h0-r1`.
No push, profile selection changes, production policy selection, B0, or R2 work.
The authoritative replacement plan was read from the main checkout:
`docs/superpowers/plans/2026-10-06-h0-r1-r2-harmonic-rhythm-plan.md`.
Its base facts were rechecked against the archived base. Static HALF-BAR retains
one event per bar. The isolated runner is connected to `run_host_tests.sh`; the
P1R/A8 runner is also connected to `scripts/ci/run_core.sh` so shared harmony
runs on core CI.

## Baseline

- `bash tests/run_host_tests.sh`: exit 0.
- `cd platform_sdl && make`: exit 0 from the fresh base worktree.
- `bash tests/run_0_9_9_phrase_p1r_tests.sh`: exit 0, including ASan/UBSan.
- `bash tests/run_0_9_14_p0_cycle_tests.sh` on the separate archived base:
  exit 0, P0 cycle GREEN.
- Cardputer dynamic-FatFs build with the exact specified FQBN: exit 0.
  DRAM globals **183220 B**, data 37900 B, BSS 145320 B; ceiling 191488 B.
  The script default uses CDCOnBoot=cdc, so the authoritative measurement used
  an explicit FQBN environment override with CDCOnBoot=default.

## RED, implementation and GREEN

The new isolated test was run before production changes. Exit 1, expected
missing-feature compiler diagnostics:

```text
error: 'PhraseHarmonicEvent' does not name a type
error: 'const struct ... PhraseHarmonicTimeline' has no member named 'events'
error: 'projectPhraseHarmonicBarMaterialization' was not declared in this scope
```

`bash tests/run_h0_r1_tests.sh` passed after implementing explicit
intervals, validation, active-event lookup and carry-in projection. The A5
lookup test produced a second expected RED while
`materializePhraseHarmonicProgression` was absent, then passed against the
production helper. The H0 test passed with ASan/UBSan and
`ASAN_OPTIONS=detect_leaks=0`.
LeakSanitizer itself cannot run under this sandbox's ptrace environment.

Coverage includes moving HALF-BAR at 1/2/4/8 bars, the explicit three-event
PROLONG representation (without implementing a PROLONG selection policy),
every active phrase step, carry-in without source advancement or extra phrase
event counts, invalid overlap/gaps/order/duration, 32 events, duration 128,
and carry-in followed by four actual harmonic changes in one physical bar.
StaticModal and PedalDrone also have explicit HALF-BAR compatibility tests
at all supported phrase lengths.

The unchanged P1R focused gate also passed after the timeline change.
The final P1R gate passed, including deterministic repeat, ASan/UBSan and A8.
`PreparedPhraseExecution` is 764 B (`PhraseSemanticResult`: 62 B).

## Physical compatibility evidence

Built `tests/test_h0_r1_halfbar_corpus.cpp` against a separate `git archive`
of the exact base and against the candidate. Both use the real phrase
preparation and materialization functions. The corpus spans all 16 base genres,
three identities and requested lengths 1/2/4/8; it serializes physical synth
and drum fields, automation and groove without C++ padding.

`cmp` passed for **545712 bytes**. Both SHA-256 digests:

```text
47b16cba1d0651e1cd1ddcd146eded7d3bc240cd37953b88dfb92d499b090836
```

This proves compatibility for this corpus, not full Step A readiness. The
owner separately ran a 29,618,460-byte corpus across 16 genres, three recipes,
P1/P2/P3, three modes, two roots, three identities and 1/2/4/8 bars; base and
candidate matched byte-for-byte with SHA `6999381882…`. No existing musical
golden was changed.

## Compact PREPARE ownership

A product-path compile first exposed a compact PREPARE failure:

```text
static assertion failed: PMB-P1: PreparedPhraseArrangement must stay a compact plan,
not per-bar physical material
the comparison reduces to '(1212 <= 1024)'
```

A compact fix keeps a single `PhraseHarmonicTimeline` on
`PreparedPhraseExecution` and removes the duplicate copy from
`PhraseSemanticResult`. Eight saved per-bar projections were also removed;
`PhraseHarmonicBarMaterialization` and `HarmonicRhythmPlan` are projected from
the canonical timeline when a bar is materialized. `PreparedPhraseExecution`
is now **764 B**. The product-path compile passed its 1024 B
`PreparedPhraseArrangement` static assertion, and P0 passed after this change.
The <=32 B `GenerationCompositionResult` contract is untouched.

The semantic result retains per-bar event ranges and counts. The canonical
timeline exists only once on prepared execution; carry-in anchors are derived
and do not affect phrase totals.

A8 adds a test-only probe at `materializeRole` input and checks the progression
plan and harmonic event onsets passed to the tonal roles on the actual phrase
path. The existing probes alone record melodic rhythm input and bass output;
neither exposed this shared input contract.

Plain SDL `make` after the header edit returned exit 0 without recompiling;
its dependency tracking is insufficient for this header-only change.
The candidate `make` rebuild passes after the prepared-state compaction.

The first candidate host-suite attempt caught the new policy header's include-
guard terminator; it was corrected and the preflash gate passed. The subsequent
full host suite passed after timeline compaction and seam migration. D0-E/D0-F
and D1-B source guards were updated where they referenced the old prepared
field; those runners passed. D1-B's existing generated Synth A origin checks
also passed. R2 and production harmonic policy selection remain pending.

Logs are in `/tmp/h0-r1-*.log`; the original user checkout is untouched.

## Step A final gates

- `bash tests/run_host_tests.sh`: exit 0, including the new H0 runner.
- `(cd platform_sdl && make)`: exit 0 after source changes.
- `bash tests/run_0_9_14_p0_cycle_tests.sh`: exit 0, P0 cycle GREEN.
- `bash tests/run_0_9_14_d0e_tests.sh`: exit 0 after the compact-state source
  guard was updated. D0-F, D1-B and pre-flash source guards pass.
- Exact Cardputer FQBN build and DRAM gate: exit 0. Baseline 183220 B;
  candidate 183220 B; delta **0 B**; budget 191488 B.
- HALF-BAR physical corpus: `cmp` passed, 545712 bytes. Baseline and candidate
  SHA-256: `47b16cba1d0651e1cd1ddcd146eded7d3bc240cd37953b88dfb92d499b090836`.

No profile policy plumbing, production SLOW/PROLONG selection, genre activation,
or R2 work was done. The accepted Step A is split into local commits 1–5;
nothing has been pushed. Before B1, add a direct event-policy builder for
SLOW/PROLONG rather than materializing and discarding F08 one-bar plans.
