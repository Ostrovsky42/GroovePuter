#pragma once
#ifndef GROOVEPUTER_SMF_DIRECTORY_CACHE_H
#define GROOVEPUTER_SMF_DIRECTORY_CACHE_H

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

namespace GroovePuterUi {

struct SmfDirectoryEntry {
    static constexpr std::size_t kNameCapacity = 48;

    char name[kNameCapacity]{};
    bool directory{false};
};

template <std::size_t Capacity>
struct SmfDirectorySnapshot {
    static constexpr std::size_t kPathCapacity = 128;

    char path[kPathCapacity]{"/midi"};
    std::array<SmfDirectoryEntry, Capacity> entries{};
    uint16_t count{0};
    uint16_t totalCount{0};
    uint32_t generation{0};
    bool scanning{false};
    bool storageReady{false};
    bool truncated{false};
};

template <std::size_t Capacity = 96>
class SmfDirectoryCache {
public:
    using Snapshot = SmfDirectorySnapshot<Capacity>;

    Snapshot snapshot() const {
        Snapshot result{};
        for (int attempt = 0; attempt < 4; ++attempt) {
            const uint32_t first = version_.load(std::memory_order_acquire);
            if ((first & 1u) != 0u) continue;
            result = state_;
            const uint32_t second = version_.load(std::memory_order_acquire);
            if (first == second && (second & 1u) == 0u) return result;
        }
        return Snapshot{};
    }

    uint32_t beginScan(const char* path) {
        Snapshot next = snapshot();
        ++next.generation;
        next.count = 0;
        next.totalCount = 0;
        next.scanning = true;
        next.storageReady = false;
        next.truncated = false;
        copyPath(next.path, path);
        publish(next);
        return next.generation;
    }

    bool publishResult(uint32_t generation,
                       const char* path,
                       const SmfDirectoryEntry* entries,
                       uint16_t count,
                       uint16_t totalCount,
                       bool storageReady) {
        Snapshot current = snapshot();
        if (current.generation != generation || !current.scanning) return false;

        Snapshot next{};
        next.generation = generation;
        next.totalCount = totalCount;
        next.storageReady = storageReady;
        next.scanning = false;
        next.truncated = count > Capacity || totalCount > Capacity;
        next.count = count > Capacity ? static_cast<uint16_t>(Capacity) : count;
        copyPath(next.path, path);
        for (uint16_t i = 0; i < next.count; ++i) {
            next.entries[i] = entries[i];
            next.entries[i].name[SmfDirectoryEntry::kNameCapacity - 1u] = '\0';
        }
        publish(next);
        return true;
    }

    bool cancel(uint32_t generation) {
        Snapshot next = snapshot();
        if (next.generation != generation || !next.scanning) return false;
        next.scanning = false;
        publish(next);
        return true;
    }

private:
    static void copyPath(char* destination, const char* source) {
        const char* safe = source != nullptr && source[0] == '/' ? source : "/midi";
        std::strncpy(destination, safe, Snapshot::kPathCapacity - 1u);
        destination[Snapshot::kPathCapacity - 1u] = '\0';
    }

    void publish(const Snapshot& next) {
        const uint32_t version = version_.load(std::memory_order_relaxed);
        version_.store(version + 1u, std::memory_order_release);
        state_ = next;
        version_.store(version + 2u, std::memory_order_release);
    }

    std::atomic<uint32_t> version_{0};
    Snapshot state_{};
};

inline SmfDirectoryCache<>& smfDirectoryCache() {
    static SmfDirectoryCache<> cache;
    return cache;
}

}  // namespace GroovePuterUi

#endif  // GROOVEPUTER_SMF_DIRECTORY_CACHE_H
