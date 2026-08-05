#pragma once
#include <concepts>

namespace mw {
template <typename Step = int32_t>
    requires std::integral<Step>
struct Point {
    Step x;
    Step y;
};

template <typename Step = int32_t>
    requires std::integral<Step>
struct Size {
    Step width;
    Step height;
};

// some convinients
using Point_t = Point<>;
using Size_t = Size<>;

} // namespace mw