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

# D must not expand resident Scene sampler state or the audio/runtime ABI.
assert "uint32_t sampleId = 0;" in scene_h
assert "SamplerPadState samplerPads[16];" in scene_h
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

print("sampler persistence source regressions passed")
