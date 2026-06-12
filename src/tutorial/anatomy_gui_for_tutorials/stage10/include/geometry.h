#pragma once

#include <algorithm>
#include <cstdint>

namespace miniui {

struct Point {
    double x = 0;
    double y = 0;
};

struct Size {
    double width  = 0;
    double height = 0;

    bool isEmpty() const { return width <= 0 || height <= 0; }

    Size boundedTo(const Size& other) const {
        return {std::min(width, other.width), std::min(height, other.height)};
    }
};

struct Rect {
    double x      = 0;
    double y      = 0;
    double width  = 0;
    double height = 0;

    double right()  const { return x + width; }
    double bottom() const { return y + height; }

    Point origin()  const { return {x, y}; }
    Size  size()    const { return {width, height}; }

    bool contains(const Point& p) const {
        return p.x >= x && p.x < right() &&
               p.y >= y && p.y < bottom();
    }

    bool isEmpty() const { return width <= 0 || height <= 0; }
};

struct Color {
    double r = 0;
    double g = 0;
    double b = 0;
    double a = 1.0;

    static Color fromRGB(double red, double green, double blue) {
        return {red, green, blue, 1.0};
    }

    static Color fromRGBA(double red, double green, double blue, double alpha) {
        return {red, green, blue, alpha};
    }
};

} // namespace miniui
