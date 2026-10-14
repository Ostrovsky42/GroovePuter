# HGS2 Phase A — software and listening evidence

Date: 2026-10-14
Worktree: `feature/smart-play-return`
Start/HEAD SHA: `21c787493e3028a53e6d28d3a2e2e7b47f3a561f`
Firmware target: Cardputer ADV, ESP32-S3, Arduino ESP32 core 3.2.2
Compiler: GCC 13.3.0; Clang is not installed in this environment.

Candidate ADV binary SHA-256: `ab460a2ed858b156d6c88a7d75be7650e2e8dd6634003f76ad69d4cf5b550e38`.
The binary was built locally and was not flashed in this checkpoint.

## Verdicts

| Gate | Result | Evidence |
|---|---|---|
| `SMART_PLAY_SOFTWARE` | PASS | `bash tests/run_smart_play_tests.sh`; host/core gates below |
| `HGS2_PHASE_A_SOFTWARE` | PASS WITH REPORTED LIMITATION | HGS suites, deterministic 17-style sweep, sanitizer smoke, Core/SDL/ADV builds |
| `HARDWARE_ACCEPTANCE` | NOT RUN | No device was flashed or physically exercised during this checkpoint |
| `LISTENING_ACCEPTANCE` | NOT RUN | Trace files are event data; no audio was rendered or auditioned |
| `PRODUCTION_PROFILE_MAPPING` | 9 mapped / 8 host-only | Mapping is derived from `harmonicStyleFor()` and policy rows; see lists below |

The software gates passed their stated boundedness and determinism checks. The sweep
also found a substantial, explicit limitation: **4,643 of 8,704 RETURN attempts
(53.3%) return `CAPACITY_EXCEEDED`** because the required bridge does not fit the
four-event RETURN budget. This is not an invalid route or silent substitution: the
request is rejected, leaving the existing material unchanged. It needs musician
acceptance and should be watched in hardware testing. It must not be described as a
successful return or hidden in an aggregate pass.

All 4,643 route failures in the sweep occurred in RETURN. There were no `INVALID`
or `UNKNOWN` results, no materializer projection failures, and no invariant failures.
The RETURN search is bounded to four bridge events and 128 search nodes. `CAPACITY_EXCEEDED`
can mean that no path within the requested four events was found, or that the bounded
search exhausted its node budget; the current public status does not distinguish those
causes. Acid, Dub Techno, Jungle, Footwork, and DnB returned home in all 512 tested
style/context/seed cases each. Failures by profile are listed below.

## Sweep definition

Command:

```sh
bash tests/run_hgs_listening_package.sh
```

The runner defaults to seeds `0..255` and writes three TSV files under
`build/hgs-listening-package/evidence/`:

- `summary.tsv`: 34 profile/context summaries;
- `traces.tsv`: event-by-event resolved harmony or typed route failure;
- `ab_examples.tsv`: ten consecutive A/B trace pairs per profile and context.

The sweep covers 17 harmonic styles × 256 seeds × major/minor home context × five
sections (`TAKE`, `KEEP`, `DEVELOP`, `BREAK`, `RETURN`) = 43,520 route requests.
Each section uses four fixed timing slots, the production materializer capability
masks, the generated corpus prior, deterministic song genomes, and persistent
within-session repeat memory. Every request is replanned against the same initial
memory and compared event-by-event. Ready events are checked against genre and
materializer masks and projected through the production harmonic materializer.
`RETURN` terminal state is checked against exact home state/context.

Full sweep totals:

| Measure | Result |
|---|---:|
| Ready routes | 38,877 |
| Typed route failures | 4,643 (`CAPACITY_EXCEEDED`, all RETURN) |
| Invariant failures | 0 |
| Route event changes | 35,885 |
| Nonzero corpus-prior observations | 51,083 |
| Corpus-prior sum | 736,997 |
| RETURN reached exact home | 4,061 / 8,704 |
| RETURN capacity rejection | 4,643 / 8,704 |
| A/B trace rows | 2,720 (10 seeds × 2 contexts × 17 profiles × 4 slots × A/B) |

The corpus counts are diagnostics that confirm the generated model contributes to
candidate scoring. They do not claim that its contribution is musically correct.
Route changes and diversity counts are useful for locating examples, not a quality
score.

## Profile mapping and RETURN results

`DEVICE_MAPPED` means a style currently resolves from the app's genre settings. A
host-only policy is still covered by the same route and materializer checks, but is
not reachable by selecting a named device GENRE today.

| Harmonic profile | Current mapping | RETURN reached home | RETURN capacity rejected |
|---|---|---:|---:|
| Acid | device mapped | 512 | 0 |
| Detroit Techno | host only | 131 | 381 |
| Electro | device mapped | 119 | 393 |
| EBM | host only | 47 | 465 |
| UK Garage | device mapped | 134 | 378 |
| Dub Techno | device mapped | 512 | 0 |
| Ambient | host only | 171 | 341 |
| Lo-Fi | device mapped | 141 | 371 |
| Chillout | host only | 122 | 390 |
| Deep House | host only | 132 | 380 |
| Jungle | host only | 512 | 0 |
| Trap | host only | 51 | 461 |
| Hip-Hop | device mapped | 125 | 387 |
| Footwork | device mapped | 512 | 0 |
| DnB | device mapped | 512 | 0 |
| Salsa | host only | 200 | 312 |
| Synthwave | device mapped | 128 | 384 |

The current mappings are Acid, Electro, UK Garage, Dub Techno, Lo-Fi, Hip-Hop,
Footwork, DnB, and Synthwave. Detroit Techno, EBM, Ambient, Chillout, Deep House,
Jungle, Trap, and Salsa are `HOST_PROFILE_ONLY`.

## A/B trace package and listening protocol

`ab_examples.tsv` contains ten successive seeds for each style and major/minor
context. Pair A uses the existing FourFloor legacy progression source; pair B uses
Graph `DEVELOP`. Both use four fixed harmonic timing slots and the same seed stream
and role fixture. Short legacy progressions repeat their period to fill the same
four slots. Graph examples carry deterministic session memory across the ten seeds.
These files compare harmonic event traces only. They do not contain synthesized
audio, rendered drums, matched sound patches, or proof of audible quality.

For listening, use the `B_GRAPH` sequence as the generated harmonic view and compare
to `A_LEGACY` under the same chosen device rhythm, notes/voices, level, and sound
patch. Record Cardputer model/revision, exact firmware SHA, project settings, profile,
context, seed, and the observed output for each comparison. Do not mark listening
PASS from a trace or from a build.

For each profile, compare at least seeds 0–9 in both contexts where reachable and
record:

- Does TAKE/KEEP preserve chord restraint where expected?
- Do voice changes sound smooth while still providing enough contrast?
- Does the route sound plausible without repeating a stock progression?
- When a pivot changes scale, is the pivot legible by ear?
- Does BREAK create a controlled departure and does RETURN reach home convincingly?
- Do Acid, Jungle, and DnB remain structurally conservative?
- When RETURN is rejected, is the rejection acceptable and is the previous material
  clearly preserved?

Phase A rejected-value review: harmonic qualities beyond the shared ten-quality
bootstrap set and scale IDs beyond the current ten-scale materializer are pruned by
capability intersection. A richer descriptor is not justified by the number of
qualities in the import corpus. Name a specific blocked progression/voicing, capture
its project/seed and trace, and accept its memory/persistence cost before proposing
Phase B.

## Verification evidence

Passed in this worktree:

```text
python3 tests/test_hgs_listening_package.py                  PASS
bash tests/run_hgs_listening_package.sh                      PASS (43,520 requests)
HGS_TRACE_CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  bash tests/run_hgs_listening_package.sh --seed-count 2     PASS
bash tests/run_hgs_tests.sh                                  PASS
bash tests/run_smart_play_tests.sh                           PASS
bash tests/run_0_9_14_p0_cycle_tests.sh                     PASS
bash scripts/ci/run_phrase_core.sh                           PASS
python3 tests/test_source_regressions.py                     PASS
bash tests/run_ui_content_bounds_tests.sh                    PASS
make -C platform_sdl -j2                                     PASS (already up to date)
GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS=-fstack-usage \
  bash scripts/build.sh --warnings all                       PASS
git diff --check                                             PASS
```

The host build uses GCC 13.3.0. `clang++` is unavailable, so cross-compiler coverage
is not claimed. ADV build result: flash 1,592,006 bytes (50% of 3,145,728), static
RAM 188,804 bytes (57% of 327,680). A baseline rebuild was not made in this
checkpoint, so no RAM delta is claimed. Existing warnings remain in the firmware
build.

The GCC `.su` reports include a 10,832-byte `generateCycle()` frame, 1,504-byte
`prepareGraphCycleSection()` frame, 9,552-byte `prepareGraphRoute()` frame, and
1,248-byte `planRoute()` frame. These are static compiler estimates, not task stack
high-water measurements. Capture the actual high-water mark on Cardputer, especially
when generating a cycle; the target task's available stack margin is not established
by the host sweep or compiler report.

## Hardware checklist

Hardware testing may start after the current ADV binary is flashed by the operator.
Begin with device-mapped styles; host-only styles cannot be selected from the current
GENRE UI. Record the firmware SHA and test with transport stopped and playing.

- [ ] Boot and inspect `GENRE → HARMONY`; confirm unsupported genre/scale shows
  `LEGACY / N/A:...` and cannot silently select Graph.
- [ ] Select a supported style and scale, select `GRAPH`, then generate a new Take.
  Confirm the existing Song keeps playing until its accepted boundary and the new
  generated material uses the persisted Graph harmony.
- [ ] Repeat `G` at least ten times for Acid, UK Garage, Dub Techno, Lo-Fi, Footwork,
  DnB, and Synthwave. Note accepted/rejected generations and any `N/A:ROUTE` report.
- [ ] Test `KEEP`, `DEVELOP`, `BREAK`, and `RETURN`; confirm a rejected return leaves
  the current Song unchanged and the UI gives a useful failure signal.
- [ ] Cycle a saved Graph phrase. Confirm the cycle follows its stored Graph state;
  save/reload, Undo, and retry the cycle.
- [ ] Test stopped and playing generation, and confirm playback is stable with no
  reboot, watchdog, or audible renderer stall.
- [ ] Record audio for A/B seeds 0–9 with the same rhythm/timbre and compare the
  listening questions above. This is the separate `LISTENING_ACCEPTANCE` gate.
- [ ] Capture actual task stack high-water and free heap before/after repeated cycle
  generation and hot project reload.

Do not mark either hardware or listening acceptance green until a person records the
physical results. Current values remain `HARDWARE_ACCEPTANCE=NOT_RUN` and
`LISTENING_ACCEPTANCE=NOT_RUN`.
