# Enter on synthesizer TYPE

Production commit: `9e6c3ddf434267f91b4cfca2e1b9ea374f85976e`, branch `integration/20261025-g6-dsp-ab-presets`.

The requested TYPE list was absent: the new real-UI assertion for `SYNTH TYPE` failed before the change (exit 134). Enter on MORE → TYPE now opens the existing selection popup with the currently supported engines. In the experimental build these are TB303, SID, AY, SH101, SN76489, WAVEMORPH and DUAL OSC. Up/Down previews the selection; Enter applies it to the current voice; Esc cancels without changing the engine or dirty revision. Choosing the current engine is a no-op. Left/Right remains the TYPE stepper.

The same modal is reused for the existing sound presets; TYPE and PRESET have separate contents and apply paths. This adds one page-local mode bit and reuses the established audio guard/engine switching path. Names and pointer-label vectors are temporary UI allocations; no audio-thread allocation or new runtime owner was added. Runtime heap/largest still requires device observation.

The expanded test passes for A and B: all seven engine names, preview/cancel, confirmation, voice isolation, TYPE wrapping, and the existing 13 DUAL OSC presets, MAIN/MORE entry, a/z and patch Save/Load. Default TB303/WAVEMORPH UI plus six-engine patch/level roundtrips pass, as do the shared prune host tests. SDL and experimental Cardputer ADV build exit 0. The interactive SDL TYPE-modal capture was not completed: the window exited normally before input reached it; Xvfb is absent. This is NOT a physical or visual modal acceptance claim.

Candidate: `build/integration-type-popup-adv/GroovePuter.ino.bin`. SHA-256 `349b8ea4f6a273486d3a6cb8cebb51cbfa2efe3775cfbb53e6b03f0b286277c1`; 1623392 B. Reported flash1,623,234 B (+908 B versus prior integration image), .data41,996 B/.bss142,256 B/static DRAM184,252 B, unchanged; limit191,488 B. Build flag remains `-DGROOVEPUTER_FELUCCA_ANALOG=1`. The image is NOT UPLOADED. The preceding `7b9a98a9…` image remains installed; physical TYPE-list and new runtime measurements are OPEN.

Code was built before its commit; the pinned production file contents match the committed inputs. No push, release merge or automatic firmware upload.
