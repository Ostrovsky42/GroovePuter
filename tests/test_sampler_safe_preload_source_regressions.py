#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_body(source: str, start: str, end: str) -> str:
    begin = source.index(start)
    finish = source.index(end, begin)
    return source[begin:finish]


def main() -> None:
    page = read("src/ui/pages/sampler_page.cpp")
    page_h = read("src/ui/pages/sampler_page.h")
    store_h = read("src/sampler/sample_store.h")
    ram_store = read("src/sampler/ram_sample_store.cpp")
    loader = read("src/sampler/sample_loader.cpp")

    assignment = function_body(
        page,
        "bool SamplerPage::assignIndexedSample(const SampleFileInfo& candidate)",
        "bool SamplerPage::selectIndexedSample(int direction)",
    )
    selection = function_body(
        page,
        "bool SamplerPage::selectIndexedSample(int direction)",
        "bool SamplerPage::clearCurrentPad()",
    )

    preload_pos = assignment.index("sampleStore->preload(candidateId)")
    guard_pos = assignment.index("withAudioGuard([&]()", preload_pos)
    assign_pos = assignment.index(".id = candidateId", guard_pos)
    require(preload_pos < guard_pos < assign_pos,
            "WAV preload must complete before the short guarded pad publication")
    require("!mini_acid_.sampleStore->preload(candidateId)" in assignment,
            "sample assignment must validate preload success")
    require("preload failed; previous pad assignment kept" in assignment,
            "failed preload must preserve the previous valid pad assignment")
    require("return true;" in assignment[assign_pos:],
            "only a successfully preloaded candidate may publish candidateId")

    require("filesInDirectory(browser_dir_)" in selection,
            "quick sample arrows must remain scoped to the current browser folder")
    require("for (int attempt = 0; attempt < fileCount; ++attempt)" in selection and
            "assignIndexedSample(candidate)" in selection,
            "failed candidates must be skipped so arrows cannot trap the user")

    # Prevent the historical realtime regression even if page code is later
    # rearranged.
    guarded_blocks = re.findall(
        r"withAudioGuard\(\[&\]\(\)\s*\{(.*?)\}\);",
        page,
        flags=re.DOTALL,
    )
    require(guarded_blocks, "SamplerPage must retain short mutation guards")
    for block in guarded_blocks:
        require("preload(" not in block,
                "SamplerPage must never call preload() while AudioGuard is held")
        require("scanDirectory(" not in block and "bindToStore(" not in block,
                "registry/filesystem work must never run while AudioGuard is held")

    require("triggerPad(" in page and any("triggerPad(" in b for b in guarded_blocks),
            "direct/prelisten trigger may remain a short guarded audio mutation")
    require("Main Thread: Request to load a sample into RAM" in store_h,
            "ISampleStore must keep preload explicitly control-thread owned")

    # 0.9.5-A replaces the 0.9.3 private probe with a public inspect/decode
    # contract. The parser itself must stay allocation-free and must bound RIFF
    # traversal including mandatory even-byte chunk padding.
    parser = function_body(
        loader,
        "bool inspectOpenFile(WavFile& file, WavInspectResult& result,",
        "int16_t decodeLe16(const uint8_t* p)",
    )
    require("SAMPLE_MALLOC" not in parser and "malloc(" not in parser,
            "WAV inspection/parser must never allocate PCM")
    require("chunkSize & 1u" in parser and "paddedBytes" in parser,
            "RIFF traversal must account for odd-sized chunk padding")
    require("riffEnd > physicalSize" in parser,
            "declared RIFF size must be bounded by physical file size")
    require("paddedBytes > riffEnd - payloadOffset" in parser,
            "chunk payload/padding must be bounded by declared RIFF end")
    require("decodedBytes64 > maxDecodedBytes" in parser,
            "inspection must enforce explicit decoded-byte admission")
    require("channels != 1 && channels != 2" in parser,
            "0.9.5-A must accept exactly mono/stereo source channels")
    require("audioFormat != 1 || bitsPerSample != 16" in parser,
            "0.9.5-A must remain PCM16-only")
    require("byteRate = readLe32(fmt + 8)" in parser and
            "expectedByteRate64" in parser and
            "byteRate != static_cast<uint32_t>(expectedByteRate64)" in parser,
            "PCM fmt byteRate must exactly match sampleRate * blockAlign")
    require("if (fmtFound)" in parser and "if (dataFound)" in parser,
            "duplicate required fmt/data chunks must fail closed")
    require("if (fmtFound && dataFound) break;" not in parser,
            "parser must traverse the complete declared RIFF after required chunks")
    require("malformed trailing chunks" in parser,
            "full-RIFF traversal intent must remain explicit in source")

    inspect = function_body(
        loader,
        "bool inspectWavFileBounded(const char* path, WavInspectResult& out,",
        "bool decodeWavFileBounded(const char* path, const WavInspectResult& inspected,",
    )
    require("inspectOpenFile(file, out, maxDecodedBytes, error)" in inspect,
            "public allocation-free inspect must use the hardened parser")

    decode = function_body(
        loader,
        "bool decodeWavFileBounded(const char* path, const WavInspectResult& inspected,",
        "bool inspectWavFileBounded(const char* path, WavInfo& outInfo,",
    )
    recheck_pos = decode.index("inspectOpenFile(file, current, maxDecodedBytes, &inspectError)")
    changed_pos = decode.index("sameInspectResult(inspected, current)", recheck_pos)
    allocation_pos = decode.index("SAMPLE_MALLOC_PSRAM(current.decodedBytes)", changed_pos)
    require(recheck_pos < changed_pos < allocation_pos,
            "decode must revalidate admitted metadata before PCM allocation")
    require("uint8_t scratch[kStereoScratchBytes]" in decode and
            "kStereoScratchBytes = 512" in loader,
            "stereo decode must use bounded chunk scratch")
    require("SAMPLE_MALLOC_PSRAM(current.sourceDataBytes)" not in decode,
            "stereo source payload must never receive a full transient allocation")

    # Near-full-pool admission remains two-pass, but production now consumes the
    # split API directly: inspect -> LRU/slot admission -> decode.
    preload_body = function_body(
        ram_store,
        "bool RamSampleStore::preload(SampleId id)",
        "void RamSampleStore::evictLRU()",
    )
    inspect_pos = preload_body.index(
        "inspectWavFileBounded(path.c_str(), inspected, maxPoolBytes_,")
    required_pos = preload_body.index(
        "const std::size_t requiredSize = inspected.decodedBytes;", inspect_pos)
    evict_pos = preload_body.index("evictLRU();", required_pos)
    budget_pos = preload_body.index(
        "const std::size_t decodeBudget = freePoolBytes();", evict_pos)
    decode_pos = preload_body.index(
        "decodeWavFileBounded(path.c_str(), inspected, &pcm, decodeBudget,",
        budget_pos)
    require(inspect_pos < required_pos < evict_pos < budget_pos < decode_pos,
            "RamSampleStore must inspect/admit/evict before split WAV decode")
    require("currentPoolUsage_ + requiredSize > maxPoolBytes_" in preload_body,
            "RamSampleStore must reclaim capacity using inspected decoded bytes")
    require("Pool is busy; no evictable sample slots" in preload_body,
            "busy referenced pool must fail before decode allocation")
    require("loadWavFileBounded(" not in preload_body,
            "production preload must not re-enter the combined compatibility wrapper")

    # The folder browser is allowed in this A checkpoint, but the unsafe old
    # heuristic KIT LOAD path remains forbidden.
    require("loadKit(" not in page and "openLoadKitDialog" not in page and
            "kit_ctrl_" not in page_h,
            "folder browsing must not reintroduce the old unsafe KIT LOAD path")

    # No filesystem/loading work belongs in the render/trigger stack.
    for path in (
        "src/sampler/sampler_voice.cpp",
        "src/sampler/sampler_pool.cpp",
        "src/sampler/drum_sampler_track.cpp",
    ):
        source = read(path)
        require("SD.open" not in source and "loadWavFile(" not in source and
                "preload(" not in source,
                f"realtime sampler path must remain free of SD/WAV/preload work: {path}")

    print("sampler safe-preload source regressions passed")


if __name__ == "__main__":
    main()
