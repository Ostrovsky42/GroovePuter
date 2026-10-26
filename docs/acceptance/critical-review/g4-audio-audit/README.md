# G4 audio evidence audit — 2026-10-26

Status: reference integrity PASS; musical and physical acceptance OPEN.

Verified all 22 stored blind WAVs against their original manifest: hashes match, mono PCM16 22050 Hz, equal durations per pair, peaks ≤0.9001, pair RMS differences <0.01 dB. Direct TB303, filter, WAVEMORPH, wavetable and sound-quality-preset sources match historical DSP source 0847dc0a4b1ed66ee5f0b497beea8441d73b1cc0 exactly. The current integration wrapper/mixer differs, so this is direct-DSP source continuity, not whole-device or full-mixer equivalence. Fresh matched-input renderer metrics in the TB303 audit provide additional host evidence.

Pairs 01–09 compare production DSP-A baseline/candidate. Pairs 10–11 compare an isolated, unmerged morph experiment; production does not include that phase-offset experiment. Preserve all 11 opinions as required by the plan, but never count preference for 10–11 as evidence of current-firmware improvement. No labels/mappings are disclosed in the rating sheet. Source mapping remains in the original REVEAL.json; participants should not see it before recording ratings.

## Listening sessions still required

1. Three blind listeners rate all 11 existing host A/B pairs at a fixed playback level: A/B/no preference, clicks/roughness, note endings and tone. Preserve worse/equal votes, not just favorable results. Use ratings.csv. This evaluates host references.
2. Separately play the same fixed accepted material on physical Cardputer through jack and speaker, with TB303/WAVEMORPH presets and synthesized drums. For each listener record whether bass, lead and the full mix are useful for sketching. At least 2/3 must agree. Verify at least two distinguishable useful TB bass presets and two WAVEMORPH lead/bass presets. The WAVEMORPH voice remains monophonic.
3. Capture ten-minute physical reference playback, engine switch, NoteOff, Save/Load and underrun telemetry. Record unwanted clicks/clipping/stuck notes. Lack of a reported problem is not equivalent to a timed recorded run.

WAV Sampler is pruned from Cardputer: this packet does not assume host WAV files can be played through its internal speaker. Host A/B and live-device output checks are separate evidence. Do not change global gain or introduce a WAV playback path for this test. No current G4 PASS is assigned.
