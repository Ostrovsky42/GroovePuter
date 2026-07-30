# Genre generator reliability test

## Purpose

Verify that GroovePuter genre generation uses the complete compiled genre
profile and never consumes uninitialized generation parameters.

This test covers the baseline repair before Atlas patterns are integrated. It
is intended to distinguish a broken generator from a musically weak but valid
profile.

## Hardware list

Host regression:

- Linux, macOS, or another environment with Python 3 and a C++17 compiler.

Hardware acceptance:

- M5Stack Cardputer ADV;
- USB-C data cable;
- built-in speaker or headphones;
- microSD card used by GroovePuter.

## Wiring

No external wiring is required.

Cardputer ADV audio assumptions:

- `PA_EN` is GPIO21;
- ES8311 audio is routed through the board's internal I2S path;
- RGB output remains disabled because GPIO21 must not receive WS2812 data.

## Build and run

From the repository root:

```bash
bash tests/run_host_tests.sh
```

The genre-specific executable can also be built directly:

```bash
g++ -std=c++17 -Wall -Wextra -Werror \
  -I. tests/test_genre_defaults.cpp \
  -o /tmp/test_genre_defaults
/tmp/test_genre_defaults
```

Flash the normal Cardputer ADV firmware only after host tests and the SDL build
are green.

## Expected behavior

Host tests must prove that:

- every field of `GenerativeParams{}` has a safe deterministic default;
- genre regeneration calls `getCompiledGenerativeParams()`;
- active recipes select their mapped groovebox mode;
- legacy `GrooveRecipe` overloads start from the compiled profile instead of a
  partially initialized struct.

On Cardputer ADV, regenerate and listen to these diagnostic cases:

1. Base Acid: stable low bass, audible accents/slides, no broadband random
   bursts.
2. UK Garage recipe: `Breaks` macro mode, broken drum placement, restrained
   synth density.
3. Dub Techno recipe: `Dub` macro mode, sparse pattern, delay/space without a
   dense random lead.

Changing a genre and regenerating with the same firmware may produce a new
variation, but values must remain inside the selected genre corridor.

## Troubleshooting

### Output is still broadband noise

Enable the engine test tone. If the test tone is also distorted, investigate
I2S/codec/output gain before changing genre data. If the test tone is clean,
inspect generated notes, velocity, timing, accent and slide values over serial.

### All genres sound structurally identical

Confirm that `syncGrooveModeToGenre()` uses `grooveboxModeForRecipe()` and that
`regeneratePatternsWithGenre()` passes `getCompiledGenerativeParams()` to both
synth and drum generation.

### UK Garage or Dub Techno selects the wrong macro mode

Expected mapping:

- UK Garage, Drum & Bass, Footwork -> `Breaks`;
- Psytrance -> `Acid`;
- Dub Techno -> `Dub`.

### Timing is unstable

Generated timing values must remain within the 96-PPQN sub-step range. Do not
reintroduce event dispatch only at 24-tick boundaries.

## Acceptance checklist

- [ ] `bash tests/run_host_tests.sh` exits with status 0.
- [ ] Linux SDL target builds successfully.
- [ ] No `GenerativeParams params;` remains in the recipe adapter block.
- [ ] Genre Apply uses the complete compiled parameters.
- [ ] UK Garage selects `Breaks`.
- [ ] Dub Techno selects `Dub`.
- [ ] Base Acid, UK Garage and Dub Techno produce stable, distinct patterns on
      Cardputer ADV.
- [ ] Serial output shows bounded notes, velocity, timing, accents and slides.
- [ ] Five consecutive regenerations per diagnostic genre produce no reset,
      hang, corrupted page or broadband random output.
