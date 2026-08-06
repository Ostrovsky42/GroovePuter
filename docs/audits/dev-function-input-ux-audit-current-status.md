# Dev Function/Input/UX Audit — Current Status

## Purpose

Update the disposition of the static audit after the pre-release branch cleanup. The original audit remains a source snapshot and is not rewritten as if it covered later commits.

## Baselines

- Original audited revision: `98d215372e3c266600f0bc2e10009e373347757c`.
- Branch synchronized with pre-release `dev`: `0514cf417b67457526e06b51e580eac19348fdac`.
- Hardware acceptance: **not completed**.

## Disposition of the major directions

### Active before 0.9

- PR #102 — project-scoped pattern storage and canonical pattern address. Synchronized with current `dev`; CI and Cardputer ADV storage acceptance remain required.
- PR #110 — genre-owned variants, Atlas P1/P2/P3 roles, and sparse Dub/Trip-Hop materialization. Synchronized with current `dev`; CI and listening acceptance remain required.
- Synth-engine P0/P1 findings from PR #106 — release-blocking until fixed, removed from the selectable surface, or explicitly accepted with evidence.

### Merged but still awaiting hardware acceptance

- PR #107 — held-value acceleration and BPM status restoration.
- Recent SMF/MIDI tempo and transport work must be exercised in the final hardware matrix even where source tests exist.

### Deferred beyond 0.9

- PR #101 — old Song-generation UX prototype. Closed because the branch contained temporary apply scripts and only a partial product diff. Rebuild cleanly after the release blockers.
- PR #90 — Phrase Arranger Stage 2. Closed because it replayed already integrated Phrase Core work and was too far diverged for safe release inclusion.

## Release cleanup rule

Do not remove or remap a function solely because it appears unused in the static audit. Before deletion, verify all of the following:

1. no active page constructs or references it;
2. no persisted page ID, key binding, scene field, or migration path depends on it;
3. host, SDL, Cardputer ADV, and MIDI acceptance still pass;
4. documentation and global help are updated in the same PR.

## Hardware assumptions

- M5Stack Cardputer ADV.
- Built-in keyboard and display.
- Headphones or built-in speaker for audio paths.
- Optional Yamaha SEQTRAK for USB-MIDI and clock acceptance.
- No external GPIO wiring is required for this audit.

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

- [ ] Every visible page is reachable and has a reliable exit path.
- [ ] Global shortcuts do not collide with local editor controls.
- [ ] Discrete selectors advance one item per event; continuous controls remain precise on tap and accelerate on hold.
- [ ] Transport, mute, generation, pattern identity, and persistence each have one documented owner.
- [ ] Save, Save As, New, Clear, reload, and autosave recovery preserve the expected project boundary.
- [ ] MIDI Player, MIDI Hub, live keyboard, and generated performance notes clean up NoteOff correctly.
- [ ] No release page depends on a temporary apply script or an unmerged experiment branch.
- [ ] Serial contains no crash, watchdog, allocation failure, or repeated recovery loop.

The original audit findings remain candidates until individually closed by a focused implementation PR or an explicit release decision.
