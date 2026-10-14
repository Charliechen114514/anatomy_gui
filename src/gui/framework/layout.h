/**
 * @file layout.h
 * @brief The strategy that arranges a container's children.
 */

#pragma once

#include "framework/geometry.h"

class Widget;

/**
 * @brief Assigns rectangles to the children of a container.
 *
 * A layout is a strategy, not a base class of the widgets: the container owns
 * one and asks it to place the children. Swapping the strategy swaps the
 * arrangement without touching a single widget.
 */
class Layout {
public:
    Layout()                         = default;
    virtual ~Layout()                = default;
    Layout(const Layout&)            = delete;
    Layout& operator=(const Layout&) = delete;

    /**
     * @brief Give every child of the container a rectangle.
     *
     * @param[in] container The node whose children get arranged.
     * @param[in] area The rectangle the container has to work with.
     */
    virtual void arrange(Widget& container, const Rect& area) = 0;
};
