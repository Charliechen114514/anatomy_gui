#pragma once
#include <cairo/cairo-xcb.h>
#include <cairo/cairo.h>
#include <memory>
#include <xcb/xcb.h>
namespace miniui {
inline xcb_screen_t* default_screen(xcb_connection_t* conn) {
    return xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
}
inline xcb_visualtype_t* find_visual(xcb_screen_t* screen) {
    xcb_depth_iterator_t di = xcb_screen_allowed_depths_iterator(screen);
    for (; di.rem; xcb_depth_next(&di)) {
        xcb_visualtype_iterator_t vi = xcb_depth_visuals_iterator(di.data);
        for (; vi.rem; xcb_visualtype_next(&vi))
            if (screen->root_visual == vi.data->visual_id) return vi.data;
    }
    return nullptr;
}
struct XcbConnDeleter { void operator()(xcb_connection_t* c)const{if(c)xcb_disconnect(c);} };
struct SurfDeleter    { void operator()(cairo_surface_t* s)const{if(s)cairo_surface_destroy(s);} };
struct CtxDeleter     { void operator()(cairo_t* cr)const{if(cr)cairo_destroy(cr);} };
using XcbConnPtr = std::unique_ptr<xcb_connection_t,XcbConnDeleter>;
using SurfPtr    = std::unique_ptr<cairo_surface_t,SurfDeleter>;
using CtxPtr     = std::unique_ptr<cairo_t,CtxDeleter>;
} // namespace miniui
