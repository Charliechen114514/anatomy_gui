#pragma once
/// @file observable.h — Observable<T> with Rx-style operators (filter, map, take_until, combine_latest).

#include <algorithm>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace miniui {

template<typename T>
class Observable {
public:
    using Observer    = std::function<void(T)>;
    using Unsubscribe = std::function<void()>;

    Observable() = default;

    // ── Subscribe / emit ───────────────────────────────
    Unsubscribe subscribe(Observer observer) {
        auto id = nextId_++;
        observers_.push_back({id, std::move(observer)});
        auto* self = this;
        return [self, id]() {
            auto& obs = self->observers_;
            obs.erase(std::remove_if(obs.begin(), obs.end(),
                      [id](const Entry& e){return e.id==id;}), obs.end());
        };
    }

    void next(T value) {
        auto snapshot = observers_; // safe against mid-emit unsubscribe
        for (auto& e : snapshot) {
            if (e.observer) e.observer(value);
        }
    }

    // ── Operator: filter ───────────────────────────────
    Observable<T> filter(std::function<bool(const T&)> predicate) {
        Observable<T> result;
        auto* src = this;
        // Subscribe to source, forward only matching values
        src->subscribe([result_ptr = &result, pred = std::move(predicate)](T val) {
            if (pred(val)) result_ptr->next(std::move(val));
        });
        return result;
    }

    // ── Operator: map ──────────────────────────────────
    template<typename U>
    Observable<U> map(std::function<U(const T&)> transform) {
        Observable<U> result;
        // We need to extend the lifetime of both source and result Observables.
        // For simplicity, we share state via shared_ptr.
        auto shared = std::make_shared<Observable<U>>();
        this->subscribe([shared, tf = std::move(transform)](T val) {
            shared->next(tf(val));
        });
        return std::move(*shared);
    }

    // ── Operator: take_until ────────────────────────────
    /// Forward values from this until `stop` fires.
    template<typename StopT>
    Observable<T> take_until(Observable<StopT>& stop) {
        auto result = std::make_shared<Observable<T>>();
        auto completed = std::make_shared<bool>(false);

        auto unsubSrc = this->subscribe([result, completed](T val) {
            if (!*completed) result->next(std::move(val));
        });

        stop.subscribe([completed, unsubSrc = std::move(unsubSrc)](StopT) mutable {
            *completed = true;
            unsubSrc(); // unsubscribe from source
        });

        return std::move(*result);
    }

    // ── Operator: combine_latest ────────────────────────
    template<typename U>
    Observable<std::pair<T, U>> combine_latest(Observable<U>& other) {
        auto result = std::make_shared<Observable<std::pair<T, U>>>();
        auto latestT = std::make_shared<std::optional<T>>();
        auto latestU = std::make_shared<std::optional<U>>();

        this->subscribe([result, latestT, latestU](T val) {
            *latestT = std::move(val);
            if (latestU->has_value()) {
                result->next({latestT->value(), latestU->value()});
            }
        });

        other.subscribe([result, latestT, latestU](U val) {
            *latestU = std::move(val);
            if (latestT->has_value()) {
                result->next({latestT->value(), latestU->value()});
            }
        });

        return std::move(*result);
    }

private:
    struct Entry {
        int      id;
        Observer observer;
    };
    std::vector<Entry> observers_;
    int nextId_ = 0;
};

} // namespace miniui
