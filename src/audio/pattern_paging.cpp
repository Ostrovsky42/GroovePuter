#include "pattern_paging.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr char kPatternDirectory[] = "/patterns";
constexpr char kMagic[4] = {'G', 'P', 'P', 'G'};
constexpr uint32_t kCrcInitial = 0xFFFFFFFFu;
constexpr uint32_t kCrcPolynomial = 0xEDB88320u;

struct PageFileHeader {
    char magic[4];
    uint16_t version;
    uint16_t headerSize;
    uint32_t payloadSize;
    uint32_t payloadCrc32;
    uint32_t layoutFingerprint;
    uint32_t synthABytes;
    uint32_t synthBBytes;
    uint32_t drumBytes;
};

struct PageStaging {
    Bank<SynthPattern> synthA[kBankCount];
    Bank<SynthPattern> synthB[kBankCount];
    Bank<DrumPatternSet> drums[kBankCount];
};

// A single static staging area avoids heap allocation and guarantees that
// Scene is untouched until the complete file has passed header, size and CRC
// validation. Paging is serialized by the audio mutation gate.
PageStaging g_stagingPage{};

uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            const uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1u) ^ (kCrcPolynomial & mask);
        }
    }
    return crc;
}

uint32_t finalizeCrc(uint32_t crc) {
    return crc ^ 0xFFFFFFFFu;
}

uint32_t payloadSize() {
    return static_cast<uint32_t>(sizeof(g_stagingPage.synthA) +
                                 sizeof(g_stagingPage.synthB) +
                                 sizeof(g_stagingPage.drums));
}

uint32_t layoutFingerprint() {
    // FNV-1a over every ABI-sensitive dimension. A changed struct size, bank
    // count or pattern count invalidates old cache pages instead of reading
    // them into a different firmware layout.
    const uint32_t values[] = {
        static_cast<uint32_t>(sizeof(DrumStep)),
        static_cast<uint32_t>(sizeof(DrumPatternSet)),
        static_cast<uint32_t>(sizeof(SynthStep)),
        static_cast<uint32_t>(sizeof(SynthPattern)),
        static_cast<uint32_t>(sizeof(Bank<DrumPatternSet>)),
        static_cast<uint32_t>(sizeof(Bank<SynthPattern>)),
        static_cast<uint32_t>(kBankCount),
        static_cast<uint32_t>(Bank<SynthPattern>::kPatterns),
        static_cast<uint32_t>(DrumPatternSet::kVoices),
        static_cast<uint32_t>(DrumPattern::kSteps),
        static_cast<uint32_t>(SynthPattern::kSteps),
    };

    uint32_t hash = 2166136261u;
    for (uint32_t value : values) {
        for (int byte = 0; byte < 4; ++byte) {
            hash ^= static_cast<uint8_t>((value >> (byte * 8)) & 0xFFu);
            hash *= 16777619u;
        }
    }
    return hash;
}

PageFileHeader makeHeader(const Scene& scene) {
    uint32_t crc = kCrcInitial;
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(scene.synthABanks),
        sizeof(scene.synthABanks));
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(scene.synthBBanks),
        sizeof(scene.synthBBanks));
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(scene.drumBanks),
        sizeof(scene.drumBanks));

    PageFileHeader header{};
    std::memcpy(header.magic, kMagic, sizeof(kMagic));
    header.version = PatternPagingService::kFormatVersion;
    header.headerSize = static_cast<uint16_t>(sizeof(PageFileHeader));
    header.payloadSize = payloadSize();
    header.payloadCrc32 = finalizeCrc(crc);
    header.layoutFingerprint = layoutFingerprint();
    header.synthABytes = sizeof(scene.synthABanks);
    header.synthBBytes = sizeof(scene.synthBBanks);
    header.drumBytes = sizeof(scene.drumBanks);
    return header;
}

bool headerIsValid(const PageFileHeader& header, size_t fileSize) {
    return std::memcmp(header.magic, kMagic, sizeof(kMagic)) == 0 &&
           header.version == PatternPagingService::kFormatVersion &&
           header.headerSize == sizeof(PageFileHeader) &&
           header.payloadSize == payloadSize() &&
           header.layoutFingerprint == layoutFingerprint() &&
           header.synthABytes == sizeof(g_stagingPage.synthA) &&
           header.synthBBytes == sizeof(g_stagingPage.synthB) &&
           header.drumBytes == sizeof(g_stagingPage.drums) &&
           fileSize == sizeof(PageFileHeader) + header.payloadSize;
}

bool writeAll(File& file, const void* data, size_t length) {
    return file.write(reinterpret_cast<const uint8_t*>(data), length) == length;
}

bool readAll(File& file, void* data, size_t length) {
    return file.read(reinterpret_cast<uint8_t*>(data), length) == length;
}

bool readAndValidatePage(const std::string& path, PageStaging& staging) {
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) return false;

    PageFileHeader header{};
    if (!readAll(file, &header, sizeof(header)) ||
        !headerIsValid(header, file.size())) {
        file.close();
        return false;
    }

    if (!readAll(file, staging.synthA, sizeof(staging.synthA)) ||
        !readAll(file, staging.synthB, sizeof(staging.synthB)) ||
        !readAll(file, staging.drums, sizeof(staging.drums))) {
        file.close();
        return false;
    }
    file.close();

    uint32_t crc = kCrcInitial;
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(staging.synthA),
        sizeof(staging.synthA));
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(staging.synthB),
        sizeof(staging.synthB));
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(staging.drums),
        sizeof(staging.drums));
    return finalizeCrc(crc) == header.payloadCrc32;
}

bool commitTemporaryPage(const std::string& mainPath,
                         const std::string& temporaryPath,
                         const std::string& oldBackupPath) {
    SD.remove(oldBackupPath.c_str());
    const bool hadMain = SD.exists(mainPath.c_str());

    if (hadMain && !SD.rename(mainPath.c_str(), oldBackupPath.c_str())) {
        SD.remove(temporaryPath.c_str());
        return false;
    }

    if (!SD.rename(temporaryPath.c_str(), mainPath.c_str())) {
        if (hadMain) SD.rename(oldBackupPath.c_str(), mainPath.c_str());
        SD.remove(temporaryPath.c_str());
        return false;
    }
    return true;
}

}  // namespace

bool PatternPagingService::validPageIndex(int pageIndex) {
    return pageIndex >= 0 && pageIndex < kMaxPages;
}

bool PatternPagingService::ensureDirectory() {
    return SD.exists(kPatternDirectory) || SD.mkdir(kPatternDirectory);
}

std::string PatternPagingService::pagePath(int pageIndex) {
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%s/page_%02d.gpp",
                  kPatternDirectory, pageIndex);
    return std::string(buffer);
}

std::string PatternPagingService::tempPath(int pageIndex) {
    return pagePath(pageIndex) + ".tmp";
}

std::string PatternPagingService::backupPath(int pageIndex) {
    return pagePath(pageIndex) + ".bak";
}

bool PatternPagingService::savePage(int pageIndex, const Scene& scene) {
    if (!validPageIndex(pageIndex) || !ensureDirectory()) return false;

    const std::string mainPath = pagePath(pageIndex);
    const std::string temporaryPath = tempPath(pageIndex);
    const std::string oldBackupPath = backupPath(pageIndex);
    SD.remove(temporaryPath.c_str());

    const PageFileHeader header = makeHeader(scene);
    File file = SD.open(temporaryPath.c_str(), FILE_WRITE);
    if (!file) return false;

    const bool wrote =
        writeAll(file, &header, sizeof(header)) &&
        writeAll(file, scene.synthABanks, sizeof(scene.synthABanks)) &&
        writeAll(file, scene.synthBBanks, sizeof(scene.synthBBanks)) &&
        writeAll(file, scene.drumBanks, sizeof(scene.drumBanks));
    file.flush();
    file.close();

    if (!wrote || !readAndValidatePage(temporaryPath, g_stagingPage)) {
        SD.remove(temporaryPath.c_str());
        return false;
    }

    return commitTemporaryPage(mainPath, temporaryPath, oldBackupPath);
}

bool PatternPagingService::loadPage(int pageIndex, Scene& scene) {
    if (!validPageIndex(pageIndex)) return false;

    const std::string mainPath = pagePath(pageIndex);
    const std::string oldBackupPath = backupPath(pageIndex);

    bool loaded = readAndValidatePage(mainPath, g_stagingPage);
    if (!loaded) {
        loaded = readAndValidatePage(oldBackupPath, g_stagingPage);
    }
    if (!loaded) return false;

    // This is the only point where active pattern state is modified.
    std::memcpy(scene.synthABanks, g_stagingPage.synthA,
                sizeof(scene.synthABanks));
    std::memcpy(scene.synthBBanks, g_stagingPage.synthB,
                sizeof(scene.synthBBanks));
    std::memcpy(scene.drumBanks, g_stagingPage.drums,
                sizeof(scene.drumBanks));
    return true;
}

void PatternPagingService::initializeEmptyPage(Scene& scene) {
    for (int bank = 0; bank < kBankCount; ++bank) {
        scene.synthABanks[bank] = Bank<SynthPattern>{};
        scene.synthBBanks[bank] = Bank<SynthPattern>{};
        scene.drumBanks[bank] = Bank<DrumPatternSet>{};
    }
}

bool PatternPagingService::pageExists(int pageIndex) {
    if (!validPageIndex(pageIndex)) return false;
    return SD.exists(pagePath(pageIndex).c_str()) ||
           SD.exists(backupPath(pageIndex).c_str());
}

bool PatternPagingService::removePage(int pageIndex) {
    if (!validPageIndex(pageIndex)) return false;
    const std::string mainPath = pagePath(pageIndex);
    const std::string temporaryPath = tempPath(pageIndex);
    const std::string oldBackupPath = backupPath(pageIndex);

    bool removedAny = false;
    if (SD.exists(mainPath.c_str())) {
        removedAny = SD.remove(mainPath.c_str()) || removedAny;
    }
    if (SD.exists(temporaryPath.c_str())) {
        removedAny = SD.remove(temporaryPath.c_str()) || removedAny;
    }
    if (SD.exists(oldBackupPath.c_str())) {
        removedAny = SD.remove(oldBackupPath.c_str()) || removedAny;
    }
    return removedAny;
}
