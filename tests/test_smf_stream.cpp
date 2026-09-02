#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

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
        if (length > maxRead_) maxRead_ = length;
        ++reads_;
        std::memcpy(dst, bytes_.data() + offset, length);
        return true;
    }

    std::size_t maxRead() const { return maxRead_; }
    std::size_t reads() const { return reads_; }
    void resetReads() { reads_ = 0; }

private:
    std::vector<uint8_t> bytes_;
    std::size_t maxRead_{0};
    std::size_t reads_{0};
};

void be16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void be32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void tag(std::vector<uint8_t>& out, const char* value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value[i]));
}

void appendTrack(std::vector<uint8_t>& file, const std::vector<uint8_t>& track) {
    tag(file, "MTrk");
    be32(file, static_cast<uint32_t>(track.size()));
    file.insert(file.end(), track.begin(), track.end());
}

std::vector<uint8_t> fixture() {
    std::vector<uint8_t> file;
    tag(file, "MThd");
    be32(file, 6);
    be16(file, 1);
    be16(file, 2);
    be16(file, 480);

    appendTrack(file, {
        0x00, 0xFF, 0x03, 0x09,
        'C','o','n','d','u','c','t','o','r',
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,
        0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x2F, 0x00,
    });
    appendTrack(file, {
        0x83, 0x60, 0x90, 60, 100,
        0x00, 64, 90,
        0x81, 0x70, 60, 0,
        0x00, 0x80, 64, 32,
        0x00, 0xC0, 10,
        0x00, 0xFF, 0x2F, 0x00,
    });
    return file;
}

std::vector<uint8_t> manyTrackFixture(uint16_t trackCount) {
    std::vector<uint8_t> file;
    tag(file, "MThd");
    be32(file, 6);
    be16(file, 1);
    be16(file, trackCount);
    be16(file, 480);
    for (uint16_t track = 0; track < trackCount; ++track) {
        appendTrack(file, {0x00, 0xFF, 0x2F, 0x00});
    }
    return file;
}

}  // namespace

int main() {
    MemorySource source(fixture());
    const SmfIndexResult indexed = SmfFileIndexer::build(source);
    assert(indexed.ok());
    assert(indexed.index.format == 1);
    assert(indexed.index.division == 480);
    assert(indexed.index.trackCount == 2);
    assert(indexed.index.declaredTrackCount == 2);
    assert(!indexed.index.tracksTruncated());
    assert(indexed.index.tracks[0].length > 0);
    assert(indexed.index.tracks[1].length > 0);

    SmfEventStreamMerger stream;
    assert(stream.open(source, indexed.index));
    assert(smfTrackMuteState().snapshot().trackCount == 2);
    assert(smfTrackMuteState().snapshot().mutedMask == 0);

    std::vector<SmfStreamEvent> events;
    SmfStreamEvent event;
    while (stream.next(event)) events.push_back(event);
    assert(stream.ended());

    assert(events.size() == 7);
    assert(events[0].event.kind == SmfEventKind::Tempo);
    assert(events[0].event.tick == 0);
    assert(events[0].event.value == 500000);
    assert(events[0].trackIndex == 0);
    assert(events[1].event.kind == SmfEventKind::TimeSignature);

    assert(events[2].event.kind == SmfEventKind::NoteOn);
    assert(events[2].event.tick == 480);
    assert(events[2].event.data1 == 60);
    assert(events[3].event.kind == SmfEventKind::NoteOn);
    assert(events[3].event.tick == 480);
    assert(events[3].event.data1 == 64);

    assert(events[4].event.kind == SmfEventKind::NoteOff);
    assert(events[4].event.tick == 720);
    assert(events[4].event.data1 == 60);
    assert(events[5].event.kind == SmfEventKind::NoteOff);
    assert(events[5].event.tick == 720);
    assert(events[5].event.data1 == 64);
    assert(events[6].event.kind == SmfEventKind::ProgramChange);
    assert(events[6].event.data1 == 10);

    const SmfStructuralInspectorSnapshot structure =
        smfStructuralInspectorState().snapshot();
    assert(structure.layerCount == 1);
    assert(structure.layers[0].trackIndex == 1);
    assert(structure.layers[0].noteCount == 2);
    assert(structure.layers[0].minNote == 60);
    assert(structure.layers[0].maxNote == 64);
    assert(structure.layers[0].form[0] > 0);
    for (uint8_t segment = 1; segment < kSmfStructuralFormSegments; ++segment) {
        assert(structure.layers[0].form[segment] == 0);
    }

    uint32_t musicStart = 0;
    bool foundMusic = false;
    for (const SmfStreamEvent& item : events) {
        if (item.event.kind == SmfEventKind::NoteOn) {
            musicStart = item.event.tick;
            foundMusic = true;
            break;
        }
    }
    assert(foundMusic && musicStart == 480);

    stream.reset();
    assert(stream.peek(event));
    assert(event.event.kind == SmfEventKind::Tempo);
    assert(event.event.tick == 0);

    assert(source.maxRead() <= kSmfTrackReadCacheBytes);
    // Two tracks share the pool, so each slice is sector-aligned and large
    // enough to hold the whole fixture track: one read per track per pass.
    assert(stream.trackCacheBytes() == kSmfTrackReadCacheBytes);
    assert(stream.trackCacheBytes() % kSmfSectorBytes == 0);
    source.resetReads();
    stream.reset();
    while (stream.next(event)) {
    }
    assert(source.reads() == indexed.index.trackCount);

    // Muting one source track removes only its future NoteOn events.
    // NoteOff remains cleanup-critical for notes queued before the mute.
    smfTrackMuteState().selectRelative(1);
    assert(smfTrackMuteState().snapshot().selectedTrack == 1);
    assert(smfTrackMuteState().toggleSelected());
    assert(smfTrackMuteState().snapshot().selectedMuted());
    stream.reset();
    events.clear();
    while (stream.next(event)) events.push_back(event);
    assert(events.size() == 5);
    assert(events[0].event.kind == SmfEventKind::Tempo);
    assert(events[1].event.kind == SmfEventKind::TimeSignature);
    assert(events[2].event.kind == SmfEventKind::NoteOff);
    assert(events[3].event.kind == SmfEventKind::NoteOff);
    assert(events[4].event.kind == SmfEventKind::ProgramChange);
    smfTrackMuteState().clear();

    // Large files are validated through their final declared MTrk, while only
    // the first bounded set is retained for playback and inspection.
    MemorySource largeSource(manyTrackFixture(41));
    const SmfIndexResult large = SmfFileIndexer::build(largeSource);
    assert(large.ok());
    assert(large.index.declaredTrackCount == 41);
    assert(large.index.trackCount == kSmfMaxTracks);
    assert(large.index.trackCount == 32);
    assert(large.index.tracksTruncated());
    assert(large.index.tracks[31].length == 4);

    SmfEventStreamMerger largeStream;
    assert(largeStream.open(largeSource, large.index));
    const SmfTrackInspectorSnapshot largeInspector =
        smfTrackInspectorState().snapshot();
    assert(largeInspector.trackCount == 32);
    assert(largeInspector.declaredTrackCount == 41);
    assert(largeInspector.tracksTruncated());
    assert(!largeStream.next(event));
    assert(largeStream.ended());

    // The arrangement projection is normalized across the actual analyzed
    // length, so a 200-bar file still addresses all sixteen display segments.
    SmfStructuralInspectorState arrangement;
    arrangement.reset(96, 1);
    const uint16_t bars[] = {0u, 50u, 100u, 150u, 199u};
    for (uint16_t bar : bars) {
        SmfEvent note{};
        note.tick = static_cast<uint32_t>(bar) * 384u;
        note.kind = SmfEventKind::NoteOn;
        note.channel = 0u;
        note.data1 = static_cast<uint8_t>(48u + (bar % 24u));
        note.data2 = 100u;
        arrangement.observe(0u, note);
        note.kind = SmfEventKind::NoteOff;
        note.tick += 96u;
        arrangement.observe(0u, note);
    }
    arrangement.finalize();
    const SmfStructuralInspectorSnapshot normalized = arrangement.snapshot();
    assert(normalized.analyzedBars == 200u);
    assert(normalized.layerCount == 1u);
    assert(normalized.layers[0].noteCount == 5u);
    assert(normalized.layers[0].form[0] > 0u);
    assert(normalized.layers[0].form[4] > 0u);
    assert(normalized.layers[0].form[8] > 0u);
    assert(normalized.layers[0].form[12] > 0u);
    assert(normalized.layers[0].form[15] > 0u);

    return 0;
}
