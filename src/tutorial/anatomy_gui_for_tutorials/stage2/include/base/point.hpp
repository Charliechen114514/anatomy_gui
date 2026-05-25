#pragma once
#include "type.hpp"
#include <utility>

namespace anatomy_gui::base {
struct Point {
    constexpr Point(point_base_t x, point_base_t y) noexcept : x_(x), y_(y) {};
    Point(const Point& p) noexcept : x_(p.x_), y_(p.y_) {};
    Point(Point&& p) noexcept : x_(std::move(p.x_)), y_(std::move(p.y_)) {}

    Point& operator=(const Point& p) {
        x_ = p.x_;
        y_ = p.y_;
        return *this;
    }

    Point& operator=(Point&& p) {
        x_ = p.x_;
        y_ = p.y_;
        p.x_ = {};
        p.y_ = {};
        return *this;
    }

    constexpr void move(point_base_t offset_x, point_base_t offset_y) noexcept {
        x_ += offset_x;
        y_ += offset_y;
    }

    constexpr point_base_t x() const noexcept { return x_; }
    constexpr point_base_t y() const noexcept { return y_; }

  private:
    point_base_t x_;
    point_base_t y_;
};

} // namespace anatomy_gui::base
