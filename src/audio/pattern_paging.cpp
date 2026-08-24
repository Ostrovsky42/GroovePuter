#include "pattern_paging.h"
#include "src/state/undo_owner.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr char kPatternRootDirectory[] = "/patterns";
constexpr char kDefaultProjectName[] = "grooveputer_scene";
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

constexpr size_t kSynthBanksSize = sizeof(Bank<SynthPattern>) * kBankCount;
constexpr size_t kDrumBanksSize = sizeof(Bank<DrumPatternSet>) * kBankCount;

std::string& activeProjectNameStorage() {
    static std::string projectName = kDefaultProjectName;
    return projectName;
}

int& activePageIndexStorage() {
    static int pageIndex = 0;
    return pageIndex;
}

std::string normalizeProjectName(const std::string& projectName) {
    return projectName.empty() ? std::string(kDefaultProjectName) : projectName;
}

std::string encodeProjectName(const std::string& projectName) {
    const std::string normalized = normalizeProjectName(projectName);
    std::string encoded;
    encoded.reserve(normalized.size());
    static constexpr char kHex[] = "0123456789ABCDEF";
    for (unsigned char ch : normalized) {
        if (std::isalnum(ch) || ch == '-') {
            encoded.push_back(static_cast<char>(ch));
        } else {
            encoded.push_back('_');
            encoded.push_back(kHex[(ch >> 4) & 0x0F]);
            encoded.push_back(kHex[ch & 0x0F]);
        }
    }
    return encoded.empty() ? std::string(kDefaultProjectName) : encoded;
}

std::string projectDirectoryFor(const std::string& projectName) {
    return std::string(kPatternRootDirectory) + "/" +
           encodeProjectName(projectName);
}

std::string pagePathFor(const std::string& projectName, int pageIndex) {
    char fileName[32];
    std::snprintf(fileName, sizeof(fileName), "/page_%02d.gpp", pageIndex);
    return projectDirectoryFor(projectName) + fileName;
}

std::string legacyPagePath(int pageIndex) {
    char buffer[48];
    std::snprintf(buffer, sizeof(buffer), "%s/page_%02d.gpp",
                  kPatternRootDirectory, pageIndex);
    return std::string(buffer);
}

bool ensureRootDirectory() {
    return SD.exists(kPatternRootDirectory) || SD.mkdir(kPatternRootDirectory);
}

bool ensureProjectDirectory(const std::string& projectName) {
    if (!ensureRootDirectory()) return false;
    const std::string directory = projectDirectoryFor(projectName);
    return SD.exists(directory.c_str()) || SD.mkdir(directory.c_str());
}

bool removeIfExists(const std::string& path) {
    return !SD.exists(path.c_str()) || SD.remove(path.c_str());
}

bool copyFile(const std::string& sourcePath, const std::string& targetPath) {
    File source = SD.open(sourcePath.c_str(), FILE_READ);
    if (!source) return false;

    if (!removeIfExists(targetPath)) {
        source.close();
        return false;
    }
    File target = SD.open(targetPath.c_str(), FILE_WRITE);
    if (!target) {
        source.close();
        return false;
    }

    uint8_t buffer[512];
    bool ok = true;
    while (source.available()) {
        const size_t readCount = source.read(buffer, sizeof(buffer));
        if (readCount == 0) {
            ok = false;
            break;
        }
        if (target.write(buffer, readCount) != readCount) {
            ok = false;
            break;
        }
    }
    target.flush();
    source.close();
    target.close();

    if (!ok) removeIfExists(targetPath);
    return ok;
}

bool clearProjectPagesFor(const std::string& projectName) {
    bool ok = true;
    for (int page = 0; page < kMaxPages; ++page) {
        const std::string mainPath = pagePathFor(projectName, page);
        ok = removeIfExists(mainPath) && ok;
        ok = removeIfExists(mainPath + ".tmp") && ok;
        ok = removeIfExists(mainPath + ".bak") && ok;
    }
    return ok;
}

bool projectHasAnyPage(const std::string& projectName) {
    for (int page = 0; page < kMaxPages; ++page) {
        const std::string mainPath = pagePathFor(projectName, page);
        if (SD.exists(mainPath.c_str()) ||
            SD.exists((mainPath + ".bak").c_str())) {
            return true;
        }
    }
    return false;
}

bool migrateLegacyPages(const std::string& targetProject) {
    if (projectHasAnyPage(targetProject)) return true;

    bool hasLegacy = false;
    for (int page = 0; page < kMaxPages; ++page) {
        const std::string legacyMain = legacyPagePath(page);
        if (SD.exists(legacyMain.c_str()) ||
            SD.exists((legacyMain + ".bak").c_str()) ||
            SD.exists((legacyMain + ".tmp").c_str())) {
            hasLegacy = true;
            break;
        }
    }
    if (!hasLegacy) return true;
    if (!ensureProjectDirectory(targetProject)) return false;

    for (int page = 0; page < kMaxPages; ++page) {
        const std::string legacyMain = legacyPagePath(page);
        const std::string targetMain = pagePathFor(targetProject, page);
        if (SD.exists(legacyMain.c_str()) &&
            !copyFile(legacyMain, targetMain)) {
            clearProjectPagesFor(targetProject);
            return false;
        }
        if (SD.exists((legacyMain + ".bak").c_str()) &&
            !copyFile(legacyMain + ".bak", targetMain + ".bak")) {
            clearProjectPagesFor(targetProject);
            return false;
        }
    }

    bool removed = true;
    for (int page = 0; page < kMaxPages; ++page) {
        const std::string legacyMain = legacyPagePath(page);
        removed = removeIfExists(legacyMain) && removed;
        removed = removeIfExists(legacyMain + ".tmp") && removed;
        removed = removeIfExists(legacyMain + ".bak") && removed;
    }
    return removed;
}

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
    return static_cast<uint32_t>(kSynthBanksSize * 2u + kDrumBanksSize);
}

uint32_t layoutFingerprint() {
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
           header.synthABytes == kSynthBanksSize &&
           header.synthBBytes == kSynthBanksSize &&
           header.drumBytes == kDrumBanksSize &&
           fileSize == sizeof(PageFileHeader) + header.payloadSize;
}

bool writeAll(File& file, const void* data, size_t length) {
    return file.write(reinterpret_cast<const uint8_t*>(data), length) == length;
}

bool readAll(File& file, void* data, size_t length) {
    return file.read(reinterpret_cast<uint8_t*>(data), length) == length;
}

bool readAndValidatePage(const std::string& path, Scene& staging) {
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) return false;

    PageFileHeader header{};
    if (!readAll(file, &header, sizeof(header)) ||
        !headerIsValid(header, file.size())) {
        file.close();
        return false;
    }

    if (!readAll(file, staging.synthABanks, sizeof(staging.synthABanks)) ||
        !readAll(file, staging.synthBBanks, sizeof(staging.synthBBanks)) ||
        !readAll(file, staging.drumBanks, sizeof(staging.drumBanks))) {
        file.close();
        return false;
    }
    file.close();

    uint32_t crc = kCrcInitial;
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(staging.synthABanks),
        sizeof(staging.synthABanks));
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(staging.synthBBanks),
        sizeof(staging.synthBBanks));
    crc = crc32Update(crc,
        reinterpret_cast<const uint8_t*>(staging.drumBanks),
        sizeof(staging.drumBanks));
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

bool PatternPagingService::setProjectName(const std::string& projectName) {
    const std::string normalized = normalizeProjectName(projectName);
    std::string& active = activeProjectNameStorage();
    const std::string previous = active;
    active = normalized;
    if (!ensureDirectory() || !migrateLegacyPages(active)) {
        active = previous;
        ensureDirectory();
        return false;
    }
    return true;
}

const std::string& PatternPagingService::currentProjectName() {
    return activeProjectNameStorage();
}

int PatternPagingService::activePageIndex() {
    return activePageIndexStorage();
}

bool PatternPagingService::ensureDirectory() {
    return ensureProjectDirectory(activeProjectNameStorage());
}

std::string PatternPagingService::projectDirectory(
    const std::string& projectName) {
    return projectDirectoryFor(projectName);
}

std::string PatternPagingService::pagePathForProject(
    const std::string& projectName, int pageIndex) {
    return pagePathFor(projectName, pageIndex);
}

std::string PatternPagingService::pagePath(int pageIndex) {
    return pagePathFor(activeProjectNameStorage(), pageIndex);
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

    const Scene* persistentScene = &scene;
    Scene& staging = sceneTransactionScratch();
    if (GroovePuterUndo::undoOwner().hasLifecycle()) {
        std::memcpy(staging.synthABanks, scene.synthABanks,
                    sizeof(scene.synthABanks));
        std::memcpy(staging.synthBBanks, scene.synthBBanks,
                    sizeof(scene.synthBBanks));
        std::memcpy(staging.drumBanks, scene.drumBanks,
                    sizeof(scene.drumBanks));
        GroovePuterUndo::undoOwner().sanitizeForPersistence(&staging);
        persistentScene = &staging;
    }

    const PageFileHeader header = makeHeader(*persistentScene);
    File file = SD.open(temporaryPath.c_str(), FILE_WRITE);
    if (!file) return false;

    const bool wrote =
        writeAll(file, &header, sizeof(header)) &&
        writeAll(file, persistentScene->synthABanks,
                 sizeof(persistentScene->synthABanks)) &&
        writeAll(file, persistentScene->synthBBanks,
                 sizeof(persistentScene->synthBBanks)) &&
        writeAll(file, persistentScene->drumBanks,
                 sizeof(persistentScene->drumBanks));
    file.flush();
    file.close();

    if (!wrote || !readAndValidatePage(temporaryPath, staging)) {
        SD.remove(temporaryPath.c_str());
        return false;
    }

    if (!commitTemporaryPage(mainPath, temporaryPath, oldBackupPath)) {
        return false;
    }
    activePageIndexStorage() = pageIndex;
    return true;
}

bool PatternPagingService::loadPage(int pageIndex, Scene& scene) {
    if (!validPageIndex(pageIndex)) return false;

    const std::string mainPath = pagePath(pageIndex);
    const std::string oldBackupPath = backupPath(pageIndex);

    Scene& staging = sceneTransactionScratch();
    bool loaded = readAndValidatePage(mainPath, staging);
    if (!loaded) loaded = readAndValidatePage(oldBackupPath, staging);
    if (!loaded) return false;

    // Loading replaces physical Pattern storage. Any retained redo backing is
    // page-local runtime ownership and must expire only after load validation.
    GroovePuterUndo::undoOwner().clear();
    std::memcpy(scene.synthABanks, staging.synthABanks,
                sizeof(scene.synthABanks));
    std::memcpy(scene.synthBBanks, staging.synthBBanks,
                sizeof(scene.synthBBanks));
    std::memcpy(scene.drumBanks, staging.drumBanks,
                sizeof(scene.drumBanks));
    activePageIndexStorage() = pageIndex;
    return true;
}

bool PatternPagingService::restoreBackup(int pageIndex) {
    if (!validPageIndex(pageIndex)) return false;
    const std::string mainPath = pagePath(pageIndex);
    const std::string oldBackupPath = backupPath(pageIndex);
    Scene& staging = sceneTransactionScratch();
    if (!readAndValidatePage(oldBackupPath, staging)) return false;
    if (!removeIfExists(mainPath)) return false;
    return SD.rename(oldBackupPath.c_str(), mainPath.c_str());
}

void PatternPagingService::initializeEmptyPage(Scene& scene) {
    GroovePuterUndo::undoOwner().clear();
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

bool PatternPagingService::copyProjectPages(
    const std::string& sourceProject,
    const std::string& targetProject) {
    const std::string source = normalizeProjectName(sourceProject);
    const std::string target = normalizeProjectName(targetProject);
    if (source == target) return true;
    if (!ensureProjectDirectory(source) || !ensureProjectDirectory(target)) {
        return false;
    }
    if (!clearProjectPagesFor(target)) return false;

    for (int page = 0; page < kMaxPages; ++page) {
        const std::string sourceMain = pagePathFor(source, page);
        const std::string targetMain = pagePathFor(target, page);
        if (SD.exists(sourceMain.c_str()) &&
            !copyFile(sourceMain, targetMain)) {
            clearProjectPagesFor(target);
            return false;
        }
        if (SD.exists((sourceMain + ".bak").c_str()) &&
            !copyFile(sourceMain + ".bak", targetMain + ".bak")) {
            clearProjectPagesFor(target);
            return false;
        }
    }
    return true;
}

bool PatternPagingService::clearProjectPages() {
    if (!ensureDirectory()) return false;
    const bool cleared = clearProjectPagesFor(activeProjectNameStorage());
    if (cleared) activePageIndexStorage() = 0;
    return cleared;
}
