#pragma once

#include "signal.h"

namespace miniui {

/// A reactive property: assignment triggers the changed() signal.
/// T must be copyable and equality-comparable.
template <typename T>
class Property {
public:
    Property() = default;
    Property(T value) : value_(std::move(value)) {}

    /// Get the current value.
    const T& get() const { return value_; }

    /// Set a new value.  Triggers changed() only if the value differs.
    void set(const T& value) {
        if (value_ != value) {
            value_ = value;
            changed_.emit(value_);
        }
    }

    /// Assignment operator (triggers changed if different).
    Property& operator=(const T& value) {
        set(value);
        return *this;
    }

    /// Implicit conversion to const T&.
    operator const T&() const { return value_; }

    /// Signal emitted when the value changes.
    Signal<const T&>& changed() { return changed_; }

private:
    T value_{};
    Signal<const T&> changed_;
};

} // namespace miniui
