#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "src/midi/smf_export_writer.h"

namespace {

class VectorSink final : public GroovePuterMidi::ISmfExportByteSink {
 public:
  bool write(const uint8_t* data, std::size_t length) override {
    bytes.insert(bytes.end(), data, data + length);
    return true;
  }

  std::vector<uint8_t> bytes;
};

class RejectingSink final : public GroovePuterMidi::ISmfExportByteSink {
 public:
  bool write(const uint8_t*, std::size_t) override { return false; }
};

void appendTrack(VectorSink& file, const VectorSink& track) {
  assert(GroovePuterMidi::writeSmfTrackHeader(file, track.bytes.size()));
  assert(file.write(track.bytes.data(), track.bytes.size()));
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc == 2);
  RejectingSink rejected;
  assert(!GroovePuterMidi::writeSmfHeader(rejected, 1, 1536));

  VectorSink invalidHeader;
  assert(!GroovePuterMidi::writeSmfHeader(invalidHeader, 0, 1536));
  assert(invalidHeader.bytes.empty());
  VectorSink file;
  assert(GroovePuterMidi::writeSmfHeader(file, 2, 1536));

  VectorSink conductorBytes;
  GroovePuterMidi::SmfExportTrackWriter conductor;
  assert(conductor.begin(conductorBytes, "Conductor"));
  assert(conductor.writeTempo(0, 500000));
  assert(conductor.writeTimeSignature(0, 4, 2, 24, 8));
  assert(conductor.finish(3072));
  appendTrack(file, conductorBytes);

  VectorSink synthBytes;
  GroovePuterMidi::SmfExportTrackWriter synth;
  assert(synth.begin(synthBytes, "Synth A"));
  assert(synth.writeChannelEvent(0, 0x90, 60, 100));
  assert(synth.writeChannelEvent(1536, 0x90, 60, 91));
  assert(synth.writeChannelEvent(2304, 0x80, 60, 0));
  assert(synth.writeChannelEvent(3072, 0x80, 60, 0));
  assert(synth.finish(3072));
  appendTrack(file, synthBytes);

  VectorSink invalidTrackBytes;
  GroovePuterMidi::SmfExportTrackWriter invalidTrack;
  assert(invalidTrack.begin(invalidTrackBytes, "Invalid"));
  assert(invalidTrack.writeChannelEvent(100, 0x90, 64, 80));
  assert(!invalidTrack.writeChannelEvent(99, 0x80, 64, 0));
  assert(!invalidTrack.good());

  RejectingSink rejectedEvent;
  GroovePuterMidi::SmfExportTrackWriter rejectedWriter;
  assert(!rejectedWriter.begin(rejectedEvent, "Rejected"));
  assert(!rejectedWriter.good());
  assert(!rejectedWriter.good());

  FILE* output = std::fopen(argv[1], "wb");
  assert(output != nullptr);
  assert(std::fwrite(file.bytes.data(), 1, file.bytes.size(), output) ==
         file.bytes.size());
  assert(std::fclose(output) == 0);
  return 0;
}
