#pragma once
/// @file raii.h
/// @brief RAII wrappers for XCB and Cairo resources.

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <xcb/xcb.h>

#include <cstdio>
#include <memory>

namespace miniui {

// ── XCB Connection RAII ──────────────────────────────────────────────────────

struct XcbConnectionDeleter {
    void operator()(xcb_connection_t* conn) const {
        if (conn) xcb_disconnect(conn);
    }
};
using XcbConnectionPtr = std::unique_ptr<xcb_connection_t, XcbConnectionDeleter>;

inline XcbConnectionPtr xcbConnect(int* screen = nullptr) {
    return XcbConnectionPtr(xcb_connect(nullptr, screen));
}

// ── Cairo Surface RAII ───────────────────────────────────────────────────────

struct CairoSurfaceDeleter {
    void operator()(cairo_surface_t* s) const {
        if (s) cairo_surface_destroy(s);
    }
};
using CairoSurfacePtr = std::unique_ptr<cairo_surface_t, CairoSurfaceDeleter>;

// ── Cairo Context RAII ───────────────────────────────────────────────────────

struct CairoDeleter {
    void operator()(cairo_t* cr) const {
        if (cr) cairo_destroy(cr);
    }
};
using CairoPtr = std::unique_ptr<cairo_t, CairoDeleter>;

// ── XCB Event RAII ───────────────────────────────────────────────────────────

struct XcbEventDeleter {
    void operator()(xcb_generic_event_t* ev) const {
        if (ev) free(ev);
    }
};
using XcbEventPtr = std::unique_ptr<xcb_generic_event_t, XcbEventDeleter>;

// ── XCB Screen helpers ───────────────────────────────────────────────────────

inline xcb_screen_t* defaultScreen(xcb_connection_t* conn) {
    return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}

inline xcb_visualtype_t* findVisual(xcb_screen_t* screen) {
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

} // namespace miniui
