# PERFORM tools, knob step, and TR-606 fix

## Purpose

Make the synth parameter knobs faster on Cardputer ADV, remove conflicting
Fn+letter performance shortcuts, and stop the TR-606 metal voices from depending
on the kick processing path.

## Hardware

- M5Stack Cardputer ADV;
- optional headphones or Yamaha SEQTRAK AUDIO IN for checking the 606 cymbal.

## Wiring

No external wiring is required. For external monitoring, connect the Cardputer
audio output to headphones or the SEQTRAK AUDIO IN at a conservative level.

## Build and flash

```bash
git checkout fix/perform-tools-knob-tr606
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

Flash the generated Cardputer ADV firmware with the normal project workflow.

## Expected behavior

### Synth knob pages

- `A/Z`, `S/X`, `D/C`, and `F/V` move the four visible knobs by the normal
  coarse step (5 internal parameter steps instead of 1);
- hold Shift for the original fine one-step adjustment;
- arrow-key focus adjustment remains unchanged.

### PERFORM

- plain `Tab` opens or closes **PERFORMANCE TOOLS**;
- while open: `1` arp, `2` direction, `3` chord, `4` memory, `5` strum,
  `6` ratchet, `7` Euclidean pulses, `8` Euclidean rotation;
- Shift reverses cyclic controls;
- playable note keys remain active, so a chord can be held and captured with `4`;
- old Fn+A/C/K/S/R/E/V commands no longer consume global Fn shortcuts.

### TR-606

- hats and cymbal continue correctly when kick is muted;
- the generic `RS` row is shown as `CY` for 606;
- the generic clap row is shown as unavailable (`--`);
- the cymbal is less piercing but remains recognizably metallic.

## Troubleshooting

- If `Tab` changes workflow, verify that Fn is not held; Fn+Tab remains global
  workflow navigation.
- If knob movement is still slow, verify the flashed commit and test A/Z rather
  than pointer drag; pointer drag intentionally remains fine-grained.
- If 606 metal voices stop with the kick mute, confirm that the build contains
  `TR606DrumSynthVoice::beginSample()` and the mixer calls `drums->beginSample()`.
- Disable drum reverb, compression, Lo-Fi, and external input gain when judging
  the raw 606 cymbal tone.

## Acceptance checklist

- [ ] A/Z, S/X, D/C, F/V reach useful knob ranges noticeably faster.
- [ ] Shift still provides fine adjustment.
- [ ] Tab opens a local 1-8 performance tool layer.
- [ ] Fn+M, Fn+Tab, and Fn+[ / ] retain their global behavior.
- [ ] Chord memory can be captured while notes are held.
- [ ] Muting track 3 (kick) does not freeze or alter active 606 hats/cymbal.
- [ ] 606 row 7 is labelled CY and row 8 is labelled --.
- [ ] Host tests, SDL build, and Cardputer ADV build pass.
