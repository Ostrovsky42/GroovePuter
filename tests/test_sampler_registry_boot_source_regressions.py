#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(text: str, needle: str, context: str) -> None:
    assert needle in text, f"missing {needle!r} in {context}"


sketch = read("GroovePuter.ino")
boot_tab = read("sampler_boot_registry.ino")
sd_cpp = read("src/platform/cardputer_sd.cpp")
sd_h = read("src/platform/cardputer_sd.h")
index_cpp = read("src/sampler/sample_index.cpp")
store_cpp = read("src/sampler/ram_sample_store.cpp")
engine_cpp = read("src/dsp/miniacid_engine.cpp")
scenes_h = read("scenes.h")

# The existing Cardputer setup already mounts storage before MiniAcid::init().
# C deliberately hooks that boundary instead of adding a second SD owner.
early_storage = sketch.index("g_sceneStorage.initializeStorage();")
engine_init = sketch.index("g_miniAcidInstance.init();")
assert early_storage < engine_init, "SD readiness must precede MiniAcid::init()"

# One platform-owned mount emits one readiness callback.
require(sd_h, "using CardputerSdReadyHook = void (*)();", "cardputer_sd.h")
require(sd_h, "setCardputerSdReadyHook", "cardputer_sd.h")
require(sd_cpp, "notifySdReadyOnce();", "cardputer_sd.cpp")
require(sd_cpp, "if (mounted) notifySdReadyOnce();", "cardputer_sd.cpp")
require(sd_cpp, "g_sdReadyNotified", "cardputer_sd.cpp")

# The sampler hook builds only control-side index/registry state. It must be
# installed before setup and must not call preload/loadWavFile itself.
require(boot_tab, "__attribute__((constructor))", "sampler_boot_registry.ino")
require(boot_tab, "setCardputerSdReadyHook", "sampler_boot_registry.ino")
require(boot_tab, 'scanDirectory("/sd/samples")', "sampler_boot_registry.ino")
require(boot_tab, 'scanDirectory("/samples")', "sampler_boot_registry.ino")
require(boot_tab, "bindToStore(g_sampleStore)", "sampler_boot_registry.ino")
require(boot_tab, "g_miniAcidInstance.sampleStore = &g_sampleStore", "sampler_boot_registry.ino")
assert "preload(" not in boot_tab
assert "loadWavFile" not in boot_tab
assert "sampleIndex.scanDirectory" not in sketch
assert "g_sampleStore.registerFile" not in sketch

# Stable SampleRef still validates every binding. D may choose a collision-safe
# compact runtime ID, but RamSampleStore must never accept last-write-wins ID
# ownership.
require(index_cpp, "calculateStableRef(file.fullPath)", "sample_index.cpp")
require(index_cpp, "runtimeIdForFile(file)", "sample_index.cpp")
require(index_cpp, "store.bindSampleIndex(this)", "sample_index.cpp")
require(index_cpp,
        "!indexBackedStore && !store.registerFile(runtimeId, file.fullPath)",
        "sample_index.cpp")
require(store_cpp, "sampleIndex_->resolveRuntimeFile(id)", "ram_sample_store.cpp")
require(store_cpp, "refusing conflicting ID", "ram_sample_store.cpp")
assert "filePaths_[id.value] = path" not in store_cpp

# D must still leave the audio/runtime Scene payload compact. Stable refs are
# translated at persistence/control boundaries; actual preload remains owned by
# applySceneStateFromManager.
require(scenes_h, "uint32_t sampleId = 0;", "scenes.h")
require(engine_cpp, "p.id.value = s.sampleId;", "miniacid_engine.cpp")
require(engine_cpp, "sampleStore->preload(p.id)", "miniacid_engine.cpp")

print("sampler registry boot source regressions passed")
