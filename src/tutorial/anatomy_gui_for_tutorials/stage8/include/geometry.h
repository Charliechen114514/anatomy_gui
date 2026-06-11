#pragma once
/// @file geometry.h
/// @brief Core geometry types for MiniUI.

#include <algorithm>
#include <cstdint>

namespace miniui {

// ── Point ────────────────────────────────────────────────────────────────────

struct Point {
    int x = 0;
    int y = 0;

    constexpr Point() = default;
    constexpr Point(int x_, int y_) : x(x_), y(y_) {}

    constexpr Point operator+(const Point& rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr Point operator-(const Point& rhs) const { return {x - rhs.x, y - rhs.y}; }
    constexpr bool operator==(const Point& rhs) const { return x == rhs.x && y == rhs.y; }
};

// ── Size ─────────────────────────────────────────────────────────────────────

struct Size {
    int width  = 0;
    int height = 0;

    constexpr Size() = default;
    constexpr Size(int w, int h) : width(w), height(h) {}

    bool isEmpty() const { return width <= 0 || height <= 0; }

    Size boundedTo(const Size& other) const {
        return {std::min(width, other.width), std::min(height, other.height)};
    }

    constexpr bool operator==(const Size& rhs) const {
        return width == rhs.width && height == rhs.height;
    }
};

// ── Rect ─────────────────────────────────────────────────────────────────────

struct Rect {
    int x      = 0;
    int y      = 0;
    int width  = 0;
    int height = 0;

    constexpr Rect() = default;
    constexpr Rect(int x_, int y_, int w, int h)
        : x(x_), y(y_), width(w), height(h) {}

    int right()  const { return x + width; }
    int bottom() const { return y + height; }

    Point origin() const { return {x, y}; }
    Size  size()   const { return {width, height}; }

    bool contains(const Point& p) const {
        return p.x >= x && p.x < right() &&
               p.y >= y && p.y < bottom();
    }

    bool isValid() const { return width > 0 && height > 0; }
    bool isEmpty() const { return width <= 0 || height <= 0; }

    static Rect fromOriginSize(Point origin, Size sz) {
        return {origin.x, origin.y, sz.width, sz.height};
    }
};

// ── Color ────────────────────────────────────────────────────────────────────

struct Color {
    double r = 0;
    double g = 0;
    double b = 0;
    double a = 1.0;

    constexpr Color() = default;
    constexpr Color(double r_, double g_, double b_, double a_ = 1.0)
        : r(r_), g(g_), b(b_), a(a_) {}

    static constexpr Color rgb(double red, double green, double blue) {
        return {red, green, blue, 1.0};
    }

    static constexpr Color rgba(double red, double green, double blue, double alpha) {
        return {red, green, blue, alpha};
    }
};

} // namespace miniui
