#pragma once
/// @file painter.h
/// @brief High-level drawing wrapper around cairo_t*.

#include "geometry.h"

#include <cairo/cairo.h>
#include <string>

namespace miniui {

/// A painter that wraps a cairo_t* context and provides drawing primitives.
/// Does NOT own the cairo_t*; the caller manages its lifetime.
class Painter {
public:
    /// Construct a painter for the given cairo context.
    explicit Painter(cairo_t* cr);

    /// Save/restore the graphics state.
    void save();
    void restore();

    /// Fill the entire area with a color.
    void clear(const Color& color);

    /// Draw a filled rectangle.
    void fillRect(const Rect& rect, const Color& color);

    /// Draw a rectangle outline.
    void strokeRect(const Rect& rect, const Color& color, double lineWidth = 1.0);

    /// Draw a line from p1 to p2.
    void drawLine(Point p1, Point p2, const Color& color, double lineWidth = 1.0);

    /// Draw text at the given position (baseline-left).
    void drawText(const std::string& text, Point pos, const Color& color,
                  const std::string& fontFamily = "monospace",
                  double fontSize = 14.0);

    /// Measure text extents. Returns (width, height) of the rendered text.
    Size measureText(const std::string& text,
                     const std::string& fontFamily = "monospace",
                     double fontSize = 14.0) const;

    /// Get font metrics: ascent and descent in pixels.
    void fontMetrics(double& ascent, double& descent, double& height,
                     const std::string& fontFamily = "monospace",
                     double fontSize = 14.0) const;

    /// Clip drawing to the given rectangle.
    void setClipRect(const Rect& rect);

    /// Reset clipping.
    void resetClip();

    /// Get the underlying cairo context (for advanced use).
    cairo_t* cairoContext() const { return cr_; }

private:
    void applyColor(const Color& c);
    void setFont(const std::string& family, double size) const;

    cairo_t* cr_;
};

} // namespace miniui
