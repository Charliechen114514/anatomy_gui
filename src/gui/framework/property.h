/**
 * @file property.h
 * @brief A value that tells listeners when it changes.
 */

#pragma once

#include <functional>
#include <utility>
#include <vector>

/**
 * @brief A value plus the list of things that care about it.
 *
 * A write notifies only when the value actually differs, so a repaint is never
 * scheduled for a write that changed nothing.
 *
 * @tparam T The stored type, comparable with `==`.
 */
template <typename T>
class Property {
public:
    /**
     * @brief The signature a listener has.
     */
    using Listener = std::function<void()>;

    Property() = default;

    /**
     * @brief Build a property with a starting value.
     *
     * @param[in] initial_value The value to start from.
     */
    explicit Property(T initial_value) : value_(std::move(initial_value)) {}

    /**
     * @brief Read the value.
     *
     * @return The current value.
     */
    [[nodiscard]] const T& value() const { return value_; }

    /**
     * @brief Write the value.
     *
     * Notifies every listener, but only when the new value differs from the old
     * one.
     *
     * @param[in] new_value The value to store.
     */
    void set(T new_value) {
        if (new_value == value_) {
            return;
        }
        value_ = std::move(new_value);
        for (const Listener& listener : listeners_) {
            listener();
        }
    }

    /**
     * @brief Add a listener.
     *
     * @param[in] listener Called whenever the value changes.
     */
    void on_changed(Listener listener) { listeners_.push_back(std::move(listener)); }

private:
    T                     value_{};
    std::vector<Listener> listeners_;
};
