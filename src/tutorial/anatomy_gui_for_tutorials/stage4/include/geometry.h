#pragma once
/// @file geometry.h
/// @brief Core geometry types for the MiniUI widget tree.
///
/// All child widget coordinates are relative to their parent widget.

namespace miniui {

struct Point {
    int x{};
    int y{};

    constexpr Point() = default;
    constexpr Point(int x_, int y_) : x(x_), y(y_) {}

    constexpr Point operator+(const Point& rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr Point operator-(const Point& rhs) const { return {x - rhs.x, y - rhs.y}; }
};

struct Size {
    int width{};
    int height{};

    constexpr Size() = default;
    constexpr Size(int w, int h) : width(w), height(h) {}
};

struct Rect {
    int x{};
    int y{};
    int width{};
    int height{};

    constexpr Rect() = default;
    constexpr Rect(int x_, int y_, int w, int h) : x(x_), y(y_), width(w), height(h) {}

    constexpr Point origin() const { return {x, y}; }
    constexpr Size  size()     const { return {width, height}; }

    constexpr bool contains(Point pt) const {
        return pt.x >= x && pt.x < x + width && pt.y >= y && pt.y < y + height;
    }
    constexpr bool isValid() const { return width > 0 && height > 0; }
};

struct Color {
    double r{0.0}, g{0.0}, b{0.0}, a{1.0};

    constexpr Color() = default;
    constexpr Color(double r_, double g_, double b_, double a_ = 1.0)
        : r(r_), g(g_), b(b_), a(a_) {}

    static constexpr Color rgb(double r, double g, double b) { return {r, g, b, 1.0}; }
};

} // namespace miniui
