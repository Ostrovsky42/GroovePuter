#include <cassert>
#include "src/ui/display_framebuffer.h"

uint16_t* rejectAllocation(std::size_t) { return nullptr; }

int main() {
  DisplayFrameBuffer frame;
  assert(frame.empty());
  assert(frame.assign(240u * 135u, 0x1234));
  uint16_t* original = frame.data();
  assert(frame[0] == 0x1234 && frame[32399] == 0x1234);
  assert(frame.assign(32400, 0, rejectAllocation));
  assert(frame.data() == original && frame[32399] == 0);
  assert(!frame.assign(1, 0, rejectAllocation));
  assert(frame.empty() && frame.data() == nullptr);
  assert(frame.assign(32400, 0xabcd));
  assert(frame[32399] == 0xabcd);
  assert(frame.assign(0, 0));
  assert(frame.empty());
}
