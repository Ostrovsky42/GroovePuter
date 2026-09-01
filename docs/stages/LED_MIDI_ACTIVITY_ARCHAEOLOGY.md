# Cardputer ADV LED + MIDI activity archaeology

## Purpose

Restore the verified onboard RGB data path on Cardputer ADV, make LED feedback brighter and more musical without moving work into realtime paths, make Beat the discoverable default, and document the safe boundary for future direct MIDI activity indication.

## Hardware

- M5Stack Cardputer ADV (ESP32-S3)
- Onboard single RGB / NeoPixel-compatible LED on GPIO21
- Optional USB MIDI target such as Yamaha SEQTRAK
- USB-C cable for flashing and MIDI testing

No external LED wiring is required. PORT.A I2C, codec I2C, SD and USB wiring are unchanged.

## Verified hardware mapping

Current M5Unified maps Cardputer ADV `RGBLED` to GPIO21. Cardputer ADV audio enable/configuration is handled through the ES8311 codec path; GPIO21 is not a PA enable GPIO.

The current 0.9.9 hardware profile already models the historical PA enable call as a typed no-op. This PR preserves that safer 0.9.9 behavior and only restores GPIO21 as the RGB data pin.

## LED architecture

`LedManager` remains the sole physical LED writer:

- `StepTrig` receives bounded synth/drum trigger pulses.
- `Beat` receives periodic transport pulses.
- `MuteState` owns low steady-state indication.
- producers publish through the existing atomic single-slot handoff.
- a busy slot drops LED activity rather than delaying audio or MIDI.

## Expression pass

The old rectangular flash is replaced with:

1. instant attack;
2. short peak up to roughly 2x configured brightness, hard-capped at 220;
3. integer quadratic decay;
4. small transient boosts for kick, snare and clap;
5. maximum physical refresh rate of 100 Hz during the tail;
6. existing `flashMs` reused as the base tail control, rendered at 2x and clamped to 40..180 ms.

No allocation, mutex, delay, logging or new queue is added.

## Default behavior

`LedSettings` defaults to `LedMode::Beat` instead of `Off`.

- fresh/default scenes start with Beat enabled;
- `PROJECT RESET` returns to Beat through the canonical `LedSettings()` default;
- saved scenes keep their serialized LED mode, including explicit `Off`;
- there is no boot-time override, so saved user preference is not silently replaced.

Beat is the default because it is source-independent and exposes the feature as soon as transport starts. `StepTrig` remains the source-specific mode for Kick, 303 and other voices.

## MIDI behavior

Internal pattern MIDI and LED `StepTrig` share the same musical trigger sites, so restoring the LED path makes source-selected internal notes visibly track notes that are also emitted over USB MIDI.

Raw SMF output still bypasses those internal trigger sites. A future dedicated `LedMode::Midi` should publish a best-effort activity event only after a successful outbound NoteOn with velocity > 0. It must not perform NeoPixel writes, retries, allocation or delays in the MIDI dispatcher.

## Build / flash

Build the normal Cardputer ADV target from the repository release tooling and flash it to Cardputer ADV. No external wiring or new dependency is required.

## Expected behavior

Default smoke test:

1. use a fresh scene or `PROJECT RESET`;
2. do not open Project > LED;
3. start transport;
4. the onboard LED pulses in Beat mode with an immediate bright attack and smooth decay.

Mode-specific behavior:

- `StepTrig`: selected source gives an immediate bright attack and short smooth decay;
- `Beat`: transport pulses use the same attack/decay character;
- `MuteState`: low steady state remains separate from transient modes;
- brightness 40 is visible indoors; 60/90 gives stronger short peaks;
- audio and USB MIDI timing remain unchanged.

## Troubleshooting

- No light: verify Cardputer ADV target, GPIO21 mapping, and that `LedManager::init()` / `update()` run.
- Fresh/reset project stays dark: verify `LedSettings::mode` is `Beat` and transport beat callbacks are active.
- Wrong source: check Project > LED > Src; `StepTrig` remains source-filtered.
- Too dim: try brightness 60 or 90.
- Too long: reduce flash duration; it now controls the decay tail.
- Audio regression: do not add WS2812 writes to audio code.
- MIDI regression: LED activity must remain best-effort and expendable.

## Acceptance checklist

- [ ] Cardputer ADV boots with internal audio intact.
- [ ] GPIO21 drives the onboard RGB LED.
- [ ] Fresh/default scene starts in Beat LED mode without visiting settings.
- [ ] `PROJECT RESET` restores Beat as the LED default.
- [ ] A saved scene with LED `Off` reloads as Off.
- [ ] StepTrig follows the selected synth/drum source.
- [ ] Beat mode visibly pulses with transport.
- [ ] Pulse has instant attack and smooth decay.
- [ ] Peak brightness never exceeds 220.
- [ ] LED refresh during decay is bounded to 100 Hz.
- [ ] No new allocation, mutex, delay or logging exists in audio/MIDI realtime paths.
- [ ] USB MIDI remains stable while LED pulses.
- [ ] Raw SMF direct MIDI activity remains isolated for a dedicated follow-up.
