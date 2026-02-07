# Debug Logging System

## Overview

MiniAcid uses a unified, color-coded debug logging system with per-module control and multiple log levels.

## Quick Start

### 1. Include the Header

```cpp
#include "debug_log.h"
```

### 2. Use Module-Specific Macros

```cpp
// UI module
LOG_DEBUG_UI("Cursor at (%d,%d)", row, col);
LOG_INFO_UI("Page changed to SONG");
LOG_WARN_UI("Invalid track: %d", track);
LOG_ERROR_UI("Pattern assignment failed");
LOG_SUCCESS_UI("Pattern saved successfully");

// DSP module
LOG_DEBUG_DSP("Generating %d samples", count);
LOG_WARN_DSP("High CPU usage: %d%%", cpu);

// Scene module
LOG_DEBUG_SCENE("Loading scene: %s", name);
LOG_SUCCESS_SCENE("Scene saved: %s", name);

// Input module
LOG_DEBUG_INPUT("Key pressed: '%c'", key);
LOG_INFO_INPUT("Encoder delta: %d", delta);

// Pattern module
LOG_DEBUG_PATTERN("Generated pattern with %d notes", notes);
LOG_SUCCESS_PATTERN("Pattern copied to clipboard");
```

## Log Levels

| Level   | Macro          | Color   | Use Case                    |
|---------|----------------|---------|------------------------------|
| DEBUG   | LOG_DEBUG_*    | Gray    | Detailed flow information   |
| INFO    | LOG_INFO_*     | Cyan    | General information         |
| WARN    | LOG_WARN_*     | Yellow  | Potential issues            |
| ERROR   | LOG_ERROR_*    | Red     | Errors, failures            |
| SUCCESS | LOG_SUCCESS_*  | Green   | Successful operations       |

## Modules

| Module  | Macro Prefix    | Controls                          |
|---------|-----------------|-----------------------------------|
| UI      | LOG_*_UI        | Pages, widgets, navigation        |
| DSP     | LOG_*_DSP       | Audio synthesis, processing       |
| SCENE   | LOG_*_SCENE     | Scene management, serialization   |
| INPUT   | LOG_*_INPUT     | Keyboard, encoder                 |
| AUDIO   | LOG_*_AUDIO     | Audio I/O, buffers                |
| PATTERN | LOG_*_PATTERN   | Pattern generation, editing       |

## Configuration

### Enable/Disable Modules

In your source file, **before** including `debug_log.h`:

```cpp
// Disable DSP logging (too verbose)
#define DEBUG_DSP_ENABLED 0

// Enable only UI and Input
#define DEBUG_UI_ENABLED 1
#define DEBUG_INPUT_ENABLED 1
#define DEBUG_DSP_ENABLED 0
#define DEBUG_SCENE_ENABLED 0
#define DEBUG_AUDIO_ENABLED 0
#define DEBUG_PATTERN_ENABLED 0

#include "debug_log.h"
```

### Set Global Log Level

```cpp
// Show only warnings and errors
#define DEBUG_LEVEL 2  // 0=OFF, 1=ERROR, 2=WARN, 3=INFO, 4=DEBUG

#include "debug_log.h"
```

### Use Presets

```cpp
// Preset: UI debugging only
#define DEBUG_PRESET_UI_ONLY
#include "debug_log.h"

// Preset: Performance profiling
#define DEBUG_PRESET_PERFORMANCE
#include "debug_log.h"

// Preset: Silent (production)
#define DEBUG_PRESET_SILENT
#include "debug_log.h"
```

## Advanced Features

### Function Entry/Exit Tracking

```cpp
void MyClass::myMethod() {
  LOG_FUNC_ENTRY("UI");

  // ... do work ...

  LOG_FUNC_EXIT("UI");
}
```

Output:
```
[  12345][DEBUG][UI] → myMethod()
[  12356][DEBUG][UI] ← myMethod()
```

### Performance Timing

```cpp
void expensiveOperation() {
  LOG_TIMER("DSP", "expensiveOperation");

  // ... expensive work ...

  // Timer automatically logs elapsed time on scope exit
}
```

Output:
```
[  12345][DEBUG][DSP] ⏱ expensiveOperation START
[  12567][DEBUG][DSP] ⏱ expensiveOperation DONE (222 ms)
```

### Conditional Logging

```cpp
LOG_IF(errorCode != 0, LOG_ERROR_UI, "Operation failed: %d", errorCode);
```

### Hex Dump

```cpp
uint8_t data[16] = {0xDE, 0xAD, 0xBE, 0xEF, ...};
LOG_HEX("SCENE", data, 16);
```

Output:
```
[  12345][DEBUG][SCENE] Hex dump (16 bytes):
DE AD BE EF 01 02 03 04 05 06 07 08 09 0A 0B 0C
```

## Output Format

```
[timestamp][level][module] message
```

Example:
```
[  12345][DEBUG][UI] Cursor at (1,2)
[  12350][INFO ][UI] Page changed to SONG
[  12351][WARN ][UI] Invalid track: 5
[  12352][ERROR][UI] Pattern assignment failed
[  12353][OK   ][UI] Pattern saved successfully
```

## Color Output

When viewed in a terminal that supports ANSI colors:

- **DEBUG** - Gray (low importance details)
- **INFO** - Cyan (general information)
- **WARN** - Yellow (potential issues)
- **ERROR** - Red (failures)
- **SUCCESS** - Green (successful operations)

### Enable Colors on ESP32

Colors are enabled by default on ESP32. Use a terminal that supports ANSI:
- Arduino IDE Serial Monitor ✅
- PlatformIO Serial Monitor ✅
- `screen /dev/ttyACM0 115200` ✅
- `minicom` ✅

## Examples

### Example 1: Song Page Pattern Assignment

```cpp
#include "debug_log.h"

bool SongPage::assignPattern(int patternIdx) {
  LOG_FUNC_ENTRY("UI");

  int row = cursorRow();
  int col = cursorTrack();

  LOG_DEBUG_UI("assignPattern: row=%d, col=%d, patternIdx=%d",
               row, col, patternIdx);

  bool trackValid = false;
  SongTrack track = trackForColumn(col, trackValid);

  if (!trackValid) {
    LOG_ERROR_UI("Invalid track column: %d", col);
    return false;
  }

  // ... perform assignment ...

  LOG_SUCCESS_UI("Pattern %d assigned to (%d,%d)", patternIdx, row, col);
  LOG_FUNC_EXIT("UI");
  return true;
}
```

Output:
```
[  12345][DEBUG][UI] → assignPattern()
[  12346][DEBUG][UI] assignPattern: row=1, col=2, patternIdx=3
[  12347][OK   ][UI] Pattern 3 assigned to (1,2)
[  12348][DEBUG][UI] ← assignPattern()
```

### Example 2: DSP Performance Monitoring

```cpp
void MiniAcid::generateAudioBuffer(int16_t* buffer, int frames) {
  LOG_TIMER("DSP", "generateAudioBuffer");

  LOG_DEBUG_DSP("Generating %d frames", frames);

  // ... synthesis code ...

  if (cpuOverload) {
    LOG_WARN_DSP("CPU overload detected: %d%% usage", cpuPercent);
  }
}
```

Output:
```
[  12345][DEBUG][DSP] ⏱ generateAudioBuffer START
[  12346][DEBUG][DSP] Generating 128 frames
[  12360][WARN ][DSP] CPU overload detected: 95% usage
[  12361][DEBUG][DSP] ⏱ generateAudioBuffer DONE (16 ms)
```

### Example 3: Scene Loading with Error Handling

```cpp
bool SceneManager::loadScene(const char* name) {
  LOG_INFO_SCENE("Loading scene: %s", name);

  if (!fileExists(name)) {
    LOG_ERROR_SCENE("Scene file not found: %s", name);
    return false;
  }

  // ... load scene ...

  if (parseError) {
    LOG_ERROR_SCENE("Parse error at line %d", lineNum);
    return false;
  }

  LOG_SUCCESS_SCENE("Scene loaded: %s (%d bytes)", name, size);
  return true;
}
```

## Best Practices

### 1. Use Appropriate Levels

- **DEBUG**: Detailed flow, variable values, loop iterations
- **INFO**: Major state changes, mode switches, user actions
- **WARN**: Recoverable issues, degraded performance, config problems
- **ERROR**: Failures, crashes, data corruption
- **SUCCESS**: Important operations completed successfully

### 2. Module Separation

Keep logs organized by module:
- UI code → `LOG_*_UI`
- DSP code → `LOG_*_DSP`
- Scene code → `LOG_*_SCENE`
- etc.

### 3. Informative Messages

❌ Bad:
```cpp
LOG_DEBUG_UI("Value: %d", x);
```

✅ Good:
```cpp
LOG_DEBUG_UI("Cursor row updated: %d", x);
```

### 4. Performance-Critical Code

Disable verbose logging in audio callback:
```cpp
#define DEBUG_DSP_ENABLED 0  // Audio too fast for serial
#include "debug_log.h"
```

### 5. Disable in Production

For release builds:
```cpp
#define DEBUG_PRESET_SILENT
#include "debug_log.h"
```

All logging becomes zero-overhead (compiled out).

## Troubleshooting

### No Output

1. Check `DEBUG_ENABLED` is defined
2. Check module is enabled: `DEBUG_UI_ENABLED 1`
3. Check log level: `DEBUG_LEVEL 4`
4. Check Serial baud rate: 115200

### Too Much Output

1. Reduce log level: `DEBUG_LEVEL 2` (WARN+ERROR only)
2. Disable verbose modules: `DEBUG_DSP_ENABLED 0`
3. Use presets: `DEBUG_PRESET_UI_ONLY`

### Colors Not Showing

1. Use ANSI-compatible terminal
2. Arduino IDE: Colors work by default
3. PlatformIO: Enable ANSI in settings

## Migration from Serial.printf

**Before:**
```cpp
#ifdef ARDUINO
Serial.printf("[SongPage] Pattern %d assigned\n", idx);
#endif
```

**After:**
```cpp
LOG_SUCCESS_UI("Pattern %d assigned", idx);
```

Benefits:
- ✅ Color coding
- ✅ Timestamps
- ✅ Module tags
- ✅ Per-module enable/disable
- ✅ Works on desktop and Arduino
- ✅ Zero overhead when disabled

---

**MiniAcid Debug Logging** - Professional debugging for professional code 🔍🎹
