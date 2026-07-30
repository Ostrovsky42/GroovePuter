# Atlas recipe runtime test

## Purpose

Verify the visible recipe selector and the five new Atlas profiles without removing the existing probabilistic recipe generators.

## Hardware list

- M5Stack Cardputer-Adv
- USB-C data cable
- built-in speaker or headphones

## Wiring

No external wiring is required. The firmware uses the internal ES8311 codec, GPIO21 `PA_EN`, and `PSRAM=disabled`.

## Build and flash

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
arduino-cli upload --port /dev/ttyACM0   --fqbn m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app .
```

## Expected behavior

On the Genre page, press TAB until Apply is focused. A recipe overlay appears. UP/DOWN selects recipes; Fn+UP/DOWN still changes morph. The existing IDs 1–5 remain, followed by Chicago Jack, Rolling Acid, Classic 2-Step, Dark Skippy, Deep Chord and Minimal Space.

`SOUND+PATTERN+TEMPO` expected values:

| Recipe | BPM | Swing | Groove |
|---|---:|---:|---|
| Chicago Jack | 124 | 52 | Acid |
| Rolling Acid | 128 | 54 | Acid |
| Classic 2-Step | 134 | 66 | Breaks |
| Dark Skippy | 136 | 68 | Breaks |
| Deep Chord | 120 | 54 | Dub |
| Minimal Space | 116 | 51 | Dub |

## Troubleshooting

- No overlay: verify Apply focus, not Genre or Texture focus.
- Selection changes but pattern does not: choose `SOUND+PATTERN` or `SOUND+PATTERN+TEMPO` before applying.
- UKG/Dub support voice sounds approximate: OPL2 is a GroovePuter preview; Atlas has no verified device preset mapping.
- Audio pauses: capture `[PERF]` lines and check `underruns` and `uiPeak`.

## Acceptance checklist

- [ ] Existing UK Garage, Drum&Bass, Footwork, Psytrance and Dub Techno generators remain selectable.
- [ ] All six Atlas recipes are visible in the overlay.
- [ ] Every Atlas recipe applies P1 reproducibly.
- [ ] Repeated Apply does not change the event matrix.
- [ ] Rolling Acid is distinct from Chicago Jack.
- [ ] Classic 2-Step and Dark Skippy retain broken kick placement and high swing.
- [ ] Deep Chord and Minimal Space remain sparse and delay-friendly.
- [ ] No reset, broadband noise or increasing underrun counter occurs during the pass.
