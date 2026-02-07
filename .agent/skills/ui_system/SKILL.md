---
name: ui_system
description: Guidelines for the MiniAcid UI architecture and hardware-specific display handling.
---

# UI System Skill

This skill documents the UI architecture optimized for the M5Stack Cardputer's small screen (240x135) and constrained memory.

## 🎨 Visual Identity
- **Style:** Retro 80s / Neon / Scanlines.
- **Color Palette:** Curated HSL colors, avoiding basic RGB.
- **Typography:** 5x7 monospaced font for maximum information density.

## 🛠 Architecture: UIPage & LayoutManager
- **UIPage:** Abstract base class for all screens. Implement `draw()`, `loop()`, and `handleKey()`.
- **LayoutManager:** Handles consistent header (title, BPM, clock) and footer (contextual help) rendering.
- **IGfx Abstraction:** NEVER call display-specific functions directly. Use the `IGfx` interface provided in `display.h`.

## ⚠️ Hardware Constraints
- **NO PSRAM:** Do not use large sprites or canvas buffers in DRAM.
- **Refresh Rate:** UI runs on **Core 0**. High-frequency UI updates should be throttled to 30-60 FPS to leave cycles for networking and storage.

## ⌨️ Input Handling
- **Cardputer Keyboard:** Keyboard is compact and case-sensitive usage is difficult.
- **Shortcuts:**
  - Prefer `plain letters` for direct actions.
  - Use `Ctrl` for "global" or "heavy" actions.
  - Use `Alt` for "alternate" modes or secondary navigation.
- **Page Navigation:** Managed in `miniacid_display.cpp`. Use the assigned Alt+1..0 shortcuts.

## 📁 Key Files
- `src/ui/pages/`: Individual UI page implementations.
- `src/ui/miniacid_display.cpp`: Main UI loop and routing.
- `display.h`: `IGfx` interface definition.
- `cardputer_display.h`: Cardputer-specific implementation of `IGfx`.
