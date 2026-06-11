#pragma once
#include "geometry.h"
#include <cairo/cairo.h>
#include <cmath>
#include <string>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
namespace miniui {
class Painter {
public:
    explicit Painter(cairo_t* cr) : cr_(cr) {}
    void save()    { cairo_save(cr_); }
    void restore() { cairo_restore(cr_); }
    void translate(double dx,double dy){ cairo_translate(cr_,dx,dy); }
    void clip(double x,double y,double w,double h){ cairo_rectangle(cr_,x,y,w,h); cairo_clip(cr_); }
    void setSourceRGB(double r,double g,double b){ cairo_set_source_rgb(cr_,r,g,b); }
    void setSourceColor(const Color& c){ cairo_set_source_rgba(cr_,c.r,c.g,c.b,c.a); }
    void rectangle(double x,double y,double w,double h){ cairo_rectangle(cr_,x,y,w,h); }
    void roundedRect(Rect r, double radius) {
        double x=r.x,y=r.y,w=r.width,h=r.height;
        cairo_move_to(cr_,x+radius,y);
        cairo_line_to(cr_,x+w-radius,y);
        cairo_arc(cr_,x+w-radius,y+radius,radius,-M_PI/2,0);
        cairo_line_to(cr_,x+w,y+h-radius);
        cairo_arc(cr_,x+w-radius,y+h-radius,radius,0,M_PI/2);
        cairo_line_to(cr_,x+radius,y+h);
        cairo_arc(cr_,x+radius,y+h-radius,radius,M_PI/2,M_PI);
        cairo_line_to(cr_,x,y+radius);
        cairo_arc(cr_,x+radius,y+radius,radius,M_PI,3*M_PI/2);
        cairo_close_path(cr_);
    }
    void fill()   { cairo_fill(cr_); }
    void fill(Color c){ setSourceColor(c); cairo_fill(cr_); }
    void stroke() { cairo_stroke(cr_); }
    void stroke(Color c, double w){ setSourceColor(c); cairo_set_line_width(cr_,w); cairo_stroke(cr_); }
    void setLineWidth(double w){ cairo_set_line_width(cr_,w); }
    void moveTo(double x,double y){ cairo_move_to(cr_,x,y); }
    void lineTo(double x,double y){ cairo_line_to(cr_,x,y); }
    void selectFontFace(const char*f,cairo_font_slant_t s=CAIRO_FONT_SLANT_NORMAL,cairo_font_weight_t w=CAIRO_FONT_WEIGHT_NORMAL){ cairo_select_font_face(cr_,f,s,w); }
    void setFontSize(double sz){ cairo_set_font_size(cr_,sz); }
    void showText(const std::string& t){ cairo_show_text(cr_,t.c_str()); }
    void drawText(int x,int y,const std::string& text,Color color,int fontSize){
        setSourceColor(color); selectFontFace("sans"); setFontSize(static_cast<double>(fontSize));
        moveTo(x,y); showText(text);
    }
    struct TextExtents { double x_bearing,y_bearing,width,height,x_advance,y_advance; };
    TextExtents textExtents(const std::string& t){ cairo_text_extents_t e; cairo_text_extents(cr_,t.c_str(),&e); return{e.x_bearing,e.y_bearing,e.width,e.height,e.x_advance,e.y_advance}; }
    void paint(){ cairo_paint(cr_); }
    cairo_t* raw()const{ return cr_; }
private:
    cairo_t* cr_;
};
} // namespace miniui
