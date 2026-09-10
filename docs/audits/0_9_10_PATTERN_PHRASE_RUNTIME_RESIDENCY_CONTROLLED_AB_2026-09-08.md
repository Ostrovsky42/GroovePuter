# 0.9.10 Pattern/Phrase Runtime — Controlled Residency A/B

Date: 2026-09-08

## Decision

The previously observed Runtime lifetime `+8 B` comparator result is classified as:

> **CROSS-TEST-IDENTITY ARTIFACT**

It is **not** a Runtime residency defect.

The controlled experiment produced zero fixed-DRAM delta and an identical persistent DRAM symbol map when the RED and FINAL implementations of `src/dsp/miniacid_engine.cpp` were compiled from the same final production tree, sequentially in one CI job with the same installed toolchain and clean, separate build directories.

## Authoritative identities

```text
FINAL TREE
9d7b041c20d34d99c76224d611d429cb12046d41

PRODUCTION FIX
fc11a958e3e74781cb493dcb11e9ffb2302897af

PRODUCTION FIX PARENT / RED IMPLEMENTATION
79aff350dee39ec9b5afb31460643b1fccb6fb67

TARGET FILE
src/dsp/miniacid_engine.cpp
```

The RED implementation was extracted directly from Git:

```text
git show 79aff350dee39ec9b5afb31460643b1fccb6fb67:src/dsp/miniacid_engine.cpp
```

The FINAL implementation was extracted directly from Git:

```text
git show 9d7b041c20d34d99c76224d611d429cb12046d41:src/dsp/miniacid_engine.cpp
```

No historical checkout was used as the A build tree.

## Source provenance

```text
RED miniacid_engine.cpp
sha256 e5f476404156d80986608cb63e4ff52d02a94ae7cffa1ebac01e2fe9a70b323f

FINAL miniacid_engine.cpp
sha256 a659ad29eaea9021da3fe9d1e0829ca91a18573fb89900a85e82c542a4fad964

production-fix.patch
sha256 f0e9057679a123b8773f95ec1deb624bddfd4ead68cc78308e448bbe5e32255a
```

The production patch adds the Pattern/Phrase source-switch lifetime barrier and routes `makePhrase()` through the sequenced-source setter. It does not add a new field, static/global state object, or other persistent owner.

## Controlled build identity

GitHub Actions run:

```text
run 34277717720
job 99944168146
```

Both variants were built sequentially inside the same `ubuntu-latest` job.

### Variant A

```text
base tree: 9d7b041c20d34d99c76224d611d429cb12046d41
only substitution:
src/dsp/miniacid_engine.cpp = 79aff350dee39ec9b5afb31460643b1fccb6fb67 version
build directory: build-A/
```

### Variant B

```text
base tree: 9d7b041c20d34d99c76224d611d429cb12046d41
src/dsp/miniacid_engine.cpp = 9d7b041c20d34d99c76224d611d429cb12046d41 version
build directory: build-B/
```

Arduino CLI and the pinned M5Stack dependencies were installed once before A and B. Each variant used a fresh build directory. The target source hash was recorded immediately before compilation.

## Measured ELF residency

The authoritative ESP32 ELF section names are used below. The initial ad-hoc report displayed `.text` / `.rodata` as zero because this target names those sections `.flash.text` / `.flash.rodata`; that presentation error did not affect the fixed-DRAM or persistent-symbol classification and is corrected in the permanent comparator.

| Metric | A — RED impl | B — FINAL impl | Delta B-A |
|---|---:|---:|---:|
| `.dram0.data` | 55,776 B | 55,776 B | 0 B |
| `.dram0.bss` | 130,936 B | 130,936 B | 0 B |
| **fixed DRAM** | **186,712 B** | **186,712 B** | **0 B** |
| `.rtc.data` | 0 B | 0 B | 0 B |
| `.rtc_noinit` | 0 B | 0 B | 0 B |
| `.ext_ram.bss` | 42,088 B | 42,088 B | 0 B |
| `.flash.text` | 938,392 B | 938,416 B | +24 B |
| `.flash.rodata` | 308,268 B | 308,296 B | +28 B |
| ELF file size | 17,844,904 B | 17,844,856 B | -48 B |

The code/read-only-data changes are implementation/layout effects. They are not retained Runtime ownership.

## Persistent owner comparison

The `.dram0.data` / `.dram0.bss` persistent symbol maps are byte-for-byte identical between A and B.

Therefore all three ownership questions resolve as follows:

```text
new persistent .data/.bss owner?       NO
existing persistent owner grew?        NO
fixed DRAM changed?                    NO
```

## Classification

The controlled experiment satisfies the first admissible outcome:

```text
fixed DRAM A == fixed DRAM B
persistent symbol map A == B
```

Therefore:

```text
OLD +8 B RESULT:
CROSS-TEST-IDENTITY ARTIFACT

RUNTIME RESIDENCY RED:
NO

CONTROLLED RESIDENCY COMPARATOR:
PASS
```

The old comparator was invalid for lifetime-cost attribution because it compared artifacts that did not share one controlled final-tree build identity. The permanent comparator must compare two implementations inside one exact production tree and one build environment; it must never infer a production residency delta from unrelated historical artifacts.

## Production freeze

This evidence does not authorize a production change.

Forbidden as part of this closure:

- editing `src/dsp/miniacid_engine.cpp`;
- padding or layout manipulation to force a zero delta;
- changing the canonical fixed-DRAM threshold;
- using a hardware smoke as a substitute for ELF/map evidence;
- merging UI/GF2 work through the Runtime branch.

After comparator sanitation, the post-sanitation exact SHA must have zero production diff relative to `9d7b041c20d34d99c76224d611d429cb12046d41`, then the complete Runtime candidate matrix must be re-attested on that exact SHA before `RUNTIME HARDWARE TEST: GO` is permitted.
