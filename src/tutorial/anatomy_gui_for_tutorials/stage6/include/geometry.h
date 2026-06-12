#pragma once
/// @file geometry.h
/// @brief Core geometry types for the MiniUI widget tree.

#include <algorithm>

namespace miniui {

struct Point {
    int x{}, y{};
    constexpr Point operator-(const Point& r) const { return {x - r.x, y - r.y}; }
    constexpr Point operator+(const Point& r) const { return {x + r.x, y + r.y}; }
};

struct Size {
    int width{}, height{};
};

struct Rect {
    int x{}, y{}, width{}, height{};

    constexpr bool contains(Point p) const {
        return p.x >= x && p.x < x + width && p.y >= y && p.y < y + height;
    }

    constexpr Point origin() const { return {x, y}; }
    constexpr Size  size()   const { return {width, height}; }
};

struct Color {
    double r{0}, g{0}, b{0}, a{1};
    static constexpr Color rgb(double r, double g, double b) { return {r, g, b, 1}; }
};

} // namespace miniui
