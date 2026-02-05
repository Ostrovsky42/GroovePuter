# FEEL/TEXTURE Transition Instructions

**Scope**
This document describes a safe migration path from the current fixed 16-step sequencer and ModePage UI to the new FEEL/TEXTURE logic, while keeping backward compatibility and avoiding audio-thread regressions.

**Current Architecture Facts (From Repo)**
- Patterns are fixed at 16 steps via `SEQ_STEPS` and `SynthPattern::kSteps`/`DrumPattern::kSteps` in `src/dsp/miniacid_engine.h` and `scenes.h`.
- REST already exists as `note == -1`. TIE exists as `note == -2`. Playback logic in `src/dsp/miniacid_engine.cpp` treats them differently.
- Retrigger already exists as `StepFx::Retrig` with `fxParam` in `scenes.h` and is executed in `MiniAcid::advanceStep()` in `src/dsp/miniacid_engine.cpp`.
- Tape FX is gated by `sceneManager_.currentScene().tape.fxEnabled` and processed in the master chain in `src/dsp/miniacid_engine.cpp`.
- LoFi is already supported in `TB303Voice::setLoFiAmount()` and `DrumSynthVoice::setLoFiMode/Amount()`.
- Scene serialization is handled in `scenes.cpp`, and any new fields must be added to both the full JSON serializer and the streaming reader paths.

**Day 1: UI Swap (No DSP Changes Yet)**
- Create `src/ui/pages/feel_texture_page.h` and `src/ui/pages/feel_texture_page.cpp` as a new page (based on IPage or Frame, consistent with your UI stack).
- Replace ModePage with FeelTexturePage at index 6 in `src/ui/miniacid_display.cpp`.
- Keep `ModePage` files in the repo for now to avoid breaking build references and documentation.
- Update user-facing shortcuts in `keys_sheet.md` and `keys_music_grok.md` to reflect `Alt+6` as FEEL/TEXTURE.

**Day 1 Data Model: Add FEEL/TEXTURE State (Persisted)**
- Add a lightweight struct to `scenes.h`, for example:
`struct FeelSettings { uint8_t gridSteps = 16; uint8_t patternBars = 1; bool lofi = false; uint8_t lofiAmt = 50; bool drive = false; uint8_t driveAmt = 70; bool tape = false; };`
- Add `FeelSettings feel;` to `Scene` in `scenes.h`.
- Update `clearSceneData()` in `scenes.cpp` to reset `scene.feel`.
- Serialize and deserialize the new `feel` fields in `scenes.cpp` JSON.
- Update the streaming/evented JSON paths in `scenes.cpp` to handle `feel` fields safely when missing.

**Day 2: REST + Clear Step (No Format Changes Needed)**
- REST can be implemented using `note = -1` to preserve existing scene compatibility.
- Add a `MiniAcid::clear303Step()` helper that resets note, slide, accent, fx, fxParam, timing, velocity, ghost for a single step. This is safer than only calling `clear303StepNote()`.
- In `src/ui/pages/pattern_edit_page.cpp`, add an `R` key to set `note = -1` and clear slide/accent/fx for the current step.
- Add `Delete/Backspace` handling to call `clear303Step()` and `Shift+Delete` to clear the entire pattern using that helper.

**Day 3: Retrigger + Probability (Minimal, Compatible)**
- Retrigger UI should map to existing `StepFx::Retrig` and `fxParam`.
- Suggested mapping: x2 => `fxParam = 1`, x3 => `fxParam = 2`. This matches current retrigger interval logic in `miniacid_engine.cpp`.
- For Probability, add `uint8_t probability = 100;` to `SynthStep` and `DrumStep` in `scenes.h`.
- Update scene JSON read/write to include `prob` as optional field. Default to 100 when missing.
- Add probability gating in `MiniAcid::advanceStep()` for both synth and drums. Skip note triggers when `rand()%100 >= probability`.
- Preserve slide behavior by carrying slide to the next played note if a step is skipped due to probability.

**Day 4: FEEL Grid Integration (Safe First Pass)**
- Introduce `stepsPerBar_` in `MiniAcid` (default 16). Store it in `Scene.feel.gridSteps`.
- Update `MiniAcid::updateSamplesPerStep()` to use:
`samplesPerStep = sampleRateValue * 240.0f / (bpmValue * stepsPerBar_);`
- Keep `SEQ_STEPS` at 16 for now. This changes time-per-step only, not pattern length or storage.
- Use `Scene.feel.patternBars` to control loop length without increasing storage. This can be implemented as a bar counter that repeats the 16 steps across N bars.
- Only after the above is stable, consider true 32-step storage. That requires changing `SynthPattern::kSteps`, `DrumPattern::kSteps`, `SEQ_STEPS`, all UI grids, caches, and JSON arrays.

**Texture Integration (LoFi / Drive / Tape)**
- LoFi should set both synth voices and drums using existing APIs:
`voice303.setLoFiAmount()` and `drums->setLoFiMode/Amount()`.
- Drive can use `TubeDistortion::setDrive()` for both 303 voices and `set303DistortionEnabled()` based on amount > 0.
- Tape should toggle `sceneManager_.currentScene().tape.fxEnabled` without touching the looper mode.
- Apply texture changes inside `AudioGuard` to avoid audio thread glitches.

**Compatibility Rules**
- Do not change pattern sizes or JSON array lengths until a full migration plan exists.
- New JSON fields must be optional and defaulted on load.
- REST remains `note == -1` to avoid breaking old scenes.
- Retrigger uses existing `StepFx` to avoid format drift.

**Test Checklist**
- FEEL/TEXTURE page opens with Alt+6 and navigation works.
- Grid selection changes `Scene.feel.gridSteps` but does not alter existing patterns.
- LoFi/Drive/Tape toggles do not crash audio and persist across scene saves.
- REST and Clear Step work without corrupting existing scenes.
- Retrigger and Probability behave consistently at both low and high BPM.

**Files Likely Touched**
- `src/ui/miniacid_display.cpp`
- `src/ui/pages/feel_texture_page.h`
- `src/ui/pages/feel_texture_page.cpp`
- `src/ui/pages/pattern_edit_page.cpp`
- `src/dsp/miniacid_engine.h`
- `src/dsp/miniacid_engine.cpp`
- `scenes.h`
- `scenes.cpp`
- `keys_sheet.md`
- `keys_music_grok.md`
