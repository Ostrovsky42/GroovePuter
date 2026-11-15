# MIDI setup and Song export

This guide describes the internal `.33–.35` development line. It is not a
published release guide, and USB/MIDI behavior has not been accepted on physical
Cardputer ADV hardware.

## Choose the output mode

Open **PROJECT → MIDI**.

- **Device Profile** selects the intended external target, such as SEQTRAK, GM
  or GENERIC.
- **External MIDI** selects `INTERNAL` or `INT + MIDI`. The built-in sound stays
  active in both modes.
- MIDI input has separate enable, channel and route settings.

The `*` marker means a setting has been changed. Press `Enter` to save it. The
active USB output uses its startup route snapshot, so saved profile/output
changes take effect after restart. GroovePuter prevents these changes while
playback is active to avoid changing the destination of notes that are already
owned by the current route.

Device Profile describes output projection. It does not rewrite Song or Pattern
notes. Export uses the applied profile's Pattern role/channel mapping; pending
next-start settings are not exported as though they were active.

## Export the committed Song

1. Stop playback.
2. Open **PROJECT → SCENES**.
3. Select **Export MIDI** and press `Enter`.
4. Find the uniquely named `.mid` file in `/midi` on the SD card.

The Standard MIDI File contains a conductor track and separate Synth A, Synth B
and Drums tracks. It represents the selected committed Song, its saved project
tempo and section order. Export is an interchange copy; keep the GroovePuter
project as the editable source of truth.

Temporary TENSION / ENERGY / SPACE / MUTATION performance values are not committed
or included. Conditional/probability events are flattened to authored notes and
marked in the file. Voice audio, Drum FX and internal Drum automation cannot be
represented faithfully as these MIDI events and are reported as omissions.

If export reports a failure, treat no new final MIDI file as successful output.
The exporter writes and validates a temporary file before promoting it to its
final name. This application-level check does not make SD storage power-loss
transactional.

## Hardware status

Host tests and mock transports do not establish USB enumeration, MIDI timing,
Cardputer keyboard behavior, SEQTRAK compatibility or physical SD reliability.
Those checks belong to the owner's single `.35` hardware acceptance pass.
