# DSP-A Sound Quality Recovery Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans. Independent read-only voice audits may use dispatching-parallel-agents; keep the production fix scope in TB303/WAVEMORPH/filter DSP only.

**Goal:** Diagnose all six engines consistently, fix confirmed TB303/WAVEMORPH defects, and deliver six explicit musical presets plus comparable audio evidence.

**Architecture:** A standalone real-voice renderer at 22050 Hz captures baseline and candidate WAV/float metrics with identical event schedules. Minimal DSP repairs are regression-tested, while other engines' raw PCM must remain identical. Explicit preset recipes never overwrite existing projects automatically.

**Tech Stack:** C++17 existing DSP classes, Python audio metrics, SDL2, existing Cardputer build and runtime telemetry.

**Spec:** User DSP-A A1/A2/A3 brief; docs/testing/dsp-a/BASELINE.md pins the accepted shared base.

## Global Constraints

- Shared base 64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f; clean starting tree; separate experiment/dsp-a-sound-quality branch.
- Preserve 22050 Hz, global gain, saved parameter layout/values, engine count, safe control-side allocation/crossfade.
- Do not change HGS/Melody/FORM/Song/MIDI routing/generation.
- No new audio-thread allocation; no accidental changes to SH101/AY/SID/SN76489.
- Numeric success is not musical acceptance: matched-level blind listening is required; unavailable physical captures remain OPEN.

## Review Focus

- Nonfinite/high-resonance/high-note values must not cause instability hidden by downstream saturation.
- NoteOff/held retrigger/slide combinations must end notes and avoid discontinuities or unsafe allocation.
- Morph gain holes and table phase are investigated across all eight adjacent pairs, not just one convenient pair.
- Presets must be explicit and monophonic; preserve project parameters and per-engine volume.
- Host rendering bypasses speaker/output-chain limits; hardware CPU/heap and listening need independent evidence.

### A1: Reproducible diagnosis
- [x] Add tools/dsp_a/render.cpp and run.sh: all six voices, fixed MIDI/velocity/accent/slide/release cases, parameters, raw PCM/float metrics, 7x5 TB filter/oscillator matrix and eight-table morph sweep.
- [x] Produce immutable baseline WAVs/metrics from the shared base before DSP changes; include clipping, peak/RMS, tail, spectrum and host timing.
- [ ] Keep speaker/3.5mm capture and project-parameter roundtrip/switch checks distinct.

### A2/A3: Minimal fixes and explicit presets
- [x] Reproduce each chosen defect with a failing focused test before production edit.
- [x] Repair confirmed TB filter instability with bounded CPU and no audio allocations; preserve numeric parameter slots/defaults.
- [ ] Repair confirmed WAVEMORPH retrigger/morph defects only after full-cycle evaluation; keep monophonic model and six controls.
- [x] Add Classic Acid/Deep Bass/Aggressive Acid and Warm Pad-like Lead/Digital Bass/Bright Lead recipes using existing controls; no UI/persistence/generation rewrite.
- [x] Capture candidate WAV/metrics; assert reference engines unchanged and build blind loudness-matched A/B assets.

### Verification and delivery
- [x] Host DSP regression + ASan/UBSan + safe engine switches + parameter roundtrip + SDL + Cardputer build.
- [ ] Compare static/runtime memory and real target CPU/underruns using the same source cases; mark missing listening/capture evidence OPEN.
- [ ] User blind A/B at comparable loudness, device release/switch tests; separate speaker/line-out limitations from DSP.
- [ ] Independent review, report confirmed fixes/risks/presets/evidence and create separate PR, no merge without permission.

## Delivery status

Software candidate and evidence complete; see docs/testing/dsp-a/README.md. WAVEMORPH retrigger repaired; phase alignment remains an isolated experiment. Independent review completed. Hardware runtime measurements, speaker/jack capture and blind musical acceptance remain OPEN. USB must be explicitly released by the other session before recovery or testing.
