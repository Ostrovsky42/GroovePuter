#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

scene_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
store_cpp = (ROOT / "scene_storage_cardputer.cpp").read_text(encoding="utf-8")
index_h = (ROOT / "src/sampler/sample_index.h").read_text(encoding="utf-8")
index_cpp = (ROOT / "src/sampler/sample_index.cpp").read_text(encoding="utf-8")
persist_h = (ROOT / "src/sampler/sample_scene_persistence.h").read_text(encoding="utf-8")
persist_cpp = (ROOT / "src/sampler/sample_scene_persistence.cpp").read_text(encoding="utf-8")
boot = (ROOT / "sampler_boot_registry.ino").read_text(encoding="utf-8")
engine_cpp = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")

# D must not expand resident Scene sampler state or the audio/runtime ABI.
assert "uint32_t sampleId = 0;" in scene_h
assert "SamplerPadState samplerPads[16];" in scene_h
assert "bool samplerEnabled = true;" in scene_h
assert "uint64_t sampleRef" not in scene_h
assert "SampleRef sampleRef" not in scene_h

# Stable identity is encoded as exact hex text, never through JSON/double.
assert '"%08x%08x"' in persist_cpp
assert "decodeSampleRefHex" in persist_cpp
assert "len != 16" in persist_cpp
assert "std::numeric_limits<uint32_t>::max()" in persist_cpp

# The Cardputer main and recovery Scene paths are both filtered without
# materializing the full Scene JSON in RAM.
assert '#include "src/sampler/sample_scene_persistence.h"' in store_cpp
assert store_cpp.count("SamplerSceneReadFilter<File>") >= 4
assert store_cpp.count("SamplerSceneWriteFilter<File>") >= 2
assert "manager.loadSceneEvented(filtered)" in store_cpp
assert "manager.writeSceneJson(filtered) && filtered.finish()" in store_cpp

# Sample IDs use all 32 bits. A signed JSON conversion turns IDs above INT_MAX
# negative and makes the stable-ref filter reject the whole Scene save.
assert "auto writeUint32" in scene_h
assert "writeUint32(p.sampleId)" in scene_h
assert "writeInt(p.sampleId)" not in scene_h
assert "samplerEnabled" in scene_h

# Keep the public raw string storage overload raw; applying the filter there as
# well would permit accidental double encoding of a stable ref.
raw_write_start = store_cpp.index("bool SceneStorageCardputer::writeScene(const std::string& data)")
manager_read_start = store_cpp.index("bool SceneStorageCardputer::readScene(SceneManager& manager)")
raw_write_body = store_cpp[raw_write_start:manager_read_start]
assert "SamplerScene" not in raw_write_body

# Runtime IDs are control-side only. Ambiguous historical IDs remain reserved,
# while stable refs can map to distinct compact IDs.
assert "runtimeIdForRef" in index_h
assert "resolveRuntimeId" in index_h
assert "runtimeCandidateReserved" in index_cpp
assert "setScenePersistenceSampleIndex" not in index_cpp
assert "if (file.id.value == candidate) return true;" in index_cpp

# Bounded scratch only: one sampler pad object, not a second Scene buffer.
assert "kMaxPadObjectBytes = 384" in persist_h
assert "kMaxOutputBytes = 448" in persist_h
assert "char padBuffer_[kMaxPadObjectBytes]" in persist_h
assert "char outputBuffer_[kMaxOutputBytes]" in persist_h

# C boot ordering remains the authority and publishes the persistence index
# before the same bind that precedes MiniAcid Scene restore. No PCM moved here.
assert "setScenePersistenceSampleIndex(&index)" in boot
assert "index.bindToStore(g_sampleStore)" in boot
assert "preload(" not in boot

# Production Save must first copy every realtime sampler pad field into the
# resident 32-bit Scene. The streaming writer can only derive SampleRef from
# the runtime SampleId after this mirror has happened.
sync_start = engine_cpp.index("void MiniAcid::syncSceneStateToManager()")
sync_end = engine_cpp.index("int dorian_intervals", sync_start)
sync_body = engine_cpp[sync_start:sync_end]
for field in (
    "sampleId = runtimePad.id.value",
    "volume = runtimePad.volume",
    "pitch = runtimePad.pitch",
    "startFrame = runtimePad.startFrame",
    "endFrame = runtimePad.endFrame",
    "chokeGroup = runtimePad.chokeGroup",
    "reverse = runtimePad.reverse",
    "loop = runtimePad.loop",
):
    assert field in sync_body, f"runtime sampler Save mirror missing: {field}"
assert "samplerEnabled = samplerTrack->isEnabled()" in sync_body
assert "setEnabled(sceneManager_.currentScene().samplerEnabled)" in engine_cpp

save_start = engine_cpp.index("bool MiniAcid::saveSceneToStorage()")
save_end = engine_cpp.index("bool MiniAcid::autoSaveSceneRecovery()", save_start)
save_body = engine_cpp[save_start:save_end]
assert save_body.index("syncSceneStateToManager()") < save_body.index("writeScene(sceneManager_)")

print("sampler persistence source regressions passed")
