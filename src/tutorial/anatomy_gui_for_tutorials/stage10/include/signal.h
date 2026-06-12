#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <vector>

namespace miniui {

template <typename F, typename... Args>
concept SlotCallable = std::invocable<F, Args...>;

/// A simple signal-slot mechanism.  Signal<Args...> can be connected to
/// any callable that accepts Args... parameters.  Each connect() returns
/// a connection ID that can be used to disconnect later.
template <typename... Args>
class Signal {
public:
    using SlotType = std::function<void(Args...)>;
    using ConnectionId = int;

    Signal() = default;

    // Non-copyable, movable
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = default;
    Signal& operator=(Signal&&) = default;

    /// Connect a callable.  Returns a connection ID for disconnect.
    template <SlotCallable<Args...> F>
    ConnectionId connect(F&& slot) {
        int id = nextId_++;
        slots_.push_back({id, std::forward<F>(slot)});
        return id;
    }

    /// Disconnect by connection ID.
    void disconnect(ConnectionId id) {
        slots_.erase(
            std::remove_if(slots_.begin(), slots_.end(),
                [id](const SlotEntry& e) { return e.id == id; }),
            slots_.end());
    }

    /// Emit the signal to all connected slots.
    void emit(Args... args) const {
        // Copy the list so a slot can disconnect during iteration.
        auto snapshot = slots_;
        for (auto& entry : snapshot) {
            entry.slot(args...);
        }
    }

    /// Convenience: call operator.
    void operator()(Args... args) const {
        emit(std::forward<Args>(args)...);
    }

private:
    struct SlotEntry {
        ConnectionId id;
        SlotType slot;
    };

    std::vector<SlotEntry> slots_;
    int nextId_ = 0;
};

} // namespace miniui
