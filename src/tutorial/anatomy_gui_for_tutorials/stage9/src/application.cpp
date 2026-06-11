#include "application.h"
#include "widget.h"
#include "painter.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <xcb/xcb_atom.h>

namespace miniui {

/// Walk up the parent chain to compute a widget's absolute (root-window) origin.
static Point absoluteOrigin(const Widget* w) {
    Point origin = w->rect().origin();
    for (auto* p = w->parent(); p; p = p->parent()) {
        origin.x += p->rect().x;
        origin.y += p->rect().y;
    }
    return origin;
}

Application& Application::instance(){
    static Application a;
    return a;
}

void Application::init(int w, int h){
    w_=w; h_=h;
    int sn=0;
    conn_=xcb_connect(nullptr,&sn);
    if(xcb_connection_has_error(conn_)){std::fprintf(stderr,"XCB connect failed\n");std::exit(1);}
    screen_=default_screen(conn_);
    visual_=find_visual(screen_);
    if(!visual_){std::fprintf(stderr,"No visual\n");std::exit(1);}
    win_=xcb_generate_id(conn_);
    uint32_t mask=XCB_CW_BACK_PIXEL|XCB_CW_EVENT_MASK;
    uint32_t vals[]={screen_->white_pixel,
        static_cast<uint32_t>(XCB_EVENT_MASK_EXPOSURE|XCB_EVENT_MASK_STRUCTURE_NOTIFY|
        XCB_EVENT_MASK_KEY_PRESS|XCB_EVENT_MASK_BUTTON_PRESS|
        XCB_EVENT_MASK_BUTTON_RELEASE|XCB_EVENT_MASK_POINTER_MOTION)};
    xcb_create_window(conn_,XCB_COPY_FROM_PARENT,win_,screen_->root,
        0,0,w_,h_,0,XCB_WINDOW_CLASS_INPUT_OUTPUT,screen_->root_visual,mask,vals);
    xcb_map_window(conn_,win_);
    xcb_flush(conn_);

    // Register WM_DELETE_WINDOW so the window manager sends us a close event
    xcb_intern_atom_cookie_t protoCookie = xcb_intern_atom(conn_, 0, 12, "WM_PROTOCOLS");
    xcb_intern_atom_cookie_t delCookie   = xcb_intern_atom(conn_, 0, 16, "WM_DELETE_WINDOW");
    xcb_intern_atom_reply_t* protoReply  = xcb_intern_atom_reply(conn_, protoCookie, nullptr);
    xcb_intern_atom_reply_t* delReply    = xcb_intern_atom_reply(conn_, delCookie, nullptr);
    if (protoReply && delReply) {
        wmProtocols_ = protoReply->atom;
        wmDeleteWin_ = delReply->atom;
        xcb_change_property(conn_, XCB_PROP_MODE_REPLACE, win_,
                            protoReply->atom, 4 /*ATOM*/, 32,
                            1, &delReply->atom);
    }
    free(protoReply);
    free(delReply);
    xcb_flush(conn_);
}

void Application::setRoot(std::shared_ptr<Widget> root){
    root_=std::move(root);
    if(root_) root_->arrange({0,0,w_,h_});
}

void Application::repaintAll(){
    if(!root_)return;
    cairo_surface_t* surf=cairo_xcb_surface_create(conn_,win_,visual_,w_,h_);
    cairo_t* cr=cairo_create(surf);
    {Painter p(cr);p.setSourceRGB(1,1,1);p.paint();p.save();root_->paint(p);p.restore();}
    cairo_destroy(cr);
    cairo_surface_flush(surf);
    cairo_surface_destroy(surf);
    xcb_flush(conn_);
}

void Application::quit(){running_=false;}

void Application::markDirty(Rect region){
    dirtyRegions_.push_back(region);
}

int Application::run(){
    running_=true;
    xcb_generic_event_t* ev=nullptr;
    while(running_ && (ev=xcb_wait_for_event(conn_))!=nullptr){
        uint8_t type=ev->response_type&~0x80;
        switch(type){
        case XCB_EXPOSE:{auto* ex=reinterpret_cast<xcb_expose_event_t*>(ev);if(ex->count==0) markDirty({ex->x,ex->y,ex->width,ex->height}); break;}
        case XCB_CONFIGURE_NOTIFY:{
            auto* cfg=reinterpret_cast<xcb_configure_notify_event_t*>(ev);
            w_=cfg->width;h_=cfg->height;
            if(root_){root_->measure({w_,h_});root_->arrange({0,0,w_,h_});}
            markDirty({0,0,w_,h_});break;}
        case XCB_CLIENT_MESSAGE:{
            auto* cm=reinterpret_cast<xcb_client_message_event_t*>(ev);
            if(cm->data.data32[0]==wmDeleteWin_){running_=false;}
            break;}
        case XCB_BUTTON_PRESS:{
            auto* bp=reinterpret_cast<xcb_button_press_event_t*>(ev);
            if(root_){Point p{bp->event_x,bp->event_y};auto t=root_->hitTest(p);if(t){
                Point abs=absoluteOrigin(t.get());t->onMousePress({p.x-abs.x,p.y-abs.y},bp->detail);}}
            break;}
        case XCB_BUTTON_RELEASE:{
            auto* br=reinterpret_cast<xcb_button_release_event_t*>(ev);
            if(root_){Point p{br->event_x,br->event_y};auto t=root_->hitTest(p);if(t){
                Point abs=absoluteOrigin(t.get());t->onMouseRelease({p.x-abs.x,p.y-abs.y},br->detail);}}
            break;}
        case XCB_MOTION_NOTIFY:{
            auto* mn=reinterpret_cast<xcb_motion_notify_event_t*>(ev);
            if(root_){Point p{mn->event_x,mn->event_y};auto t=root_->hitTest(p);if(t){
                Point abs=absoluteOrigin(t.get());t->onMouseMove({p.x-abs.x,p.y-abs.y});}}
            break;}
        case XCB_KEY_PRESS:{
            auto* kp=reinterpret_cast<xcb_key_press_event_t*>(ev);
            if(kp->detail==9){free(ev);return 0;}
            break;}
        }
        free(ev);

        // Process accumulated dirty regions
        if(!dirtyRegions_.empty()){
            repaintAll();
            dirtyRegions_.clear();
        }
    }
    xcb_disconnect(conn_);
    return 0;
}

} // namespace miniui
