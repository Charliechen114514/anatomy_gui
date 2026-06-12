#pragma once
/// @file application.h — Simple event loop for Stage 9 (Observable demo).

#include "geometry.h"
#include "raii.h"
#include <memory>
#include <vector>
#include <xcb/xcb.h>

namespace miniui {

class Widget;

class Application {
public:
  static Application &instance();
  void init(int w = 800, int h = 600);
  int run();
  void quit();

  xcb_connection_t *connection() const { return conn_; }
  xcb_window_t window() const { return win_; }
  xcb_visualtype_t *visual() const { return visual_; }

  void setRoot(std::shared_ptr<Widget> root);
  void repaintAll();

  /// Schedule a repaint for the given dirty region.
  void markDirty(Rect region);

private:
  Application() = default;
  xcb_connection_t *conn_ = nullptr;
  xcb_screen_t *screen_ = nullptr;
  xcb_visualtype_t *visual_ = nullptr;
  xcb_window_t win_ = 0;
  xcb_atom_t wmProtocols_ = XCB_ATOM_NONE;
  xcb_atom_t wmDeleteWin_ = XCB_ATOM_NONE;
  int w_ = 800, h_ = 600;
  bool running_ = false;
  std::shared_ptr<Widget> root_;
  std::vector<Rect> dirtyRegions_;
};

} // namespace miniui
