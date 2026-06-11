#pragma once

#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <memory>
#include <xcb/xcb.h>

namespace miniui {

// ── RAII Deleters ──────────────────────────────────────────────

struct XcbConnectionDeleter {
    void operator()(xcb_connection_t* conn) const {
        if (conn) xcb_disconnect(conn);
    }
};

struct CairoSurfaceDeleter {
    void operator()(cairo_surface_t* s) const {
        if (s) cairo_surface_destroy(s);
    }
};

struct CairoContextDeleter {
    void operator()(cairo_t* cr) const {
        if (cr) cairo_destroy(cr);
    }
};

// ── RAII smart-pointer aliases ─────────────────────────────────

using XcbConnectionPtr = std::unique_ptr<xcb_connection_t, XcbConnectionDeleter>;
using CairoSurfacePtr  = std::unique_ptr<cairo_surface_t, CairoSurfaceDeleter>;
using CairoContextPtr  = std::unique_ptr<cairo_t, CairoContextDeleter>;

} // namespace miniui
