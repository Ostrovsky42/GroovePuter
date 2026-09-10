#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "../scenes.h"
#include "../src/state/song_cell.h"
#include "../src/state/song_edit.h"

int main() {
  using GroovePuterSong::SongCell;

  static_assert(sizeof(SongCell) == sizeof(int16_t),
                "SongCell must stay exactly two bytes");
  static_assert(sizeof(SongPosition) == 8,
                "SongPosition must stay exactly eight bytes");
  static_assert(
      std::is_same<
          std::remove_reference<
              decltype(std::declval<SongPosition&>().patterns[0])>::type,
          SongCell>::value,
      "SongPosition must store SongCell values");

  const SongCell empty = SongCell::empty();
  assert(empty.isEmpty());
  assert(!empty.isRest());
  assert(!empty.hasMaterial());
  assert(empty.patternRef() == -1);

  const SongCell rest = SongCell::rest();
  assert(!rest.isEmpty());
  assert(rest.isRest());
  assert(!rest.hasMaterial());
  assert(rest.patternRef() == -1);

  const SongCell p0 = SongCell::material(0);
  const SongCell p255 = SongCell::material(255);
  assert(p0.hasMaterial());
  assert(p0.patternRef() == 0);
  assert(p255.hasMaterial());
  assert(p255.patternRef() == 255);

  assert(SongCell::fromRaw(-1).isEmpty());
  assert(SongCell::fromRaw(-2).isRest());
  assert(SongCell::fromRaw(12).hasMaterial());
  assert(SongCell::fromRaw(12).patternRef() == 12);
  assert(SongCell::fromRaw(9999).isEmpty());

  Song edited{};
  edited.length = 3;
  edited.positions[0].patterns[0] = SongCell::material(4);
  edited.positions[1].patterns[0] = SongCell::rest();
  edited.positions[2].patterns[0] = SongCell::empty();

  GroovePuterUndo::SongEdit::insertRow(edited, 1);
  assert(edited.length == 4);
  assert(edited.positions[1].patterns[0].isEmpty());
  assert(edited.positions[2].patterns[0].isRest());

  GroovePuterUndo::SongEdit::deleteRow(edited, 1);
  assert(edited.length == 3);
  assert(edited.positions[1].patterns[0].isRest());

  Song active{};
  active.length = 1;
  active.positions[0].patterns[0] = SongCell::rest();
  active.positions[0].patterns[1] = SongCell::empty();

  Song fallback{};
  fallback.length = 1;
  fallback.positions[0].patterns[0] = SongCell::material(7);
  fallback.positions[0].patterns[1] = SongCell::material(9);

  GroovePuterUndo::SongEdit::mergeFrom(active, fallback);
  assert(active.positions[0].patterns[0].isRest());
  assert(active.positions[0].patterns[1].hasMaterial());
  assert(active.positions[0].patterns[1].patternRef() == 9);

  Song trimmed{};
  trimmed.length = 3;
  trimmed.positions[0].patterns[0] = SongCell::material(2);
  trimmed.positions[1].patterns[0] = SongCell::empty();
  trimmed.positions[2].patterns[0] = SongCell::rest();
  assert(GroovePuterUndo::SongEdit::trimmedLength(trimmed) == 3);

  trimmed.positions[2].patterns[0] = SongCell::empty();
  assert(GroovePuterUndo::SongEdit::trimmedLength(trimmed) == 1);

  return 0;
}
