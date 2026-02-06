# GroovePuter - AI Agent Guide

Welcome, collaborator! This document provides essential context and technical guidelines for working on GroovePuter, a groovebox project optimized for the **M5Stack Cardputer**.

---

## 🚀 Project Vision
GroovePuter is a portable, high-performance acid house groovebox.
- **Goal:** Professional-grade DSP on constrained hardware.
- **Style:** Retro 80s aesthetic (neon, scanlines) mixed with modern generative features.
- **Hardware Target:** M5Stack Cardputer (ESP32-S3FN8).

---

## 🛠 Critical Technical Context

### 1. Hardware Constraints (CRITICAL)
- **NO PSRAM:** The StampS3 in the Cardputer has zero PSRAM. Every byte of DRAM is precious.
- **Memory Buffer:** Do NOT use large static buffers. Use `ArduinoJson` carefully (preferably streaming/evented).
- **Core Split:** 
  - **Core 1:** Audio Engine (strictly realtime).
  - **Core 0:** UI, Logic, Networking, Storage.

### 2. Thread Safety & Synchronization
- **Avoid `volatile`:** For cross-core communication, use `std::atomic` and proper memory ordering.
- **Waveform Buffer:** Uses internal atomic double-buffering. Access it via `mini_acid_.getWaveformBuffer()`.
- **AudioGuard:** Use the provided locking mechanism when the UI thread modifies synth parameters to prevent audio glitches.

### 3. Build Configuration
- **Script:** Use `build-psram.sh` (Note: The name is legacy; it builds for DRAM-only by default).
- **Retro Theme:** Enabled via `-DUSE_RETRO_THEME`. Use conditional guards `#ifdef USE_RETRO_THEME` around heavy UI/Widget code.

---

## 🎨 UI/UX Guidelines

### Compact Display (240x135)
- **Primary Font:** 5x7 monospaced.
- **Vertical Space:** Headers are 16-18px. Gain space by using 1px line spacing between list items.
- **Abbreviations:** Use 3-letter uppercase for params (e.g., `CUT`, `RES`, `ENV`, `OSC`).
- **Visual Feedback:** Use LED states and visual "glow" effects (optimized with single shadow on ESP32).

### Page Patterns
- Use the `UIPage` abstraction.
- Use `LayoutManager` for consistent header/footer positioning.
- **Page Numbers:** Always display `Current/Total` indicator in the header.

---

## 🔉 Audio Engine Rules

### DSP Standards
- **Sample Rate:** Default 22050 Hz for Cardputer, 44100 Hz for Desktop/PCM.
- **Normalization:** Internal parameters use 0.0 to 1.0. Voices should handle mapping to frequency/milliseconds.
- **Soft Clamping:** Every voice should have a soft limiter at the end of its chain to prevent digital clipping before the master mix.

### Genre System
- Use `GenreManager` for switching visual/audio "flavors".
- **Texture Bias:** Reset bias tracking when loading new scenes.

---

## 💾 State & Persistence

### Saving Reliability
- **Auto-Save:** Implemented in `stop()`, `loadSceneByName()`, and `cleanup()`.
- **Storage:** Uses SD card (Cardputer) or JSON files (Desktop/SDL).
- **Hard-Flush:** Always call `file.flush()` before `file.close()` on ESP32 to prevent truncation.
- **Verification:** After writing, re-open the file in `FILE_READ` mode and verify `file.size()` matches the bytes written.

### JSON Streaming Pitfalls
- **Structural Integrity:** Streaming writers (e.g., `writeSceneJson`) are sensitive. A missing `}` or `,` will cause "Unexpected EOF" on read.
- **Nesting Alert:** Always double-check that fields are closed in the correct order. (e.g., `led` must be closed before `vocal` if they are siblings in `state`).
- **Debugging:** For large streaming files, use `Serial.print(".")` during writing and a byte counter in the reader's lambda to pinpoint the exact failure offset.

---

## 🤖 Future Roadmap

If you are implementing new features, consider these planned items:
1. **Formant Vocal Synth:** Robotic vocals using compact filter banks (~4KB ROM).
2. **Song Mode Quick Actions:** Fast pattern layout generation.
3. **Lazy Scene Loading:** Dynamic asset loading from LittleFS to save DRAM.

---

## 📝 Tips for AI Agents
1. **Check the Platform:** Always verify if you are editing `platform_sdl` (Desktop) or `src` (Common/ESP32).
2. **Review `deep_review.md`:** Check the last deep audit for context on architecture scores.
3. **Use `IGfx`:** Never call display-specific functions directly. Use the abstraction.
4. **Mind the Audio Thread:** Never perform I/O or allocate memory in the audio callback.

---

*Stay creative, stay efficient, keep the acid flowing.* 🎧💊
