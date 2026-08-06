#pragma once

#include "../../scenes.h"
#include "src/dsp/deterministic_rng.h"
#include "src/state/scene_revision.h"

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

    explicit operator bool() const { return error == Error::None; }
};

struct PreparedMaterial {
    int localSlot[kEditableTrackCount] = {-1, -1, -1};
    int globalPattern[kEditableTrackCount] = {-1, -1, -1};
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

inline const DrumPatternSet& drumPatternAtLocalSlot(
        const Scene& scene, int localSlot) {
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

inline bool globalPatternIsReferenced(
        const Scene& scene, SongTrack track, int globalPattern) {
    const int trackIndex = editableTrackIndex(track);
    if (trackIndex < 0) return true;
    for (int songSlot = 0; songSlot < 2; ++songSlot) {
        const Song& song = scene.songs[songSlot];
        for (int row = 0; row < Song::kMaxPositions; ++row) {
            if (song.positions[row].patterns[trackIndex] == globalPattern) {
                return true;
            }
        }
    }
    return false;
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
    song.positions[row].patterns[trackIndex] =
        static_cast<int16_t>(prepared.globalPattern[trackIndex]);
}

// Generator signature:
//   bool(SongTrack, uint32_t seed, SynthPattern&, DrumPatternSet&)
// Commit signature:
//   void(callable) and must execute the callable synchronously.
//
// Preparation is allocation-free and read-only. Scene data and Song references
// are written only after every requested track has a safe destination and valid
// generated material, so generator failure needs no rollback write.
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
        const int localSlot = findSafeFreeLocalSlot(
            scene,
            request.pageIndex,
            track,
            request.preferredLocalSlot[trackIndex]);
        if (localSlot < 0) {
            result.error = Error::NoEmptyPatternSlots;
            return result;
        }
        prepared.localSlot[trackIndex] = localSlot;
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
            return result;
        }
        const bool empty = track == SongTrack::Drums
            ? drumPatternSetIsStrictlyEmpty(prepared.drums)
            : synthPatternIsStrictlyEmpty(synth);
        if (empty) {
            result.error = Error::GenerationFailed;
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
        ++result.generatedTracks;
    }
    return result;
}

}  // namespace SongPatternMaterializer
