# Voice / Formant audit — memory-prune-a

Audited source HEAD `af4d5107`, using existing `build/long-melody-integrated-runtime/GroovePuter.ino.elf` and map. Read global `/home/gg/AGENTS.md` and `/home/gg/.codex/AGENTS.md`; no worktree AGENTS files found. Read-only investigation except this document. No hardware test or firmware rebuild performed.

## Decision

**Keep FormantSynth, VoiceCompressor, Song voice lane and persisted vocal data.** Voice synthesis has an actual musical consumer even though the standalone VoicePage has no constructor call or registration in current source. Removing the subsystem would silently remove Song vocals. SONG FORM is energy-curve arrangement generation (`src/state/energy_curve.h`, `song_page_r4_owner.inc`), independent of formant speech; never conflate the two. Preserve FORM, effects, SMF and GRAB.

VoiceCache and VocalMixer are credible future cleanup candidates, but only **44 bytes of direct object storage combined** before alignment; no removal implemented or saving claimed. This is a poor primary memory target. Cache initialization may induce SD allocations, but those are not quantified here and SD remains necessary for other features.

## Active paths and preservation evidence

* `MiniAcid::applySongPositionSelection`, engine.cpp:1985–2015, reads `SongTrack::Voice`; while playing, IDs 0–15 call `speakPhrase`, IDs >=16 call `speakCustomPhrase`. This is actual Song playback, independent of VoicePage visibility.
* engine.cpp:2878–2882 renders active, unmuted FormantSynth through VoiceCompressor and adds the result to audio. Compressor removal changes vocal timbre/loudness.
* engine.cpp:2984–2987 routes VoicePitch/Speed/Robotness/Volume parameter changes to live synthesis. Engine scene load/save, :3601–3613 and :3756–3765, transfers vocal settings and custom phrases. `scenes.cpp` serializes/loads vocal settings and customPhrases (including :2458–2460, :2957–2988); compatibility must remain.
* VoicePage supports phrase selection/editing, preview and parameter presets in source, but `rg VoicePage` finds no instantiation outside its implementation/declaration. ELF `nm -C` has no VoicePage function symbols: its object appears in map cross references/discarded sections, which is not proof of retained code. Removing its source cannot be credited with its nominal 180-byte class size.
* No direct formant-speech MIDI input/output or SMF routing was found: current voice trigger is Song phrase selection; generic MIDI “voice” names largely refer to synth/drum ownership, not speech. This does not authorize deleting MIDI paths or Song Voice state.
* Formant synthesis and built-in/custom phrase storage are not the melodic generation engine. SONG FORM uses energy-curve arrangement generation, not FormantSynth.

## Measured target object sizes

Measured from the existing target ELF DWARF with host `gdb -batch -ex 'file ...elf' -ex 'p sizeof(TYPE)'`, not host ABI or source comments:

| Type | ESP32-S3 bytes | Interpretation |
|---|---:|---|
| FormantSynth | 780 | Embedded engine member; 512 bytes are 16 × 32 custom phrases, 108 filter states, 4-byte text pointer. Header's “~4KB” comment is not a measurement. |
| VoiceCompressor | 36 | Embedded member, actively processes live speech. |
| VocalMixer | 4 | Embedded member; only setDuckAmount reset/getter are used. No update/mix call sites. |
| VoiceCache | 40 | Embedded member, including target `fs::File` 24 bytes; no PCM buffer member. |
| VoicePage | 180 | Type layout only; current ELF has no live VoicePage methods, no instantiated page found. |
| Scene | 26304 | Whole scene, not a voice-only saving. Do not attribute entire Scene to speech. |

The four engine members total 860 bytes before owner-layout effects. Removing FormantSynth is prohibited by functional evidence, and removing 44 bytes of cache/mixer members may not yield 44 bytes after layout/alignment. The ELF has `g_miniAcidInstance` size 0x4698 (18072 bytes); these embedded members do not appear as separately removable BSS symbols.

`xtensa-esp32s3-elf-nm -S -C` reports VOWEL_PHONEMES 0x180 (384 bytes) and CONSONANT_PHONEMES 0x360 (864 bytes) at 0x3c13e0cc and 0x3c13dd6c. These are mapped flash/DROM addresses despite nm's lowercase `d`; **not 1248 bytes of internal heap/BSS savings**. No removed firmware was built, so no flash or SRAM delta is claimed.

## Dynamic allocation and incomplete cache

FormantSynth has no new/malloc/vector PCM allocation. `speak` borrows a text pointer; custom phrases are fixed inline arrays. `FORMANT_BUFFER_SIZE=256` does not instantiate a buffer. VoiceCompressor/VocalMixer allocate no heap. VoiceCache constants “5 seconds” and “512 samples” do not allocate 441000 or 1024 bytes.

VoiceCache init creates/checks `/scenes/voices`. Path construction uses std::string; file open/SD access can allocate implementation-owned file/FAT buffers. Quantifying those requires runtime heap tracing; no invented number is assigned. `cachePhrase` and `readSamples` have no production callers. VoicePage C action is explicitly TODO and only prints a request. `speakCached` can open a raw file, but no audio renderer consumes its readSamples; it is not an established functional musical cache. stopSpeaking stops both synthesis and cache state. Cache clear/count/query functions exist but are not evidence of audible cached playback.

A separate pre-existing risk: VoicePage custom preview passes `phonetized.c_str()` from a local std::string to a synth that borrows the pointer. This page is currently uninstantiated; flagged for any future restoration, not changed in this memory audit.

## Checks

* Target DWARF sizes and member offsets inspected via gdb (`ptype /o FormantSynth`, `ptype /o VoiceCache`).
* Target nm and map inspected for live symbols, DROM lookup tables, and engine storage.
* Repository-wide rg call-site searches for all four classes, speak/custom/cached methods, readSamples/cachePhrase and mixer update/mix.
* `python3 tests/test_scene_revision_source_regressions.py` exited 0. This checks existing source invariants, not audible or hardware behavior.

No production changes, SDK modifications, flashing, commit, or excluded feature. Safest decision for this pass: **defer Voice/Formant pruning; retain musical speech and its persistence**. If later removing cache/mixer scaffolding, measure the resulting ELF and exercise Song built-in/custom vocal playback, mute, scene round-trip, and unchanged FORM/SMF/GRAB/effects first.
