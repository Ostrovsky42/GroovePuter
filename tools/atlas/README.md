# Atlas runtime compiler

## Purpose

Compile the reviewed SEQTRAK Pattern Atlas v2.6 recipes into compact C++ tables. GroovePuter reads no CSV and performs no Atlas I/O from the audio callback.

The runtime catalog contains:

- Chicago Jack — ID 6;
- Rolling Acid — ID 7;
- Classic 2-Step — ID 8;
- Dark Skippy — ID 9;
- Deep Chord — ID 10;
- Minimal Space — ID 11.

Legacy probabilistic recipes IDs 1–5 remain available and are not replaced by Atlas.

## Hardware list

Generation requires Python 3.10+, the original Atlas ZIP and the repository. Runtime verification uses M5Stack Cardputer-Adv, a USB-C data cable and the built-in speaker or headphones.

## Wiring

No external wiring is required. Cardputer-Adv uses GPIO21 for `PA_EN`, the internal ES8311 audio path and no PSRAM.

## Build and flash

```bash
python3 tools/atlas/compile_atlas_runtime.py   /path/to/seqtrak_pattern_atlas_csv_v2_6.zip   /tmp/atlas-generated

diff -ru src/generated /tmp/atlas-generated
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

Flash with the normal Arduino CLI workflow documented in `docs/tests/CARDPUTER_ADV_BUILD_AND_STALL_DIAGNOSTICS.md`.

## Track mapping

| Atlas track | GroovePuter target |
|---|---|
| KICK..CLAP | Drum voices 0..7 |
| SYNTH1 | Synth A |
| SYNTH2 | Synth B chord-root preview |
| DX | Synth B melodic preview |
| SAMPLER | Reported but not applied yet |

SYNTH2 wins when SYNTH2 and DX occupy the same monophonic Synth B step.

## Expected behavior

Every recipe exposes P1, P2 and P3. Genre Apply currently loads P1. P2/P3 are compiled and host-tested for a later variation selector.

Preview sound profiles are deliberately separate from Atlas evidence:

- Chicago Jack and Rolling Acid: TB303 A/B;
- UK Garage and Dub Techno Atlas variants: TB303 bass plus OPL2 support voice;
- sound preset IDs are not claimed as Atlas-verified.

## Troubleshooting

- Hash mismatch: use the unmodified v2.6 ZIP; do not bypass the gate.
- Correct rhythm, wrong timbre: tune the GroovePuter preview profile without changing generated events.
- Missing sampler layer: expected until sampler-slot semantics are defined; the manifest reports all 40 ignored events.
- Recipe not visible: focus the Apply area with TAB; UP/DOWN opens and navigates the visible recipe overlay.

## Acceptance checklist

- [ ] Atlas schema is 2.6.0 with zero validation failures.
- [ ] Six recipes and 18 P1/P2/P3 patterns compile reproducibly.
- [ ] Manifest reports 535 runtime events and 40 ignored sampler events.
- [ ] Legacy IDs 1–5 still appear and randomize independently.
- [ ] Host, SDL and Cardputer-Adv compile gates pass.
- [ ] Recipe overlay shows IDs 0–11 and long names without clipping.
- [ ] Each new recipe Apply loads P1 without reset or broadband noise.
- [ ] BPM/swing match the manifest when tempo application is enabled.
