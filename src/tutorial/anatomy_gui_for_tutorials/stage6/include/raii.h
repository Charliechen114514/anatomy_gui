#pragma once
/// @file raii.h
/// @brief RAII wrappers for XCB and Cairo resources.

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <memory>
#include <xcb/xcb.h>

namespace miniui {

// ── XCB helpers ──────────────────────────────────────────────────────────────

inline xcb_screen_t* default_screen(xcb_connection_t* conn) {
    return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}

inline xcb_visualtype_t* find_visual(xcb_screen_t* screen) {
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

// ── RAII deleters ────────────────────────────────────────────────────────────

struct XcbConnDeleter {
    void operator()(xcb_connection_t* c) const {
        if (c) xcb_disconnect(c);
    }
};

struct SurfDeleter {
    void operator()(cairo_surface_t* s) const {
        if (s) cairo_surface_destroy(s);
    }
};

struct CtxDeleter {
    void operator()(cairo_t* cr) const {
        if (cr) cairo_destroy(cr);
    }
};

// ── RAII unique_ptr aliases ──────────────────────────────────────────────────

using XcbConnPtr = std::unique_ptr<xcb_connection_t, XcbConnDeleter>;
using SurfPtr    = std::unique_ptr<cairo_surface_t, SurfDeleter>;
using CtxPtr     = std::unique_ptr<cairo_t, CtxDeleter>;

} // namespace miniui
