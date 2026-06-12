// Minimal demo: Cairo + XCB drawing

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <cstdio>
#include <cstdlib>
#include <xcb/xcb.h>

namespace {

xcb_screen_t *default_screen(xcb_connection_t *conn) {
  return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}

xcb_visualtype_t *find_visual(xcb_screen_t *screen) {
  xcb_depth_iterator_t depth_it = xcb_screen_allowed_depths_iterator(screen);

  for (; depth_it.rem; xcb_depth_next(&depth_it)) {
    xcb_visualtype_iterator_t vis_it =
        xcb_depth_visuals_iterator(depth_it.data);

    for (; vis_it.rem; xcb_visualtype_next(&vis_it)) {
      if (screen->root_visual == vis_it.data->visual_id) {
        return vis_it.data;
      }
    }
  }

  return nullptr;
}

void paint(cairo_t *cr, int w, int h) {
  cairo_set_source_rgb(cr, 1, 1, 1);
  cairo_paint(cr);

  cairo_set_source_rgb(cr, 0.2, 0.4, 0.8);
  cairo_rectangle(cr, 20, 20, 150, 100);
  cairo_fill(cr);

  cairo_set_source_rgb(cr, 0.9, 0.15, 0.15);
  cairo_set_line_width(cr, 3);
  cairo_arc(cr, 350, 150, 60, 0, 2 * 3.14159265);
  cairo_stroke(cr);

  cairo_set_source_rgb(cr, 0.1, 0.75, 0.2);
  cairo_set_line_width(cr, 6);
  cairo_move_to(cr, 20, 250);
  cairo_line_to(cr, w - 20, 250);
  cairo_stroke(cr);

  cairo_set_source_rgb(cr, 0, 0, 0);
  cairo_select_font_face(cr, "sans", CAIRO_FONT_SLANT_NORMAL,
                         CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 28);
  cairo_move_to(cr, 20, 320);
  cairo_show_text(cr, "Hello MiniUI");
}

} // namespace

int main() {
  int screen_index = 0;
  xcb_connection_t *conn = xcb_connect(nullptr, &screen_index);

  if (xcb_connection_has_error(conn)) {
    std::fprintf(stderr, "Cannot connect to X server\n");
    return 1;
  }

  xcb_screen_t *screen = default_screen(conn);
  xcb_visualtype_t *visual = find_visual(screen);

  if (!visual) {
    std::fprintf(stderr, "Cannot find root visual\n");
    xcb_disconnect(conn);
    return 1;
  }

  constexpr int initial_w = 500;
  constexpr int initial_h = 400;

  int win_w = initial_w;
  int win_h = initial_h;

  xcb_window_t win = xcb_generate_id(conn);

  uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  uint32_t values[] = {screen->white_pixel,
                       XCB_EVENT_MASK_EXPOSURE |
                           XCB_EVENT_MASK_STRUCTURE_NOTIFY |
                           XCB_EVENT_MASK_KEY_PRESS};

  xcb_void_cookie_t cookie = xcb_create_window_checked(
      conn, XCB_COPY_FROM_PARENT, win, screen->root, 100, 100, initial_w,
      initial_h, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual, mask,
      values);

  xcb_generic_error_t *err = xcb_request_check(conn, cookie);
  if (err) {
    std::fprintf(stderr, "xcb_create_window failed: error_code=%u\n",
                 err->error_code);
    free(err);
    xcb_disconnect(conn);
    return 1;
  }

  xcb_map_window(conn, win);
  xcb_flush(conn);

  cairo_surface_t *surface =
      cairo_xcb_surface_create(conn, win, visual, win_w, win_h);

  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
    std::fprintf(stderr, "cairo_xcb_surface_create failed: %s\n",
                 cairo_status_to_string(cairo_surface_status(surface)));
    cairo_surface_destroy(surface);
    xcb_disconnect(conn);
    return 1;
  }

  cairo_t *cr = cairo_create(surface);

  if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
    std::fprintf(stderr, "cairo_create failed: %s\n",
                 cairo_status_to_string(cairo_status(cr)));
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    xcb_disconnect(conn);
    return 1;
  }

  xcb_generic_event_t *ev = nullptr;

  while ((ev = xcb_wait_for_event(conn)) != nullptr) {
    uint8_t type = ev->response_type & ~0x80;

    if (type == XCB_CONFIGURE_NOTIFY) {
      auto *cfg = reinterpret_cast<xcb_configure_notify_event_t *>(ev);

      win_w = cfg->width;
      win_h = cfg->height;

      cairo_xcb_surface_set_size(surface, win_w, win_h);
    }

    if (type == XCB_EXPOSE) {
      auto *ex = reinterpret_cast<xcb_expose_event_t *>(ev);

      // count != 0 表示后面还有 expose，最后一次再画即可
      if (ex->count == 0) {
        paint(cr, win_w, win_h);

        cairo_surface_flush(surface);
        xcb_flush(conn);

        if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
          std::fprintf(stderr, "cairo paint failed: %s\n",
                       cairo_status_to_string(cairo_status(cr)));
        }
      }
    }

    if (type == XCB_KEY_PRESS) {
      free(ev);
      break;
    }

    free(ev);
  }

  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  xcb_disconnect(conn);

  return 0;
}