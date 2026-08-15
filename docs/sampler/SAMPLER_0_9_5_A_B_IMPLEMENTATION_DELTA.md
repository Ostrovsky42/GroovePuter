# GroovePuter 0.9.5-A/B — Implementation Delta from Final 0.9.3

## Purpose

Freeze the **minimal** production delta for 0.9.5-A (WAV loader hardening) and 0.9.5-B (canonical kit model) after final 0.9.3 sampler recovery.

Reference source head:

```text
agent/20260814-0.9.3-sampler-final-acceptance
54a30847a7974655e54e3e472a8de0fbbb1042c2
```

0.9.5 must build on the safety already recovered there. It must not replace the sampler store, audio ABI, Scene ABI, or preload ownership without evidence.

## Hardware list

### Host contract work

- repository checkout;
- Python 3;
- normal host compiler/test environment used by GroovePuter CI.

### Later production acceptance

- M5Stack Cardputer ADV / ESP32-S3;
- microSD card with WAV/kit fixtures;
- USB-C data cable;
- final 0.9.4-derived Cardputer build configuration.

No PORT.A wiring or external display is required for A/B acceptance.

## Wiring

No wiring changes. Use the standard Cardputer ADV USB-C and microSD setup.

---

## 0.9.5-A — what final 0.9.3 already does

The current `sample_loader.cpp` and `RamSampleStore::preload()` already provide the recovery-level two-pass contract:

```text
inspect metadata
-> decoded mono byte admission
-> reclaim evictable pool capacity / slot
-> bounded decode
-> publish sample slot
```

Do not regress these properties.

Already present at the final 0.9.3 source head:

- allocation-free metadata probe before PCM allocation;
- PCM format `1` only;
- PCM16 only;
- source channels restricted to mono or stereo;
- sample rate must be non-zero;
- `blockAlign == channels * sizeof(int16_t)`;
- data size must contain complete source frames;
- physical data payload truncation is rejected before LRU reclamation;
- decoded mono byte count is overflow checked;
- decoded byte admission occurs before data read/allocation;
- store reclaims pool bytes before decode;
- store admits a free/quiescent slot before decode;
- a file changed between inspect and decode is re-probed against the actual remaining decode budget.

This means 0.9.5-A is **not** a new preload architecture.

---

## 0.9.5-A — remaining production delta

### A1. RIFF boundary ownership

The parser must derive and validate an effective RIFF end from the RIFF header and physical file size.

Required rules:

```text
RIFF/WAVE header present
RIFF declared end does not overflow
RIFF declared end <= physical file size
all traversed chunk headers fit inside RIFF
all chunk payloads + padding fit inside RIFF
```

Do not seek using unchecked `position + chunkSize` arithmetic.

Trailing bytes outside the declared RIFF container may be ignored, but must never extend chunk traversal.

### A2. Generic chunk traversal

Traverse chunks until the required `fmt ` and `data` chunks are resolved or the RIFF boundary is exhausted.

Chunk traversal uses:

```text
next = payloadStart + chunkSize + (chunkSize & 1)
```

The pad byte is not part of the chunk payload.

Unknown chunks are skipped with the same checked boundary arithmetic.

### A3. Chunk order

Do not stop parsing merely because `data` appears before `fmt `.

The metadata probe may record the data offset/size and continue walking the RIFF container until a valid `fmt ` is found.

No PCM data is allocated or read during this scan.

### A4. `fmt ` hardening

For schema accepted by 0.9.5-A:

- `fmt ` payload must contain at least the canonical 16-byte PCM fields;
- `audioFormat == 1`;
- channels exactly `1` or `2`;
- bits exactly `16`;
- sample rate > 0;
- `blockAlign == channels * 2`;
- `byteRate == sampleRate * blockAlign`, checked without overflow.

Additional bytes in a larger PCM `fmt ` chunk are skipped using normal padded chunk traversal.

WAVE_FORMAT_EXTENSIBLE, float, 8/24/32-bit PCM and compressed formats remain unsupported in 0.9.5.

### A5. Duplicate required chunks

Research default for production implementation:

- reject a second `fmt ` chunk;
- reject a second `data` chunk if traversal encounters it before metadata resolution completes;
- never silently combine multiple data chunks.

This keeps the supported format deterministic and small.

If later compatibility evidence requires multiple data chunks, treat that as a separate extension rather than weakening the initial parser silently.

### A6. Exact decoded allocation

The final resident PCM format remains:

```text
signed PCM16
mono
numFrames * 2 bytes
```

Mono source:

```text
allocate decodedBytes once
-> read data directly into final buffer
```

Stereo source:

```text
allocate decodedBytes once
-> read bounded stereo chunks into scratch
-> mix L/R to final mono buffer progressively
```

Never allocate the full stereo payload as the resident candidate.

Never use:

```text
full stereo allocation
+
second mono allocation
```

### A7. Stereo scratch

Use a small fixed/bounded control-thread scratch buffer. Research target:

```text
512 bytes
```

The exact compile-time size may change after stack review, but it must remain bounded and independent of WAV duration.

For 16-bit stereo, every scratch read must contain complete 4-byte frames. The final short chunk must therefore be frame aligned.

### A8. Allocation/accounting consistency

`RamSampleStore::currentPoolUsage_` must equal actual resident PCM allocation ownership.

There is no permitted fallback where a stereo-sized allocation is retained while the store accounts only decoded mono bytes.

If final mono allocation or conversion cannot complete, loading fails and all temporary/final allocations owned by the attempt are released.

### A9. Preserve external API where practical

Prefer retaining the existing recovery-level control API:

```cpp
bool inspectWavFileBounded(const char* path, WavInfo& outInfo,
                           std::size_t maxDecodedBytes);

bool loadWavFileBounded(const char* path, WavInfo& outInfo,
                        int16_t** outPcm,
                        std::size_t maxDecodedBytes);
```

Internal parser types may change.

Do not widen `SampleView`, `SampleHandle`, `SampleSlot`, sampler voice state or realtime Scene just to harden WAV parsing.

---

## 0.9.5-B — canonical storage model

Canonical roots:

```text
/samples/
/kits/
```

Loose sample example:

```text
/samples/kick.wav
```

Kit example:

```text
/kits/SP12/
    kit.json
    kick.wav
    snare.wav
    closed_hat.wav
    open_hat.wav
```

The physical directory name is presentation/storage location, not stable kit identity.

Historical `/bonnethead/...` paths are not the canonical 0.9.5 model.

---

## 0.9.5-B — kit manifest v1 wire contract

### File

Every product kit directory contains:

```text
kit.json
```

Maximum encoded manifest size for v1 research contract:

```text
1536 bytes
```

The bound is sufficient for eight 96-byte asset locators plus the v1 metadata and prevents unbounded JSON materialization.

### Encoding

- UTF-8 without BOM;
- one JSON object;
- no trailing non-whitespace content;
- duplicate JSON keys rejected;
- non-finite JSON values rejected;
- unknown schema-v1 fields rejected.

### Exact top-level fields

```json
{
  "schema": 1,
  "id": "sp12.factory.v1",
  "name": "SP12",
  "pads": [
    {"pad": 1, "file": "kick.wav"},
    {"pad": 2, "file": "snare.wav"}
  ]
}
```

Top-level v1 fields are exactly:

```text
schema
id
name
pads
```

Pad entry fields are exactly:

```text
pad
file
```

### Stable kit identity

`id`:

- 1..64 ASCII characters;
- begins with `[a-z0-9]`;
- remaining characters restricted to `[a-z0-9._-]`;
- independent from physical directory name.

If two discovered manifests claim the same stable kit ID, the catalog fails that identity closed. Do not use enumeration order as ownership.

### Display name

`name`:

- non-empty UTF-8;
- maximum 32 encoded bytes.

### Pads

v1 contains 1..8 assignments.

User-facing pad indices are exactly:

```text
1..8
```

Duplicate pad assignments are rejected.

Pads 9..16 remain outside the recovered product surface in this schema.

### Asset locator

`file` is relative to the physical kit directory and:

- maximum 96 encoded bytes;
- must end in `.wav` case-insensitively;
- no absolute path;
- no backslash alias;
- no empty component;
- no `.` component;
- no `..` component;
- no NUL/control characters.

The resolved physical path must remain inside the selected kit directory.

---

## 0.9.5-B — bounded catalog ownership

Do **not** recursively index every WAV under `/kits` into the current global `SampleIndex` at boot.

Desired ownership:

```text
SampleIndex
    loose /samples catalog / current stable sample registry

KitCatalog
    bounded kit metadata: stable id + physical directory + display name

KitLoadPlan
    only the currently selected kit's <=8 asset resolutions / WAV metadata
```

The number of WAVs stored on SD must not linearly expand boot-time strings/maps in the active sampler warehouse.

### Kit discovery

Kit catalog discovery reads immediate child directories of `/kits` and validates each bounded `kit.json`.

It does not decode WAV audio.

A malformed manifest is omitted/reported; it must not corrupt the loose sample registry or active kit.

### Stable locator for persistence/UX

A kit asset's reversible product locator is based on stable kit identity:

```text
kit:<stable-kit-id>:<relative-asset-path>
```

Example:

```text
kit:sp12.factory.v1:snare.wav
```

Renaming the physical directory from `SP12` to `MySP12` therefore does not change logical kit identity.

Physical resolution at load time is:

```text
stable kit id
-> current catalog directory
-> validated relative asset path
```

---

## Explicitly not in A/B

Do not pull the following into A/B:

- fixed sampler arena decision;
- SD realtime streaming;
- slicing;
- timestretch;
- recording;
- waveform editor;
- round robin;
- arbitrary 16-pad product UI expansion;
- WAVE_FORMAT_EXTENSIBLE;
- per-pad effect chains;
- recursive whole-library sample indexing.

Transactional multi-pad publication belongs to 0.9.5-C. RELINK UI belongs to D. Allocator selection belongs to E.

---

## Build / flash steps

### Research gates

From repository root:

```bash
bash tests/run_sampler_0_9_5_research_tests.sh
```

### Production A/B candidate

Only after final 0.9.4 is frozen, build from its exact descendant:

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash using the normal Cardputer workflow:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

---

## Expected behavior

### A — screen

No new user-visible page is required by WAV hardening. Existing sample selection remains functional.

Malformed/unsupported WAV selection fails without replacing the current pad assignment.

### A — serial

The loader should identify a bounded failure class sufficiently for hardware diagnosis: malformed RIFF/chunk, unsupported PCM format, decoded size rejection, allocation failure, or read failure.

Exact log strings are implementation-owned.

### B — screen

Kit selection eventually lists canonical valid kits by display name. Invalid manifests do not appear as usable kits.

Renaming a kit directory without changing manifest `id` does not create a new logical kit identity.

### B — serial

Catalog diagnostics should report rejected malformed manifests and duplicate stable kit identities without crashing or mutating the active sample registry.

---

## Troubleshooting

### Odd-sized metadata chunk breaks discovery

Check that chunk traversal advances by `chunkSize + (chunkSize & 1)` and validates the padded end against the RIFF boundary.

### Stereo WAV passes admission but malloc fails at roughly twice decoded size

The old full-stereo transient path was reintroduced. The 0.9.5-A decoder must allocate only final decoded mono PCM plus bounded scratch.

### `freePoolBytes()` looks correct but actual heap usage is larger

Check that the loader did not retain a stereo-sized buffer while the store accounted only `numFrames * 2` bytes.

### Kit directory rename breaks project locator

The persistence locator is incorrectly based on physical directory name/path instead of stable manifest `id`.

### Hundreds of kits consume boot heap

The implementation is likely recursively materializing kit WAV paths. Keep KitCatalog metadata bounded and resolve WAV assets only for the selected kit.

### Two kits share one ID

Fail that logical ID closed and report the duplicate. Never pick the first/last directory silently.

---

## Acceptance checklist

### Research

- [ ] final 0.9.3 two-pass preload/admission behavior is preserved as the A starting point;
- [ ] RIFF declared boundary is validated;
- [ ] odd chunk padding is part of traversal contract;
- [ ] chunk order does not require `fmt ` before `data`;
- [ ] `fmt ` minimum length and `byteRate` are validated;
- [ ] duplicate required chunks have deterministic fail-closed behavior;
- [ ] mono decode uses one final allocation;
- [ ] stereo decode uses one final allocation plus bounded scratch;
- [ ] store accounting equals actual resident allocation;
- [ ] existing loader/store realtime APIs are not widened without necessity;
- [ ] `/samples` and `/kits` are canonical roots;
- [ ] `kit.json` v1 is <=1536 bytes and strict UTF-8 JSON;
- [ ] stable kit identity is independent from directory name;
- [ ] v1 exposes pads 1..8 only;
- [ ] asset paths are bounded and traversal-safe;
- [ ] KitCatalog does not recursively materialize all WAVs at boot;
- [ ] duplicate kit IDs fail closed.

### Future Cardputer ADV

- [ ] mono PCM16 valid fixture loads and plays;
- [ ] stereo PCM16 valid fixture chunk-converts and plays as mono;
- [ ] odd-size unknown chunk fixture loads correctly;
- [ ] data-before-fmt fixture resolves correctly;
- [ ] malformed/truncated/overflow fixtures fail without pad mutation or WDT;
- [ ] stereo loading does not show a full-stereo + mono transient heap peak;
- [ ] canonical `/kits` catalog enumerates valid manifests;
- [ ] malformed/duplicate-ID kits fail closed;
- [ ] large kit library does not linearly materialize every WAV into boot registry;
- [ ] existing sampler playback, Scene Save/Load and 1/4/8 voice behavior remain intact.
