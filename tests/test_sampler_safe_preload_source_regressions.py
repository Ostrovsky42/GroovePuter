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

    selection = function_body(
        page,
        "bool SamplerPage::selectIndexedSample(int direction)",
        "void SamplerPage::adjustFocusedElement(int direction)",
    )

    preload_pos = selection.index("sampleStore->preload(candidateId)")
    guard_pos = selection.index("withAudioGuard([&]()", preload_pos)
    assign_pos = selection.index(".id = candidateId", guard_pos)
    require(preload_pos < guard_pos < assign_pos,
            "WAV preload must complete before the short guarded pad publication")
    require("if (!mini_acid_.sampleStore->preload(candidateId))" in selection,
            "sample assignment must validate preload success")
    require("previous pad assignment kept" in selection,
            "failed preload must preserve the previous valid pad assignment")
    require("return false;" in selection[preload_pos:guard_pos],
            "failed preload must exit before publishing candidateId")

    # Prevent the historical regression even if code is later rearranged.
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

    # Oversized WAV admission belongs before any PCM allocation or data-chunk
    # read. Metadata parsing may read RIFF/fmt/data headers first.
    bounded_start = loader.index("bool loadWavFileBounded(")
    wrapper_start = loader.index("\nbool loadWavFile(", bounded_start)
    bounded = loader[bounded_start:wrapper_start]
    admission_pos = bounded.index("decodedBytes > maxDecodedBytes")
    allocation_pos = bounded.index("SAMPLE_MALLOC_PSRAM(rawBytes)")
    payload_pos = bounded.index("Data-chunk read starts only after final decoded-size admission")
    require(admission_pos < allocation_pos < payload_pos,
            "decoded-size admission must happen before PCM allocation/data read")
    require("loadWavFileBounded(path.c_str(), info, &pcm, maxPoolBytes_)" in ram_store,
            "RamSampleStore must pass the active sampler pool budget into WAV admission")

    # 0.9.3 deliberately removes the unsafe historical kit shortcut rather
    # than leaving a second scan/register/preload path with different rules.
    require("loadKit(" not in page and "openLoadKitDialog" not in page and
            "kit_ctrl_" not in page_h,
            "0.9.3 must not expose the old unsafe KIT LOAD path")

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
