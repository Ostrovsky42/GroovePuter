# Song Page Debug Guide

## Problem

Patterns not assigned when cursor is on row 2+ (works only on first row).

## ✨ New: Unified Debug Logging System

MiniAcid now uses a professional debug logging system with:
- ✅ Color-coded output (ERROR=red, WARN=yellow, SUCCESS=green, etc.)
- ✅ Timestamps
- ✅ Per-module enable/disable
- ✅ Multiple log levels
- ✅ Function entry/exit tracking
- ✅ Performance profiling

See [`docs/DEBUG_LOGGING.md`](DEBUG_LOGGING.md) for full documentation.

## Quick Test

### 1. Compile and Upload

```bash
./build.sh
./upload.sh /dev/ttyACM0
```

### 2. Open Serial Monitor

```bash
# Arduino IDE: Tools -> Serial Monitor (115200 baud)
# Or use terminal:
screen /dev/ttyACM0 115200
```

### 3. Test Pattern Assignment

**Test A: First Row (Should Work)**
1. Navigate to SONG page
2. Cursor at row 0, column 0
3. Press `Q`

**Expected output:**
```
[  12345][DEBUG][INPUT] Key 'q' -> patternIdx=0, cursor=(0,0)
[  12346][DEBUG][UI] → assignPattern()
[  12347][DEBUG][UI] assignPattern: row=0, col=0, patternIdx=0, trackValid=1
[  12348][DEBUG][UI] Setting pattern: bankIndex=0, combined=0, track=0
[  12349][OK   ][UI] Pattern 0 assigned to (0,0)
[  12350][DEBUG][UI] ← assignPattern()
```

**Test B: Second Row (Currently Failing)**
1. Press Down Arrow → cursor to row 1
2. Press `Q`

**If it fails, you'll see:**
```
[  12351][DEBUG][INPUT] Key 'q' -> patternIdx=0, cursor=(1,0)
[  12352][DEBUG][UI] → assignPattern()
[  12353][WARN ][UI] Pattern assignment rejected: trackValid=0, onModeButton=0
```

**If it works:**
```
[  12351][DEBUG][INPUT] Key 'q' -> patternIdx=0, cursor=(1,0)
[  12352][DEBUG][UI] → assignPattern()
[  12353][OK   ][UI] Pattern 0 assigned to (1,0)
```

## Log Output Explained

| Color    | Level   | Meaning                              |
|----------|---------|--------------------------------------|
| Gray     | DEBUG   | Detailed flow, variable values       |
| Cyan     | INFO    | Important events                     |
| Yellow   | WARN    | Potential issues                     |
| Red      | ERROR   | Failures, things that shouldn't happen |
| Green    | OK      | Successful operations                |

## Common Issues

### Issue 1: "trackValid=0"

**Symptom:**
```
[WARN][UI] Pattern assignment rejected: trackValid=0
```

**Cause:** Cursor on invalid column (4 or 5)

**Fix:** Check cursor position - should be 0-3

### Issue 2: "onModeButton=1"

**Symptom:**
```
[WARN][UI] Pattern assignment rejected: onModeButton=1
```

**Cause:** Cursor on mode button (column 5)

**Fix:** Move cursor left to track columns

### Issue 3: Pattern Assigned but Not Visible

**Symptom:**
```
[OK][UI] Pattern 0 assigned to (1,0)
```
But nothing shows on screen.

**Cause:** Display not refreshing, or TE_GRID style issue

**Fix:** Check `drawTEGridStyle()` or use MINIMAL style temporarily

## Advanced Debugging

### Enable More Verbose Logging

Edit `src/ui/pages/song_page.cpp`, add at top:

```cpp
// Before #include "debug_log.h"
#define DEBUG_LEVEL 4        // Maximum verbosity
#define DEBUG_UI_ENABLED 1
#define DEBUG_INPUT_ENABLED 1
```

### Track Cursor Movement

Add to `moveCursorVertical()`:

```cpp
void SongPage::moveCursorVertical(int delta, bool extend_selection) {
  // ... existing code ...
  cursor_row_ = row;

  LOG_DEBUG_UI("Cursor moved: row=%d, track=%d", cursor_row_, cursor_track_);

  syncSongPositionToCursor();
}
```

### Profile Performance

Wrap expensive operations:

```cpp
void SongPage::draw(IGfx& gfx) {
  LOG_TIMER("UI", "SongPage::draw");

  // ... rendering code ...
}
```

Output:
```
[  12345][DEBUG][UI] ⏱ SongPage::draw START
[  12360][DEBUG][UI] ⏱ SongPage::draw DONE (15 ms)
```

## Files Modified

- `src/debug_log.h` - ✨ **NEW**: Unified logging system
- `src/ui/pages/song_page.cpp`:
  - Added `#include "debug_log.h"`
  - Replaced `Serial.printf` with `LOG_*_UI` / `LOG_*_INPUT`
  - Lines 282-320: `assignPattern()` with detailed logging
  - Lines 655-665: Key detection logging

## Configuration

### Disable Logging (Production Build)

Add to top of `song_page.cpp`:

```cpp
#define DEBUG_PRESET_SILENT
#include "debug_log.h"
```

### UI Debugging Only

```cpp
#define DEBUG_PRESET_UI_ONLY
#include "debug_log.h"
```

## Next Steps

1. **Compile with logging enabled** (default)
2. **Open Serial Monitor** (115200 baud)
3. **Test pattern assignment** on different rows
4. **Copy color-coded output** and analyze
5. **Share output** if issue persists

## Expected Result

Debug output will pinpoint exactly where the flow breaks:
- ✅ Key detected? → `LOG_DEBUG_INPUT`
- ✅ Function called? → `LOG_FUNC_ENTRY`
- ✅ Track valid? → `trackValid=1`
- ✅ Pattern set? → `LOG_SUCCESS_UI`
- ❌ Rejected? → `LOG_WARN_UI` with reason

---

**Professional debugging made simple** 🔍✨

See also: [`docs/DEBUG_LOGGING.md`](DEBUG_LOGGING.md) for complete documentation.

