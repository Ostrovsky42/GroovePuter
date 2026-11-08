#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>

// Setup/control-side storage. Allocation failure is explicit, never an
// uncaught vector exception. Reinitializing the same dimensions reuses RAM.
class DisplayFrameBuffer {
 public:
  using Allocator = uint16_t* (*)(std::size_t);
  bool assign(std::size_t count, uint16_t color,
              Allocator allocate = allocateDefault) {
    if (count != count_) {
      pixels_.reset();
      count_ = 0;
      if (count > std::numeric_limits<std::size_t>::max() / sizeof(uint16_t))
        return false;
      if (count != 0) {
        pixels_.reset(allocate(count));
        if (!pixels_) return false;
      }
      count_ = count;
    }
    if (count_) std::fill(begin(), end(), color);
    return true;
  }
  bool empty() const { return count_ == 0; }
  std::size_t size() const { return count_; }
  uint16_t* data() { return pixels_.get(); }
  const uint16_t* data() const { return pixels_.get(); }
  uint16_t* begin() { return data(); }
  uint16_t* end() { return count_ ? data() + count_ : data(); }
  uint16_t& operator[](std::size_t index) { return pixels_[index]; }

 private:
  static uint16_t* allocateDefault(std::size_t count) {
    return new (std::nothrow) uint16_t[count];
  }
  std::unique_ptr<uint16_t[]> pixels_;
  std::size_t count_ = 0;
};
