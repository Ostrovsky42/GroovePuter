#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "src/midi/smf_session_generation.h"
#include "src/midi/smf_stream.h"
#include "src/midi/smf_structural_inspector.h"
#include "src/midi/smf_track_inspector.h"
#include "src/midi/smf_track_mute.h"

using namespace GroovePuterMidi;

namespace {

class MemorySource final : public ISmfByteSource {
public:
    explicit MemorySource(std::vector<uint8_t> bytes)
        : bytes_(std::move(bytes)) {}

    uint32_t size() const override {
        return static_cast<uint32_t>(bytes_.size());
    }

    bool readAt(uint32_t offset, uint8_t* dst, std::size_t length) override {
        if (!dst || offset > bytes_.size() || length > bytes_.size() - offset) {
            return false;
        }
        std::memcpy(dst, bytes_.data() + offset, length);
        return true;
    }

private:
    std::vector<uint8_t> bytes_;
};

SmfFileIndex indexForTracks(uint16_t trackCount) {
    SmfFileIndex index{};
    index.format = trackCount == 1u ? 0u : 1u;
    index.division = 96u;
    index.trackCount = trackCount;
    index.declaredTrackCount = trackCount;
    for (uint16_t track = 0; track < trackCount; ++track) {
        index.tracks[track] = SmfTrackSpan{
            static_cast<uint32_t>(track * 4u),
            4u,
        };
    }
    return index;
}

std::vector<uint8_t> endOfTrackBytes(uint16_t trackCount) {
    std::vector<uint8_t> bytes;
    bytes.reserve(static_cast<std::size_t>(trackCount) * 4u);
    for (uint16_t track = 0; track < trackCount; ++track) {
        bytes.insert(bytes.end(), {0x00, 0xFF, 0x2F, 0x00});
    }
    return bytes;
}

void assertNoLoadedSession() {
    assert(smfSessionGeneration() == 0u);

    const SmfStructuralInspectorSnapshot layers =
        smfStructuralInspectorState().snapshot();
    const SmfTrackInspectorSnapshot tracks =
        smfTrackInspectorState().snapshot();
    const SmfTrackMuteSnapshot mute = smfTrackMuteState().snapshot();

    assert(layers.generation == 0u);
    assert(layers.layerCount == 0u);
    assert(tracks.generation == 0u);
    assert(tracks.trackCount == 0u);
    assert(mute.generation == 0u);
    assert(mute.trackCount == 0u);
    assert(mute.mutedMask == 0u);
}

}  // namespace

int main() {
    assertNoLoadedSession();

    MemorySource sourceA(endOfTrackBytes(1u));
    MemorySource sourceB(endOfTrackBytes(2u));
    const SmfFileIndex indexA = indexForTracks(1u);
    const SmfFileIndex indexB = indexForTracks(2u);

    SmfEventStreamMerger merger;
    assert(merger.open(sourceA, indexA));
    const uint32_t generationA = smfSessionGeneration();
    assert(generationA != 0u);

    const SmfStructuralInspectorSnapshot layersA =
        smfStructuralInspectorState().snapshot();
    const SmfTrackInspectorSnapshot tracksA =
        smfTrackInspectorState().snapshot();
    const SmfTrackMuteSnapshot muteA = smfTrackMuteState().snapshot();
    assert(smfSnapshotGenerationsMatch(
        generationA, layersA.generation, tracksA.generation, muteA.generation));
    assert(tracksA.trackCount == 1u);
    assert(muteA.trackCount == 1u);

    assert(merger.open(sourceB, indexB));
    const uint32_t generationB = smfSessionGeneration();
    assert(generationB > generationA);

    const SmfStructuralInspectorSnapshot layersB =
        smfStructuralInspectorState().snapshot();
    const SmfTrackInspectorSnapshot tracksB =
        smfTrackInspectorState().snapshot();
    const SmfTrackMuteSnapshot muteB = smfTrackMuteState().snapshot();
    assert(smfSnapshotGenerationsMatch(
        generationB, layersB.generation, tracksB.generation, muteB.generation));
    assert(tracksB.trackCount == 2u);
    assert(muteB.trackCount == 2u);

    // A frame assembled across the A -> B boundary is never valid.
    assert(!smfSnapshotGenerationsMatch(
        generationB, layersA.generation, tracksB.generation, muteB.generation));
    assert(!smfSnapshotGenerationsMatch(
        generationB, layersB.generation, tracksA.generation, muteB.generation));

    // A mute command formed from A cannot mutate the physical tracks of B.
    assert(!smfTrackMuteState().toggleTrack(0u, muteA.generation));
    const SmfTrackMuteSnapshot afterStaleMute =
        smfTrackMuteState().snapshot();
    assert(afterStaleMute.generation == generationB);
    assert(afterStaleMute.mutedMask == 0u);

    assert(smfTrackMuteState().toggleTrack(1u, generationB));
    assert((smfTrackMuteState().snapshot().mutedMask & (uint64_t{1} << 1u)) != 0u);
    assert(smfTrackMuteState().clear(generationB));

    // Even an early validation failure invalidates the previous session.
    const SmfFileIndex emptyIndex{};
    assert(!merger.open(sourceB, emptyIndex));
    assertNoLoadedSession();

    assert(merger.open(sourceA, indexA));
    const uint32_t generationC = smfSessionGeneration();
    assert(generationC > generationB);

    // A lower-level track-open failure also invalidates the active session.
    SmfFileIndex invalidSpan = indexA;
    invalidSpan.tracks[0] = SmfTrackSpan{sourceA.size() + 1u, 4u};
    assert(!merger.open(sourceA, invalidSpan));
    assertNoLoadedSession();

    assert(merger.open(sourceB, indexB));
    const uint32_t generationD = smfSessionGeneration();
    assert(generationD > generationC);
    assert(generationA != generationB);
    assert(generationB != generationC);
    assert(generationC != generationD);

    return 0;
}
