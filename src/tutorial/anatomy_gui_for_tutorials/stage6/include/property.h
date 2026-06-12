#pragma once
/// @file property.h
/// @brief A reactive Property<T> that automatically emits a changed() signal
///        when its value is updated.

#include "signal.h"

namespace miniui {

template <typename T>
class Property {
public:
    Property() = default;
    explicit Property(T initial) : value_(std::move(initial)) {}

    // ── Assignment — triggers changed() only when the value differs ───────
    Property& operator=(const T& newValue) {
        if (!(value_ == newValue)) {
            value_ = newValue;
            changed_.emit(value_);
        }
        return *this;
    }

    // ── Read access ───────────────────────────────────────────────────────
    const T& get() const { return value_; }
    operator const T&() const { return value_; }

    // ── Change notification ───────────────────────────────────────────────
    Signal<const T&>& changed() { return changed_; }

private:
    T                    value_{};
    Signal<const T&>     changed_;
};

} // namespace miniui
