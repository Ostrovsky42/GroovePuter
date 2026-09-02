#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct DirtyTileRun {
  int x = 0;
  int y = 0;
  int w = 0;
  int h = 0;
};

struct DirtyTileScanResult {
  int dirty_tiles = 0;
  int total_tiles = 0;
  int run_count = 0;
  bool full_refresh = false;
};

class DirtyTileTracker {
public:
  static constexpr int kTileWidth = 16;
  static constexpr int kTileHeight = 16;
  // Cardputer ADV is rendered at 240x135. Other dimensions safely fall back
  // to a full display flush instead of reserving unreachable tile storage.
  static constexpr int kMaxScreenWidth = 240;
  static constexpr int kMaxScreenHeight = 135;
  static constexpr int kMaxColumns =
      (kMaxScreenWidth + kTileWidth - 1) / kTileWidth;
  static constexpr int kMaxRows =
      (kMaxScreenHeight + kTileHeight - 1) / kTileHeight;
  static constexpr int kMaxTiles = kMaxColumns * kMaxRows;
  static constexpr int kFullRefreshDirtyPercent = 50;
  static constexpr int kMaxPartialRuns = 18;

  bool reset(int width, int height) {
    width_ = width;
    height_ = height;
    columns_ = width > 0 ? (width + kTileWidth - 1) / kTileWidth : 0;
    rows_ = height > 0 ? (height + kTileHeight - 1) / kTileHeight : 0;
    valid_ = width > 0 && height > 0 && width <= kMaxScreenWidth &&
             height <= kMaxScreenHeight && columns_ <= kMaxColumns &&
             rows_ <= kMaxRows;
    initialized_ = false;
    previous_hashes_.fill(0);
    dirty_.fill(0);
    return valid_;
  }

  void forceFullRefresh() { initialized_ = false; }

  bool valid() const { return valid_; }

  template <typename EmitRun>
  DirtyTileScanResult scan(const uint16_t* pixels, int stride, EmitRun emit_run) {
    DirtyTileScanResult result{};
    if (!valid_ || pixels == nullptr || stride < width_) return result;

    result.total_tiles = columns_ * rows_;
    for (int row = 0; row < rows_; ++row) {
      for (int column = 0; column < columns_; ++column) {
        const int index = row * columns_ + column;
        const uint32_t hash = hashTile_(pixels, stride, column, row);
        const bool changed = !initialized_ || previous_hashes_[index] != hash;
        previous_hashes_[index] = hash;
        dirty_[index] = changed ? 1u : 0u;
        if (changed) ++result.dirty_tiles;
      }
    }

    if (!initialized_) {
      initialized_ = true;
      result.run_count = 1;
      result.full_refresh = true;
      emit_run(DirtyTileRun{0, 0, width_, height_});
      return result;
    }

    if (result.dirty_tiles == 0) return result;

    result.run_count = countRuns_();
    const bool tooMuchArea =
        result.dirty_tiles * 100 >= result.total_tiles * kFullRefreshDirtyPercent;
    const bool tooFragmented = result.run_count > kMaxPartialRuns;
    if (tooMuchArea || tooFragmented) {
      result.run_count = 1;
      result.full_refresh = true;
      emit_run(DirtyTileRun{0, 0, width_, height_});
      return result;
    }

    emitRuns_(emit_run);
    return result;
  }

private:
  static uint32_t fnvMix_(uint32_t hash, uint16_t value) {
    hash ^= static_cast<uint8_t>(value & 0xFFu);
    hash *= 16777619u;
    hash ^= static_cast<uint8_t>(value >> 8);
    hash *= 16777619u;
    return hash;
  }

  uint32_t hashTile_(const uint16_t* pixels,
                     int stride,
                     int column,
                     int row) const {
    const int x0 = column * kTileWidth;
    const int y0 = row * kTileHeight;
    const int x1 = x0 + kTileWidth < width_ ? x0 + kTileWidth : width_;
    const int y1 = y0 + kTileHeight < height_ ? y0 + kTileHeight : height_;
    uint32_t hash = 2166136261u;
    for (int y = y0; y < y1; ++y) {
      const uint16_t* line = pixels + y * stride + x0;
      for (int x = x0; x < x1; ++x) {
        hash = fnvMix_(hash, *line++);
      }
    }
    return hash;
  }

  int countRuns_() const {
    int runs = 0;
    for (int row = 0; row < rows_; ++row) {
      int column = 0;
      while (column < columns_) {
        while (column < columns_ && dirty_[row * columns_ + column] == 0u) {
          ++column;
        }
        if (column >= columns_) break;
        ++runs;
        while (column < columns_ && dirty_[row * columns_ + column] != 0u) {
          ++column;
        }
      }
    }
    return runs;
  }

  template <typename EmitRun>
  void emitRuns_(EmitRun emit_run) const {
    for (int row = 0; row < rows_; ++row) {
      int column = 0;
      while (column < columns_) {
        while (column < columns_ && dirty_[row * columns_ + column] == 0u) {
          ++column;
        }
        if (column >= columns_) break;
        const int first = column;
        while (column < columns_ && dirty_[row * columns_ + column] != 0u) {
          ++column;
        }
        const int x = first * kTileWidth;
        const int y = row * kTileHeight;
        const int right = column * kTileWidth < width_ ? column * kTileWidth : width_;
        const int bottom = y + kTileHeight < height_ ? y + kTileHeight : height_;
        emit_run(DirtyTileRun{x, y, right - x, bottom - y});
      }
    }
  }

  int width_ = 0;
  int height_ = 0;
  int columns_ = 0;
  int rows_ = 0;
  bool valid_ = false;
  bool initialized_ = false;
  std::array<uint32_t, kMaxTiles> previous_hashes_{};
  std::array<uint8_t, kMaxTiles> dirty_{};
};

static_assert(sizeof(DirtyTileTracker) <= 704,
              "Cardputer dirty tracking must stay below 704 bytes");
