#pragma once
/// @file painter.h
/// @brief A thin wrapper around cairo_t* for 2-D drawing operations.

#include "geometry.h"

#include <cairo/cairo.h>
#include <string>

namespace miniui {

class Painter {
public:
    explicit Painter(cairo_t* cr);

    void save();
    void restore();

    void translate(double dx, double dy);

    /// Draw a filled rectangle.
    void fillRect(Rect rect, const Color& c);

    /// Draw a stroked (outlined) rectangle.
    void strokeRect(Rect rect, const Color& c, double lineWidth = 1.0);

    /// Draw text at (x, y baseline) with the given font size.
    void drawText(int x, int y, const std::string& text,
                  const Color& color = Color::rgb(0, 0, 0),
                  int fontSize = 16);

    /// Draw a rounded rectangle path.
    void roundedRect(Rect rect, double radius);

    /// Fill the current path with the given colour.
    void fill(const Color& c);

    /// Stroke the current path with the given colour and line width.
    void stroke(const Color& c, double lineWidth = 1.0);

    /// Return the raw cairo_t*.
    cairo_t* raw() const { return cr_; }

private:
    cairo_t* cr_;
};

} // namespace miniui
