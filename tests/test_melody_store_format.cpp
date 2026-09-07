// M2a: the melody payload format.
//
// A melody has to survive a reboot, so it needs a file. The one thing this
// format must not be is a raw dump of RuntimeSynthEventBuffer: that would make
// a C++ struct layout the on-disk ABI, and the next time anyone adds a field
// or the compiler pads differently, every saved melody becomes garbage that
// still passes a size check.
//
// So fields are written one at a time, little-endian, with a header carrying
// magic, version, counts and an integrity check. Everything below is about the
// cases where a file is *not* trustworthy, because those are the ones that
// decide whether a bad read can damage the melody already in memory.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "src/state/melody_store.h"

namespace {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "M2a FAIL: %s\n", message);
  ++g_failures;
}

Buffer makeMelody() {
  Buffer melody{};
  melody.lengthTicks = 2 * PhraseRuntime::kTicksPerBar;
  melody.count = 3;
  melody.events[0] = {0, 96 * 16, 60, 100, 100, PhraseRuntime::kEventAccent, 0, 0};
  melody.events[1] = {100, 12 * 16, 67, 90, 80, PhraseRuntime::kEventSlide, 2, 7};
  melody.events[2] = {480, 24 * 16, 72, 127, 100, 0, 0, 0};
  return melody;
}

bool sameMelody(const Buffer& a, const Buffer& b) {
  if (a.count != b.count || a.lengthTicks != b.lengthTicks) return false;
  for (uint16_t i = 0; i < a.count; ++i) {
    const auto& x = a.events[i];
    const auto& y = b.events[i];
    if (x.startTick != y.startTick || x.durationSubticks != y.durationSubticks ||
        x.note != y.note || x.velocity != y.velocity ||
        x.probability != y.probability || x.flags != y.flags ||
        x.fx != y.fx || x.fxParam != y.fxParam) {
      return false;
    }
  }
  return true;
}

}  // namespace

int main() {
  // 1. A melody survives the round trip exactly, including the fields nobody
  //    looks at yet -- fx and probability travel or they are lost forever.
  {
    const Buffer original = makeMelody();
    std::vector<uint8_t> blob;
    expect(MelodyStore::encode(original, blob), "encode refused a valid melody");

    Buffer restored{};
    expect(MelodyStore::decode(blob.data(), blob.size(), restored),
           "decode refused what encode produced");
    expect(sameMelody(original, restored), "the melody did not survive intact");
  }

  // 2. The file is not the struct. Its size is header plus ten bytes per
  //    event, which is smaller than the fixed-capacity buffer and independent
  //    of how the compiler lays that buffer out.
  {
    const Buffer original = makeMelody();
    std::vector<uint8_t> blob;
    (void)MelodyStore::encode(original, blob);
    expect(blob.size() == MelodyStore::kHeaderBytes +
                          3u * MelodyStore::kEventBytes,
           "the encoded size is not header + events");
    expect(blob.size() != sizeof(Buffer),
           "the payload is the size of the struct, which would make the "
           "C++ layout the on-disk ABI");
  }

  // 3. An empty melody is a legitimate thing to store: a slot can be promoted
  //    and then emptied, and that state must persist rather than reverting.
  {
    Buffer empty{};
    empty.lengthTicks = PhraseRuntime::kTicksPerBar;
    std::vector<uint8_t> blob;
    expect(MelodyStore::encode(empty, blob), "encode refused an empty melody");
    Buffer restored = makeMelody();
    expect(MelodyStore::decode(blob.data(), blob.size(), restored),
           "decode refused an empty melody");
    expect(restored.count == 0, "an empty melody came back with events");
  }

  // 4. A corrupted payload is refused, and the target is left alone. A half
  //    written melody replacing a good one in memory is the worst outcome
  //    available here.
  {
    const Buffer original = makeMelody();
    std::vector<uint8_t> blob;
    (void)MelodyStore::encode(original, blob);
    blob[MelodyStore::kHeaderBytes + 4] ^= 0xFFu;

    Buffer target = makeMelody();
    target.events[0].note = 42;
    const Buffer before = target;
    expect(!MelodyStore::decode(blob.data(), blob.size(), target),
           "a corrupted payload was accepted");
    expect(sameMelody(before, target),
           "a rejected decode still modified the target");
  }

  // 5. Wrong magic is refused. Reading some other file as a melody must not
  //    depend on its bytes happening to look plausible.
  {
    const Buffer original = makeMelody();
    std::vector<uint8_t> blob;
    (void)MelodyStore::encode(original, blob);
    blob[0] = 'X';
    Buffer target{};
    expect(!MelodyStore::decode(blob.data(), blob.size(), target),
           "a file with the wrong magic was accepted");
  }

  // 6. An unknown version is refused rather than guessed at. When a version 2
  //    exists, this is the check that gets a migration instead of silence.
  {
    const Buffer original = makeMelody();
    std::vector<uint8_t> blob;
    (void)MelodyStore::encode(original, blob);
    blob[4] = 99;
    Buffer target{};
    expect(!MelodyStore::decode(blob.data(), blob.size(), target),
           "an unknown format version was accepted");
  }

  // 7. Truncation at every length is refused. Power loss during a write
  //    produces exactly this, and it must not read as a shorter melody.
  {
    const Buffer original = makeMelody();
    std::vector<uint8_t> blob;
    (void)MelodyStore::encode(original, blob);
    bool anyAccepted = false;
    for (size_t cut = 0; cut < blob.size(); ++cut) {
      Buffer target{};
      if (MelodyStore::decode(blob.data(), cut, target)) anyAccepted = true;
    }
    expect(!anyAccepted, "a truncated payload was accepted at some length");
  }

  // 8. A header claiming more events than the buffer can hold is refused
  //    before anything is copied.
  {
    const Buffer original = makeMelody();
    std::vector<uint8_t> blob;
    (void)MelodyStore::encode(original, blob);
    blob[8] = 0xFF;
    blob[9] = 0xFF;
    Buffer target{};
    expect(!MelodyStore::decode(blob.data(), blob.size(), target),
           "an impossible event count was accepted");
  }

  // 9. A length that is not a legal phrase length is refused: the runtime
  //    divides by it, and a zero there is undefined behaviour in the audio
  //    path.
  {
    Buffer bad = makeMelody();
    bad.lengthTicks = 999;
    std::vector<uint8_t> blob;
    expect(!MelodyStore::encode(bad, blob),
           "encode accepted an illegal phrase length");
  }

  if (g_failures == 0) {
    std::printf("M2a melody store format: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M2a melody store format: %d failure(s)\n", g_failures);
  return 1;
}
