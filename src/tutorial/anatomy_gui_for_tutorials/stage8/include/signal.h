#pragma once
/// @file signal.h
/// @brief Type-erased signal/slot mechanism.

#include <algorithm>
#include <functional>
#include <vector>
#include <cstdint>

namespace miniui {

/// A signal that can be emitted and connected to callable slots.
/// Usage:
///   Signal<int> sig;
///   sig.connect([](int v) { printf("%d\n", v); });
///   sig(42);
template <typename... Args>
class Signal {
public:
    using SlotType = std::function<void(Args...)>;
    using SlotId = uint64_t;

    Signal() = default;

    // Non-copyable, movable
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = default;
    Signal& operator=(Signal&&) = default;

    /// Connect a slot. Returns an ID that can be used to disconnect.
    SlotId connect(SlotType slot) {
        SlotId id = nextId_++;
        slots_.push_back({id, std::move(slot)});
        return id;
    }

    /// Disconnect a slot by ID.
    void disconnect(SlotId id) {
        slots_.erase(
            std::remove_if(slots_.begin(), slots_.end(),
                [id](const SlotEntry& e) { return e.id == id; }),
            slots_.end());
    }

    /// Emit the signal, calling all connected slots.
    void operator()(Args... args) const {
        // Copy slots_ in case a slot modifies it during iteration
        auto copy = slots_;
        for (auto& entry : copy) {
            entry.slot(args...);
        }
    }

    /// Returns a function object that, when called, disconnects the given slot.
    /// Useful for ScopedConnection.
    std::function<void()> makeDisconnecter(SlotId id) {
        // We capture a pointer to this signal and the id
        // The signal must outlive the disconnecter
        return [this, id]() { this->disconnect(id); };
    }

    bool empty() const { return slots_.empty(); }

private:
    struct SlotEntry {
        SlotId id;
        SlotType slot;
    };

    std::vector<SlotEntry> slots_;
    SlotId nextId_ = 1;
};

} // namespace miniui
