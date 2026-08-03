# Unified MIDI File Manager Stage

## Purpose

Provide one MIDI library browser for both workflows that read `/midi`:

- Project → Import MIDI;
- MIDI Player → Load MIDI.

The manager owns directory scanning, sorting, rename and confirmed delete. MIDI
parsing, importing and realtime playback remain unchanged.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable for build/flash and serial monitoring
- microSD card formatted for the existing GroovePuter SD setup
- one or more `.mid` files under `/midi`

## Wiring

No external wiring is required. PORT.A is unused. Existing Cardputer ADV I2C
wiring remains:

```text
SDA GPIO2
SCL GPIO1
```

## Build / Flash

```bash
bash tests/run_host_tests.sh
python3 tests/test_midi_file_manager_source_regressions.py
mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -I. \
  tests/test_midi_file_name_policy.cpp \
  -o build/host-tests/test_midi_file_name_policy
build/host-tests/test_midi_file_name_policy
bash scripts/build.sh --warnings all
```

Flash with the existing repository process after the Cardputer ADV build passes.

## Controls

The same controls are used in Project Import and MIDI Player:

```text
Up / Down   select entry
Enter       open folder or choose MIDI file
Backspace   parent folder; leave browser at /midi
R           rename selected MIDI file
X           open delete confirmation
F           refresh SD listing
Esc         cancel rename/delete; otherwise parent/leave
```

Rename edits the filename stem only. The manager always writes a `.mid`
extension. Allowed characters are letters, digits, spaces, `-`, `_`, `(` and `)`.

Delete defaults to **NO**. Select **YES** with Right or `Y`, then press Enter.
Directories can be opened, but are not renamed or deleted in this stage.

## Expected behavior

- Project Import and MIDI Player show the same path, ordering and file names.
- Directories are listed before MIDI files; names are sorted case-insensitively.
- `.mid` and `.MID` files are visible; hidden and non-MIDI files are ignored.
- The last folder and selection are shared when moving between Project and Player.
- Rename rejects empty/invalid names, path separators and existing targets.
- Delete never happens on the first `X` press.
- Rename/delete are blocked for the MIDI file currently held by the SMF player.
- Renaming or deleting refreshes both workflows because they use one manager.
- Import settings and SMF transport behavior are unchanged.

## Memory and realtime

```text
Shared manager instances: 1
Maximum cached entries: 48
Manager size contract: <= 4096 bytes
Dynamic allocation in audio/playback path: none
AudioTask work: none
Per-sample work: none
```

SD scans and file mutations occur only from UI actions. No filesystem operation
is added to the audio callback, MIDI scheduler or frame-independent transport.

## Troubleshooting

### `SD UNAVAILABLE`

Press `F` to retry. Confirm the card is mounted and `/midi` can be opened. The
manager retries the existing Cardputer SD mount helper before reporting failure.

### File does not appear

Only immediate children with a `.mid` extension are shown. Enter subdirectories
to browse them. Names longer than the fixed entry contract are skipped and the
header shows `+` to indicate truncation.

### Rename or delete says file is in use

The SMF player keeps its source file open for streaming. Load another MIDI file
or reboot before mutating that exact file. Other files remain manageable.

### Rename says name already exists

Choose a different filename. Overwrite-on-rename is intentionally forbidden.

## Acceptance checklist

- [ ] Project Import and MIDI Player render the same shared browser.
- [ ] Enter opens folders and Backspace returns to `/midi`.
- [ ] Selection/path carry across Project Import and Player.
- [ ] `.mid` and `.MID` are listed; other extensions are hidden.
- [ ] Directories appear before files and names are sorted consistently.
- [ ] `R` opens a filename editor with `.mid` preserved.
- [ ] Invalid names and existing targets are rejected without changing the file.
- [ ] Successful rename updates the list and the file remains playable/importable.
- [ ] `X` opens confirmation with **NO** selected.
- [ ] Enter on **NO** cancels without deleting.
- [ ] Right/`Y`, then Enter, deletes the selected file.
- [ ] Parent entries and directories cannot be deleted.
- [ ] The active SMF source cannot be renamed or deleted.
- [ ] Project MIDI import still reaches the advanced routing screen.
- [ ] MIDI Player still loads and plays the selected file.
- [ ] Host regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV build passes with `--warnings all`.
