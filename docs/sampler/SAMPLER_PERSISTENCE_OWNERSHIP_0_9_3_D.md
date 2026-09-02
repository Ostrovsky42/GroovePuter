# GroovePuter 0.9.3-D — Sampler Persistence Ownership

Base: `dev_0.9.3 @ ad4e07c1f84eeaa6b6cd1a003b3966afad2296e2`

Status: DRAFT / CI + Cardputer ADV acceptance required before merge.

## Purpose

Persist sampler pad identity by the stable 64-bit path-derived `SampleRef` introduced in 0.9.3-B, while keeping the existing realtime sampler ABI compact and preserving old `uint32_t` Scene assignments.

D is a recovery/integration change, not a sampler redesign.

The existing runtime remains:

- 16 pads;
- 8 sampler voices;
- 32-bit `SampleId` / handle identity in the audio path;
- existing pitch, reverse, loop, start/end, choke and velocity behavior;
- existing `RamSampleStore` and preload path.

## Persistence contract

New Cardputer Scene JSON keeps the historical numeric `id` and adds an exact stable ref string:

```json
{"id":123456789,"ref":"a17f340bc82d119e","vol":1,"pch":1,"str":0,"end":22050,"chk":1,"rev":false,"lop":false}
```

`ref` is 16 hexadecimal characters representing the full 64-bit `SampleRef`.

It is deliberately **not** a JSON number. The streaming Scene parser represents numeric primitives through `double`; integers above `2^53` cannot be represented exactly there.

Rules:

1. If `ref` is present, it is authoritative.
2. A malformed, zero, or unresolved `ref` fails Scene load transactionally.
3. No fallback from a bad stable ref to the legacy numeric `id` is allowed.
4. An old Scene with no `ref` remains readable.
5. An old unambiguous legacy `id` is translated to the current runtime ID.
6. An old missing legacy `id` retains the historical missing-sample behavior: Scene loads, existing preload reports the missing registry entry.
7. An old ambiguous legacy collision fails closed.
8. Saving a currently resolved runtime sample writes its stable ref.
9. Saving an unresolved legacy assignment does not invent a stable ref.

## Runtime identity

`Scene`, `SamplerPadState`, `SampleId`, `SampleHandle`, `SampleSlot` and the audio thread do not gain a 64-bit field.

For files whose old basename FNV32 ID is unique, the runtime ID stays exactly the old value.

For a legacy collision set, D creates deterministic compact runtime IDs from stable refs while reserving every historical basename ID. This means an old ambiguous value can never accidentally become a valid runtime binding for one member of the collision set.

The frozen real collision pair remains:

- `5oetw2k1.wav`
- `qp363n87.wav`
- legacy FNV-1a32: `3960902837`

Under D:

- both files can be registered and addressed by stable Scene refs;
- neither is registered as runtime ID `3960902837`;
- an old Scene containing only `3960902837` still fails closed.

## Streaming / memory boundary

Cardputer persistence is filtered at `SceneStorageCardputer`, not by expanding `Scene`.

The filter buffers at most one flat sampler pad object:

- input scratch: 384 B;
- transformed output scratch: 448 B;
- no second Scene object;
- no full Scene JSON string on the Cardputer manager read/write path;
- no PCM allocation in the filter.

Both main Scene and `.auto` recovery Scene use the same filter.

The public raw `writeScene(std::string)` overload remains raw to prevent accidental double encoding.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- microSD card
- USB-C data cable
- PSRAM-disabled GroovePuter build
- one or more valid WAV files under `/samples` (historical `/sd/samples` alias is also probed)
- Yamaha SEQTRAK is optional for D; USB-MIDI issue #268 is a separate connected-device reproduction

## Wiring

No external wiring is required for sampler persistence.

PORT.A / I2C is not used by this test.

SEQTRAK does not need to be connected for D sampler acceptance.

## Build / flash

Dedicated host contract:

```bash
bash tests/run_sampler_persistence_ownership_tests.sh
bash tests/run_sampler_registry_boot_tests.sh
```

Normal product gates:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash / monitor:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Host

The D contract proves:

- 64-bit refs above `2^53` round-trip exactly as 16-char hex;
- all existing pad parameters survive Save/Load transformation;
- old id-only unique scenes still load;
- missing old IDs remain missing rather than rebinding;
- malformed stable refs fail closed;
- unresolved stable refs fail closed;
- the real FNV32 collision pair receives two different runtime IDs;
- neither collision runtime ID reuses `3960902837`;
- both collision samples save/load through stable refs;
- old ambiguous `3960902837` cannot resolve;
- reader/writer operation is streaming and bounded;
- oversized pad objects are rejected rather than overflowing scratch.

### Cardputer ADV

Cold boot ordering from C must remain:

```text
[SD] mount result=1 ...
SampleIndex::scanDirectory: Scanning ...
[SAMPLER-REGISTRY] ready discovered=... registered=...
...
6. Engine Init...
  - MiniAcid::init: Loading scene from storage...
```

For a normal unique sample, Save must write `"ref":"................"` beside the pad `id` in the Scene file.

After reboot, the same pad must resolve and existing preload must load the same WAV.

PLAY must trigger the same sample and preserve pad parameters.

## Hardware acceptance procedure

1. Put a known short WAV at `/samples/d_accept.wav`.
2. Boot D firmware and confirm `[SAMPLER-REGISTRY] ready` precedes engine Scene load.
3. Assign the WAV to one sampler pad using any currently available recovery/test assignment path.
4. Set non-default pad values that are easy to recognize: volume, pitch, start/end, choke, reverse or loop.
5. Save the Scene.
6. Power-cycle the Cardputer; do not only soft-reload.
7. Load the same Scene if it is not the boot Scene.
8. Confirm the same sample is assigned and the pad parameters are unchanged.
9. Start PLAY and verify the sample triggers without crash/reset.
10. Observe audio diagnostics / serial and confirm `underruns=0` during the smoke.
11. Record fixed DRAM output from the normal Cardputer budget gate.

Optional collision acceptance, if the two frozen test WAV names are placed in `/samples`:

- boot should report `legacyReject=2` but `registered` should still include both files;
- the registry note should say ambiguous legacy IDs require stable Scene refs;
- neither file should silently own the old shared ID.

## Troubleshooting

If a new Scene saves only `id` and no `ref` for a WAV that is present, verify the C registry ran before Scene save and that the pad runtime ID resolves in `SampleIndex`.

If a Scene with a valid-looking `ref` refuses to load, confirm the exact WAV path still resolves to the same stable ref. D intentionally fails closed instead of rebinding a different file.

If an old Scene with a removed WAV still loads but preload reports the ID missing, that is the preserved legacy missing-sample behavior. Product missing-sample UX belongs to 0.9.4.

If an old Scene uses the known ambiguous FNV32 collision ID, it is intentionally not recoverable by choosing first/last file. Reassign one of the physical WAVs through a stable-ref capable workflow and save again.

If fixed DRAM grows materially, stop. D must not add `SampleRef` to `Scene` or realtime pad structures.

If audio underruns appear, capture the first underrun and memory telemetry. D does no audio-thread work; a new underrun would be a regression.

USB-MIDI endpoint stall tracking remains issue #268 and requires reproduction with SEQTRAK physically connected. Do not fold it into D.

## Acceptance checklist

### Identity / schema

- [ ] `SampleRef > 2^53` exact hex round-trip passes.
- [ ] New persisted pad contains both legacy `id` and stable `ref`.
- [ ] Stable `ref` is authoritative on load.
- [ ] Malformed/zero/unresolved stable ref fails closed.
- [ ] Old id-only unique Scene remains readable.
- [ ] Old missing ID does not silently rebind.
- [ ] Old ambiguous legacy ID fails closed.
- [ ] Known collision pair is independently addressable by stable refs.

### Memory / realtime

- [ ] `SamplerPadState` remains 32-bit `sampleId`; no resident 64-bit ref array.
- [ ] `SampleId` / `SampleHandle` audio ABI is unchanged.
- [ ] Cardputer Scene read/write remains streaming.
- [ ] No PCM preload or SD I/O moves into the audio thread.
- [ ] Fixed DRAM gate remains green.
- [ ] Hardware smoke reports `underruns=0`.

### Persistence behavior

- [ ] Main Scene Save -> power-cycle -> Load returns the same WAV.
- [ ] Volume survives.
- [ ] Pitch survives.
- [ ] Start/end survives.
- [ ] Choke survives.
- [ ] Reverse survives.
- [ ] Loop survives.
- [ ] `.auto` recovery path uses the same stable-ref filter.

### Regression

- [ ] Dedicated D workflow green.
- [ ] Existing sampler registry/boot workflow green.
- [ ] Full host/Core suite green.
- [ ] SDL green.
- [ ] Cardputer ADV normal build green.
- [ ] Cardputer ADV fixed DRAM green.
- [ ] SEQTRAK MIDI-only build green.
- [ ] Existing synth persistence / Stage 15 matrix green.

## Out of scope

Do not pull into 0.9.3-D:

- async/control-side preload queue (`0.9.3-E`)
- SamplerPage workflow recovery (`0.9.3-F`)
- broad WAV parser changes
- stereo-to-mono conversion changes
- kit redesign / transactional kits
- arena / streaming experiments
- SYNTH/SAMPLE/LAYER output ownership
- missing-sample product UX
- USB-MIDI issue #268
- Tape/Voice/Recorder recovery
