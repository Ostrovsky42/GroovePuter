# GroovePuter Manual (Current `dev_0.9` Firmware)

This manual describes reachable user-visible behavior. Persisted compatibility fields and legacy page IDs are not proof that a page is available.

For the exhaustive key map, use [`src/ui/docs/keys.md`](src/ui/docs/keys.md) and [`docs/keys_sheet.md`](docs/keys_sheet.md).

## Quick keys

| Key | Action |
|---|---|
| `Space` | Play/stop the active transport |
| `Arrows` | Move cursor, selection, or list focus |
| `Enter` | Confirm/apply/toggle |
| `Tab` | Change section; on PERFORM, open PERFORMANCE TOOLS |
| `Alt/Fn+1..0` | Direct page jump where mapped |
| `Alt+[` / `Alt+]` | Previous/next page in the current workflow |
| `Fn+Tab` | Next workflow |
| `Fn+Shift+Tab` | Previous workflow |
| `Alt+P` | Open MIDI Player |
| `Alt+W` | Toggle waveform overlay |
| `Alt+\` | Toggle `CARBON ↔ CYBER` |
| `Esc` | Back/dismiss |

Page-local commands have priority over global fallbacks.

## 1. Generate workflow

The reachable workflow has three pages:

```text
GENRE 1/3 → FEEL 2/3 → GENERATION 3/3
```

- `GENRE`: corridor, variant, morph and explicit Apply policy.
- `FEEL`: swing, timing variation and velocity variation.
- `GENERATION`: materialize one Song bar in the current empty Song row.

The former user-facing TEXTURE page is removed. Legacy persisted page ID 8 resolves to FEEL. Existing Scene sound/texture fields remain compatibility data, but the firmware does not promise a standalone TEXTURE, Tape, Sampler or Generator main page.

Browsing Genre, variant, morph or a FEEL preset does not commit Scene state. Applying a changed FEEL value/preset or Genre policy marks one logical Scene mutation.

## 2. Sound engines

Synth A and Synth B can select:

- `TB303`
- `SID`
- `AY` / YM2149
- `SH101`
- `SN76489`
- `WAVEMORPH`

OPL2 is not selectable. Its legacy persisted value remains decode-only and maps to `TB303`.

The final 0.9 persistence gate is still open: do not accept a release until TYPE and every visible parameter, including parameter 5 where present, survive Save/reboot/Load independently on Synth A and Synth B.

## 3. Pattern identity and projects

A pattern address is:

```text
page + bank + slot
```

Examples: `1A1`, `2B7`.

Each project owns its pattern-page namespace. Save As copies the source project's pages into the new namespace. New starts with its own pages. Clear removes `.gpp`, `.tmp` and `.bak` only from the active project. The binary pattern format remains version 3.

Use [`docs/tests/PROJECT_PATTERN_STORAGE_CARDPUTER_ADV.md`](docs/tests/PROJECT_PATTERN_STORAGE_CARDPUTER_ADV.md) for the post-merge smoke.

## 4. Main editing pages

- `Pattern Edit A/B`: note, accent, slide, probability and selection editing.
- `Synth Params A/B`: select and edit the current synth engine.
- `Drum Sequencer`: drum grid and selection editing.
- `Song`: arrangements A/B, selection, generation, copy/paste, markers, reverse and loop.
- `Sequencer Hub`: compact pattern overview; HUB MIDI is available from a loaded MIDI Player session.
- `Groove Lab`: mode/flavor and corridor preview.
- `Project`: Scene storage, MIDI import and settings.
- `PERFORM`: live keyboard, targets and PERFORMANCE TOOLS.
- `MIDI Player`: realtime SMF playback, inspection, mute and routing.

## 5. Song workflow

- `Q..I`: assign existing slot `1..8` in the selected bank; content is not regenerated.
- `Ctrl+1..8`: select Song pattern page `1..8`.
- `B`: flip bank `A/B` for the selected cell or selection.
- `Alt+B`: edit Song slot A/B.
- `Ctrl+B`: play Song slot A/B.
- `X`: split compare.
- `Alt+X`: LiveMix on/off.
- `Bksp` or `Tab`: clear the selected Song cell/area.
- `Ctrl+C` / `Ctrl+V`: copy/paste selection.
- `Ctrl+Z`: undo the last supported Song edit.

`G` generates a new pattern for the selected Song cell. It commits the assignment only after a safe destination has been generated successfully.

Double-tap `G` prepares Synth A, Synth B and Drums for the current row and commits the row as one logical mutation after destination validation.

`NO EMPTY PATTERN SLOTS` and `GENERATION FAILED` leave the Song assignment and destination banks unchanged.

Detailed navigation: [`docs/SONG_PAGE_QUICKSTART.md`](docs/SONG_PAGE_QUICKSTART.md).

## 6. PERFORM and PERFORMANCE TOOLS

### Live keyboard

- `N`: NOTE mode on/off.
- `ASDFGHJKL`: lower scale-aware manual.
- `QWERTYUIOP`: upper manual.
- `\`: cycle target.
- `,` / `.`: previous/next scale.
- `-` / `=`: octave down/up.
- `X`: panic the live-owned target.

Native drums use MIDI `CH1..CH7`; Synth A uses `CH8`, Synth B `CH9`, and DX `CH10` for SEQTRAK performance output.

### Tools layer

Press `Tab`, then:

| Key | Tool |
|---|---|
| `1` | ARPEGGIATOR |
| `2` | DIRECTION |
| `3` | CHORD |
| `4` | MEMORY |
| `5` | STRUM |
| `6` | RATCHET |
| `7` | EUCLIDEAN |
| `8` | ROTATE |

Use `Shift+1..8` to cycle adjustable values backward. Events use the normal performance event router and MIDI dispatcher.

## 7. MIDI Player and HUB MIDI

Open with `Alt+P`, choose a MIDI file and press `Enter`.

- `Space`: Player play/pause.
- `G`: GroovePuter Pattern/Song transport.
- `M`: RAW / SEQTRAK-safe routing.
- `T`: original-file/project tempo mode.
- `C`: internal/SEQTRAK clock-source control.
- `Left/Right`: seek one bar; Shift seeks four.
- `U`: physical-track mute mixer.
- `I`: channel inspector.
- `S`: structural inspector.
- `D`: throughput panel.
- `H`: Player ↔ HUB MIDI.

Inside the `U` mixer, arrows select a physical track, `Enter` or `K` toggles it, and `A` unmutes all.

Direct `1..9` mute shortcuts are not documented as reliable for 0.9 until Cardputer ADV acceptance confirms them. Use the `U` mixer.

RAW preserves source channels. SEQTRAK-safe routing uses `CH1..CH7` for drums, `CH8` for Synth 1, `CH9` for Synth 2 and `CH10` for DX.

## 8. Persistence, dirty state and recovery

UI session state and Scene state are separate.

A successful persistent Scene mutation adds the dirty `*`. FEEL and Genre preview/browse actions do not. Explicit Save must clear dirty only after the main Scene and recovery cleanup succeed. Successful Load must mark the loaded Scene clean; failed Save/Load must leave the previous revision state unchanged. Recovery autosave must not masquerade as manual Save.

Synth TYPE/parameter migration and load ownership remain release blockers documented in [`docs/releases/PRE_0_9_RELEASE_GATE.md`](docs/releases/PRE_0_9_RELEASE_GATE.md).

## 9. Safety and troubleshooting

- Monitor `[PERF]`; `underruns` must not continuously increase.
- A failed generation or Apply transaction must not leave partial data or increment revision.
- After Stop, page changes, mutes, route changes and Panic, verify no internal or external note remains stuck.
- A TYPE or parameter-5 mismatch after Load is a release blocker.
- The fixed-DRAM CI gate is a regression budget, not hardware runtime acceptance.

## 10. Release documents

- [`docs/releases/PRE_0_9_RELEASE_GATE.md`](docs/releases/PRE_0_9_RELEASE_GATE.md): open blockers and automated gate.
- [`docs/releases/0_9_FINAL_ACCEPTANCE.md`](docs/releases/0_9_FINAL_ACCEPTANCE.md): exact final hardware record.
- [`docs/reviews/SYNTH_ENGINE_AUDIT_0_9_CURRENT_STATUS.md`](docs/reviews/SYNTH_ENGINE_AUDIT_0_9_CURRENT_STATUS.md): synth finding disposition.
