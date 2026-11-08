#ifndef GROOVEPUTER_SRC_STATE_REUSE_MARKS_H
#define GROOVEPUTER_SRC_STATE_REUSE_MARKS_H

#include <cstdint>
#include <type_traits>

#include "../../scenes.h"

// PML-C: session-only "allow replacement" marks for resident Pattern slots of ONE page.
//
// A mark is permission for a FUTURE generation to replace the slot's content; it does not free,
// erase or promise anything. It is (slot, content token, MaterialId): the generation re-checks
// all three and every holder before using it. Never persisted; dropped on scene load, project
// change and page change. Bounded by the page size, no heap.
namespace GroovePuterMaterial {

struct ReuseMarks {
  uint64_t token[kPatternsPerPage]{};
  uint32_t id[kPatternsPerPage]{};
  uint16_t mask = 0;
  int8_t page = -1;

  static_assert(kPatternsPerPage <= 16, "mask is 16 bits");

  bool marked(int slot) const {
    return slot >= 0 && slot < kPatternsPerPage && (mask & (1u << slot)) != 0;
  }
  void set(int slot, int pageIndex, uint64_t contentToken, uint32_t materialId) {
    if (slot < 0 || slot >= kPatternsPerPage) return;
    if (page != pageIndex) clear();
    page = static_cast<int8_t>(pageIndex);
    token[slot] = contentToken;
    id[slot] = materialId;
    mask = static_cast<uint16_t>(mask | (1u << slot));
  }
  void revoke(int slot) {
    if (slot < 0 || slot >= kPatternsPerPage) return;
    mask = static_cast<uint16_t>(mask & ~(1u << slot));
    token[slot] = 0;
    id[slot] = 0;
  }
  void clear() { *this = ReuseMarks{}; }
  int count() const {
    int n = 0;
    for (int i = 0; i < kPatternsPerPage; ++i) n += marked(i) ? 1 : 0;
    return n;
  }
};

// Session-only record of the slots THIS session's generator wrote, with the content token the
// generator left there. A slot whose token still matches is "generated here and not edited since":
// the only class the one-screen MAKE ROOM action may offer. Older or hand-made material never
// appears here (it can still be chosen slot by slot). Dropped with the marks.
struct GeneratedLedger {
  uint64_t token[kPatternsPerPage]{};
  uint16_t mask = 0;
  int8_t page = -1;

  bool has(int slot) const {
    return slot >= 0 && slot < kPatternsPerPage && (mask & (1u << slot)) != 0;
  }
  void set(int slot, int pageIndex, uint64_t contentToken) {
    if (slot < 0 || slot >= kPatternsPerPage) return;
    if (page != pageIndex) clear();
    page = static_cast<int8_t>(pageIndex);
    token[slot] = contentToken;
    mask = static_cast<uint16_t>(mask | (1u << slot));
  }
  void clear() { *this = GeneratedLedger{}; }
};

static_assert(std::is_trivially_copyable<GeneratedLedger>::value, "ledger must stay fixed value state");
static_assert(sizeof(GeneratedLedger) <= 144, "ledger is budgeted against the DRAM headroom");

static_assert(std::is_trivially_copyable<ReuseMarks>::value, "reuse marks must stay fixed value state");
static_assert(sizeof(ReuseMarks) <= 208, "reuse marks are budgeted against the DRAM headroom");

}  // namespace GroovePuterMaterial

#endif  // GROOVEPUTER_SRC_STATE_REUSE_MARKS_H
