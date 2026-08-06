# Synth Engine Audit 0.9 — Current Status Addendum

## Purpose

Record changes made after `SYNTH_ENGINE_AUDIT_0_9.md` was written, without rewriting its code-level evidence or pretending that hardware acceptance has occurred.

## Baselines

- Original audit evidence: the commit recorded in `SYNTH_ENGINE_AUDIT_0_9.md`.
- Branch synchronized with pre-release `dev`: `0514cf417b67457526e06b51e580eac19348fdac`.
- Hardware listening and FFT acceptance: **not completed**.

## Changed since the audit

PR #107 added bounded held-arrow acceleration (`x1 -> x2 -> x4 -> x5`) to active continuous synth controls and restored BPM in the global status chrome.

This changes the disposition of finding N-5:

- the fixed per-event step remains range-insensitive;
- a held key now reduces the worst interaction cost;
- the control is still not range-aware, so this is a **partial mitigation**, not closure;
- discrete TYPE/OSC/FLT selectors intentionally remain one item per event.

No other P0 or P1 synth-audit finding is considered resolved by the synchronization.

## Release disposition

### P0 — must be fixed or explicitly removed from the 0.9 selectable surface

- TB303 note lifecycle and missing amplitude-envelope behavior;
- scene/load ownership that can overwrite the selected synth engine;
- live-note clamp/NoteOff mismatch;
- persistence loss for the sixth generic synth parameter.

### P1 — must receive focused host coverage and Cardputer ADV listening acceptance

- AY and SN76489 pitch collapse;
- non-TB303 defaults that can produce maximum noise;
- TB303/SID DC behavior before per-voice effects;
- realtime allocation/filter stability concerns;
- cross-engine loudness mismatch;
- distortion enable/drive restoration;
- truthful parameter ranges and responsiveness.

## Hardware assumptions

- M5Stack Cardputer ADV.
- Built-in mono audio path at the repository sample rate.
- Headphones recommended for noise, DC-click, release-tail, and loudness comparisons.
- No external wiring is required.

## Build and flash

```bash
git fetch origin
git switch dev
git reset --hard origin/dev
rm -rf build .pio .pioenvs .piolibdeps
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
```

Flash using the normal Cardputer ADV upload command and monitor serial at `115200` baud.

## Acceptance checklist

- [ ] Every selectable synth produces a stable chromatic response over its advertised range.
- [ ] NoteOff releases every live note without a stuck tone or discontinuous full-level cut.
- [ ] TYPE and all six generic parameters survive save/reload.
- [ ] Loading a scene does not replace the selected TYPE unexpectedly.
- [ ] AY and SN76489 pass the short pitch runs documented in the main audit.
- [ ] Neutral patches do not start with maximum noise.
- [ ] Switching TYPE does not produce a destructive click while stopped or playing.
- [ ] Track loudness is acceptably matched and does not collapse into silence or clipping.
- [ ] Serial shows no allocation failure, watchdog reset, or audio underrun.

Until these boxes are completed, the synth audit remains a release gate rather than a completed acceptance report.
