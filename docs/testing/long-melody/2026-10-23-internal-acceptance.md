# Internal-audio acceptance and monitor — 2026-10-23

Installed image: FS1B persistence diagnostic `6cb56545a2ae8331873a98c15a54de4dc34aa6c9592d3dd01cee697315789d00`.

The user confirmed Save/Save As/Reset/Load restored both eight-bar Melody voices with all notes. Earlier user acceptance covers Cyber Synth A/B STEPS, slot/source switching, the eight-bar loop, and combined A+B/drums/FX/NEXT/GO/Song interaction. The user considers the internal-audio direction accepted and requested integration with the advanced Melody/genre and Energy Transitions lines.

## Captured monitor

`logs/serial-180s-20261023-140802.log`, connected without auto-reset:

- 178 heap samples, all `integrity=1`; 36 PERF samples, all `underruns=0`; no panic or reboot marker.
- 202 key events and page navigation. 64 state samples were A=STEPS/B=MELODY, then 114 were A+B=MELODY.
- Lengths progressed A=0→1→2→4→8B while B remained 4B. There were 67 A=8B/B=4B samples. This capture does not establish simultaneous A+B=8B, nor a persistence operation: SD read/write/save counters stayed constant through the window.
- Free internal heap sampled 18,912–20,788 B; largest block 10,228–12,276 B. Last sample: 19,904/11,764 B. Final stack free high-water values: loop 17,376 B, audio 6,640 B, dispatcher 1,260 B.
- PERF peak counter reached 101.9%; it is a cumulative counter and was already present near the start, so do not assign its occurrence to a particular operation in this window. Underrun counter stayed zero.
- All 178 state samples had `playing=1`. No post-STOP idle plateau or cold boot was captured. End state was project `test-save`, A=8B/20 events/dirty, B=4B/25 events/clean.

## Acceptance boundary

Accept internal functionality based on the user's observed run and the healthy captured runtime. Keep formal cold-boot repeatability, same-boot post-STOP heap recovery, unavailable-SD behavior, and external MIDI explicitly open. The frozen E0–E6 goal is not globally complete. Integration is authorized by the user's new direction; it must retain these evidence boundaries and repeat software/memory checks after merges.
