/**
 * @file geometry.h
 * @brief Layout geometry, in logical units.
 */

#pragma once

/**
 * @brief A point in a parent's coordinate space.
 */
struct Point {
    int x = 0;
    int y = 0;
};

/**
 * @brief A width and a height.
 */
struct Size {
    int width  = 0;
    int height = 0;
};

/**
 * @brief An axis-aligned rectangle.
 *
 * The framework keeps its own rectangle type on purpose: nothing under
 * framework/ includes windows.h, so this layer never learns what a RECT is.
 */
struct Rect {
    int left   = 0;
    int top    = 0;
    int right  = 0;
    int bottom = 0;

    /**
     * @brief Read the horizontal extent.
     *
     * @return right minus left.
     */
    [[nodiscard]] int width() const { return right - left; }

    /**
     * @brief Read the vertical extent.
     *
     * @return bottom minus top.
     */
    [[nodiscard]] int height() const { return bottom - top; }
};
