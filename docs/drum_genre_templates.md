# Drum Genre Templates (Phase 1)

`src/dsp/drum_genre_templates.h` defines one template per `GenerativeMode`.

Bitmask convention:
- Step `0` is the leftmost sequencer step.
- Step `0` maps to bit `15`, step `15` maps to bit `0`.

Voice map used by the generator:
- `0` kick
- `1` snare
- `2` closed hat
- `3` open hat
- `6` rim
- `7` clap

Template fields:
- `kickMask` primary kick placement
- `snareMask` primary snare/clap placement
- `hatMask` closed hat pattern
- `openHatMask` open-hat accents
- `kickGhostProb`/`snareGhostProb` ghost-note probability
- `hatVariation` per-step hat velocity jitter strength
- `kickVelBase`/`snareVelBase`/`hatVelBase` base dynamics (scaled to genre velocity range)
- `useRim` routes ghost snare notes to rim voice
- `useClap` routes primary snare lane to clap voice

Current genre intent:
- `Acid`: driving 4-on-floor with dense hats and clap backbeat.
- `Outrun` (minimal): sparse kick, restrained hats, more space.
- `Darksynth` (techno): loud mechanical 4-on-floor and dense hats.
- `Electro`: syncopated kick with offbeat movement.
- `Rave`: high-energy 4-on-floor with dense hats and clap.
- `Reggae`: one-drop kick and offbeat rim emphasis.
- `TripHop`: lazy sparse kick, dusty hats, ghosted snare lane.
- `Broken`: displaced kick/snare with irregular hat grid.
- `Chip`: tight quantized grid, minimal human variation.

## Dynamics and timing
- Template base velocities are mapped into genre velocity ranges (`velocityMin..velocityMax`).
- Per-step timing (`DrumStep::timing`) is generated and then swing is applied.
- Per-pattern groove override can add extra swing/humanize on top of base timing.

## Drum Step FX (implemented)
- `Retrig`
- `Reverse`
- `Flam` (gap in ticks)
- `Roll` (hit count)
- `PitchUp`
- `PitchDown`

Runtime handling lives in `src/dsp/miniacid_engine.cpp`.

## Automation lanes (implemented)
Each `DrumPatternSet` supports 4 lanes:
- Reverb Mix
- Compression
- Transient Attack
- Engine Switch (`808/909/606`)

Node curves:
- `Linear`
- `EaseIn`
- `EaseOut`

See `docs/DRUM_AUTOMATION.md` for data format, UI, and runtime details.
