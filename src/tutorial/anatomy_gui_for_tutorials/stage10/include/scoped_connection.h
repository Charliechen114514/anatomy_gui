#pragma once

#include <functional>

namespace miniui {

/// RAII wrapper for a Signal connection.
/// Disconnects automatically when destroyed.
class ScopedConnection {
public:
    ScopedConnection() = default;

    ScopedConnection(std::function<void()> disconnector)
        : disconnector_(std::move(disconnector)) {}

    ~ScopedConnection() { disconnect(); }

    // Non-copyable, movable
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    ScopedConnection(ScopedConnection&& other) noexcept
        : disconnector_(std::move(other.disconnector_)) {
        other.disconnector_ = nullptr;
    }

    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            disconnect();
            disconnector_ = std::move(other.disconnector_);
            other.disconnector_ = nullptr;
        }
        return *this;
    }

    void disconnect() {
        if (disconnector_) {
            disconnector_();
            disconnector_ = nullptr;
        }
    }

    bool isConnected() const { return disconnector_ != nullptr; }

private:
    std::function<void()> disconnector_;
};

} // namespace miniui
