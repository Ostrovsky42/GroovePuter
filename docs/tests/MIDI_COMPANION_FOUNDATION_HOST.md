# MIDI Companion foundation — host acceptance

## Purpose

Verify the pure MIDI Companion configuration, profile, validation, and persistence
components before they are connected to PR #8 runtime scheduling.

## Hardware list

No physical hardware is required for this stage.

The future integration target remains:

- M5Stack Cardputer-Adv;
- Yamaha SEQTRAK;
- data-capable USB-C cable.

## Wiring

None for this host-only acceptance stage.

Future device wiring:

```text
Cardputer-Adv USB-C -> data-capable USB-C cable -> Yamaha SEQTRAK USB-C
```

## Build and run

```bash
bash tests/run_host_tests.sh
```

Focused test:

```bash
mkdir -p build/host-tests

g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_midi_companion_settings.cpp \
  src/midi/midi_companion_settings.cpp \
  src/midi/midi_companion_settings_codec.cpp \
  -o build/host-tests/test_midi_companion_settings

build/host-tests/test_midi_companion_settings
python3 tests/test_midi_companion_foundation_source_regressions.py
```

## Expected behavior

The tests exit successfully without output from the C++ binary. The source
regression prints:

```text
MIDI companion foundation source regressions: OK
```

Validated contracts:

- SEQTRAK Native routes use external channels 1-7 for drums and 8/9 for synths;
- all eight internal drum voices retain explicit routes;
- General MIDI drums use external channel 10;
- Custom profile selection preserves current routes;
- channel conversion is bounded to UI range 1-16;
- invalid values sanitize deterministically;
- a 44-byte versioned settings blob round-trips exactly;
- corrupt blobs do not partially mutate output;
- missing storage loads defaults;
- storage errors preserve active settings.

## Troubleshooting

### Compiler cannot find platform headers

The foundation has gained an invalid runtime or hardware dependency. Remove the
dependency; do not add Arduino/M5 stubs to this host test.

### CRC test unexpectedly succeeds after corruption

Confirm the modified byte is inside the encoded header/payload and that decode
compares the stored CRC32 before assigning output.

### SEQTRAK drum routes use channel 10

That is the General MIDI profile, not SEQTRAK Native. SEQTRAK Native uses one
fixed channel per native track.

## Acceptance checklist

- [ ] focused C++ test passes with `-Werror`;
- [ ] source-boundary regression passes;
- [ ] full host regression script passes;
- [ ] no scene schema change exists;
- [ ] no runtime, UI, TinyUSB, Arduino, M5, or DSP include exists;
- [ ] branch remains independent of PR #8 implementation files;
- [ ] SDL build passes in CI;
- [ ] Cardputer-Adv compile passes with core 3.2.2.
