#pragma once

#include "../../scenes.h"
#include "src/dsp/deterministic_rng.h"
#include "src/state/scene_revision.h"
#include "src/state/undo_owner.h"

#include <algorithm>
#include <cstdint>
#include <utility>

namespace SongPatternMaterializer {

enum class Error : uint8_t {
    None = 0,
    InvalidRequest,
    NoEmptyPatternSlots,
    GenerationFailed,
};

constexpr uint8_t kSynthAMask = 1u << 0;
constexpr uint8_t kSynthBMask = 1u << 1;
constexpr uint8_t kDrumsMask = 1u << 2;
constexpr uint8_t kEditableTrackMask =
    kSynthAMask | kSynthBMask | kDrumsMask;
constexpr int kEditableTrackCount = 3;

// Song-generated ownership lives in a bit that was already reserved in the
// raw pattern step layout. This costs no additional Scene/DRAM and survives
// PatternPagingService .gpp persistence without changing the page format.
// Legacy/manual/imported material has the bit clear and is never reclaimed.
constexpr uint8_t kSongGeneratedOwnershipBit = 0x01u;

struct Request {
    int row = 0;
    int pageIndex = 0;
    uint32_t seed = 0x6D2B79F5u;
    uint8_t modeTag = 0;
    uint8_t trackMask = 0;
    int preferredLocalSlot[kEditableTrackCount] = {0, 0, 0};
};

struct Result {
    Error error = Error::None;
    int generatedTracks = 0;
    int globalPattern[kEditableTrackCount] = {-1, -1, -1};
    int reusedGeneratedTracks = 0;
    int failedTrackIndex = -1;

    explicit operator bool() const { return error == Error::None; }
};

struct PreparedMaterial {
    int localSlot[kEditableTrackCount] = {-1, -1, -1};
    int globalPattern[kEditableTrackCount] = {-1, -1, -1};
    bool reusedGenerated[kEditableTrackCount] = {false, false, false};
    SynthPattern synthA{};
    SynthPattern synthB{};
    DrumPatternSet drums{};
};

inline int editableTrackIndex(SongTrack track) {
    switch (track) {
        case SongTrack::SynthA: return 0;
        case SongTrack::SynthB: return 1;
        case SongTrack::Drums: return 2;
        case SongTrack::Voice: break;
    }
    return -1;
}

inline SongTrack editableTrackForIndex(int index) {
    switch (index) {
        case 0: return SongTrack::SynthA;
        case 1: return SongTrack::SynthB;
        default: return SongTrack::Drums;
    }
}

inline uint8_t maskForTrack(SongTrack track) {
    const int index = editableTrackIndex(track);
    return index < 0 ? 0 : static_cast<uint8_t>(1u << index);
}

inline bool synthPatternIsStrictlyEmpty(const SynthPattern& pattern) {
    for (int step = 0; step < SynthPattern::kSteps; ++step) {
        const SynthStep& value = pattern.steps[step];
        if (value.note != -1 || value.slide || value.accent || value.ghost ||
            value.velocity != 100 || value.timing != 0 || value.fx != 0 ||
            value.fxParam != 0 || value.probability != 100) {
            return false;
        }
    }
    return true;
}

inline bool drumPatternSetIsStrictlyEmpty(const DrumPatternSet& pattern) {
    for (int voice = 0; voice < DrumPatternSet::kVoices; ++voice) {
        for (int step = 0; step < DrumPattern::kSteps; ++step) {
            const DrumStep& value = pattern.voices[voice].steps[step];
            if (value.hit || value.accent || value.velocity != 100 ||
                value.timing != 0 || value.fx != 0 || value.fxParam != 0 ||
                value.probability != 100) {
                return false;
            }
        }
    }
    for (int lane = 0; lane < DrumPatternSet::kMaxLanes; ++lane) {
        if (pattern.lanes[lane].targetParam != DRUM_AUTOMATION_NONE ||
            pattern.lanes[lane].nodeCount != 0) {
            return false;
        }
    }
    return pattern.groove.swing < 0.0f && pattern.groove.humanize < 0.0f;
}

inline const SynthPattern& synthPatternAtLocalSlot(
        const Scene& scene, SongTrack track, int localSlot) {
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    return track == SongTrack::SynthA
        ? scene.synthABanks[bank].patterns[index]
        : scene.synthBBanks[bank].patterns[index];
}

inline SynthPattern& synthPatternAtLocalSlot(
        Scene& scene, SongTrack track, int localSlot) {
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;
    return track == SongTrack::SynthA
        ? scene.synthABanks[bank].patterns[index]
        : scene.synthBBanks[bank].patterns[index];
}

inline const DrumPatternSet& drumPatternAtLocalSlot(
        const Scene& scene, int localSlot) {
    const int bank = localSlot / Bank<DrumPatternSet>::kPatterns;
    const int index = localSlot % Bank<DrumPatternSet>::kPatterns;
    return scene.drumBanks[bank].patterns[index];
}

inline DrumPatternSet& drumPatternAtLocalSlot(Scene& scene, int localSlot) {
    const int bank = localSlot / Bank<DrumPatternSet>::kPatterns;
    const int index = localSlot % Bank<DrumPatternSet>::kPatterns;
    return scene.drumBanks[bank].patterns[index];
}

inline bool slotContentIsEmpty(
        const Scene& scene, SongTrack track, int localSlot) {
    if (localSlot < 0 || localSlot >= kPatternsPerPage) return false;
    if (track == SongTrack::SynthA || track == SongTrack::SynthB) {
        return synthPatternIsStrictlyEmpty(
            synthPatternAtLocalSlot(scene, track, localSlot));
    }
    if (track == SongTrack::Drums) {
        return drumPatternSetIsStrictlyEmpty(
            drumPatternAtLocalSlot(scene, localSlot));
    }
    return false;
}

inline bool slotIsSongGenerated(
        const Scene& scene, SongTrack track, int localSlot) {
    if (localSlot < 0 || localSlot >= kPatternsPerPage) return false;
    if (track == SongTrack::SynthA || track == SongTrack::SynthB) {
        return (synthPatternAtLocalSlot(scene, track, localSlot).steps[0].unused &
                kSongGeneratedOwnershipBit) != 0;
    }
    if (track == SongTrack::Drums) {
        return (drumPatternAtLocalSlot(scene, localSlot)
                    .voices[0].steps[0].unused &
                kSongGeneratedOwnershipBit) != 0;
    }
    return false;
}

inline void markSlotSongGenerated(
        Scene& scene, SongTrack track, int localSlot) {
    if (localSlot < 0 || localSlot >= kPatternsPerPage) return;
    if (track == SongTrack::SynthA || track == SongTrack::SynthB) {
        SynthStep& step = synthPatternAtLocalSlot(scene, track, localSlot).steps[0];
        step.unused = static_cast<uint8_t>(
            step.unused | kSongGeneratedOwnershipBit);
        return;
    }
    if (track == SongTrack::Drums) {
        DrumStep& step = drumPatternAtLocalSlot(scene, localSlot)
                             .voices[0].steps[0];
        step.unused = static_cast<uint8_t>(
            step.unused | kSongGeneratedOwnershipBit);
    }
}

inline bool phraseSlotHasPatternReferences(const PhraseCore::PhraseSlot& phrase) {
    const PhraseCore::PhraseMetadata& metadata = phrase.metadata;
    return (metadata.flags & PhraseCore::kFlagValid) != 0 &&
           metadata.phraseId != PhraseCore::kNoPhraseId &&
           PhraseCore::isValidLength(metadata.lengthBars) &&
           PhraseCore::isValidRole(metadata.role) &&
           PhraseCore::isSongReferenceSource(metadata.source) &&
           metadata.storage == PhraseCore::StorageMode::ReferenceView &&
           PhraseCore::isValidTrackMask(metadata.trackMask) &&
           metadata.sourceSongSlot <= 1 &&
           metadata.sourceStartRow < Song::kMaxPositions;
}

inline int phrasePatternReferenceCount(
        const Scene& scene, SongTrack track, int globalPattern) {
    const int trackIndex = editableTrackIndex(track);
    if (trackIndex < 0 || globalPattern < 0) return 0;
    const uint8_t trackMask = PhraseCore::maskForTrackIndex(trackIndex);
    int references = 0;
    for (int slotIndex = 0; slotIndex < PhraseCore::kSlotCount; ++slotIndex) {
        const PhraseCore::PhraseSlot& phrase = scene.phraseBank.slots[slotIndex];
        if (!phraseSlotHasPatternReferences(phrase) ||
            (phrase.metadata.trackMask & trackMask) == 0) {
            continue;
        }
        for (int bar = 0; bar < phrase.metadata.lengthBars; ++bar) {
            if (phrase.patternRefs[bar][trackIndex] == globalPattern) {
                ++references;
            }
        }
    }
    return references;
}

// Persistent references are Scene-owned Song/Phrase refs only. P1b keeps this
// separate from canonical Undo retention so cleanup/persistence can distinguish
// live persistent ownership from runtime-only redo ownership.
inline int persistentGlobalPatternReferenceCount(
        const Scene& scene, SongTrack track, int globalPattern) {
    const int trackIndex = editableTrackIndex(track);
    if (trackIndex < 0 || globalPattern < 0) return 0;
    int references = 0;
    for (int songSlot = 0; songSlot < 2; ++songSlot) {
        const Song& song = scene.songs[songSlot];
        for (int row = 0; row < Song::kMaxPositions; ++row) {
            if (song.positions[row].patterns[trackIndex] == globalPattern) {
                ++references;
            }
        }
    }
    return references + phrasePatternReferenceCount(scene, track, globalPattern);
}

inline int globalPatternReferenceCount(
        const Scene& scene, SongTrack track, int globalPattern) {
    const int persistent =
        persistentGlobalPatternReferenceCount(scene, track, globalPattern);
    const uint8_t trackMask = maskForTrack(track);
    return persistent +
        (GroovePuterUndo::undoOwner().retainsPatternBacking(
             globalPattern, trackMask) ? 1 : 0);
}

inline bool globalPatternIsReferenced(
        const Scene& scene, SongTrack track, int globalPattern) {
    return globalPatternReferenceCount(scene, track, globalPattern) > 0;
}

inline int localSlotFromGlobalPattern(int globalPattern) {
    if (globalPattern < 0) return -1;
    const int bank = songPatternBank(globalPattern);
    const int index = songPatternIndexInBank(globalPattern);
    if (bank < 0 || bank >= kBankCount || index < 0 ||
        index >= Bank<SynthPattern>::kPatterns) {
        return -1;
    }
    return bank * Bank<SynthPattern>::kPatterns + index;
}

inline int findReusableLocalSlot(
        const Scene& scene,
        const Request& request,
        SongTrack track,
        int preferredLocalSlot,
        bool& reusedGenerated) {
    reusedGenerated = false;
    if (request.pageIndex < 0 || request.pageIndex >= kMaxPages ||
        request.row < 0 || request.row >= Song::kMaxPositions ||
        editableTrackIndex(track) < 0) {
        return -1;
    }

    const int activeSongSlot = std::clamp(scene.activeSongSlot, 0, 1);
    const int trackIndex = editableTrackIndex(track);
    const int currentGlobal =
        scene.songs[activeSongSlot].positions[request.row].patterns[trackIndex];
    if (currentGlobal >= 0 && songPatternPage(currentGlobal) == request.pageIndex) {
        const int localSlot = localSlotFromGlobalPattern(currentGlobal);
        if (localSlot >= 0 && slotIsSongGenerated(scene, track, localSlot) &&
            globalPatternReferenceCount(scene, track, currentGlobal) == 1) {
            reusedGenerated = true;
            return localSlot;
        }
    }

    int start = preferredLocalSlot;
    if (start < 0 || start >= kPatternsPerPage) start = 0;
    for (int offset = 0; offset < kPatternsPerPage; ++offset) {
        const int localSlot = (start + offset) % kPatternsPerPage;
        const int bank = localSlot / Bank<SynthPattern>::kPatterns;
        const int index = localSlot % Bank<SynthPattern>::kPatterns;
        const int globalPattern = songPatternFromPageBankIndex(
            request.pageIndex, bank, index);
        if (globalPatternReferenceCount(scene, track, globalPattern) != 0) {
            continue;
        }
        if (slotContentIsEmpty(scene, track, localSlot)) {
            return localSlot;
        }
        if (slotIsSongGenerated(scene, track, localSlot)) {
            reusedGenerated = true;
            return localSlot;
        }
    }
    return -1;
}

inline int findSafeFreeLocalSlot(
        const Scene& scene,
        int pageIndex,
        SongTrack track,
        int preferredLocalSlot) {
    if (pageIndex < 0 || pageIndex >= kMaxPages ||
        editableTrackIndex(track) < 0) {
        return -1;
    }
    int start = preferredLocalSlot;
    if (start < 0 || start >= kPatternsPerPage) start = 0;
    for (int offset = 0; offset < kPatternsPerPage; ++offset) {
        const int localSlot = (start + offset) % kPatternsPerPage;
        const int bank = localSlot / Bank<SynthPattern>::kPatterns;
        const int index = localSlot % Bank<SynthPattern>::kPatterns;
        const int globalPattern = songPatternFromPageBankIndex(
            pageIndex, bank, index);
        if (slotContentIsEmpty(scene, track, localSlot) &&
            !globalPatternIsReferenced(scene, track, globalPattern)) {
            return localSlot;
        }
    }
    return -1;
}

inline int reusableSlotCount(
        const Scene& scene, int pageIndex, SongTrack track) {
    if (pageIndex < 0 || pageIndex >= kMaxPages ||
        editableTrackIndex(track) < 0) {
        return 0;
    }
    int available = 0;
    for (int localSlot = 0; localSlot < kPatternsPerPage; ++localSlot) {
        const int bank = localSlot / Bank<SynthPattern>::kPatterns;
        const int index = localSlot % Bank<SynthPattern>::kPatterns;
        const int globalPattern = songPatternFromPageBankIndex(
            pageIndex, bank, index);
        if (globalPatternReferenceCount(scene, track, globalPattern) != 0) {
            continue;
        }
        if (slotContentIsEmpty(scene, track, localSlot) ||
            slotIsSongGenerated(scene, track, localSlot)) {
            ++available;
        }
    }
    return available;
}

inline uint32_t actionSeed(const Request& request, int localSlot) {
    uint32_t seed = request.seed;
    seed ^= static_cast<uint32_t>(request.pageIndex + 1) * 0x9E3779B9u;
    seed ^= static_cast<uint32_t>(request.row + 1) * 0x85EBCA6Bu;
    seed ^= static_cast<uint32_t>(request.modeTag + 1) * 0xC2B2AE35u;
    seed ^= static_cast<uint32_t>(localSlot + 1) * 0x27D4EB2Fu;
    DeterministicRng rng(seed);
    return rng.next();
}

inline void writePreparedTrack(
        Scene& scene,
        Song& song,
        int row,
        SongTrack track,
        const PreparedMaterial& prepared) {
    const int trackIndex = editableTrackIndex(track);
    const int localSlot = prepared.localSlot[trackIndex];
    const int bank = localSlot / Bank<SynthPattern>::kPatterns;
    const int index = localSlot % Bank<SynthPattern>::kPatterns;

    switch (track) {
        case SongTrack::SynthA:
            scene.synthABanks[bank].patterns[index] = prepared.synthA;
            break;
        case SongTrack::SynthB:
            scene.synthBBanks[bank].patterns[index] = prepared.synthB;
            break;
        case SongTrack::Drums:
            scene.drumBanks[bank].patterns[index] = prepared.drums;
            break;
        case SongTrack::Voice:
            return;
    }
    markSlotSongGenerated(scene, track, localSlot);
    song.positions[row].patterns[trackIndex] =
        static_cast<int16_t>(prepared.globalPattern[trackIndex]);
}

template <typename Generator, typename Commit>
Result generate(
        Scene& scene,
        const Request& request,
        Generator&& generator,
        Commit&& commit) {
    Result result{};
    if (request.row < 0 || request.row >= Song::kMaxPositions ||
        request.pageIndex < 0 || request.pageIndex >= kMaxPages ||
        (request.trackMask & kEditableTrackMask) == 0 ||
        (request.trackMask & ~kEditableTrackMask) != 0) {
        result.error = Error::InvalidRequest;
        return result;
    }

    PreparedMaterial prepared{};
    for (int trackIndex = 0; trackIndex < kEditableTrackCount; ++trackIndex) {
        const uint8_t bit = static_cast<uint8_t>(1u << trackIndex);
        if ((request.trackMask & bit) == 0) continue;
        const SongTrack track = editableTrackForIndex(trackIndex);
        bool reusedGenerated = false;
        const int localSlot = findReusableLocalSlot(
            scene,
            request,
            track,
            request.preferredLocalSlot[trackIndex],
            reusedGenerated);
        if (localSlot < 0) {
            result.error = Error::NoEmptyPatternSlots;
            result.failedTrackIndex = trackIndex;
            return result;
        }
        prepared.localSlot[trackIndex] = localSlot;
        prepared.reusedGenerated[trackIndex] = reusedGenerated;
        const int bank = localSlot / Bank<SynthPattern>::kPatterns;
        const int index = localSlot % Bank<SynthPattern>::kPatterns;
        prepared.globalPattern[trackIndex] = songPatternFromPageBankIndex(
            request.pageIndex, bank, index);
    }

    auto&& generateTrack = generator;
    for (int trackIndex = 0; trackIndex < kEditableTrackCount; ++trackIndex) {
        const uint8_t bit = static_cast<uint8_t>(1u << trackIndex);
        if ((request.trackMask & bit) == 0) continue;
        const SongTrack track = editableTrackForIndex(trackIndex);
        SynthPattern& synth = track == SongTrack::SynthA
            ? prepared.synthA
            : prepared.synthB;
        if (!generateTrack(
                track,
                actionSeed(request, prepared.localSlot[trackIndex]),
                synth,
                prepared.drums)) {
            result.error = Error::GenerationFailed;
            result.failedTrackIndex = trackIndex;
            return result;
        }
        const bool empty = track == SongTrack::Drums
            ? drumPatternSetIsStrictlyEmpty(prepared.drums)
            : synthPatternIsStrictlyEmpty(synth);
        if (empty) {
            result.error = Error::GenerationFailed;
            result.failedTrackIndex = trackIndex;
            return result;
        }
    }

    const int activeSongSlot = std::clamp(scene.activeSongSlot, 0, 1);
    auto&& commitPrepared = commit;
    commitPrepared([&]() {
        Song& song = scene.songs[activeSongSlot];
        for (int trackIndex = 0; trackIndex < kEditableTrackCount; ++trackIndex) {
            const uint8_t bit = static_cast<uint8_t>(1u << trackIndex);
            if ((request.trackMask & bit) == 0) continue;
            writePreparedTrack(
                scene,
                song,
                request.row,
                editableTrackForIndex(trackIndex),
                prepared);
        }
        song.length = std::max(song.length, request.row + 1);
    });

    GroovePuterState::markSceneMutated();
    result.error = Error::None;
    for (int trackIndex = 0; trackIndex < kEditableTrackCount; ++trackIndex) {
        const uint8_t bit = static_cast<uint8_t>(1u << trackIndex);
        if ((request.trackMask & bit) == 0) continue;
        result.globalPattern[trackIndex] = prepared.globalPattern[trackIndex];
        if (prepared.reusedGenerated[trackIndex]) {
            ++result.reusedGeneratedTracks;
        }
        ++result.generatedTracks;
    }
    return result;
}

}  // namespace SongPatternMaterializer
