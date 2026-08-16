# GroovePuter 0.9.9-A — Compatibility + Pattern/Phrase Liveness

## Purpose

Freeze the compatibility surface before 0.9.9 changes live activation or arrangement behavior. This stage protects generated Pattern ownership across paging, makes persisted Phrase reference views real Pattern liveness roots, freezes the current generation/Lo-Fi identities, and proves that already-realized Lo-Fi material survives Scene load without regeneration.

Base: `dev_0.9.6` @ `9f80cb179f530089bd46f27e03bdde0f7684ba72`.

This stage intentionally does **not** implement 0.9.8 Undo ownership, quantized activation changes, or live Phrase/Song editing.

## Contracts

- A generated Pattern referenced only by a valid Phrase `ReferenceView` cannot be reclaimed as an orphan.
- `InternalPattern`, `Generated`, and `Derived` Phrase sources are Pattern liveness roots.
- Song + Phrase sharing forces copy-on-write instead of in-place generated reroll.
- Manual/imported non-generated material remains non-reclaimable.
- Generated ownership bit `0x01` survives PatternPagingService save/load, project switching, and backup recovery without a persistence schema change.
- Scene JSON and `.gpp` Pattern pages are separate persistence layers: Scene round-trip owns realized musical step data + generation semantics; `.gpp` paging owns the generated ownership marker.
- Persisted `GenerativeMode` values remain append-only: `Acid=0` through `LoFi=15`.
- Lo-Fi recipe identities remain: `Classic Chill=12`, `Drunken Groove=13`, `Lo-Fi House=14`, `Minimal Sleep=15`, `Golden Era=16`, `Dusty Jazz=17`.
- Lo-Fi remains a semantic identity even where its current low-level parameters share Trip-Hop defaults.
- A realized Lo-Fi synth/drum Pattern is restored byte-for-byte at its musical fields after Scene dump/load instead of being regenerated.
- 0.9.6 Output Ownership and 0.9.7 Device Profile contracts are rerun explicitly by the focused 0.9.9-A workflow.
- Legacy TEXTURE keys remain decode-only/ignored; 0.9.9-A does not restore a TEXTURE runtime owner.

## Hardware list

For host acceptance: no hardware required.

For build acceptance:
- M5Stack Cardputer ADV / ESP32-S3 target supported by the repository toolchain.
- Yamaha SEQTRAK is not required to execute this stage; the repository SEQTRAK MIDI-only firmware variant must compile.

## Wiring

None for host/CI acceptance. No MIDI, I2C, or external display wiring is changed by 0.9.9-A.

## Build / test

Run the focused gate:

```bash
mkdir -p build/host-tests

g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_generation_0_9_9_compatibility.cpp \
  -o build/host-tests/test_generation_0_9_9_compatibility
build/host-tests/test_generation_0_9_9_compatibility
python3 tests/test_generation_0_9_9_source_regressions.py

g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-variable \
  -Wno-unused-but-set-variable -Wno-c++20-extensions \
  -I. -Iplatform_sdl -include platform_sdl/arduino_compat.h \
  tests/test_generation_scene_roundtrip.cpp \
  scenes.cpp json_evented.cpp src/audio/pattern_paging.cpp \
  -o build/host-tests/test_generation_scene_roundtrip
build/host-tests/test_generation_scene_roundtrip

g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_song_pattern_phrase_liveness.cpp \
  -o build/host-tests/test_song_pattern_phrase_liveness
build/host-tests/test_song_pattern_phrase_liveness

g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-c++20-extensions -I. \
  tests/test_pattern_paging_ownership.cpp \
  src/audio/pattern_paging.cpp \
  src/audio/pattern_project_cleanup.cpp \
  -o build/host-tests/test_pattern_paging_ownership
build/host-tests/test_pattern_paging_ownership

bash tests/run_output_ownership_tests.sh
bash tests/run_midi_device_profile_ui_0_9_7_tests.sh
```

Also run the normal repository gates:

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

The PR CI additionally builds SDL and applies the fixed Cardputer ADV DRAM budget gate through `Core regressions`.

## Expected behavior

Host tests exit with status 0. A Phrase-only generated Pattern remains unavailable to the Song generator allocator. A Pattern shared by Song and Phrase uses a new safe slot. Ownership markers are still present after paging/recovery. Generation and Lo-Fi numeric/semantic identities remain unchanged. A serialized Lo-Fi Pattern reloads its existing synth/drum notes, timing, velocity, probability and FX values rather than silently generating new material.

There is no intentional visible UI or musical behavior change in this stage.

## Troubleshooting

- `Phrase-only generated Pattern was reclaimed as an orphan`: Phrase liveness is not included in the allocator reference count; do not promote.
- `copy-on-write destination` failure: Song + Phrase sharing is being treated as unique ownership; do not promote.
- paging ownership assertion failure: generated ownership metadata is being lost by `.gpp` persistence/recovery; do not promote.
- Scene round-trip musical-field assertion failure: realized material changed across load; do not promote.
- Do not require the `.gpp` ownership marker from the Scene JSON codec; ownership persistence is validated by the dedicated PatternPaging gate.
- Lo-Fi ID/name assertion failure: an existing persisted identity was renumbered or renamed; require an explicit migration instead of accepting the change.
- Output Ownership / Device Profile suite failure: treat as a cross-release compatibility regression even if 0.9.9 did not intentionally edit those owners.
- Cardputer/SEQTRAK/SDL build failure: compare the exact candidate SHA against the exact base SHA before classifying it as inherited.

## Acceptance checklist

- [ ] focused `0.9.9-A generation compatibility` workflow PASS
- [ ] Core host regressions PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV normal build PASS
- [ ] Cardputer ADV fixed DRAM gate PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] Phrase-only `InternalPattern` root protected
- [ ] Phrase-only `Generated` root protected
- [ ] Phrase-only `Derived` root protected
- [ ] Song + Phrase sharing uses copy-on-write
- [ ] Pattern ownership survives paging round-trip
- [ ] Pattern ownership survives project switch/reload
- [ ] Pattern ownership survives backup recovery
- [ ] realized Lo-Fi synth/drum material survives Scene round-trip
- [ ] `GenerativeMode::LoFi == 15`
- [ ] Lo-Fi recipe IDs `12..17` unchanged
- [ ] Output Ownership suite PASS
- [ ] Device Profile R8 suite PASS
- [ ] legacy TEXTURE remains decode-only/ignored
- [ ] no 0.9.8 Undo/mutation ownership introduced
- [ ] hardware behavior not claimed from host CI
