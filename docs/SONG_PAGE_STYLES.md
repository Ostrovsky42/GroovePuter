# SongPage Visual Style Switching

## Overview

`SongPage` uses global UI styles with a direct mapping:

- **MINIMAL** — clean minimal
- **AMBER** — night/low-light
- **RETRO_CLASSIC** — retro futurism

Legacy compatibility:
- `MINIMAL_DARK` renders the TE-grid variant.

## Quick Start

### Default: TE_GRID Style

The page uses **TE_GRID** by default (Teenage Engineering inspired):

```cpp
// Nothing needed - TE_GRID is active by default
```

### Switch to Another Style

```cpp
// In your display initialization:
auto songPage = std::make_unique<SongPage>(gfx, mini_acid, audio_guard);
songPage->setVisualStyle(SongPage::VisualStyle::MINIMAL);
pages_.push_back(std::move(songPage));
```

## Visual Styles

### TE_GRID Style (Default)

**Inspiration**: Teenage Engineering OP-1, OP-Z, PO series

```
┌─────────────────────────────────┐
│█ SONG A          > 001/032 █████│
├─────────────────────────────────┤
│ #  A    B    D    V             │
│ ───────────────────────────────  │
│ 01 ┌───┐┌───┐┌───┐┌───┐         │
│    │1-1││2-3││1-2││ --│         │
│ 02 ├───┤├───┤├───┤├───┤         │
│    │1-2││2-4││1-3││ --│         │
│ 03 ├───┤├───┤├───┤├───┤         │
│    │1-3││2-5││1-4││1-1│         │
├─────────────────────────────────┤
│ Q-I:PAT G:GEN A+J/K:SLOT  LP:1-8│
└─────────────────────────────────┘
```

**Features**:
- ✅ Strict monochrome (black/white/gray)
- ✅ Grid-always-visible design
- ✅ Square cells with borders
- ✅ Brutalist header/footer bars
- ✅ Compact, dense layout
- ✅ Professional utilitarian look

**Design principles**:
- Every cell has a border
- No gradients, no shadows
- Function over form
- Maximum information density
- Clear visual hierarchy

### MINIMAL Style

Clean, spacious layout:

```
┌─────────────────────────────────┐
│ POS  303A  303B  Drums  Voice   │
│ 01   1-1   2-3   1-2    ---     │
│ 02   1-2   2-4   1-3    ---     │
│ 03   1-3   2-5   1-4    1-1     │
└─────────────────────────────────┘
```

### RETRO_CLASSIC Style

Neon cyberpunk with glow effects:

```
╔═════════════════════════════════╗
║ ⚡ SONG ║ PLAY ║ 🔴 140 BPM     ║
╠═════════════════════════════════╣
║ 01 │◉1-1│◉2-3│◉1-2│ -- │        ║
║ 02 │ 1-2│ 2-4│ 1-3│ -- │        ║
╚═════════════════════════════════╝
```

### AMBER Style

Amber monochrome terminal:

```
┌─────────────────────────────────┐
│ SONG A          > 001/032       │
│ 01  1-1  2-3  1-2  ---          │
│ 02  1-2  2-4  1-3  ---          │
└─────────────────────────────────┘
```

## API

### SongPage Methods

```cpp
// Set visual style
void setVisualStyle(VisualStyle style);

// Get current style
VisualStyle getVisualStyle() const;

// Available styles
enum class VisualStyle {
    MINIMAL,        // Clean minimal
    RETRO_CLASSIC,  // Neon cyberpunk
    AMBER,          // Amber terminal
    TE_GRID         // Teenage Engineering (default)
};
```

### Dynamic Style Switching

```cpp
// Add to handleEvent():
if (ui_event.key == 'v' && ui_event.ctrl) {
    // Cycle through styles
    int current = static_cast<int>(getVisualStyle());
    int next = (current + 1) % 4;
    setVisualStyle(static_cast<VisualStyle>(next));
    return true;
}
```

## Design Comparison

| Style          | Density | Colors | Borders | Aesthetic    |
|----------------|---------|--------|---------|--------------|
| TE_GRID        | High    | Mono   | Always  | Brutalist    |
| MINIMAL        | Medium  | Multi  | Select  | Clean        |
| RETRO_CLASSIC  | Medium  | Neon   | Glow    | Cyberpunk    |
| AMBER          | High    | Amber  | Minimal | Terminal     |

## TE_GRID Design Details

### Color Palette

```cpp
const IGfxColor TE_BLACK  = 0x000000;  // Background
const IGfxColor TE_WHITE  = 0xFFFFFF;  // Header/footer
const IGfxColor TE_GRID   = 0x404040;  // Grid lines
const IGfxColor TE_ACCENT = 0xC0C0C0;  // Selected cells
const IGfxColor TE_DIM    = 0x808080;  // Inactive text
```

### Layout Grid

```
Cell size: 28×9 pixels (square-ish ratio)
Spacing: 1px (tight grid)
Header: 11px (solid bar)
Footer: 11px (command bar)
```

### Typography

- Monospace font only
- 2px padding inside cells
- All-caps for labels
- Numbers always 2-digit (zero-padded)

### Interaction States

1. **Default**: Black bg, white grid, white text
2. **Selected**: Light gray bg, black text
3. **Playhead**: Dark gray bg, white text
4. **Empty**: Dim gray "--"

## Usage Examples

### Global Style Selection

```cpp
// In settings or initialization:
SongPage::VisualStyle globalStyle = SongPage::VisualStyle::TE_GRID;

#ifdef DEVICE_OLED_SCREEN
    globalStyle = SongPage::VisualStyle::AMBER;
#endif

songPage->setVisualStyle(globalStyle);
```

### Per-Session Style

```cpp
// Save in scene data:
struct Scene {
    // ...
    int songPageStyle = 3;  // 0=MINIMAL, 1=RETRO, 2=AMBER, 3=TE_GRID
};

// Apply on load:
songPage->setVisualStyle(
    static_cast<SongPage::VisualStyle>(scene.songPageStyle)
);
```

## Performance

| Style          | Draw Time | Flash | RAM  |
|----------------|-----------|-------|------|
| TE_GRID        | ~10ms     | +2 KB | 0 KB |
| MINIMAL        | ~8ms      | 0 KB  | 0 KB |
| RETRO_CLASSIC  | ~14ms     | +8 KB | 0 KB |
| AMBER          | ~10ms     | +4 KB | 0 KB |

**Recommendation**: TE_GRID is the sweet spot between aesthetics and performance.

## Why TE_GRID Style?

Inspired by Teenage Engineering's design philosophy:

1. **Brutalism**: Raw, functional, no decoration
2. **Grid thinking**: Everything aligns to a grid
3. **Information density**: Maximum data in minimum space
4. **Monochrome**: Focus on function, not color
5. **Borders matter**: Clear visual separation
6. **Typography**: Clean, consistent, readable

This matches devices like:
- **OP-1**: Grid-based workflow, monochrome UI elements
- **OP-Z**: Dense information, clear grid
- **PO series**: Brutalist approach, maximum utility

## Troubleshooting

### Text is cut off

**Solution**: Reduce cell_w or adjust font size:
```cpp
int cell_w = 26;  // Instead of 28
```

### Grid lines too thick

**Solution**: Use thinner lines:
```cpp
gfx.drawLine(..., IGfxColor(0x202020));  // Darker gray
```

### Not enough visible rows

**Solution**: Reduce cell height:
```cpp
int cell_h = 8;  // Instead of 9
```

## Contributing

When adding new styles:
1. Follow existing pattern (separate `draw*Style()` method)
2. Add enum value
3. Update `draw()` switch statement
4. Document in this file

---

**Created for MiniAcid**
*Bringing professional sequencer aesthetics to DIY hardware* 🎹⚡
