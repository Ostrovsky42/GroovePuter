// Pure PERFORM helpers: the key line of the header and the EUCLID step test.
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/ui/perform_key_header.h"

using namespace PerformKeyHeader;

static void testEuclid() {
  // 5 of 16: exactly five hits for every rotation; 0 and full are all-hit.
  for (uint8_t rot = 0; rot < 16; ++rot) {
    int hits = 0;
    for (uint8_t s = 0; s < 16; ++s) hits += euclidStepHit(s, 5, 16, rot) ? 1 : 0;
    assert(hits == 5);
  }
  assert(euclidStepHit(0, 5, 16, 0));  // step 0 always opens an unrotated pattern
  assert(euclidStepHit(3, 0, 16, 0) && euclidStepHit(3, 16, 16, 0));
  // 3 of 8 unrotated: hits at 0, 3, 6.
  const bool expect[8] = {true, false, false, true, false, false, true, false};
  for (uint8_t s = 0; s < 8; ++s) assert(euclidStepHit(s, 3, 8, 0) == expect[s]);
  // rotation shifts the pattern.
  assert(euclidStepHit(0, 3, 8, 1) == expect[1]);
}

static void testHeader() {
  char out[64];
  KeyHeaderInput in;
  in.root = "C"; in.scale = "DORIAN"; in.octaveShift = 0;
  formatKeyHeader(out, sizeof(out), in, 38);
  assert(std::strcmp(out, "C DORIAN +0") == 0);

  in.chord = "SCALE3"; in.euclidPulses = 5; in.euclidLength = 16; in.octaveShift = -1;
  formatKeyHeader(out, sizeof(out), in, 38);
  assert(std::strcmp(out, "C DORIAN -1 [C:SCALE3] [E:5/16]") == 0);

  in.chord = "OFF"; in.arp = "UP"; in.ratchet = 3;
  formatKeyHeader(out, sizeof(out), in, 38);
  assert(std::strcmp(out, "C DORIAN -1 [A:UP] [R:3] [E:5/16]") == 0);

  // A badge that does not fit is left out whole, never cut.
  in.root = "C#"; in.scale = "NAT MINOR"; in.strumMs = 30;
  const std::size_t n = formatKeyHeader(out, sizeof(out), in, 38);
  assert(n <= 38);
  assert(std::strcmp(out, "C# NAT MINOR -1 [A:UP] [R:3] [E:5/16]") == 0);  // [S:30] did not fit
  char tiny[8];
  formatKeyHeader(tiny, sizeof(tiny), in, 38);
  assert(std::strlen(tiny) < sizeof(tiny));
}

int main() {
  testEuclid();
  testHeader();
  std::puts("test_perform_key_header: OK");
  return 0;
}
