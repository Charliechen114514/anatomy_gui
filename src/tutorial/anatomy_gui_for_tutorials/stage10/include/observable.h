#pragma once

#include <any>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace miniui {

/// Observable<T> — a reactive event stream that can produce multiple values.
/// Supports subscribe/next and chaining operators: filter, map, take_until,
/// combine_latest.
template <typename T>
class Observable {
public:
    using Observer    = std::function<void(T)>;
    using Unsubscribe = std::function<void()>;

    Observable() = default;

    // Non-copyable, movable
    Observable(const Observable&) = delete;
    Observable& operator=(const Observable&) = delete;
    Observable(Observable&&) = default;
    Observable& operator=(Observable&&) = default;

    /// Subscribe an observer.  Returns an unsubscribe callable.
    Unsubscribe subscribe(Observer observer) {
        auto shared = state_;
        int id = shared->nextId++;
        shared->observers.push_back({id, std::move(observer)});
        return [shared, id]() {
            auto& obs = shared->observers;
            obs.erase(
                std::remove_if(obs.begin(), obs.end(),
                    [id](const Entry& e) { return e.id == id; }),
                obs.end());
        };
    }

    /// Push a value to all subscribers.
    void next(T value) {
        // Snapshot under lock so re-entrant subscribe/unsubscribe is safe.
        typename State::ObserverList snapshot;
        {
            snapshot = state_->observers;
        }
        for (auto& entry : snapshot) {
            entry.observer(value);
        }
    }

    // --- Operators ---

    /// Filter: create a new Observable that only forwards values satisfying
    /// the predicate.
    Observable<T> filter(std::function<bool(const T&)> predicate) {
        Observable<T> result;
        auto resState = result.state_;
        state_->observers.push_back(
            {state_->nextId++,
             [resState, pred = std::move(predicate)](T val) {
                 if (pred(val)) {
                     Observable<T> tmp;
                     tmp.state_ = resState;
                     tmp.next(std::move(val));
                 }
             }});
        return result;
    }

    /// Map: transform each T into a U, producing a new Observable<U>.
    template <typename U>
    Observable<U> map(std::function<U(const T&)> transform) {
        Observable<U> result;
        auto resState = result.state_;
        state_->observers.push_back(
            {state_->nextId++,
             [resState, tr = std::move(transform)](T val) {
                 U mapped = tr(val);
                 Observable<U> tmp;
                 tmp.state_ = resState;
                 tmp.next(std::move(mapped));
             }});
        return result;
    }

    /// take_until: forward values from this Observable until `until`
    /// pushes its first value, then stop.
    Observable<T> take_until(Observable<std::any>& until) {
        Observable<T> result;
        auto resState   = result.state_;
        auto completed  = std::make_shared<bool>(false);
        auto sourceUnsub = std::make_shared<Unsubscribe>();

        // Subscribe to source
        state_->observers.push_back(
            {state_->nextId++,
             [resState, completed](T val) {
                 if (!*completed) {
                     Observable<T> tmp;
                     tmp.state_ = resState;
                     tmp.next(std::move(val));
                 }
             }});

        // Subscribe to terminator
        until.state_->observers.push_back(
            {until.state_->nextId++,
             [completed](std::any) {
                 *completed = true;
             }});

        return result;
    }

    /// combine_latest: whenever either this or `other` pushes a value,
    /// emit std::pair<T, U> with the latest values from both.
    template <typename U>
    Observable<std::pair<T, U>> combine_latest(Observable<U>& other) {
        Observable<std::pair<T, U>> result;
        auto resState = result.state_;

        auto latestT = std::make_shared<std::optional<T>>();
        auto latestU = std::make_shared<std::optional<U>>();

        // Subscribe to *this
        state_->observers.push_back(
            {state_->nextId++,
             [resState, latestT, latestU](T val) {
                 *latestT = std::move(val);
                 if (latestU->has_value()) {
                     Observable<std::pair<T, U>> tmp;
                     tmp.state_ = resState;
                     tmp.next({latestT->value(), latestU->value()});
                 }
             }});

        // Subscribe to other
        other.state_->observers.push_back(
            {other.state_->nextId++,
             [resState, latestT, latestU](U val) {
                 *latestU = std::move(val);
                 if (latestT->has_value()) {
                     Observable<std::pair<T, U>> tmp;
                     tmp.state_ = resState;
                     tmp.next({latestT->value(), latestU->value()});
                 }
             }});

        return result;
    }

private:
    struct Entry {
        int id;
        Observer observer;
    };

    struct State {
        using ObserverList = std::vector<Entry>;
        ObserverList observers;
        int nextId = 0;
    };

    std::shared_ptr<State> state_ = std::make_shared<State>();
};

} // namespace miniui
