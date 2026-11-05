/**
 * @file event.h
 * @brief Typed events handed to widgets.
 */

#pragma once

#include "framework/geometry.h"

class Widget;

/**
 * @brief What happened.
 *
 * The list stays tiny on purpose: one kind per thing the demo actually reacts
 * to. Every kind added here costs a branch somewhere.
 */
enum class EventKind {
    Click,
    TextChanged,
};

/**
 * @brief One typed occurrence, carrying where it happened.
 *
 * A widget answers two separate questions about an event: whether it wants the
 * event at all, and whether it handled it. This object carries the answer to
 * the second one, so a router can decide whether to keep bubbling.
 */
class Event {
public:
    /**
     * @brief Build an event aimed at one node.
     *
     * @param[in] kind What happened.
     * @param[in] target The node the event is delivered to first.
     */
    Event(EventKind kind, Widget* target) : kind_(kind), target_(target) {}

    /**
     * @brief Read what happened.
     *
     * @return The event kind.
     */
    [[nodiscard]] EventKind kind() const { return kind_; }

    /**
     * @brief Read the node the event is aimed at.
     *
     * @return The target node, never null.
     */
    [[nodiscard]] Widget* target() const { return target_; }

    /**
     * @brief Ask whether somebody already claimed the event.
     *
     * @return true once a node handled it.
     */
    [[nodiscard]] bool handled() const { return handled_; }

    /**
     * @brief Mark the event as handled.
     *
     * Stops the router from bubbling any further.
     */
    void mark_handled() { handled_ = true; }

private:
    EventKind kind_;
    Widget*   target_;
    bool      handled_ = false;
};
