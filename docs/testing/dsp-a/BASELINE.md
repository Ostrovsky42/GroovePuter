# DSP-A shared baseline

Accepted checkpoint: `64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f` (MEMORY-PRUNE-A, PR #495). PR #494 reference remains `af4d51072e99b7cd7497c332bfc6da8877294c26`.

Before branching, HEAD was 64ae4b8b and `git status --short` was empty. Working branch: `experiment/dsp-a-sound-quality`. Use this exact accepted commit as the common base for the two DSP work streams; no changes were discarded.

The accepted product retained TempoDelay and FormantSynth. Its quiet heap was 21184 B (largest 12276 B), versus 15188/7668 B before pruning: 5996 B recovered heap, **not 21 KB recovered**. Static DRAM 184252 B / unchanged gate 191488 B. Global sample rate remains 22050 Hz. The experimental no-delay image is not the common baseline.

Installed baseline product BIN SHA-256: `b5a6a0e3dc8ea6604a20f7022ff548cbca2209d59bf4797ec8ac3c4fe45841e3`.

A1 references must be generated before production DSP edits. Direct voice renders isolate DSP/defaults; full-engine/persistence tests separately cover applied parameters, crossfade and output chain. Host CPU time is not Cardputer CPU. Internal speaker, 3.5mm path and blind loudness-matched listening require hardware/user evidence and remain OPEN until actually observed.

No changes to HGS, Melody, FORM/Song, MIDI routing or generation are authorized in this DSP task. Saved parameter values/order and engine identity must be preserved. Presets are explicit opt-in parameter recipes; no automatic migration of existing projects or global volume/sample-rate changes.
