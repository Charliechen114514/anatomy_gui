#pragma once
/// @file property.h
/// @brief Reactive property with change notification via Signal.

#include "signal.h"

namespace miniui {

/// A reactive property that emits changed() when its value is set.
template <typename T>
class Property {
public:
    Property() = default;
    Property(const T& value) : value_(value) {}

    /// Get the current value.
    const T& get() const { return value_; }

    /// Set a new value; emits changed() if the value differs.
    void set(const T& newValue) {
        if (value_ != newValue) {
            value_ = newValue;
            changed_(value_);
        }
    }

    /// Implicit get.
    operator const T&() const { return value_; }

    /// Assign via =.
    Property& operator=(const T& newValue) {
        set(newValue);
        return *this;
    }

    /// Signal emitted when the value changes, passing the new value.
    Signal<const T&>& changed() { return changed_; }

private:
    T value_{};
    Signal<const T&> changed_;
};

} // namespace miniui
