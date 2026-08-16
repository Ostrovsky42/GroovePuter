#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


ram_h = read("src/sampler/ram_sample_store.h")
ram_cpp = read("src/sampler/ram_sample_store.cpp")
index_h = read("src/sampler/sample_index.h")
voice_h = read("src/sampler/sampler_voice.h")
pool_h = read("src/sampler/sampler_pool.h")
track_h = read("src/sampler/drum_sampler_track.h")
scenes_h = read("scenes.h")
page_h = read("src/ui/pages/sampler_page.h")

# Fixed sampler surface. S1 is an audit and must not silently shrink it.
require(ram_h, "static constexpr int kMaxSampleSlots = 64;", "64-slot baseline")
require(pool_h, "static constexpr int kMaxVoices = 8;", "8 logical voices")
require(track_h, "static constexpr int kNumPads = 16;", "16 internal pads")
require(scenes_h, "SamplerPadState samplerPads[16];", "16 persisted pad states")

# Fixed idle objects versus dynamic ownership.
require(ram_h, "std::array<SampleSlot, kMaxSampleSlots> slots_;", "fixed SampleSlot table")
require(ram_h, "std::map<uint32_t, std::string> filePaths_;", "store path registry")
require(index_h, "std::vector<SampleFileInfo> files_;", "full catalog vector")
require(index_h, "std::map<std::string, SampleId> nameToId_;", "basename lookup map")
require(index_h, "std::string filename;", "per-file filename ownership")
require(index_h, "std::string fullPath;", "per-file full-path ownership")
require(pool_h, "std::array<SamplerVoice, kMaxVoices> voices_;", "fixed voice array")
require(track_h, "std::array<SamplerPad, kNumPads> pads_;", "fixed pad array")

# Whole-file resident PCM remains the current production-base architecture.
require(ram_cpp, "int16_t* pcm = nullptr;", "whole-file PCM pointer")
require(ram_cpp, "decodeWavFileBounded", "whole-file bounded decoder call")
require(voice_h, "const int16_t* pcm_ = nullptr;", "voice resident PCM pointer")

# UI remains lazy and owns shared components; exact dynamic cost is a hardware/runtime measurement.
require(page_h, "std::shared_ptr<LabelValueComponent>", "SamplerPage component ownership")

print("SAMPLER_MEMORY_OWNERSHIP_BEGIN")
print("A_FIXED_IDLE: RamSampleStore owns fixed SampleSlot[64]")
print("A_FIXED_IDLE: SamplerPool owns fixed SamplerVoice[8]")
print("A_FIXED_IDLE: DrumSamplerTrack owns SamplerPad[16] + SamplerPool")
print("B_CATALOG: SampleIndex owns vector<SampleFileInfo> for the full library")
print("B_CATALOG: SampleFileInfo owns filename + fullPath strings")
print("B_CATALOG: SampleIndex also owns nameToId map")
print("B_CATALOG: RamSampleStore also owns runtime-id -> fullPath map")
print("C_FRAGMENTATION: catalog strings/maps are long-lived dynamic ownership candidates")
print("D_UI: SamplerPage owns shared_ptr components; runtime heap delta still required")
print("E_PCM: preload decodes one complete PCM allocation and SamplerVoice pins it")
print("SAMPLER_MEMORY_OWNERSHIP_END")
