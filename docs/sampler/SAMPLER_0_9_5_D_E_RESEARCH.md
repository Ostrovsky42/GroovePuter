# GroovePuter 0.9.5-D/E — Missing/Relink and Memory Policy Research

## Purpose

This document refines the 0.9.5-D and 0.9.5-E contracts after the final 0.9.3 sampler recovery was merged.

Current production fact from final 0.9.3:

- a missing stable sample reference no longer destroys the project load;
- the affected pad becomes silent rather than selecting the wrong WAV;
- the stable reference survives Save/reboot through the bounded persistence sidecar;
- when the original path returns, the stable reference can resolve again;
- the realtime Scene/sample voice ABI remains compact.

Therefore 0.9.5-D does **not** need a second missing-sample ownership mechanism. It extends the existing persistence boundary with a bounded reversible locator and product UX.

0.9.5-E remains a measurement/decision stage. A sampler arena is not selected in advance.

## Hardware list

### Host research gate

- Linux/macOS/Windows environment capable of running Python 3;
- GroovePuter repository checkout.

### Later 0.9.5-E hardware campaign

- M5Stack Cardputer ADV / ESP32-S3;
- microSD containing the controlled sampler fixtures/kits;
- USB-C data cable;
- normal Cardputer ADV production profile with the same PSRAM policy as the final 0.9.4 candidate.

No PORT.A wiring, external display or SEQTRAK is required for the allocator/fragmentation campaign itself. Existing full-release MIDI/audio regressions remain separate gates.

## Wiring

No wiring changes.

For the hardware campaign use the normal Cardputer ADV USB-C connection and microSD setup.

## 0.9.5-D — bounded reversible locator

### Why SampleRef is insufficient for UX

`SampleRef` is a stable hash identity. It is intentionally compact and one-way. If the WAV is missing, the UI cannot reconstruct a useful path string from the hash alone.

The persistence boundary therefore needs a reversible logical locator in addition to the stable ref.

The research contract uses two logical forms:

```text
samples/<relative-path>
```

for loose samples and:

```text
kit:<stable-kit-id>:<relative-asset-path>
```

for kit assets.

Examples:

```text
samples/kick.wav
samples/drums/snare.wav
kit:sp12.factory.v1:snare.wav
kit:my-kit.v2:drums/open_hat.wav
```

### Locator invariants

- maximum encoded locator length: 128 bytes for the research contract;
- no absolute paths;
- no `..` or `.` path segments;
- no empty path segments;
- no backslash aliases;
- loose samples are rooted logically below `/samples`;
- kit locators contain stable kit identity, not physical kit directory name;
- kit identity contains no slash, backslash, colon or whitespace;
- the locator is persistence/control-side data only;
- audio continues to use compact `SampleId` / `SampleHandle`.

The exact storage JSON spelling may be finalized with the production implementation, but these logical semantics are frozen.

### Missing state

A persisted pad with a valid stable ref and locator whose asset cannot currently resolve becomes:

```text
runtime SampleId = 0
stable SampleRef = preserved
locator          = preserved
pad parameters   = preserved
```

User-visible behavior:

```text
PAD 3
MISSING
kit:sp12.factory.v1:snare.wav
```

No wrong sample substitution is allowed.

### Save while missing

Saving a project while the sample is still missing must preserve:

- stable ref;
- locator;
- volume;
- pitch;
- start/end;
- loop;
- reverse;
- choke.

Repeated Save -> reboot -> Load cycles while the asset remains absent must not erase the ownership information.

### Automatic recovery

If the original stable asset returns, normal project load may resolve it automatically without forcing the user through RELINK.

RELINK is for choosing a replacement physical asset, not for repairing an asset that has simply returned at its original identity.

### RELINK transaction

RELINK follows the same prepare-before-publication rule as sample selection and kit loading:

```text
candidate selected
-> validate locator
-> inspect WAV
-> decoded-byte admission
-> preload candidate
-> resolve stable identity
-> short atomic identity publication
-> persist new ref + locator
-> Scene revision +1 exactly once
```

Any failure before commit leaves the old missing state bit-for-bit unchanged and does not dirty Scene.

Relink changes sample identity/locator only. Musical pad parameters survive unless the user explicitly resets them.

## 0.9.5-E — memory policy evidence contract

### Required pool matrix

The completed hardware campaign must exercise at least:

```text
8 KiB
16 KiB
24 KiB
32 KiB
48 KiB negative/admission control
```

The 48 KiB case is not an expectation that the current Cardputer policy will support 48 KiB PCM. It proves that rejection happens deliberately rather than through allocator collapse.

### Total bytes are not enough

Every policy must test both total resident PCM and the largest individual allocation.

Required comparison:

```text
32 KiB total = 8 x 4 KiB
32 KiB total = 24 KiB + 8 KiB
```

These can behave differently even with the same total because ESP32 allocation also depends on the largest contiguous free block.

### Admission reasons stay distinct

The instrumentation/test model distinguishes:

```text
POOL_LIMIT
TOTAL_HEAP
CONTIGUOUS_BLOCK
SLOT_LIMIT
```

Do not diagnose every failed allocation as fragmentation.

A characteristic fragmentation failure is:

```text
total free8 >= requested allocation
but
largest8 < requested allocation
```

A failure where total free heap is itself below the request is general memory pressure, not proof that a fixed arena is the right answer.

### Required telemetry

Every measured checkpoint records:

```text
freeInt
largestInt
free8
largest8
resident PCM bytes
resident sample count
largest resident sample
sampler pool limit
audio underruns
audio CPU peak
heap integrity
```

Reject inconsistent telemetry such as `largest8 > free8` or resident PCM greater than the configured pool.

### Required checkpoints

At minimum:

```text
runtime baseline
after PCM load
Sampler page
Scene Save
Scene Load
Song page
1 active voice
4 active voices
8 active voices
Stop/Play
after eviction
```

### Fragmentation campaign

Minimum research contract:

```text
50 kit/sample-set switches
```

across varying allocation shapes.

The campaign must preserve heap integrity throughout. Any WDT/reset/heap-corruption result is a product bug/general memory failure and must be resolved before using the run to choose an allocator policy.

### Decision interpretation

Possible evidence classes:

```text
INCOMPLETE
PER_SAMPLE_HEALTHY
FRAGMENTATION_EVIDENCE
GENERAL_MEMORY_PRESSURE
```

`FRAGMENTATION_EVIDENCE` means at least one practical allocation failed while total free heap was sufficient but the largest contiguous block was too small.

This is evidence to evaluate a sampler arena; it is not an automatic instruction to implement one.

`PER_SAMPLE_HEALTHY` keeps per-sample allocation as the default architecture.

`GENERAL_MEMORY_PRESSURE` means an arena may not solve the actual problem because the device lacks total usable heap for the requested workload.

## Build / flash steps

### Host research gates

From repository root:

```bash
bash tests/run_sampler_0_9_5_research_tests.sh
```

The runner executes the WAV, kit transaction, missing/relink and memory-policy reference contracts.

### Hardware campaign

Do not run 0.9.5-E hardware verdicts on the historical research branch.

After the final 0.9.4 SHA is frozen:

```bash
git checkout <final-0.9.4-derived-0.9.5-E-candidate>
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

The production 0.9.5-E implementation must provide the sampler-specific telemetry/fixture procedure before hardware acceptance.

## Expected behavior

### Host

- all four 0.9.5 research contract suites pass;
- no production sampler source is compiled or modified by this research gate;
- missing/relink tests prove non-mutating failure and one-shot successful commit;
- memory tests prove that pool, total heap, contiguous-block and slot failures remain distinguishable.

### Future Cardputer screen

For a missing asset the Sampler page should show the affected pad as missing and expose its persisted logical locator rather than `(empty)` or an unrelated filename.

A failed RELINK must leave that display/state unchanged. A successful RELINK must show the replacement asset and retain the existing pad parameters.

### Future Serial

0.9.5-D/E diagnostics should make the rejection class explicit, for example:

```text
[SAMPLER] RELINK rejected: admission
[SAMPLER] LOAD rejected: contiguous-block need=24576 largest=21492 free8=38360
```

Exact log wording is implementation-owned; silent ambiguous failure is not acceptable for the hardware campaign.

## Troubleshooting

### Research test fails on locator validation

Check that the locator is logical and bounded. Do not add exceptions for absolute paths, `..`, backslashes or physical kit-directory names.

### Missing sample disappears after Save

This violates the existing 0.9.3 stable-ref sidecar contract before 0.9.5-D even begins. Fix persistence ownership first; do not mask it with RELINK UI.

### 32 KiB total PCM fails although free8 is above 32 KiB

Inspect `largest8` and the largest individual WAV allocation. Total free heap does not guarantee one large contiguous allocation.

### Fragmentation classifier reports GENERAL_MEMORY_PRESSURE

Check whether `free8` itself was below the requested allocation. If so, do not use that failure as evidence for an arena.

### 48 KiB control loads successfully unexpectedly

Record the exact pool policy and allocator configuration. The negative control is only meaningful when the configured sampler policy is below 48 KiB.

## Acceptance checklist

### Research

- [ ] 0.9.3 final missing-ref persistence behavior is treated as the starting point, not reimplemented;
- [ ] locator is reversible, bounded and outside realtime Scene/audio ABI;
- [ ] loose and kit locator forms reject traversal/absolute/backslash aliases;
- [ ] Save while missing preserves stable ref + locator + pad parameters;
- [ ] original returning asset auto-resolves without mandatory RELINK;
- [ ] failed RELINK leaves pad and Scene revision unchanged;
- [ ] successful RELINK commits once and preserves musical pad parameters;
- [ ] memory matrix includes 8/16/24/32/48 KiB;
- [ ] 32 KiB allocation-shape comparison is included;
- [ ] pool/heap/contiguous/slot rejection classes are separate;
- [ ] fragmentation campaign requires at least 50 switches;
- [ ] arena remains a measured decision, not a preselected implementation.

### Future Cardputer ADV

- [ ] missing pad is visible without crash/WDT/wrong substitution;
- [ ] Save/reboot while missing preserves ownership and parameters;
- [ ] restored original asset resolves again;
- [ ] failed and successful RELINK behavior matches the transaction contract;
- [ ] every E checkpoint records complete heap/pool/audio telemetry;
- [ ] 1/4/8 voice workloads remain valid;
- [ ] 50+ switch campaign completes with heap integrity intact;
- [ ] final allocator decision cites measured evidence from the exact post-0.9.4 candidate.
