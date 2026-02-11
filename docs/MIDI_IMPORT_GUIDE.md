# MIDI Import Guide

The GroovePuter features a sophisticated MIDI import engine designed for long songs and multi-track arrangements.

## Basic Workflow
1. Put your `.mid` files in the `/midi` folder on your SD card.
2. Go to the **Project Page**.
3. Scroll list contents with focus on `MIDI FILES`.
4. Press **ENTER** on a file to open the **Advanced MIDI Import** dialog.

## Advanced Settings

### 1. Multi-Track Routing
Each of the three MIDI source types can be independently routed to any internal hardware track or skipped:
- **Track A** (from `synthAChannel`): Route to `> HW A`, `> HW B`, `> DRUMS`, or `SKIP`.
- **Track B** (from `synthBChannel`): Route to `> HW A`, `> HW B`, `> DRUMS`, or `SKIP`.
- **Track D** (from `drumChannel`): Route to `> HW A`, `> HW B`, `> DRUMS`, or `SKIP`.

*Tip: You can route both A and B to the same Synth HW for complex layered sequences.*

### 2. Import Profiles
- **LOUD**: Maximizes dynamics, ensuring accents are clearly triggered.
- **CLEAN**: Keeps velocities closer to the source for a more "polite" sound.

### 3. Slicing & Range
- **From Bar**: Choose where to start reading from the MIDI file (e.g., skip the intro).
- **Length**: Specify how many bars to import. Set to `AUTO` to import the entire file.

### 4. Smart Target Allocation
- **Target Pattern**: Choose the starting pattern index in memory.
- **Mode (APPEND / OVERWRITE)**:
    - **OVERWRITE**: Replaces existing patterns in the target range.
    - **APPEND**: Places imported patterns after existing song data and automatically extends the Song arrangement.
- **Auto-Find (F)**: Press `F` while focused on the Mode or Target setting to automatically find a block of empty patterns large enough for your import.

## Technical Details
- **Octave Folding**: Melodic notes outside the 303 range (C2-C7) are automatically folded back into the range.
- **Slide Detection**: Overlapping notes in the MIDI file are converted to Sequencer Slides.
- **Drum Mapping**: MIDI notes are mapped to the 808-style drum voices (Kick=35/36, Snare=38/40, etc.).
- **Long Songs**: You can import up to 128 patterns (8 pages of 16 patterns). The engine will automatically handle paging during playback.
