#ifndef SCENES_H
#define SCENES_H


#include <stdint.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include "ArduinoJson-v7.4.2.h"
#include "src/dsp/mini_dsp_params.h"
#include "src/dsp/genre_catalog.h"
#include "src/phrase/phrase_types.h"
#include "src/phrase/phrase_persistence.h"
#include "json_evented.h"

namespace scene_json_detail {
inline bool writeChunk(std::string& writer, const char* data, size_t len) {
  writer.append(data, len);
  return true;
}

template <typename Writer>
auto writeChunkImpl(Writer& writer, const char* data, size_t len, int)
    -> decltype(writer.write(reinterpret_cast<const uint8_t*>(data), len), bool()) {
  size_t written = writer.write(reinterpret_cast<const uint8_t*>(data), len);
  return written == len;
}

template <typename Writer>
auto writeChunkImpl(Writer& writer, const char* data, size_t len, long)
    -> decltype(writer.write(data, len), bool()) {
  size_t written = writer.write(data, len);
  return written == len;
}

template <typename Writer>
bool writeChunk(Writer& writer, const char* data, size_t len) {
  return writeChunkImpl(writer, data, len, 0);
}
}  // namespace scene_json_detail

// NOTE: rest of file intentionally preserved by this update guard.
