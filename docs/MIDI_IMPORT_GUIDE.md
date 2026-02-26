# MIDI Import Guide

The GroovePuter features a sophisticated MIDI import engine designed for long songs and multi-track arrangements.

## Basic Workflow
1. Put your `.mid` files in the `/midi` folder on your SD card.
2. Go to the **Project Page**.
3. Scroll list contents with focus on `MIDI FILES`.
4. Press **ENTER** on a file to open the **Advanced MIDI Import** dialog.

## Advanced Settings

### 1. Matrix Routing (Track Map)
The GroovePuter uses an interactive 4x4 **Track Map** for flexible MIDI routing. Each of the 16 MIDI channels can be routed to one of the internal tracks:
- **Navigation:** Use `Arrows` to move the cursor within the 4x4 grid (Channels 1-16).
- **Toggle Routing:** Press `Enter` or `Space` to cycle a channel's destination:
    - `·` (None): Skip this channel.
    - `A` (Synth A): Route to the first TB-303 voice.
    - `B` (Synth B): Route to the second TB-303 voice.
    - `D` (Drums): Route to the internal drum machine.

*Tip: You can route multiple MIDI channels to the same internal track (e.g., merging a Kick track on Ch 1 and Snare on Ch 2 into > DRUMS).*

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
