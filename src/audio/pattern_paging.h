#ifndef GROOVEPUTER_SRC_AUDIO_PATTERN_PAGING_H
#define GROOVEPUTER_SRC_AUDIO_PATTERN_PAGING_H

#include "../../scenes.h"
#include "../state/material_publication_record.h"
#include "../generation/harmony/harmonic_source_codec.h"
#include <cstdint>
#include <array>
#include <string>

enum class SongHarmonicStorageStatus : uint8_t {
    Ok = 0,
    NotFound,
    InvalidInput,
    InvalidData,
    Corrupt,
    UnsupportedVersion,
    IoError,
};

class PatternPagingService {
public:
    // Version 6 introduces dual-generation A/B ping-pong storage with
    // storageGeneration counter and NVS publication record.
    // Version 5 persists the complete MaterialSlotDescriptor (kind + stable
    // MaterialId). Version 4 persisted kind only; version 3 predates material
    // metadata entirely. Legacy pages decode with id=0 so identity fails closed.
    static constexpr uint16_t kDualGenFormatVersion = 6;
    static constexpr uint16_t kFormatVersion = 6;
    static constexpr uint16_t kLegacyV5FormatVersion = 5;
    static constexpr uint16_t kKindOnlyFormatVersion = 4;
    static constexpr uint16_t kLegacyFormatVersion = 3;

    // Select the project namespace used by all following page operations.
    // Names are encoded before becoming SD paths; callers pass scene names.
    static bool setProjectName(const std::string& projectName);
    // Restore an already-selected namespace in RAM after a failed scene load.
    // This deliberately performs no filesystem access.
    static void restoreProjectNameInMemory(const std::string& projectName);
    static const std::string& currentProjectName();
    static int activePageIndex();
    // A successfully parsed project starts with its page-zero resident payload.
    static void resetLoadedPageIndex();

    // Allocate an opaque non-zero identity inside the active project namespace.
    // The high-water mark is persistent and is deliberately not reset by
    // clearProjectPages(), so stale references cannot alias new material.
    static GroovePuterMaterial::MaterialId allocateMaterialId();

    // Reserve a contiguous range of bounded [1..8] MaterialIds in the active project namespace.
    // Exactly one durable high-water update is performed for the batch.
    // Overflow, invalid count, or I/O failure fail closed and return an invalid reservation.
    static GroovePuterMaterial::MaterialIdReservation reserveMaterialIds(uint8_t count);

    // Persist or load all pattern banks for one logical page. Both operations
    // are transactional from the caller's perspective: a failed save keeps the
    // previous page file, and a failed load leaves Scene unchanged.
    static bool savePage(int pageIndex, const Scene& scene);
    static bool loadPage(int pageIndex, Scene& scene, bool publishPageIndex = true);
    using PageMaterialDescriptors = std::array<GroovePuterMaterial::MaterialSlotDescriptor,
        Scene::kMaterialVoices * Scene::kMaterialSlotsPerVoice>;
    // Control-side integrity read; does not touch Scene scratch or page identity.
    static bool readPageMaterialDescriptors(int pageIndex, PageMaterialDescriptors& out);

    // Atomically commit a page candidate (pattern or descriptor) to the inactive dual-generation slot,
    // readback-verify, write NVS publication record, and update scene RAM infallibly.
    static bool commitPageCandidate(int pageIndex,
                                    Scene& scene,
                                    int voice,
                                    int bank,
                                    int pattern,
                                    const SynthPattern* candidatePattern,
                                    GroovePuterMaterial::MaterialKind candidateKind,
                                    GroovePuterMaterial::MaterialId candidateId,
                                    bool publishResident = true);

    // Complete only the infallible resident update after a verified deferred
    // commit. The caller must exclude the audio reader at this boundary.
    static void publishPageCandidateRam(
        int pageIndex, Scene& scene, int voice, int bank, int pattern,
        const SynthPattern* candidatePattern,
        GroovePuterMaterial::MaterialKind candidateKind,
        GroovePuterMaterial::MaterialId candidateId);

    static bool commitPatternCandidate(int pageIndex,
                                       Scene& scene,
                                       int voice,
                                       int bank,
                                       int pattern,
                                       const SynthPattern& candidatePattern);

    static bool commitMelodyDescriptor(int pageIndex,
                                       Scene& scene,
                                       int voice,
                                       int bank,
                                       int pattern,
                                       GroovePuterMaterial::MaterialId candidateId);

    // Versioned, project-scoped route sidecar. Records are immutable and keyed
    // by song placement plus the first Synth A material id/version, so edits
    // and Undo retain the prior exact record instead of overwriting it.
    static SongHarmonicStorageStatus saveSongHarmonicState(
        const GroovePuterHarmony::SongHarmonicState& state);
    static SongHarmonicStorageStatus loadSongHarmonicState(
        uint8_t songSlot, uint8_t startPosition,
        const GroovePuterHarmony::SongHarmonicParticipant* currentParticipants,
        uint8_t currentParticipantCount,
        GroovePuterHarmony::SongHarmonicState& out);
    static SongHarmonicStorageStatus removeSongHarmonicState(
        uint8_t songSlot, uint8_t startPosition,
        GroovePuterMaterial::MaterialId anchorMaterialId,
        GroovePuterMaterial::MaterialVersionToken anchorVersion);

    // Query active slot and generation from publication record / inspected headers
    static std::string slotPath(int pageIndex, GroovePuterMaterial::PublicationSlot slot);
    static std::string slotPathForProject(const std::string& projectName,
                                          int pageIndex,
                                          GroovePuterMaterial::PublicationSlot slot);
    static uint32_t activeStorageGeneration(int pageIndex);
    static GroovePuterMaterial::PublicationSlot activePublicationSlot(int pageIndex);

    // Restore the previous validated page version created by savePage().
    // Used by multi-page import transactions when a later page fails.
    static bool restoreBackup(int pageIndex);

    // Initialize only pattern banks. Scene metadata, songs, sampler settings,
    // genre and mix state remain unchanged.
    static void initializeEmptyPage(Scene& scene);

    static bool pageExists(int pageIndex);
    static bool removePage(int pageIndex);

    // Project lifecycle helpers. Copy is used by Save As. New/Clear remove
    // every page plus transactional .tmp/.bak siblings in one project only.
    // Save As copies the MaterialId high-water mark; Clear keeps it.
    static bool copyProjectPages(const std::string& sourceProject,
                                 const std::string& targetProject);
    static bool clearProjectPages();
    static bool clearProjectPages(const std::string& projectName);
    static bool clearSongHarmonicStorage(const std::string& projectName);

    // Compatibility entry point for the existing SceneManager constructor.
    // It will be removed when early SD access is migrated out of static init.
    static bool ensureDirectory();

private:
    static bool validPageIndex(int pageIndex);
    static std::string projectDirectory(const std::string& projectName);
    static std::string pagePathForProject(const std::string& projectName,
                                          int pageIndex);
    static std::string pagePath(int pageIndex);
    static std::string tempPath(int pageIndex);
    static std::string backupPath(int pageIndex);
};

#endif  // GROOVEPUTER_SRC_AUDIO_PATTERN_PAGING_H
