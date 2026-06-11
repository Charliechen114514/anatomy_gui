#pragma once
/// @file signal.h — Signal<Args...> + ScopedConnection + SlotCallable concept

#include <concepts>
#include <functional>
#include <vector>
#include <algorithm>

namespace miniui {

class ScopedConnection {
public:
    ScopedConnection() = default;
    ScopedConnection(std::function<void()> disconnector, int id)
        : disconnect_(std::move(disconnector)), id_(id) {}
    ~ScopedConnection() { disconnect(); }
    ScopedConnection(ScopedConnection&& o) noexcept
        : disconnect_(std::move(o.disconnect_)), id_(o.id_) { o.disconnect_=nullptr; o.id_=-1; }
    ScopedConnection& operator=(ScopedConnection&& o) noexcept {
        if(this!=&o){ disconnect(); disconnect_=std::move(o.disconnect_); id_=o.id_; o.disconnect_=nullptr; o.id_=-1; }
        return *this;
    }
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    void disconnect() { if(disconnect_){disconnect_(); disconnect_=nullptr; id_=-1;} }
    bool connected()const{ return disconnect_!=nullptr; }
private:
    std::function<void()> disconnect_;
    int id_ = -1;
};

template<typename F, typename... Args>
concept SlotCallable = std::invocable<F, Args...>;

template<typename... Args>
class Signal {
public:
    template<SlotCallable<Args...> F>
    ScopedConnection connect(F&& callback) {
        int id = nextId_++;
        slots_.push_back({id, std::forward<F>(callback)});
        auto* self = this;
        std::function<void()> disconnector = [self, id]() {
            auto& s = self->slots_;
            s.erase(std::remove_if(s.begin(), s.end(), [id](const Slot& sl){return sl.id==id;}), s.end());
        };
        return ScopedConnection(std::move(disconnector), id);
    }
    void emit(Args... args) {
        auto snapshot = slots_;
        for (auto& slot : snapshot) { if(slot.callback) slot.callback(args...); }
    }
    void disconnect_all() { slots_.clear(); }
private:
    struct Slot { int id; std::function<void(Args...)> callback; };
    std::vector<Slot> slots_;
    int nextId_ = 0;
};

} // namespace miniui
