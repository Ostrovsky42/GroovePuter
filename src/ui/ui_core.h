#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "../dsp/miniacid_engine.h"
#include "display.h"

enum KeyScanCode {
  MINIACID_NO_SCANCODE = 0,
  MINIACID_DOWN,
  MINIACID_UP,
  MINIACID_LEFT,
  MINIACID_RIGHT,
  MINIACID_ESCAPE,
  MINIACID_TAB,
  MINIACID_F1,
  MINIACID_F2,
  MINIACID_F3,
  MINIACID_F4,
  MINIACID_F5,
  MINIACID_F6,
  MINIACID_F7,
  MINIACID_F8,
};

enum EventType {
  MINIACID_NO_TYPE = 0,
  MINIACID_KEY_DOWN,
  MINIACID_MOUSE_MOVE,
  MINIACID_MOUSE_DOWN,
  MINIACID_MOUSE_UP,
  MINIACID_MOUSE_DRAG,
  MINIACID_MOUSE_SCROLL,
  MINIACID_APPLICATION_EVENT,
};

enum ApplicationEventType {
  MINIACID_APP_EVENT_NONE = 0,
  MINIACID_APP_EVENT_COPY,
  MINIACID_APP_EVENT_PASTE,
  MINIACID_APP_EVENT_CUT,
  MINIACID_APP_EVENT_UNDO,

  MINIACID_APP_EVENT_TOGGLE_SONG_MODE,
  MINIACID_APP_EVENT_SAVE_SCENE,
  
  MINIACID_APP_EVENT_START_RECORDING,
  MINIACID_APP_EVENT_STOP_RECORDING,

  MINIACID_APP_EVENT_MULTIPAGE_DOWN,
  MINIACID_APP_EVENT_MULTIPAGE_UP,

  MINIACID_APP_EVENT_TOGGLE_MODE,
  MINIACID_APP_EVENT_OPEN_GENRE,
  MINIACID_APP_EVENT_SET_VISUAL_STYLE,
};

enum class VisualStyle { MINIMAL, RETRO_CLASSIC };

enum MouseButtonType {
  MOUSE_BUTTON_NONE = 0,
  MOUSE_BUTTON_LEFT,
  MOUSE_BUTTON_MIDDLE,
  MOUSE_BUTTON_RIGHT,
};

struct UIEvent {
  EventType event_type = MINIACID_NO_TYPE;
  KeyScanCode scancode = MINIACID_NO_SCANCODE;
  ApplicationEventType app_event_type = MINIACID_APP_EVENT_NONE;
  char key = 0;
  bool alt = false;
  bool ctrl = false;
  bool shift = false;
  bool meta = false;
  int x = 0;
  int y = 0;
  int dx = 0;
  int dy = 0;
  int wheel_dx = 0;
  int wheel_dy = 0;
  MouseButtonType button = MOUSE_BUTTON_NONE;
};

class Point {
  public:
    int x = 0;
    int y = 0;
};

class Rect {
  public:
    Rect() = default;
    Rect(int x_, int y_, int w_, int h_) : x(x_), y(y_), w(w_), h(h_) {}
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
  
    bool contains(Point p) const {
      return p.x >= x && p.x < x + w && p.y >= y && p.y < y + h;
    }
};
class Frame {
  public:
    virtual ~Frame() = default;
    virtual const Rect& getBoundaries() const  { return boundaries_; }
    virtual void setBoundaries(const Rect& rect) { boundaries_ = rect; }
    int dx() const { return getBoundaries().x; }
    int dy() const { return getBoundaries().y; }
    int width() const { return getBoundaries().w; }
    int height() const { return getBoundaries().h; }
    bool contains(int x, int y) const { return boundaries_.contains(Point{x, y}); }

    virtual void draw(IGfx& gfx) = 0;
  private:
    Rect boundaries_;
};

class EventHandler {
  public:
    virtual bool handleEvent(UIEvent& ui_event) = 0;
    virtual ~EventHandler() = default;
};

class Component : public Frame, public EventHandler {
 public:
  bool handleEvent(UIEvent& ui_event) override { (void)ui_event; return false; }
  virtual bool isFocusable() const { return false; }
  virtual bool isFocused() const { return false; }
  virtual void setFocused(bool focused) { (void)focused; }
  virtual ~Component() = default;
};

class FocusableComponent : public Component {
 public:
  bool isFocusable() const override { return focusable_; }
  void setFocusable(bool focusable) { focusable_ = focusable; }
  bool isFocused() const override { return focused_; }
  void setFocused(bool focused) override { focused_ = focused; }

 private:
  bool focusable_ = true;
  bool focused_ = false;
};

class Container : public EventHandler, public Frame {
 public:
  void addChild(std::shared_ptr<Component> child) {
    children_.push_back(child);
    if (focus_index_ < 0) {
      if (child->isFocusable()) {
        focus_index_ = static_cast<int>(children_.size()) - 1;
        child->setFocused(true);
      }
    }
  }

  const std::vector<std::shared_ptr<Component>>& getChildren() const {
    return children_;
  }

  void focusNext() { moveFocus(1); }
  void focusPrev() { moveFocus(-1); }

  Component* focusedChild() const {
    if (focus_index_ < 0 ||
        focus_index_ >= static_cast<int>(children_.size())) {
      return nullptr;
    }
    return children_[focus_index_].get();
  }

  // EventHandler methods
  bool handleEvent(UIEvent& ui_event) override {
    if (isMouseEvent(ui_event.event_type)) {
      return handleMouseEvent(ui_event);
    }

    Component* focused = focusedChild();
    if (focused) {
      if (focused->handleEvent(ui_event)) {
        return true;
      }
    }
    for (auto& child : children_) {
      if (child.get() == focused) {
        continue;
      }
      if (child->handleEvent(ui_event)) {
        return true;
      }
    }
    return false;
  }

  // Frame methods
  void draw(IGfx& gfx) override {
    for (auto& child : children_) {
      child->draw(gfx);
    }
  }

 private:
  static bool isMouseEvent(EventType type) {
    switch (type) {
      case MINIACID_MOUSE_MOVE:
      case MINIACID_MOUSE_DOWN:
      case MINIACID_MOUSE_UP:
      case MINIACID_MOUSE_DRAG:
      case MINIACID_MOUSE_SCROLL:
        return true;
      default:
        return false;
    }
  }

  Component* childAt(int x, int y) {
    for (int i = static_cast<int>(children_.size()) - 1; i >= 0; --i) {
      if (children_[i]->contains(x, y)) {
        return children_[i].get();
      }
    }
    return nullptr;
  }

  bool handleMouseEvent(UIEvent& ui_event) {
    if (ui_event.event_type == MINIACID_MOUSE_DOWN) {
      Component* target = childAt(ui_event.x, ui_event.y);
      if (target && target->isFocusable()) {
        setFocusIndex(indexOf(target));
      }
      if (target && target->handleEvent(ui_event)) {
        mouse_capture_ = target;
        return true;
      }
      mouse_capture_ = nullptr;
      return target != nullptr;
    }

    if (ui_event.event_type == MINIACID_MOUSE_UP) {
      Component* target = mouse_capture_ ? mouse_capture_ : childAt(ui_event.x, ui_event.y);
      bool handled = target ? target->handleEvent(ui_event) : false;
      mouse_capture_ = nullptr;
      return handled;
    }

    if (ui_event.event_type == MINIACID_MOUSE_DRAG) {
      Component* target = mouse_capture_ ? mouse_capture_ : childAt(ui_event.x, ui_event.y);
      if (target) {
        if (target->handleEvent(ui_event)) {
          return true;
        }
      }
      return mouse_capture_ != nullptr;
    }

    if (ui_event.event_type == MINIACID_MOUSE_MOVE || ui_event.event_type == MINIACID_MOUSE_SCROLL) {
      Component* target = childAt(ui_event.x, ui_event.y);
      if (target && target->handleEvent(ui_event)) {
        return true;
      }
      return false;
    }

    return false;
  }

  int indexOf(Component* target) const {
    for (int i = 0; i < static_cast<int>(children_.size()); ++i) {
      if (children_[i].get() == target) {
        return i;
      }
    }
    return -1;
  }

  void setFocusIndex(int index) {
    if (index == focus_index_) {
      return;
    }
    if (Component* prev = focusedChild()) {
      prev->setFocused(false);
    }
    focus_index_ = index;
    if (Component* next = focusedChild()) {
      next->setFocused(true);
    }
  }

  void moveFocus(int delta) {
    if (children_.empty()) {
      return;
    }
    int index = focus_index_;
    int count = static_cast<int>(children_.size());
    for (int i = 0; i < count; ++i) {
      index += delta;
      if (index < 0) {
        index = count - 1;
      } else if (index >= count) {
        index = 0;
      }
      if (children_[index]->isFocusable()) {
        setFocusIndex(index);
        return;
      }
    }
  }

  std::vector<std::shared_ptr<Component>> children_;
  int focus_index_ = -1;
  Component* mouse_capture_ = nullptr;
};

class MultiPageHelpDialog;
class IPage : public Container {
 public:
  virtual const std::string& getTitle() const = 0;
  virtual void setVisualStyle(VisualStyle style) { (void)style; }
  // Help dialog factory, return nullptr when the page does not provide help.
  virtual std::unique_ptr<MultiPageHelpDialog> getHelpDialog();

  virtual ~IPage() = default;

  // EventHandler methods
  virtual bool handleEvent(UIEvent& ui_event) = 0;
  // Frame methods
  virtual void draw(IGfx& gfx) = 0;

  // Request transition to another page
  bool hasPageRequest() const { return requestedPage_ >= 0; }
  int getRequestedPage() const { return requestedPage_; }
  int getRequestedContext() const { return requestedContext_; }
  void clearPageRequest() { requestedPage_ = -1; requestedContext_ = -1; }

  // Receive context when being navigated TO
  virtual void setContext(int context) { (void)context; }

 protected:
  void requestPageTransition(int pageIndex, int context = 0) {
      requestedPage_ = pageIndex;
      requestedContext_ = context;
  }

 private:
  int requestedPage_ = -1;
  int requestedContext_ = -1;
};

class MultiPage : public IPage {
 public:
  void addPage(std::shared_ptr<Container> page) {
    pages_.push_back(page);
    if (active_index_ < 0) {
      active_index_ = 0;
    }
  }

  int pageCount() const { return static_cast<int>(pages_.size()); }
  int activePageIndex() const { return active_index_; }

  bool handleEvent(UIEvent& ui_event) override {
    if (ui_event.event_type == MINIACID_APPLICATION_EVENT) {
      switch (ui_event.app_event_type) {
        case MINIACID_APP_EVENT_MULTIPAGE_DOWN:
          return stepActivePage(1);
        case MINIACID_APP_EVENT_MULTIPAGE_UP:
          return stepActivePage(-1);
        default:
          break;
      }
    }
    Container* active = activePage();
    if (!active) return false;
    active->setBoundaries(getBoundaries());
    return active->handleEvent(ui_event);
  }

  void draw(IGfx& gfx) override {
    Container* active = activePage();
    if (!active) return;
    active->setBoundaries(getBoundaries());
    active->draw(gfx);
  }

 protected:
  bool setActivePageIndex(int index) {
    int count = static_cast<int>(pages_.size());
    if (count <= 0) return false;
    if (index < 0) index = 0;
    if (index >= count) index = count - 1;
    if (index == active_index_) return true;
    active_index_ = index;
    return true;
  }

  bool stepActivePage(int delta) {
    int count = static_cast<int>(pages_.size());
    if (count <= 0) return false;
    if (count == 1) return true;
    int next = active_index_;
    if (next < 0) next = 0;
    next = (next + delta) % count;
    if (next < 0) next += count;
    active_index_ = next;
    return true;
  }

  Container* activePage() const {
    if (active_index_ < 0 ||
        active_index_ >= static_cast<int>(pages_.size())) {
      return nullptr;
    }
    return pages_[active_index_].get();
  }

  std::shared_ptr<Container> getPagePtr(int index) const {
      if (index < 0 || index >= static_cast<int>(pages_.size())) return nullptr;
      return pages_[index];
  }

 private:
  std::vector<std::shared_ptr<Container>> pages_;
  int active_index_ = -1;
};

typedef void (*AudioGuardLockFn)(void*);

struct AudioGuard {
  AudioGuardLockFn lock = nullptr;
  AudioGuardLockFn unlock = nullptr;
  void* context = nullptr;

  template <typename F>
  void operator()(F&& fn) const {
    if (lock) lock(context);
    fn();
    if (unlock) unlock(context);
  }

  // Only consider valid if BOTH lock and unlock are present (prevents deadlock)
  explicit operator bool() const { return lock != nullptr && unlock != nullptr; }
};
