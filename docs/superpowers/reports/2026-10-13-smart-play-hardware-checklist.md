# Smart Play hardware acceptance

Status: `NOT_RUN`. The software gates are green, so this build is ready for device testing.
The firmware was built for Cardputer ADV but was not flashed from this checkpoint.

## Standalone: no external MIDI controller

| ID | Action | Expected result | Status |
|---|---|---|---|
| S1 | Boot with no controller attached. Open PERFORM with `Fn+N`; move through KEY, CHORD, ARP, RHYTHM, INPUT. | No USB wait/error. Context and selected row stay visible. | NOT_RUN |
| S2 | In INPUT set `ROLE=RAW`; play a Cardputer note, release it. | Note sounds and releases. Existing Song/Pattern remains unchanged. | NOT_RUN |
| S3 | Set ARP source to HELD, enable ARP, hold a chord with Cardputer keys. Try UP, DOWN, UPDOWN, RANDOM and available rates; stop/release. | Arp follows selected source/contour and stops without stuck notes. | NOT_RUN |
| S4 | Set MEMORY source with a captured chord, then clear memory and select MEMORY again. | Captured pool plays; empty memory is visibly unavailable/silent rather than silently changing source. | NOT_RUN |
| S5 | Select HARMONY source with a stable supported Song harmonic event, then STOP or select a material with no valid snapshot. | HARMONY plays only when the UI says context is available; unavailable context does not resurrect stale notes. | NOT_RUN |
| S6 | Change LEAD mode RAW/SCALE/CHORD/VOICE while a Song is playing; then play a new live note. | Changing role/mode alone does not rewrite Song material. The newly played note follows the selected live interpretation. | NOT_RUN |
| S7 | Change ARP/latch/rhythm/chord settings, reboot, revisit PERFORM. | Settings persist. Latch cannot remain enabled with ARP disabled. | NOT_RUN |

## External MIDI enhancement

Set the Cardputer USB boot role to host/keyboard input as appropriate for the board, connect a
class-compliant MIDI keyboard, and route MIDI IN to PERFORM. Use the intended sound destination
(external MIDI receiver or a supported live output target) and record it with the result.

| ID | Action | Expected result | Status |
|---|---|---|---|
| M1 | Send MIDI notes with different velocities in RAW, then BASS/LEAD/CHORD. | RAW preserves pitch; smart roles affect newly received live notes only. Song material is unchanged. | NOT_RUN |
| M2 | Send sustain on/off from the keyboard while Cardputer holds a note; repeat with both release orders. | Each source releases only its own note ownership. | NOT_RUN |
| M3 | Send CC80 values 0, 64, 127, 63, then repeated 127; inspect INPUT status and ARP. | 0–63 disables; 64–127 enables; repeated ON remains ON. | NOT_RUN |
| M4 | With ARP disabled send CC81=127, then enable ARP and repeat CC81=127. | First latch request is rejected visibly; second enables latch. | NOT_RUN |
| M5 | While notes/arp/sustain are active, unplug USB, reconnect, send a new note. | No stuck external notes; transport, material and internal owners continue. Reconnected session works; stale state does not return. | NOT_RUN |
| M6 | Exercise pitch-bend and CC sweeps while playing and watch diagnostics/audio. | No reboot, global panic, or unrelated owner release. Note/control input remains responsive. | NOT_RUN |

## Record after the test

For every row record firmware SHA, board revision, USB role, controller/receiver model, exact
action, observed result, and PASS/FAIL. Attach a serial log for reboot, queue-drop, or latency
issues. Do not change any row from `NOT_RUN` without a physical run.

## Limits of this checkpoint

- Idiom-origin harmonic snapshots remain unavailable until HGS2-7c supplies an authoritative
  graph/idiom producer; HARMONY availability here depends on the current supported legacy
  publication path.
- HGS genre diversity/listening acceptance is a separate software and listening checkpoint.
- Hardware acceptance and listening acceptance are both `NOT_RUN` until physically performed.
