// H2: the idiom variant of Acid, Techno (dub) and UK garage alternates with the G press
// (Request::press), not with the salt. Neighbouring presses differ; the choice does not
// depend on the salt, so one press always gets the same variant.
#include <cassert>
#include <cstdio>

#include "src/generation/idiom/genre_idiom.h"

namespace {

const GenreIdiom::IdiomVariant* pick(uint8_t mode, uint32_t press, uint32_t salt) {
  return GenreIdiom::variantFor(mode, 0, salt, press);
}

void testNeighbouringPressesDiffer() {
  const uint8_t modes[] = {0, 10};  // Acid, Techno: by G press (UK garage is salt, see below)
  for (uint8_t mode : modes) {
    for (uint32_t press = 0; press < 16; ++press) {
      const auto* now = pick(mode, press, 12345u);
      const auto* next = pick(mode, press + 1, 12345u);
      assert(now != nullptr && next != nullptr);
      assert(now != next);
      // Same press, any salt: the same variant.
      for (uint32_t salt : {0u, 1u, 977u, 0xFFFFFFFFu})
        assert(pick(mode, press, salt) == now);
    }
  }
  // UK garage keeps the salt choice: the press does not change it, the salt does.
  for (uint32_t press = 0; press < 8; ++press) {
    const auto* a = pick(13, press, 0u);
    const auto* b = pick(13, press, 1u);
    assert(a != nullptr && b != nullptr && a != b);
    assert(pick(13, press, 0u) == pick(13, press + 1, 0u));
  }
  std::printf("press alternation: Acid, Techno neighbours differ; UK garage follows the salt\n");
}

}  // namespace

int main() {
  testNeighbouringPressesDiffer();
  std::printf("genre press alternation: OK\n");
  return 0;
}
