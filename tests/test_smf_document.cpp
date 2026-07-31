#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "src/midi/smf_document.h"

using namespace GroovePuterMidi;

namespace {

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

std::vector<uint8_t> makeType1Fixture() {
    std::vector<uint8_t> file;
    tag(file, "MThd");
    be32(file, 6);
    be16(file, 1);
    be16(file, 2);
    be16(file, 480);

    const std::vector<uint8_t> conductor = {
        0x00, 0xFF, 0x03, 0x09,
        'C','o','n','d','u','c','t','o','r',
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,
        0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x2F, 0x00,
    };
    appendTrack(file, conductor);

    const std::vector<uint8_t> notes = {
        0x00, 0xFF, 0x03, 0x04, 'L','e','a','d',
        0x83, 0x60, 0x90, 60, 100,
        0x00, 64, 90,
        0x81, 0x70, 60, 0,
        0x00, 0x80, 64, 32,
        0x00, 0xC0, 10,
        0x00, 0xFF, 0x2F, 0x00,
    };
    appendTrack(file, notes);
    return file;
}

}  // namespace

int main() {
    const std::vector<uint8_t> bytes = makeType1Fixture();
    const SmfParseResult parsed = SmfParser::parse(bytes.data(), bytes.size());
    assert(parsed.ok());
    const SmfDocument& doc = parsed.document;

    assert(doc.format == 1);
    assert(doc.division == 480);
    assert(doc.tracks.size() == 2);
    assert(doc.tracks[0].name == "Conductor");
    assert(doc.tracks[1].name == "Lead");
    assert(doc.tracks[1].noteCount == 2);
    assert(doc.musicStartTick == 480);
    assert(doc.endTick == 720);

    assert(doc.events.size() == 7);
    assert(doc.events[0].kind == SmfEventKind::Tempo);
    assert(doc.events[0].tick == 0);
    assert(doc.events[0].value == 500000);
    assert(doc.events[1].kind == SmfEventKind::TimeSignature);
    assert(doc.events[1].data1 == 4);
    assert(doc.events[1].data2 == 2);

    assert(doc.events[2].kind == SmfEventKind::NoteOn);
    assert(doc.events[2].tick == 480);
    assert(doc.events[2].channel == 0);
    assert(doc.events[2].data1 == 60);
    assert(doc.events[2].data2 == 100);

    assert(doc.events[3].kind == SmfEventKind::NoteOn);
    assert(doc.events[3].tick == 480);
    assert(doc.events[3].data1 == 64);
    assert(doc.events[3].data2 == 90);

    assert(doc.events[4].kind == SmfEventKind::NoteOff);
    assert(doc.events[4].tick == 720);
    assert(doc.events[4].data1 == 60);
    assert(doc.events[5].kind == SmfEventKind::NoteOff);
    assert(doc.events[5].tick == 720);
    assert(doc.events[5].data1 == 64);
    assert(doc.events[6].kind == SmfEventKind::ProgramChange);
    assert(doc.events[6].data1 == 10);

    SmfTimelineCursor cursor;
    cursor.load(&doc);
    assert(cursor.loaded());
    assert(cursor.state() == SmfPlaybackState::Stopped);

    cursor.restart(SmfRestartOrigin::FileStart);
    assert(cursor.state() == SmfPlaybackState::Playing);
    assert(cursor.tick() == 0);
    assert(cursor.peek() && cursor.peek()->kind == SmfEventKind::Tempo);
    assert(cursor.pop() && cursor.eventIndex() == 1);

    cursor.pause();
    assert(cursor.state() == SmfPlaybackState::Paused);
    assert(cursor.pop() == nullptr);
    cursor.play();
    assert(cursor.state() == SmfPlaybackState::Playing);

    cursor.restart(SmfRestartOrigin::MusicStart);
    assert(cursor.tick() == 480);
    assert(cursor.peek() && cursor.peek()->kind == SmfEventKind::NoteOn);
    assert(cursor.peek()->data1 == 60);

    const SmfEvent* first = cursor.pop();
    const SmfEvent* second = cursor.pop();
    assert(first && second);
    assert(first->tick == 480 && second->tick == 480);
    assert(first->data1 == 60 && second->data1 == 64);

    cursor.seekTick(720);
    assert(cursor.tick() == 720);
    assert(cursor.peek() && cursor.peek()->kind == SmfEventKind::NoteOff);

    while (cursor.pop()) {}
    assert(cursor.peek() == nullptr);
    cursor.play();
    assert(cursor.state() == SmfPlaybackState::Playing);
    assert(cursor.tick() == doc.musicStartTick);
    assert(cursor.peek() && cursor.peek()->tick == 480);

    cursor.stop();
    assert(cursor.state() == SmfPlaybackState::Stopped);
    assert(cursor.tick() == 0);

    std::vector<uint8_t> smpte = bytes;
    smpte[12] = 0xE7;
    smpte[13] = 0x28;
    assert(SmfParser::parse(smpte.data(), smpte.size()).error ==
           SmfParseError::UnsupportedDivision);

    assert(SmfParser::parse(bytes.data(), bytes.size() - 3).error != SmfParseError::None);
    assert(std::strcmp(SmfParser::errorString(SmfParseError::None), "OK") == 0);
    return 0;
}
