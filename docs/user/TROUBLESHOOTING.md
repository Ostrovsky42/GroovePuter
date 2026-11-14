# Troubleshooting

These notes describe the internal `.33–.35` development line. Physical
Cardputer ADV, USB MIDI and SEQTRAK behavior remains unverified.

## MIDI export

| Message | What to do |
|---|---|
| `STOP TO EXPORT` | Stop playback, then run **PROJECT → SCENES → Export MIDI** again. |
| `EXPORT: MATERIAL READ FAILED` | Check that the Song's referenced Pattern pages and Melody material are present. Reload the project if needed; do not assume an incomplete export is usable. |
| `EXPORT: SONG CHANGED; RETRY` | Let the current edit/save operation finish, then start a new export. |
| `EXPORT: MIDI VALIDATION FAILED` | Keep the GroovePuter project. The temporary output did not pass validation; retry once after returning to a stable stopped state. |
| `EXPORT: SD WRITE FAILED` | Check that the SD card is mounted, writable and has free space, then retry. |
| `MIDI NOTES FLATTENED; SEE TRACK MARKERS` | Export succeeded with documented limitations. Conditional/probability notes are always present in SMF; Voice audio and Drum-only processing are omitted. |

Export writes a temporary file, validates it and then promotes it to a new final
name. A failure message does not mean the export succeeded. Keep the editable
project as your recovery copy.

## MIDI profile and routing

- If Device Profile or External MIDI cannot be changed, stop playback first.
- A saved profile/output change applies to the next output startup; restart the
  project/device before expecting the active output route to change.
- Changing the output profile does not regenerate or rewrite musical notes.
- If a profile's custom route cannot be represented safely, GroovePuter fails
  closed instead of guessing a destination.
- USB endpoint discovery, reconnect, MIDI clock and SEQTRAK reception are not
  confirmed by SDL or fake-transport tests. Record those results during owner
  acceptance of the `.35` candidate.

## Save/load

Keep a saved GroovePuter project before experimenting with material or export.
SMF is a playback/interchange projection and does not preserve all GroovePuter
data such as temporary macros, internal audio, Drum FX or every conditional-step
semantics.
