/**
 * @file event_router.h
 * @brief Moves an event through the widget tree.
 */

#pragma once

#include "framework/event.h"

/**
 * @brief Delivers events and owns the routing rules.
 *
 * The rules live here rather than in the widgets. A widget answers two
 * questions and nothing else: whether it wants the event at all, and whether it
 * handled the event.
 *
 * Routing today starts at the event's target, because Windows already did the
 * hit test for native controls. When M2 takes input routing over, the hit test
 * moves in here.
 */
class EventRouter {
public:
    /**
     * @brief Deliver an event to its target, then bubble up the parent chain.
     *
     * Stops at the first node that reports the event as handled.
     *
     * @param[in,out] event The event to deliver.
     * @return true when some node handled the event.
     */
    [[nodiscard]] bool dispatch(Event& event) const;
};
