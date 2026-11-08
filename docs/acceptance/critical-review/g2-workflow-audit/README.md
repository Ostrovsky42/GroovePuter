# G2 native-workflow feasibility audit — 2026-10-26

Production source: 9e6c3ddf434267f91b4cfca2e1b9ea374f85976e; repository HEAD at run: a0ad73df007f498193a264d4cf3d378c8db9c35d.

This is a host feasibility probe, not G2 musical or hardware acceptance. Uses the existing pruned host archive (default six engines), not the installed experimental DUAL OSC firmware. No audio has been judged and no seed has been replaced.

Inputs are the unchanged preregistered manifest: Techno 10 / 124 BPM and Dub/Reggae 5 / 82 BPM, recipe 0, AUTO rhythm, Legacy harmony, seeds 1369899009 through 1369899016. Each case runs in a fresh process/project at P3. Genre mode is synchronized with syncGrooveModeToGenre before generation. An earlier unsynchronized probe gave the same layout outcomes; its build artifacts remain separately retained.

The probe calls the MATERIAL TAKE4 and cycle domain operations. RETURN uses the existing SongPage COPY/PASTE handlers over all three tracks, copying the original four rows to the append position. No notes or Song rows are manufactured by the harness. Return identity checks compare Song references; these do not yet prove canonical event identity or musical quality.

| Genre | Complete 16-bar layouts | Observed refusal |
| --- | --- | --- |
| Techno | 8/8 | None |
| Dub/Reggae | 0/8 | All eight cycle calls return NotAdmitted (5); TAKE4 succeeds |

The current automatic Dub/Reggae path blocks the preregistered workflow. Do not widen phrase admission or change seeds/genres merely to pass. Admission diagnosis: all eight Dub cases prepare successfully with Sparse Skank (411), phraseEvolutionEnabled=false; both DevelopReturn and SparseDrift return NotAdmitted. All eight Techno cases select Machine Syncopation (420), enabled=true, and both laws apply. The catalogue whitelist explicitly includes Steppers (410), but excludes Sparse Skank. This is an existing unsupported evolution route; enabling a new archetype law is outside the current acceptance fix scope. The Phrase page already handles this typed refusal with a genre-change message. GUI input equivalence remains untested; this host evidence proves the domain-path limitation, not a completed physical scenario.

G2 remains OPEN with a reproducible feasibility blocker. Blind ratings, canonical dumps/hashes, normalized audio and physical workflow are NOT TESTED.

Reproduce from platform_sdl using the compiler invocation in compile.txt, then run probe GENRE SEED ABSOLUTE_EMPTY_PROJECT_DIRECTORY. Full case logs and parsed outcomes are retained alongside the source.

Event extraction: `python3 extract_events.py BUILD_ADMISSION_DIRECTORY OUTPUT_DIRECTORY`. Dumps resolve Song rows to stored A/B/drum patterns, retain all serialized step fields, and hash ordered events independently of timbre. These are serialized pattern-event dumps, not a timed playback/MIDI trace. RETURN equality is asserted; empty missing sections are not fabricated.

## Audio preview audit

Eight dry TB303 A+B / synthesized-drum native Song previews were generated under `build/g2-native-workflow/audio-20261026/`, with hashes in audio-manifest.json. All eight raw WAVs are byte-identical; the version-controlled representative is `techno-identity-1369899009.wav`. Matched target is -20 dBFS RMS with a -1 dBFS peak ceiling; no production gain changes. Playback-row traces are retained. This renderer uses nominal duration and its final block includes the wrap into blank row 0 (less than 256 samples); these are preliminary previews, not qualified listening stimuli.

**Critical sampling limitation:** all eight Techno cases have the same TAKE event hash AND the same complete raw WAV hash. Thus eight successful layouts do not establish eight independent musical examples. The current probe's mode-manager project seed changes, but the P1R Legacy materializer derives phrase identity from generation attempt ordinal; fresh processes all begin at the same ordinal. The preregistered seed needs a verified mapping to the native material-generation input before any blind scoring. Do not silently substitute attempt ordinals for project seeds or count duplicates as independent successes. G2 is blocked at preparation, and blind listening remains NOT RUN.
