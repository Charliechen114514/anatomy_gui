#pragma once

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <xcb/xcb.h>

#include <memory>

namespace miniui {

// --- XCB Connection RAII ---

struct XcbConnectionDeleter {
    void operator()(xcb_connection_t* conn) const {
        if (conn) xcb_disconnect(conn);
    }
};

using XcbConnectionPtr = std::unique_ptr<xcb_connection_t, XcbConnectionDeleter>;

inline XcbConnectionPtr connectXcb(int* screen_index = nullptr) {
    return XcbConnectionPtr(xcb_connect(nullptr, screen_index));
}

// --- Cairo Surface RAII ---

struct CairoSurfaceDeleter {
    void operator()(cairo_surface_t* surface) const {
        if (surface) cairo_surface_destroy(surface);
    }
};

using CairoSurfacePtr = std::unique_ptr<cairo_surface_t, CairoSurfaceDeleter>;

inline CairoSurfacePtr createXcbSurface(
    xcb_connection_t* conn, xcb_window_t win,
    xcb_visualtype_t* visual, int w, int h)
{
    return CairoSurfacePtr(
        cairo_xcb_surface_create(conn, win, visual, w, h));
}

// --- Cairo Context RAII ---

struct CairoContextDeleter {
    void operator()(cairo_t* cr) const {
        if (cr) cairo_destroy(cr);
    }
};

using CairoContextPtr = std::unique_ptr<cairo_t, CairoContextDeleter>;

inline CairoContextPtr createContext(cairo_surface_t* surface) {
    return CairoContextPtr(cairo_create(surface));
}

// --- Helpers ---

inline xcb_screen_t* defaultScreen(xcb_connection_t* conn) {
    return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}

inline xcb_visualtype_t* findVisual(xcb_screen_t* screen) {
    xcb_depth_iterator_t depth_it =
        xcb_screen_allowed_depths_iterator(screen);

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

} // namespace miniui
