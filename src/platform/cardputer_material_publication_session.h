#pragma once
#ifndef GROOVEPUTER_SRC_PLATFORM_CARDPUTER_MATERIAL_PUBLICATION_SESSION_H
#define GROOVEPUTER_SRC_PLATFORM_CARDPUTER_MATERIAL_PUBLICATION_SESSION_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include "src/state/material_publication_record.h"

#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
#include <Preferences.h>
#endif

namespace GroovePuterPlatform {
namespace detail {

constexpr const char* kPublicationNamespace = "gp-matpub";

inline uint32_t hashProjectName(const std::string& name) {
    uint32_t hash = 2166136261u;
    for (char c : name) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

inline std::string formatKey(const std::string& projectName, int pageIndex) {
    char key[16];
    std::snprintf(key, sizeof(key), "%08x_p%d", hashProjectName(projectName), pageIndex);
    return std::string(key);
}

#if !(defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM)))
inline std::map<std::string, GroovePuterMaterial::MaterialPublicationRecord>& mockStore() {
    static std::map<std::string, GroovePuterMaterial::MaterialPublicationRecord> store;
    return store;
}
#endif

}  // namespace detail

inline bool loadMaterialPublication(const std::string& projectName,
                                    int pageIndex,
                                    GroovePuterMaterial::MaterialPublicationRecord& out) {
#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
    Preferences preferences;
    if (!preferences.begin(detail::kPublicationNamespace, true)) return false;

    const std::string key = detail::formatKey(projectName, pageIndex);
    GroovePuterMaterial::MaterialPublicationRecord record{};
    const size_t storedSize = preferences.getBytesLength(key.c_str());
    const size_t read = storedSize == sizeof(record)
        ? preferences.getBytes(key.c_str(), &record, sizeof(record))
        : 0;
    preferences.end();

    if (read != sizeof(record) || !GroovePuterMaterial::validPublicationRecord(record, pageIndex)) {
        return false;
    }
    out = record;
    return true;
#else
    const std::string key = detail::formatKey(projectName, pageIndex);
    auto& store = detail::mockStore();
    auto it = store.find(key);
    if (it == store.end()) return false;
    if (!GroovePuterMaterial::validPublicationRecord(it->second, pageIndex)) return false;
    out = it->second;
    return true;
#endif
}

inline bool saveMaterialPublication(const std::string& projectName,
                                    int pageIndex,
                                    const GroovePuterMaterial::MaterialPublicationRecord& record) {
    if (!GroovePuterMaterial::validPublicationRecord(record, pageIndex)) return false;

#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
    Preferences preferences;
    if (!preferences.begin(detail::kPublicationNamespace, false)) return false;

    const std::string key = detail::formatKey(projectName, pageIndex);
    const size_t written = preferences.putBytes(key.c_str(), &record, sizeof(record));
    preferences.end();
    return written == sizeof(record);
#else
    const std::string key = detail::formatKey(projectName, pageIndex);
    detail::mockStore()[key] = record;
    return true;
#endif
}

inline bool clearMaterialPublication(const std::string& projectName, int pageIndex) {
#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
    Preferences preferences;
    if (!preferences.begin(detail::kPublicationNamespace, false)) return false;

    const std::string key = detail::formatKey(projectName, pageIndex);
    const bool removed = preferences.remove(key.c_str());
    preferences.end();
    return removed;
#else
    const std::string key = detail::formatKey(projectName, pageIndex);
    return detail::mockStore().erase(key) > 0;
#endif
}

inline void resetMaterialPublicationMock() {
#if !(defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM)))
    detail::mockStore().clear();
#endif
}

}  // namespace GroovePuterPlatform

#endif  // GROOVEPUTER_SRC_PLATFORM_CARDPUTER_MATERIAL_PUBLICATION_SESSION_H
