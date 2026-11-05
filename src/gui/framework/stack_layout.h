/**
 * @file stack_layout.h
 * @brief Stacks a container's children vertically.
 */

#pragma once

#include "framework/layout.h"

/**
 * @brief Places children one below another, top to bottom.
 *
 * A child that asks for a width of zero stretches to the container's width; a
 * child that asks for a width keeps it. Heights always come from the child,
 * because this layout has no opinion about how tall a thing should be.
 */
class StackLayout : public Layout {
public:
    /**
     * @brief Build a vertical stack.
     *
     * @param[in] padding Space between the container's edge and its children.
     * @param[in] gap Space between two neighbouring children.
     */
    StackLayout(int padding, int gap);

    /**
     * @brief Stack the children top to bottom.
     *
     * @param[in] container The node whose children get arranged.
     * @param[in] area The rectangle the container has to work with.
     */
    void arrange(Widget& container, const Rect& area) override;

private:
    int padding_;
    int gap_;
};
