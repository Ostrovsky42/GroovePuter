# Generation Stage 15 — Final Tonal Integration Acceptance

## Purpose

Stage 15 closes the current generation stack by giving production generation one explicit owner of **absolute MIDI pitch** while preserving the already-reviewed owners of rhythm, FEEL, harmonic timing, semantic intent and physical synth allocation.

Final production path:

```text
Genre / Variant data
        |
        +--> BassRhythm --------> BassPitchBehavior ----+
        |                                               |
        +--> ChordRhythm --> ChordProgression ----------+--> TonalMaterializer
        |                                               |        |
        +--> MelodicMotif --> MelodicPitchIntent -------+        v
        |                                                    TonalProjector
        |                                                         |
        +---------------------------------------------------------+
                                                                  v
                                                         absolute MIDI notes
                                                                  |
                                                         SynthPattern adapter
                                                                  |
                                                               FEEL
                                                                  |
                                                    existing Stage 14 commit
```

`TonalMaterializer` owns only the conversion from semantic tonal intent plus the existing harmonic timeline into absolute MIDI notes. It does not own Scene, Song, PhraseCore, rhythm generation, FEEL, synth engine selection or persistence.

This is the **final Stage 15 integration**. There is no Stage 15D/E/F after this acceptance.

## Hardware list

- M5Stack Cardputer-Adv / Cardputer ADV (ESP32-S3).
- USB-C cable for build, flash and Serial monitoring.
- Built-in speaker for primary musical audition.
- Optional Yamaha SEQTRAK for confirming the same generated absolute MIDI notes through the existing MIDI route.

No external I2C hardware is required.

## Wiring

No new wiring or peripheral ownership is introduced.

- Cardputer ADV is powered/programmed over USB-C.
- Existing PORT.A invariant remains GPIO2 SDA / GPIO1 SCL.
- No new GPIO, I2C, SPI or UART use is introduced by Stage 15.
- Existing audio and MIDI routing remain unchanged.

## Build / Flash

Automated focused gates:

```bash
bash tests/run_tonal_projector_tests.sh
bash tests/run_tonal_materializer_tests.sh
bash tests/run_tonal_materializer_global_scale_test.sh
bash tests/run_stage15_tonal_integration_tests.sh
bash tests/run_stage15_tonal_register_sweep.sh
bash tests/run_stage15_tonal_baseline_dump.sh > /tmp/stage15_legacy.tsv
diff -u tests/data/stage15_tonal_legacy_baseline.tsv /tmp/stage15_legacy.tsv
```

Repository-wide gate:

```bash
bash tests/run_host_tests.sh
```

The final frozen SHA must also pass:

- SDL build;
- Cardputer ADV normal build;
- fixed DRAM gate;
- SEQTRAK MIDI-only build;
- Stage 15B Melody-current gate;
- Stage 15C Bass-current gate;
- Tonal Projector gate;
- scale-quantization gate;
- synth persistence / Phrase regressions.

Flash the **exact frozen PR head** reported in the final PR checkpoint. Do not audition a moving branch.

## Expected behavior

### Ownership

- Live strong-rhythm regeneration passes `Scene.generatorParams.scaleRoot` and `ScaleType` only as transient values in `StrongRhythmMigrationContext`.
- `roles/` never call Tonal Projector or TonalMaterializer.
- `tonal/` never imports Scene, Song, PhraseCore or physical synth types.
- ChordRhythm remains the owner of harmonic event timing.
- ChordProgression supplies degree / quality / chromatic root-offset content only.
- BassPitchBehavior and MelodicPitchIntent supply semantic contour intent only.
- TonalMaterializer resolves every scale-degree target against the one Scene/global `ScaleType`.
- Each harmonic event is projected independently into the role corridor and may therefore choose its own TonalProjector root anchor.
- Event-local anchor selection is not voice leading: no previous event note is used to choose the next event anchor.
- FEEL is applied after pitch materialization exactly as before.
- The legacy `projectLegacyPitchPattern` path remains available when tonal materialization is explicitly disabled and must continue to match the frozen legacy corpus byte-for-byte.

### Register corridors

Production tonal profiles use non-overlapping role corridors inside the existing supported synth-note range:

```text
Synth A / bass       24..47
Synth B / secondary  48..71
```

Every exact generation profile and every production pattern address `0..255` is swept at the canonical C/Dorian context. The address-space contract therefore covers `33 × 256 = 8448` production cases; the bridge does not accept 512 distinct addresses because `kMaxGlobalPatterns == 256`.

A second exhaustive key/scale gate checks all exact profiles across all ten `ScaleType` values, all twelve roots and eight deterministic addresses per profile:

```text
33 profiles × 10 scales × 12 roots × 8 addresses = 31,680 cases
```

Together the production register gate executes **40,128 cases**. Every successful case must keep Synth A in `24..47` and Synth B in `48..71`.

Within one harmonic event, the role-specific adjacent-leap guard remains active. A transition to a new harmonic event starts a new projector request/root anchor; there is deliberately no cross-event leap optimization or voice-leading policy in Stage 15.

### Conservative accepted genres

Base House, Techno and Rave use conservative tonal policy:

```text
Bass:   RootAnchor only
Melody: Static only
```

This prevents the new owner from adding a new contour axis to styles already accepted primarily for their rhythm/FEEL identity. The hardware verdict is still auditory: no unacceptable tonal regression is allowed.

### Styles expected to gain movement

The production data profiles permit wider tonal intent for Acid, Outrun, Darksynth, Electro, Broken, TripHop, HipHop, FunkSoul, UK Garage, Drum & Bass, LoFi, Reggae and Chip where the existing rhythmic framework alone did not provide sufficient pitch motion.

`RootOctave` remains a valid Stage 15C API vocabulary item but is intentionally excluded from current production AUTO synth-profile selection: with a moving harmonic root it cannot be guaranteed to fit the fixed bass corridor for every scale/progression combination without octave-folding or register collision. No projector invariant is weakened to force it.

### Global-scale harmonic semantics

A harmonic event changes the active harmonic root, not the global scale definition. Degree intent is evaluated against `Scene.generatorParams.scale` before being converted to an exact event-local semitone displacement.

Canonical regression:

```text
Scene: C major
harmonic event: B
melodic intent: +1 scale degree
result: C
not: C#
```

Chromatic `rootOffsetSemitones` shifts the event root after the global scale degree has been resolved. TonalProjector receives exact tagged semitone offsets for event-local projection; it does not receive a transposed `ScaleType`.

### Absolute-pitch independence from legacy source

When tonal materialization is enabled, a generated bass/melody/chord plan must still materialize if the compatibility source pattern contains no pitch notes. Legacy source pitch is metadata fallback only; it is no longer the owner of generated pitch.

## Frozen A/B measurement corpus

Pre-materialization golden:

```text
tests/data/stage15_tonal_legacy_baseline.tsv
```

It contains all 16 base modes × pattern addresses `0..7` × A/B and separates:

- topology hash;
- pitch hash;
- articulation hash;
- full-step hash.

The golden is a measurement oracle, not a claim that all pitch hashes remain equal. The production integration must preserve semantic topology under identical generation context while pitch differences are classified by selected progression/contour.

## Hardware audition matrix

Use base recipe unless noted. Compare the exact same pattern address before/after tonal materialization logic; do not judge randomly changing seeds.

| Direction | Pattern addresses | Narrow verdict |
| --- | --- | --- |
| Acid | 1, 3, 5 | bass has audible pitch movement on at least one case; timing/groove remains intact |
| Outrun | 0, 3, 6 | tonal bass/lead motion is audible; A/B registers remain separated |
| Darksynth | 1, 5 | previously projection-stressing cases generate cleanly with no missing bar or register jump outside the bass range |
| Electro | 2, 6 | bass movement remains coherent with broken rhythm |
| LoFi | 0, 4, 7 | movement remains restrained; no forced busy melody |
| Techno | 0, 3, 6 | no unwanted melodic/bass contour is introduced |
| House | 0, 3, 6 | no unwanted tonal wandering is introduced |
| Rave | 0, 3, 6 | accepted rhythmic character remains intact |

For each case also listen for:

- bass and secondary voice occupying clearly separate registers;
- no stuck notes;
- no missing generated Synth A/B pattern;
- no octave-fold artifacts;
- no unexpected rhythm/onset movement caused by tonal code;
- chord/event transitions sounding intentional rather than like octave jumps chosen to connect adjacent events.

Status remains **HARDWARE_PENDING** until this matrix is heard on Cardputer ADV.

## Serial / screen acceptance

Stage 15 adds no required UI page, label or toast. Existing generation UI should behave as before. No new Serial protocol is required.

If the normal firmware displays a new Stage 15-specific page or requires a new user action merely to generate tonal material, the integration has crossed its scope.

## Troubleshooting

### `ProjectionFailed / NoteOutOfRegister`

Do not octave-fold in TonalProjector and do not widen bass into the secondary register. Check the selected production contour/progression policy first. A contour that cannot be guaranteed inside its role corridor should not be reachable from that AUTO profile.

### `ProjectionFailed / LeapExceeded`

Check the role-specific `TonalRegisterCorridor` policy. The limit is profile data, not a global projector constant. The guard applies within one harmonic-event projection. Do not add cross-event voice leading or remove the TonalProjector guard globally to make a failing case pass.

### Legacy baseline changes

If `stage15_tonal_legacy_baseline.tsv` no longer matches when `tonalMaterializationEnabled == false`, treat that as a regression. Do not regenerate the golden to make CI green.

### Topology differs between legacy and tonal paths

TonalMaterializer must not move onsets/continuations. Check BassPitchBehavior/MelodicPitchIntent policy first: production currently keeps melodic rhythm operation `Preserve`, and bass timing is immutable by Stage 15C.

### Empty source pattern fails

The new tonal path must not return `MissingPitchSource`. Check `TonalPatternAdapter`: source pattern data is only compatibility metadata after absolute MIDI has already been materialized.

## Assertion mutation ledger

| Assertion | Production mutation that must fail it |
| --- | --- |
| legacy rollback remains byte-identical | change the `!tonalMaterializationEnabled` legacy projector branch |
| key/scale stay Scene-owned | add Scene access inside `roles/` or `tonal/` |
| roles do not own MIDI projection | call `projectTonalIntent()` from BassPitchBehavior or MelodicPitchIntent |
| ChordRhythm owns harmonic timing | derive new harmonic event positions inside TonalMaterializer |
| event-local anchors stay independent | collapse all harmonic events into one phrase-global TonalProjectionRequest |
| global scale is preserved across events | derive or transpose a new `ScaleType` from the active harmonic root |
| bass/secondary registers never overlap | raise bass max to 48 or lower secondary min to 47 |
| all production addresses materialize | introduce a contour/progression combination that returns projection failure in the 8448-case sweep |
| all key/scale contexts materialize | introduce a root/scale-dependent failure in the 31,680-case sweep |
| exact fifth stays chromatic | route tagged `+7` through scale-degree conversion |
| pentatonic/chromatic cardinality is correct | restore `% 7` or a fixed seven-entry scale loop |
| empty legacy pitch source still works | require `projectLegacyPitchPattern` before tonal materialization |
| no hidden Genre switch in tonal layer | add `switch (GenerativeMode)` to `roles/`, `tonal/` or tonal policy resolver |
| materializer stays bounded | introduce VLA/heap allocation or exceed stack/sizeof gates |
| failed integration is atomic | write caller SynthPattern before all role projections/adapters succeed |

## Explicit non-goals / next musical layer

Stage 15 does **not** add:

- polyphony;
- chord voicing / inversion / Drop2;
- voice leading;
- SATB / voice crossing;
- a separate arpeggiator generation axis;
- multi-bar production generation;
- a new Scene persistence field;
- a new RNG domain.

`ChordQuality` is carried by the harmonic plan but current production timing still models one `ChordRhythm` onset as one harmonic event. Making `Minor7/Major7/...` audible as multiple chord tones requires an explicit harmonic-event grouping / voicing contract. That is the next musical layer, not a hidden Stage 15 substage.

## Acceptance checklist

- [ ] Scale interval data for generation has one canonical ScaleCatalog source.
- [ ] Legacy AdvancedPatternGenerator consumes ScaleCatalog and keeps exact seven-mode compatibility plus corrected pentatonic/chromatic behavior.
- [ ] TonalProjector consumes ScaleCatalog and all its existing tests pass.
- [ ] TonalMaterializer source ownership gate passes.
- [ ] TonalMaterializer GCC/Clang/ASan+UBSan gate passes.
- [ ] TonalMaterializer global-scale/event-local harmonic regression passes.
- [ ] TonalMaterializer measured stack stays under its explicit gate.
- [ ] Stage 15 production integration GCC/Clang/ASan+UBSan gate passes.
- [ ] 16 base modes × 8 addresses deterministic matrix passes.
- [ ] 33 exact profiles × all 256 production addresses register sweep passes (`8,448`).
- [ ] 33 exact profiles × 10 scales × 12 roots × 8 addresses sweep passes (`31,680`).
- [ ] Synth A notes stay in 24..47.
- [ ] Synth B notes stay in 48..71.
- [ ] Legacy tonal baseline remains byte-for-byte unchanged with tonal materialization disabled.
- [ ] Empty compatibility source still produces tonal material when intent exists.
- [ ] Invalid tonal context leaves drums/A/B untouched.
- [ ] `GenerationCompositionResult` stays under its existing 32 B ceiling; no tonal fields are added to it.
- [ ] Full repository host suite passes.
- [ ] SDL passes.
- [ ] Cardputer ADV normal + fixed DRAM passes.
- [ ] SEQTRAK MIDI-only passes.
- [ ] Three clean review passes complete on one unchanged frozen head.
- [ ] Hardware matrix above is heard on the exact frozen head.
- [ ] Hardware verdict remains **HARDWARE_PENDING** until user confirmation.
