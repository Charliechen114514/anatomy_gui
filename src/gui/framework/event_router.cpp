#include "framework/event_router.h"

#include "framework/widget.h"

bool EventRouter::dispatch(Event& event) const {
    // Walk from the target towards the root. A node that does not accept the
    // event is skipped without stopping the walk.
    for (Widget* node = event.target(); node != nullptr; node = node->parent()) {
        if (!node->accepts(event)) {
            continue;
        }
        if (node->on_event(event)) {
            event.mark_handled();
            return true;
        }
    }
    return false;
}
