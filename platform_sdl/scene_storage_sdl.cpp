#include "scene_storage_sdl.h"

#include <fstream>
#include <iterator>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

EM_JS(int, wasm_read_scene, (char* out, int maxLen), {
  const key = 'miniacid:scene';
  try {
    const data = localStorage.getItem(key);
    if (data === null) return -1;
    const len = lengthBytesUTF8(data) + 1;
    if (!out) return len;
    if (len > maxLen) return -2;
    stringToUTF8(data, out, maxLen);
    return len - 1;
  } catch (e) {
    return -3;
  }
});

EM_JS(int, wasm_write_scene, (const char* data), {
  const key = 'miniacid:scene';
  try {
    const value = UTF8ToString(data);
    localStorage.setItem(key, value);
    return 1;
  } catch (e) {
    return 0;
  }
});
#endif

namespace {
constexpr const char* kSceneFile = "miniacid_scene.json";
}

bool SceneStorageSdl::readScene(std::string& out) {
#ifdef __EMSCRIPTEN__
  int length = wasm_read_scene(nullptr, 0);
  if (length <= 0) return false;
  std::string buffer;
  buffer.resize(static_cast<size_t>(length));
  int written = wasm_read_scene(buffer.data(), length);
  if (written <= 0) return false;
  buffer.resize(static_cast<size_t>(written));
  out = buffer;
  return true;
#else
  std::ifstream file(kSceneFile, std::ios::in);
  if (!file.is_open()) return false;

  out.assign((std::istreambuf_iterator<char>(file)),
             std::istreambuf_iterator<char>());
  return !out.empty();
#endif
}

bool SceneStorageSdl::writeScene(const std::string& data) {
#ifdef __EMSCRIPTEN__
  return wasm_write_scene(data.c_str()) > 0;
#else
  std::ofstream file(kSceneFile, std::ios::out | std::ios::trunc);
  if (!file.is_open()) return false;
  file << data;
  return file.good();
#endif
}
