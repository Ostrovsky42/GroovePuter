#!/usr/bin/env python3
from pathlib import Path

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
    index_h = read("src/sampler/sample_index.h")
    index_cpp = read("src/sampler/sample_index.cpp")
    page_h = read("src/ui/pages/sampler_page.h")
    page = read("src/ui/pages/sampler_page.cpp")
    boot = read("sampler_boot_registry.ino")

    require("rootDirectory() const" in index_h,
            "SampleIndex must expose the fixed loose-sample browse root")
    require("filesInDirectory(" in index_h and "indexedSubdirectories(" in index_h,
            "folder browser must use memory-only indexed directory views")
    require("scanDirectoryRecursive(" in index_h,
            "loose sample discovery must recurse without replacing the registry later")
    require("nameToId_" not in index_h and "std::map" not in index_h,
            "recursive catalog must not duplicate every basename in a second map")
    require("fileSizeBytes" in index_h,
            "indexed WAV metadata must retain file size for filesystem-free rendering")

    scan = function_body(
        index_cpp,
        "void SampleIndex::scanDirectory(const std::string& dirPath)",
        "void SampleIndex::scanDirectoryRecursive(const std::string& dirPath, int depth)",
    )
    require("rootDirectory_ = normalizeDirectoryPath(dirPath)" in scan,
            "scan root must be frozen before recursive discovery")
    require("scanDirectoryRecursive(rootDirectory_, 0)" in scan,
            "root scan must recurse once during registry construction")
    require("nameToId_" not in scan,
            "root scan must not maintain the removed duplicate basename map")

    recursive = function_body(
        index_cpp,
        "void SampleIndex::scanDirectoryRecursive(const std::string& dirPath, int depth)",
        "std::vector<const SampleFileInfo*> SampleIndex::filesInDirectory(",
    )
    require("kMaxSampleDirectoryDepth" in recursive,
            "recursive discovery must have a bounded directory depth")
    require("childDirectories.push_back(fullPath)" in recursive and
            "scanDirectoryRecursive(child, depth + 1)" in recursive,
            "nested loose sample folders must be traversed")
    require("isHiddenName(name)" in recursive,
            "hidden/system directories must not become sampler folders")
    require("isWavName(name)" in recursive,
            "recursive catalog must remain WAV-only")
    require("nameToId_" not in recursive,
            "recursive discovery must not duplicate path ownership in a basename map")

    indexed_dirs = function_body(
        index_cpp,
        "std::vector<std::string> SampleIndex::indexedSubdirectories(",
        "SampleId SampleIndex::findIdByFilename(",
    )
    require("file.fullPath.rfind(prefix, 0)" in indexed_dirs,
            "browser subdirectories must derive from the stable in-memory catalog")
    require("SD.open" not in indexed_dirs and "opendir(" not in indexed_dirs,
            "indexed folder enumeration must not touch filesystem during browsing")

    legacy_name_lookup = function_body(
        index_cpp,
        "SampleId SampleIndex::findIdByFilename(",
        "GroovePuterSampler::SampleRef SampleIndex::findRefByFilename(",
    )
    require("const SampleFileInfo* match = nullptr" in legacy_name_lookup and
            "match->fullPath != file.fullPath" in legacy_name_lookup and
            "return {0};" in legacy_name_lookup,
            "legacy basename lookup must fail closed when duplicate names exist")

    require("sample_browser_open_" in page_h and
            "browser_subdirs_" in page_h and "browser_files_" in page_h,
            "SamplerPage must retain explicit bounded browser state")

    refresh = function_body(
        page,
        "void SamplerPage::refreshSampleBrowser()",
        "bool SamplerPage::browserGoParent()",
    )
    require("indexedSubdirectories(browser_dir_)" in refresh and
            "filesInDirectory(browser_dir_)" in refresh,
            "browser refresh must use the boot-built in-memory catalog")
    require("SD." not in refresh and "scanDirectory(" not in refresh and
            "getSubdirectories(" not in refresh,
            "browser refresh must never rescan SD")

    draw = function_body(
        page,
        "void SamplerPage::drawSampleBrowser(IGfx& gfx)",
        "bool SamplerPage::handleSampleBrowserEvent(UIEvent& ui_event)",
    )
    require("SD." not in draw and "scanDirectory(" not in draw and
            "preload(" not in draw,
            "browser rendering must stay free of filesystem/loading work")
    require('label = "< .."' in draw and 'label = "> "' in draw,
            "browser must visibly distinguish parent and directory entries")
    require("compactFileSize(file->fileSizeBytes)" in draw,
            "browser file rows must display indexed WAV file size")

    browser_input = function_body(
        page,
        "bool SamplerPage::handleSampleBrowserEvent(UIEvent& ui_event)",
        "bool SamplerPage::handleEvent(UIEvent& ui_event)",
    )
    require("GROOVEPUTER_UP" in browser_input and "GROOVEPUTER_DOWN" in browser_input,
            "browser must support list navigation")
    require("GROOVEPUTER_LEFT" in browser_input and "browserGoParent()" in browser_input,
            "left must navigate to the parent folder")
    require("GROOVEPUTER_RIGHT" in browser_input and
            "activateSampleBrowserSelection()" in browser_input,
            "right must enter/activate the selected browser entry")
    require("GROOVEPUTER_ESCAPE" in browser_input and "closeSampleBrowser()" in browser_input,
            "escape must close the browser")

    page_input = function_body(
        page,
        "bool SamplerPage::handleEvent(UIEvent& ui_event)",
        "const std::string& SamplerPage::getTitle() const",
    )
    require("if (sample_browser_open_) return handleSampleBrowserEvent(ui_event);" in page_input,
            "open browser must own navigation before normal page hotkeys")
    require("file_ctrl_->isFocused()" in page_input and "openSampleBrowser();" in page_input,
            "Enter on SAMPLE must open the folder browser")

    activate = function_body(
        page,
        "bool SamplerPage::activateSampleBrowserSelection()",
        "void SamplerPage::drawSampleBrowser(IGfx& gfx)",
    )
    assign_pos = activate.index("assignIndexedSample(*selected)")
    dirty_pos = activate.index("GroovePuterState::markSceneMutated()", assign_pos)
    close_pos = activate.index("closeSampleBrowser()", dirty_pos)
    require(assign_pos < dirty_pos < close_pos,
            "browser file commit must dirty Scene only after successful assignment")

    assign = function_body(
        page,
        "bool SamplerPage::assignIndexedSample(const SampleFileInfo& candidate)",
        "bool SamplerPage::selectIndexedSample(int direction)",
    )
    preload_pos = assign.index("sampleStore->preload(candidateId)")
    guard_pos = assign.index("withAudioGuard([&]()", preload_pos)
    publish_pos = assign.index(".id = candidateId", guard_pos)
    require(preload_pos < guard_pos < publish_pos,
            "browser assignment must preload before the short audio publication")

    quick_select = function_body(
        page,
        "bool SamplerPage::selectIndexedSample(int direction)",
        "bool SamplerPage::clearCurrentPad()",
    )
    require("filesInDirectory(browser_dir_)" in quick_select,
            "left/right quick selection must stay within the current sample folder")

    require('index.scanDirectory("/sd/samples")' in boot and
            'index.scanDirectory("/samples")' in boot,
            "boot must retain the loose-sample mount fallback")
    require('logSamplerRegistryHeap("before-scan")' in boot and
            'logSamplerRegistryHeap("after-scan")' in boot and
            'logSamplerRegistryHeap("after-bind")' in boot and
            "heap_caps_get_largest_free_block" in boot,
            "ADV acceptance must retain registry heap phase diagnostics")
    require("/kits" not in page and "loadKit(" not in page,
            "folder browser must not silently become the future kit loader")

    print("sampler folder browser source regressions passed")


if __name__ == "__main__":
    main()
