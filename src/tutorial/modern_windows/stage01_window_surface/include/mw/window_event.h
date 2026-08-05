#pragma once

#include "mw/define.h"
namespace mw {

struct IEvent {
    enum class Type { Resize } type;
    IEvent(Type t) : type(t) {}
};

struct ResizeEvent : IEvent {
    ResizeEvent() : IEvent(IEvent::Type::Resize) {}

    Size_t oldSize{};
    Size_t newSize{};
};
} // namespace mw
