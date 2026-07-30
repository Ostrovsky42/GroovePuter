# Atlas runtime compiler

## Purpose

Compile a reviewed subset of SEQTRAK Pattern Atlas v2.6 into small C++ tables
that GroovePuter can use without CSV parsing, dynamic allocation or SD reads in
the audio path.

The first vertical slice contains:

- `REC_ACID_CHICAGO_JACK`;
- P1 `BASE`;
- P2 `DEVELOPMENT`;
- P3 `BREAKDOWN`.

The compiler accepts only the reviewed Atlas archive whose SHA-256 is embedded
in the script. A different or modified archive is rejected instead of silently
producing different firmware data.

## Hardware list

Compilation requires only a development computer with:

- Python 3.10 or newer;
- the Atlas v2.6 ZIP archive;
- the GroovePuter repository.

Runtime acceptance requires:

- M5Stack Cardputer ADV;
- USB-C data cable;
- built-in speaker or headphones.

## Wiring

No external wiring is required.

Cardputer ADV assumptions remain:

- GPIO21 is `PA_EN`, not RGB data;
- the internal ES8311 codec uses the board I2S path;
- no Atlas operation performs I/O from the audio callback.

## Build and flash

Generate the checked-in files:

```bash
python3 tools/atlas/compile_chicago_runtime.py \
  /path/to/seqtrak_pattern_atlas_csv_v2_6.zip \
  /tmp/atlas-generated

diff -u src/generated/atlas_runtime_types.generated.h \
  /tmp/atlas-generated/atlas_runtime_types.generated.h
diff -u src/generated/rec_acid_chicago_jack.generated.h \
  /tmp/atlas-generated/rec_acid_chicago_jack.generated.h
diff -u src/generated/atlas_runtime.generated.h \
  /tmp/atlas-generated/atlas_runtime.generated.h
```

Run host acceptance:

```bash
bash tests/run_host_tests.sh
```

Then build and flash the normal Cardputer ADV sketch from the recovery branch.
No separate data upload is required because the compiled tables are firmware
sources.

## Track mapping

| Atlas track | GroovePuter runtime target |
|---|---|
| KICK | Drum voice 0 |
| SNARE | Drum voice 1 |
| HAT1 | Closed hat, voice 2 |
| HAT2 | Open hat, voice 3 |
| PERC1 | Mid tom/percussion, voice 4 |
| PERC2 | High tom/percussion, voice 5 |
| RIM | Rim, voice 6 |
| CLAP | Clap, voice 7 |
| SYNTH1 | Synth A |
| SYNTH2 | Synth B, chord-root preview |
| DX | Synth B, melodic preview |
| SAMPLER | Not applied yet |

When SYNTH2 and DX occupy the same step, SYNTH2 has priority because the current
GroovePuter synth track is monophonic and cannot represent both events.

## Expected behavior

The compiler must report:

```text
compiled REC_ACID_CHICAGO_JACK: 102 runtime events, 5 sampler events ignored
```

Recipe ID 6 must expose three runtime variations. Applying P1 must produce:

- four-on-the-floor kick anchors;
- snare/clap backbeats;
- restrained hats and percussion;
- a repeating acid bass line on Synth A;
- sparse chord-root/melodic support on Synth B;
- Atlas BPM 124 and swing 52 when tempo application is enabled.

The firmware currently applies P1 from the Genre page. P2 and P3 are compiled
and tested, but are not yet exposed as a UI variation selector.

## Limitations

- Atlas does not contain verified GroovePuter or SEQTRAK sound preset IDs.
  `GenreBehavior::timbre` is therefore a GroovePuter preview sound profile.
- Polyphonic chord events are reduced to one chord-root note on Synth B.
- Five sampler events are intentionally ignored until sampler-slot semantics
  are defined.
- Patterns longer than one bar are rejected by this first compiler rather than
  being truncated.

## Troubleshooting

### Archive hash mismatch

Use the original `seqtrak_pattern_atlas_csv_v2_6.zip` archive. Do not bypass the
hash check. A new Atlas release needs a reviewed compiler update and regenerated
manifest.

### Generated files differ

Check the validation summary, schema version and source archive hash. Do not
hand-edit generated headers; fix the compiler or mapping and regenerate them.

### Pattern is correct but timbre is wrong

This is a sound-profile problem, not an Atlas event problem. Compare the event
matrices first, then tune the GroovePuter-specific Acid preview timbre without
changing the compiled pattern.

### Missing sampler details

This is expected in the first vertical slice. The compiler reports the ignored
count so the loss is visible.

## Acceptance checklist

- [ ] Source ZIP SHA-256 matches the manifest.
- [ ] Atlas validation reports schema 2.6.0 and zero failures.
- [ ] Compiler outputs 102 runtime events.
- [ ] Compiler reports exactly five ignored sampler events.
- [ ] All three generated files match the checked-in files byte-for-byte.
- [ ] `bash tests/run_host_tests.sh` passes.
- [ ] SDL target links `atlas_runtime.cpp` successfully.
- [ ] Chicago Jack appears as recipe ID 6.
- [ ] `SOUND+PATTERN` Apply loads P1 without a reset or audio corruption.
- [ ] `SOUND+PATTERN+TEMPO` finishes at 124 BPM and swing 52.
- [ ] Physical listening confirms a stable acid groove rather than broadband
      or random noise.
