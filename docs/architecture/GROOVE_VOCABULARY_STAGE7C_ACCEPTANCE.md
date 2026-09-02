# Generation Stage 7C — Production Rhythm Routing Acceptance

Status: host-complete; Cardputer/SEQTRAK listening smoke pending

Implementation base record:

```text
current origin/dev_0.9_test: 0a2fe15696eb7c8f0bdcc1a986aed3ca342dd948
stage base:                 874a91f
merge-base with devtest:    0a2fe15696eb7c8f0bdcc1a986aed3ca342dd948
devtest advanced in stage:  no
```

Independent AY/SID articulation follow-ups, unified note ownership, DSP musical
regressions and event/performance looper work were deliberately excluded. None
blocks one-bar drum rhythm selection in Stage 7C.

## Purpose

Validate that all 24 admitted `ReferenceVocabulary` identities are reachable
through the ordinary `GENERATE -> GENRE` workflow without the Stage 7A audition
screen. This stage changes rhythm selection and drum materialization only. Legacy
synth pitch, synth TYPE/patch, automation, FEEL, transport, MIDI routing and clock
remain authoritative.

## Architecture contract

```text
Scene Genre/Variant + RHYTHM intent + pattern address
                         |
                         v
              rhythm compatibility data
                         |
                         v
             AUTO/MANUAL stable rhythm ID
                         |
                         v
            existing RhythmRealizer/materializer
```

- `AUTO` sorts compatible candidates by stable ID before weighted selection.
- `MANUAL` fixes the stable rhythm ID; seed/address/P-level vary realization only.
- An incompatible or unknown manual ID resolves as `AUTO` and is reported to the UI.
- Scene persists only `rsm` (`AUTO/MANUAL`) and `rid` (manual stable ID).
- Missing legacy fields decode as `AUTO` with ID `0`.
- Compatibility tables are fixed-capacity static data; selection allocates no heap.

## Host validation

From the repository root:

```bash
ASAN_OPTIONS=detect_leaks=0 bash tests/run_rhythm_stage7c_tests.sh
g++ -std=c++17 -Wall -Wextra -Werror \
  -Wno-unused-variable -Wno-unused-but-set-variable \
  -Wno-c++20-extensions -I. -Iplatform_sdl \
  -include platform_sdl/arduino_compat.h \
  tests/test_scene_roundtrip.cpp scenes.cpp json_evented.cpp \
  src/audio/pattern_paging.cpp \
  -o build/host-tests/test_scene_roundtrip
build/host-tests/test_scene_roundtrip
```

The Stage 7C matrix proves:

- 24/24 stable IDs occur in at least one production compatibility profile;
- the deterministic AUTO corpus selects 24/24 IDs;
- AUTO never selects outside the active profile;
- reversing compatibility-table declaration order does not change selection;
- every MANUAL ID survives address and P1/P2/P3 changes;
- every MANUAL ID has measured P2 topology variation;
- incompatible MANUAL intent falls back observably to AUTO;
- Scene manual intent round-trips and legacy JSON decodes as AUTO.

## Hardware list

- M5Stack Cardputer ADV with GroovePuter Stage 7C firmware.
- Optional Yamaha SEQTRAK connected by the already accepted USB-MIDI path.
- Headphones or powered speakers. Keep the initial level low.

No new wiring, I2C device or GPIO assignment is introduced by this stage.

## Build and flash

Use the existing Cardputer ADV normal and fixed-DRAM build scripts. Record the
exact git SHA and build profile before listening. Do not classify musical quality
from SDL or host tests.

## Expected behavior

1. Open `GENERATE -> GENRE`.
2. Use `Tab` or `Up/Down` to focus `RHYTHM`.
3. `Left/Right` cycles `AUTO` and only compatible named identities.
4. Set `APPLY` to `MATERIALIZE` and press `Enter`.
5. The named manual identity stays selected while changing pattern slot/address.
6. Changing to an incompatible Genre/Variant resets the row to `AUTO` and shows
   `RHYTHM RESET TO AUTO` once.
7. Save, reboot and load: AUTO/MANUAL selection intent returns unchanged.

## Troubleshooting

- Only `AUTO` appears: confirm the pending Genre/Variant is valid, then leave and
  re-enter the page after loading the Scene.
- A manual rhythm resets: the selected identity is not compatible with the new
  Genre/Variant; this is intentional.
- Synth notes or timbre differ: Stage 7C does not own those layers. Reproduce on
  the frozen base SHA before attributing the change to rhythm routing.
- No audible change: use `MATERIALIZE`, not `PROFILE ONLY`, and compare two
  structurally distinct manual identities in the same compatible profile.

## Acceptance checklist

- [ ] Cardputer layout fits 240x135 with all five rows readable.
- [ ] One AUTO result is audible in every affected Genre family.
- [ ] `stacked_quarters` is reachable from Minimal/Techno.
- [ ] `electro_backskip` is reachable from Electro.
- [ ] `funk_house_bridge` is reachable from Minimal/TripHop.
- [ ] `electro_gap_push` is reachable from Electro/TripHop.
- [ ] Manual selection survives Save/reboot/Load.
- [ ] Incompatible Genre/Variant change resets MANUAL to AUTO with one notice.
- [ ] P1/P2/P3 preserve the named identity.
- [ ] No stuck note, clock, routing, synth TYPE/patch or automation regression.
- [ ] Result is recorded as `HARDWARE_ACCEPTED` or `HARDWARE_PENDING` on the
      frozen SHA; host results alone are never described as listening approval.
