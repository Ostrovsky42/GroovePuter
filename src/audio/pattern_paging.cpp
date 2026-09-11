#include "pattern_paging.h"

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
constexpr char kMaterialIdentityMagic[4] = {'G', 'P', 'M', 'I'};
constexpr uint16_t kMaterialIdentityMetaVersion = 1;
constexpr uint32_t kCrcInitial = 0xFFFFFFFFu;
constexpr uint32_t kCrcPolynomial = 0xEDB88320u;

// Version 3 is the shared prefix. Version 4 appended materialKindBytes, and
// version 5 appends materialIdBytes. Existing fields are never reordered so a
// reader can inspect the version before consuming the rest of the header.
struct PageFileHeaderV3 {
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

struct PageFileHeaderV4 : PageFileHeaderV3 {
    uint32_t materialKindBytes;
};

struct PageFileHeader : PageFileHeaderV4 {
    uint32_t materialIdBytes;
};

struct MaterialIdentityMeta {
    char magic[4];
    uint16_t version;
    uint16_t size;
    uint32_t highWater;
    uint32_t crc32;
};

constexpr size_t kSynthBanksSize = sizeof(Bank<SynthPattern>) * kBankCount;
constexpr size_t kDrumBanksSize = sizeof(Bank<DrumPatternSet>) * kBankCount;
constexpr size_t kMaterialSlotCount =
    Scene::kMaterialVoices * Scene::kMaterialSlotsPerVoice;
constexpr size_t kMaterialKindsSize = sizeof(uint8_t) * kMaterialSlotCount;
constexpr size_t kMaterialIdsSize =
    sizeof(GroovePuterMaterial::MaterialId) * kMaterialSlotCount;

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
        // '_' is the escape prefix and must itself be encoded. This keeps
        // "a b" (_20) distinct from a literal "a_20b" project name.
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

std::string identityMetaPathFor(const std::string& projectName) {
    return projectDirectoryFor(projectName) + "/material_id.meta";
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
    // Material identity high-water is intentionally not removed here. Address
    // reuse after Clear must still receive a fresh identity in the same project.
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

bool writeAll(File& file, const void* data, size_t length) {
    return file.write(reinterpret_cast<const uint8_t*>(data), length) == length;
}

bool readAll(File& file, void* data, size_t length) {
    return file.read(reinterpret_cast<uint8_t*>(data), length) == length;
}

MaterialIdentityMeta makeIdentityMeta(uint32_t highWater) {
    MaterialIdentityMeta meta{};
    std::memcpy(meta.magic, kMaterialIdentityMagic, sizeof(meta.magic));
    meta.version = kMaterialIdentityMetaVersion;
    meta.size = static_cast<uint16_t>(sizeof(MaterialIdentityMeta));
    meta.highWater = highWater;
    uint32_t crc = kCrcInitial;
    crc = crc32Update(crc, reinterpret_cast<const uint8_t*>(&meta),
                      offsetof(MaterialIdentityMeta, crc32));
    meta.crc32 = finalizeCrc(crc);
    return meta;
}

bool readIdentityMeta(const std::string& path, uint32_t& highWater) {
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) return false;
    if (file.size() != sizeof(MaterialIdentityMeta)) {
        file.close();
        return false;
    }
    MaterialIdentityMeta meta{};
    const bool read = readAll(file, &meta, sizeof(meta));
    file.close();
    if (!read ||
        std::memcmp(meta.magic, kMaterialIdentityMagic, sizeof(meta.magic)) != 0 ||
        meta.version != kMaterialIdentityMetaVersion ||
        meta.size != sizeof(MaterialIdentityMeta)) {
        return false;
    }
    uint32_t crc = kCrcInitial;
    crc = crc32Update(crc, reinterpret_cast<const uint8_t*>(&meta),
                      offsetof(MaterialIdentityMeta, crc32));
    if (finalizeCrc(crc) != meta.crc32) return false;
    highWater = meta.highWater;
    return true;
}

bool loadIdentityHighWater(const std::string& projectName, uint32_t& highWater) {
    const std::string mainPath = identityMetaPathFor(projectName);
    const std::string backupPath = mainPath + ".bak";
    const std::string temporaryPath = mainPath + ".tmp";
    const std::string paths[] = {mainPath, backupPath, temporaryPath};

    bool sawAny = false;
    bool sawValid = false;
    uint32_t highest = 0;
    for (const std::string& path : paths) {
        if (!SD.exists(path.c_str())) continue;
        sawAny = true;
        uint32_t candidate = 0;
        if (!readIdentityMeta(path, candidate)) continue;
        sawValid = true;
        if (candidate > highest) highest = candidate;
    }
    if (sawAny && !sawValid) return false;
    highWater = highest;
    return true;
}

bool writeIdentityHighWater(const std::string& projectName, uint32_t highWater) {
    if (!ensureProjectDirectory(projectName)) return false;
    const std::string mainPath = identityMetaPathFor(projectName);
    const std::string temporaryPath = mainPath + ".tmp";
    const std::string backupPath = mainPath + ".bak";

    if (!removeIfExists(temporaryPath)) return false;
    const MaterialIdentityMeta meta = makeIdentityMeta(highWater);
    File file = SD.open(temporaryPath.c_str(), FILE_WRITE);
    if (!file) return false;
    const bool wrote = writeAll(file, &meta, sizeof(meta));
    file.flush();
    file.close();

    uint32_t verified = 0;
    if (!wrote || !readIdentityMeta(temporaryPath, verified) ||
        verified != highWater) {
        removeIfExists(temporaryPath);
        return false;
    }

    removeIfExists(backupPath);
    const bool hadMain = SD.exists(mainPath.c_str());
    if (hadMain && !SD.rename(mainPath.c_str(), backupPath.c_str())) {
        removeIfExists(temporaryPath);
        return false;
    }
    if (!SD.rename(temporaryPath.c_str(), mainPath.c_str())) {
        if (hadMain) SD.rename(backupPath.c_str(), mainPath.c_str());
        removeIfExists(temporaryPath);
        return false;
    }
    return true;
}

void collectMaterialKinds(const Scene& scene, uint8_t* kinds) {
    size_t index = 0;
    for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
        for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
            kinds[index++] = static_cast<uint8_t>(
                scene.materialSlots[voice][slot].kind);
        }
    }
}

void collectMaterialIds(
    const Scene& scene, GroovePuterMaterial::MaterialId* ids) {
    size_t index = 0;
    for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
        for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
            ids[index++] = scene.materialSlots[voice][slot].id;
        }
    }
}

void resetMaterialMetadata(Scene& scene) {
    for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
        for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
            scene.materialSlots[voice][slot] =
                GroovePuterMaterial::MaterialSlotDescriptor{};
        }
    }
}

uint32_t legacyPayloadSize() {
    return static_cast<uint32_t>(kSynthBanksSize * 2u + kDrumBanksSize);
}

uint32_t kindOnlyPayloadSize() {
    return legacyPayloadSize() + static_cast<uint32_t>(kMaterialKindsSize);
}

uint32_t payloadSize() {
    return kindOnlyPayloadSize() + static_cast<uint32_t>(kMaterialIdsSize);
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
    uint8_t kinds[kMaterialSlotCount]{};
    GroovePuterMaterial::MaterialId ids[kMaterialSlotCount]{};
    collectMaterialKinds(scene, kinds);
    collectMaterialIds(scene, ids);

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
    crc = crc32Update(crc, kinds, sizeof(kinds));
    crc = crc32Update(crc, reinterpret_cast<const uint8_t*>(ids), sizeof(ids));

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
    header.materialKindBytes = static_cast<uint32_t>(kMaterialKindsSize);
    header.materialIdBytes = static_cast<uint32_t>(kMaterialIdsSize);
    return header;
}

bool headerCommonIsValid(const PageFileHeaderV3& header) {
    return std::memcmp(header.magic, kMagic, sizeof(kMagic)) == 0 &&
           header.layoutFingerprint == layoutFingerprint() &&
           header.synthABytes == kSynthBanksSize &&
           header.synthBBytes == kSynthBanksSize &&
           header.drumBytes == kDrumBanksSize;
}

bool headerIsValid(const PageFileHeader& header, size_t fileSize) {
    if (!headerCommonIsValid(header)) return false;
    if (header.version == PatternPagingService::kFormatVersion) {
        return header.headerSize == sizeof(PageFileHeader) &&
               header.payloadSize == payloadSize() &&
               header.materialKindBytes == kMaterialKindsSize &&
               header.materialIdBytes == kMaterialIdsSize &&
               fileSize == sizeof(PageFileHeader) + header.payloadSize;
    }
    if (header.version == PatternPagingService::kKindOnlyFormatVersion) {
        return header.headerSize == sizeof(PageFileHeaderV4) &&
               header.payloadSize == kindOnlyPayloadSize() &&
               header.materialKindBytes == kMaterialKindsSize &&
               fileSize == sizeof(PageFileHeaderV4) + header.payloadSize;
    }
    if (header.version == PatternPagingService::kLegacyFormatVersion) {
        return header.headerSize == sizeof(PageFileHeaderV3) &&
               header.payloadSize == legacyPayloadSize() &&
               fileSize == sizeof(PageFileHeaderV3) + header.payloadSize;
    }
    return false;
}

bool readAndValidatePage(const std::string& path, Scene& staging) {
    File file = SD.open(path.c_str(), FILE_READ);
    if (!file) return false;

    PageFileHeader header{};
    if (!readAll(file, static_cast<PageFileHeaderV3*>(&header),
                 sizeof(PageFileHeaderV3))) {
        file.close();
        return false;
    }

    const bool hasMaterialKinds =
        header.version == PatternPagingService::kKindOnlyFormatVersion ||
        header.version == PatternPagingService::kFormatVersion;
    const bool hasMaterialIds =
        header.version == PatternPagingService::kFormatVersion;
    if (hasMaterialKinds &&
        !readAll(file, &header.materialKindBytes,
                 sizeof(header.materialKindBytes))) {
        file.close();
        return false;
    }
    if (hasMaterialIds &&
        !readAll(file, &header.materialIdBytes,
                 sizeof(header.materialIdBytes))) {
        file.close();
        return false;
    }
    if (!headerIsValid(header, file.size())) {
        file.close();
        return false;
    }

    if (!readAll(file, staging.synthABanks, sizeof(staging.synthABanks)) ||
        !readAll(file, staging.synthBBanks, sizeof(staging.synthBBanks)) ||
        !readAll(file, staging.drumBanks, sizeof(staging.drumBanks))) {
        file.close();
        return false;
    }

    resetMaterialMetadata(staging);
    uint8_t kinds[kMaterialSlotCount]{};
    GroovePuterMaterial::MaterialId ids[kMaterialSlotCount]{};
    if (hasMaterialKinds) {
        if (!readAll(file, kinds, sizeof(kinds))) {
            file.close();
            return false;
        }
        for (size_t index = 0; index < kMaterialSlotCount; ++index) {
            if (!GroovePuterMaterial::validKindValue(kinds[index])) {
                file.close();
                return false;
            }
            const int voice =
                static_cast<int>(index / Scene::kMaterialSlotsPerVoice);
            const int slot =
                static_cast<int>(index % Scene::kMaterialSlotsPerVoice);
            staging.materialSlots[voice][slot].kind =
                static_cast<GroovePuterMaterial::MaterialKind>(kinds[index]);
        }
    }
    if (hasMaterialIds) {
        if (!readAll(file, ids, sizeof(ids))) {
            file.close();
            return false;
        }
        for (size_t index = 0; index < kMaterialSlotCount; ++index) {
            const int voice =
                static_cast<int>(index / Scene::kMaterialSlotsPerVoice);
            const int slot =
                static_cast<int>(index % Scene::kMaterialSlotsPerVoice);
            staging.materialSlots[voice][slot].id = ids[index];
        }
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
    if (hasMaterialKinds) crc = crc32Update(crc, kinds, sizeof(kinds));
    if (hasMaterialIds) {
        crc = crc32Update(crc, reinterpret_cast<const uint8_t*>(ids), sizeof(ids));
    }
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

GroovePuterMaterial::MaterialId PatternPagingService::allocateMaterialId() {
    if (!ensureDirectory()) return {};
    uint32_t highWater = 0;
    if (!loadIdentityHighWater(activeProjectNameStorage(), highWater)) return {};
    if (highWater == 0xFFFFFFFFu) return {};
    const uint32_t next = highWater + 1u;
    if (next == 0 ||
        !writeIdentityHighWater(activeProjectNameStorage(), next)) {
        return {};
    }
    return GroovePuterMaterial::MaterialId{next};
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

    uint8_t kinds[kMaterialSlotCount]{};
    GroovePuterMaterial::MaterialId ids[kMaterialSlotCount]{};
    collectMaterialKinds(scene, kinds);
    collectMaterialIds(scene, ids);

    const PageFileHeader header = makeHeader(scene);
    File file = SD.open(temporaryPath.c_str(), FILE_WRITE);
    if (!file) return false;

    const bool wrote =
        writeAll(file, &header, sizeof(header)) &&
        writeAll(file, scene.synthABanks, sizeof(scene.synthABanks)) &&
        writeAll(file, scene.synthBBanks, sizeof(scene.synthBBanks)) &&
        writeAll(file, scene.drumBanks, sizeof(scene.drumBanks)) &&
        writeAll(file, kinds, sizeof(kinds)) &&
        writeAll(file, ids, sizeof(ids));
    file.flush();
    file.close();

    Scene& staging = sceneTransactionScratch();
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

    std::memcpy(scene.synthABanks, staging.synthABanks,
                sizeof(scene.synthABanks));
    std::memcpy(scene.synthBBanks, staging.synthBBanks,
                sizeof(scene.synthBBanks));
    std::memcpy(scene.drumBanks, staging.drumBanks,
                sizeof(scene.drumBanks));
    for (int voice = 0; voice < Scene::kMaterialVoices; ++voice) {
        for (int slot = 0; slot < Scene::kMaterialSlotsPerVoice; ++slot) {
            scene.materialSlots[voice][slot] = staging.materialSlots[voice][slot];
        }
    }
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
    for (int bank = 0; bank < kBankCount; ++bank) {
        scene.synthABanks[bank] = Bank<SynthPattern>{};
        scene.synthBBanks[bank] = Bank<SynthPattern>{};
        scene.drumBanks[bank] = Bank<DrumPatternSet>{};
    }
    resetMaterialMetadata(scene);
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

    uint32_t sourceHighWater = 0;
    uint32_t targetHighWater = 0;
    if (!loadIdentityHighWater(source, sourceHighWater) ||
        !loadIdentityHighWater(target, targetHighWater)) {
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

    const uint32_t copiedHighWater =
        sourceHighWater > targetHighWater ? sourceHighWater : targetHighWater;
    if (copiedHighWater != 0 &&
        !writeIdentityHighWater(target, copiedHighWater)) {
        clearProjectPagesFor(target);
        return false;
    }
    return true;
}

bool PatternPagingService::clearProjectPages() {
    if (!ensureDirectory()) return false;
    const bool cleared = clearProjectPagesFor(activeProjectNameStorage());
    if (cleared) activePageIndexStorage() = 0;
    return cleared;
}
