#pragma once
#include <cstdint>
#include <algorithm>

struct ScrollListState {
  int selected = 0;
  int scroll = 0;
};

class ScrollList {
public:
  ScrollList(int itemCount, int rowH)
    : itemCount_(itemCount), rowH_(rowH) {}

  void setItemCount(int c) { itemCount_ = std::max(0, c); clamp(); }
  int itemCount() const { return itemCount_; }
  int rowHeight() const { return rowH_; }

  // number of rows visible inside bounds.h
  int visibleRows(int heightPx) const { 
      if (rowH_ <= 0) return 0;
      return std::max(1, heightPx / rowH_); 
  }

  // call on draw to keep selection visible
  void ensureVisible(int heightPx) {
    int vis = visibleRows(heightPx);
    int maxScroll = std::max(0, itemCount_ - vis);

    if (state_.selected < state_.scroll) state_.scroll = state_.selected;
    if (state_.selected >= state_.scroll + vis) state_.scroll = state_.selected - vis + 1;

    state_.scroll = std::clamp(state_.scroll, 0, maxScroll);
  }

  // move selection by delta, optionally wrap
  void move(int delta, bool wrap) {
    if (itemCount_ <= 0) return;
    int s = state_.selected + delta;
    if (wrap) {
      // Correct modulo for negative numbers
      s = (s % itemCount_ + itemCount_) % itemCount_;
    } else {
      s = std::clamp(s, 0, itemCount_ - 1);
    }
    state_.selected = s;
  }

  // jump by pages
  void page(int dir, int heightPx, bool wrap) {
    int vis = visibleRows(heightPx);
    // Page jump usually means jumping by visible count
    move(dir * vis, wrap);
  }

  int selected() const { return state_.selected; }
  int scroll() const { return state_.scroll; }
  void setSelected(int idx) { state_.selected = idx; clamp(); }

private:
  void clamp() {
    if (itemCount_ <= 0) { state_.selected = 0; state_.scroll = 0; return; }
    state_.selected = std::clamp(state_.selected, 0, itemCount_ - 1);
    state_.scroll = std::clamp(state_.scroll, 0, itemCount_ - 1);
  }

  int itemCount_;
  int rowH_;
  ScrollListState state_;
};
