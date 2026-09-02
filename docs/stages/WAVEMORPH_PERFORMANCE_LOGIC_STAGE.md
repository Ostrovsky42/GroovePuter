# WaveMorph and performance note tools

## Purpose

Add one low-cost wavetable synth and live note-generation tools without adding work to the 22.05 kHz sample loop beyond WaveMorph's bounded oscillator/filter path.

WaveMorph provides eight fixed 128-sample tables with interpolation and morphing. The PERFORM page adds arpeggiator, chord modes and chord memory, strum, ratchet/retrigger, and 16-step Euclidean gating.

## Hardware list

- M5Stack Cardputer ADV.
- USB-C cable for flash and Serial.
- Optional class-compliant USB-MIDI host/instrument such as Yamaha SEQTRAK.
- Optional headphones or the Cardputer ADV internal speaker.

## Wiring

No external wiring is required.

- Audio uses the Cardputer ADV ES8311/I2S path already owned by GroovePuter.
- Optional external MIDI uses the Cardputer USB-C port. The receiving instrument must operate as USB host.
- Cardputer ADV is a DRAM-only target for this build; WaveMorph uses fixed flash/stack state and performs no runtime allocation.

## Build and flash

```bash
./tests/run_host_tests.sh
./scripts/build_cardputer_adv.sh
```

Flash the generated Cardputer ADV firmware using the repository's normal M5Launcher or USB upload procedure. Confirm the boot profile is the full GroovePuter build, not the MIDI-only profile.

## Controls

Open **PERFORM**. Normal note controls remain unchanged.

- `Fn+A`: arpeggiator on/off.
- `Fn+V`: arp direction: UP, DOWN, UPDN.
- `Fn+C`: chord mode: OFF, MAJ, MIN, 5TH, MIN7, MEM when captured.
- Hold two or more notes, then `Fn+K`: capture chord-memory intervals. `Fn+K` with fewer than two held notes clears memory.
- `Fn+S`: strum 0/8/16/24/36 ms.
- `Fn+R`: ratchet x1..x4.
- `Fn+E`: Euclidean pulses 0/3/5/7/9/11/13/16 over 16 steps.
- `Shift+Fn+E`: rotate the Euclidean pattern.
- `X`: scoped panic for the selected target.

Select **WAVEMORPH** from Synth A or Synth B settings. Parameters are Wave, Morph, Sub, Cutoff, Reso, and Decay.

## Expected behavior

- The synth list contains TB303, SID, AY, SH101, SN76489, and WAVEMORPH; OPL2 is absent.
- WaveMorph changes timbre continuously with Morph and remains finite/bounded at all tested settings.
- With all performance transforms off, keyboard behavior and note ownership are unchanged.
- Generated notes use the same router as normal performance notes, reaching the internal Synth A/B output and the existing USB-MIDI dispatcher.
- Drum target keeps its native seven-lane direct mapping and ignores melodic transforms.
- Starting GroovePuter transport clears live/generated notes and returns Synth A/B ownership to PatternPlayer.

Internal Synth A/B are monophonic: chord modes are heard as last-note priority internally, while external USB-MIDI targets receive the full chord. Arpeggiator, ratchet, and Euclidean modes are therefore the most useful internal-synth combinations.

## Troubleshooting

- **Fn+M opens the workspace launcher:** expected. Chord memory uses `Fn+K` to avoid that collision.
- **Keys do not sound:** stop PatternPlayer, verify NOTE MODE is ON, and select Synth A/B or DX rather than a disabled route.
- **Chord is not polyphonic on the speaker:** the internal synth voices are intentionally monophonic; test the same chord through USB MIDI on a polyphonic receiver.
- **Timing appears late while navigating:** disable detailed Serial/audio diagnostics and avoid full-screen redraw stress. Generated events are serviced by the main-loop heartbeat and never from the audio sample loop.
- **Old OPL2 scene:** it loads through the retained numeric compatibility slot and safely falls back to TB303.

## Acceptance checklist

- [ ] `./tests/run_host_tests.sh` passes.
- [ ] SDL and Cardputer ADV builds pass.
- [ ] WAVEMORPH appears in both Synth A and Synth B selectors; OPL2 does not.
- [ ] Wave/Morph/Sub/Cutoff/Reso/Decay all change the sound without a stall.
- [ ] Ten minutes of A+B WaveMorph, full drums, and normal FX produce zero I2S underruns.
- [ ] Fn+A/C/K/S/R/E/V controls update the PERFORM status line and produce the expected notes.
- [ ] Releasing keys, changing target/mode, starting transport, and pressing X leave no stuck internal or external notes.
- [ ] Drum keys A/S/D/F/G/H/J still route directly to native CH1..7.
