#pragma once

#include "property.h"
#include "widget.h"

#include <string>

namespace miniui {

/// A simple text label.
class Label : public Widget {
public:
    Label() = default;
    explicit Label(const std::string& text) : text_(std::move(text)) {}

    /// The label text.
    Property<std::string>& text() { return text_; }
    const Property<std::string>& text() const { return text_; }

    /// Font size in points.
    void setFontSize(double s) { fontSize_ = s; }
    double fontSize() const { return fontSize_; }

    /// Text color.
    void setColor(const Color& c) { color_ = c; }
    const Color& color() const { return color_; }

    // --- Widget overrides ---
    Size measure(const Size& available) override;
    void paint(Painter& p) override;

private:
    Property<std::string> text_;
    double fontSize_ = 14.0;
    Color color_ = Color::fromRGB(0, 0, 0);
};

} // namespace miniui
