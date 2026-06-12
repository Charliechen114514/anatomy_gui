#pragma once
/// @file scoped_connection.h
/// @brief RAII wrapper for signal connections.

#include <functional>
#include <vector>

namespace miniui {

/// Represents a single connection that auto-disconnects on destruction.
class ScopedConnection {
public:
    using DisconnectFn = std::function<void()>;

    ScopedConnection() = default;
    explicit ScopedConnection(DisconnectFn fn) : disconnect_(std::move(fn)) {}

    ScopedConnection(ScopedConnection&& other) noexcept
        : disconnect_(std::move(other.disconnect_)) {
        other.disconnect_ = nullptr;
    }

    ScopedConnection& operator=(ScopedConnection&& other) noexcept {
        if (this != &other) {
            disconnect();
            disconnect_ = std::move(other.disconnect_);
            other.disconnect_ = nullptr;
        }
        return *this;
    }

    // Non-copyable
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;

    ~ScopedConnection() { disconnect(); }

    void disconnect() {
        if (disconnect_) {
            disconnect_();
            disconnect_ = nullptr;
        }
    }

    bool isConnected() const { return disconnect_ != nullptr; }

private:
    DisconnectFn disconnect_;
};

/// Holds multiple ScopedConnections, disconnects all on destruction.
class ScopedConnections {
public:
    ScopedConnections() = default;

    void add(ScopedConnection conn) {
        connections_.push_back(std::move(conn));
    }

    void disconnectAll() {
        connections_.clear();
    }

private:
    std::vector<ScopedConnection> connections_;
};

} // namespace miniui
