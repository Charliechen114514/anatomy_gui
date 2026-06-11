#pragma once
/// @file task.h
/// @brief Task<T> coroutine return type for MiniUI async operations.

#include <coroutine>
#include <exception>
#include <optional>
#include <stdexcept>
#include <type_traits>

// Forward declaration — Application is defined in application.h
namespace miniui {
class Application;
/// Schedule a coroutine handle for resumption on the event loop.
/// Implemented in application.cpp.
void scheduleTaskResume(std::coroutine_handle<> handle);
}

namespace miniui {

// ── Task<T> for non-void types ───────────────────────────────────────────────

template <typename T>
class Task {
public:
    struct promise_type {
        Task get_return_object() {
            return Task{Handle::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }

        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }

                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<> h) noexcept {
                    // Cast to the typed handle to access our promise
                    auto& prom = Handle::from_address(h.address()).promise();
                    if (prom.continuation_) {
                        return prom.continuation_;
                    }
                    return std::noop_coroutine();
                }

                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }

        void return_value(T value) {
            result_ = std::move(value);
        }

        void unhandled_exception() {
            exception_ = std::current_exception();
        }

        // Members
        std::optional<T>        result_;
        std::exception_ptr      exception_;
        std::coroutine_handle<> continuation_;
    };

    using Handle = std::coroutine_handle<promise_type>;

    // ── Constructors / Destructor ──────────────────────────────────────────

    explicit Task(Handle h) : handle_(h) {}

    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    // Non-copyable
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() {
        if (handle_) handle_.destroy();
    }

    // ── Awaitable interface ────────────────────────────────────────────────

    bool await_ready() const noexcept {
        return handle_.done();
    }

    void await_suspend(std::coroutine_handle<> caller) {
        handle_.promise().continuation_ = caller;
    }

    T await_resume() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        return std::move(*handle_.promise().result_);
    }

    // ── Start without co_await (schedule on Application) ───────────────────

    void start() {
        if (handle_ && !handle_.done()) {
            // Defined out-of-line in application.cpp to avoid circular include
            scheduleTaskResume(std::coroutine_handle<>(handle_));
        }
    }

    // ── Access ─────────────────────────────────────────────────────────────

    bool isDone() const noexcept { return handle_.done(); }

    std::coroutine_handle<> coroHandle() const { return handle_; }

private:
    Handle handle_;
};

// ── Task<void> specialization ────────────────────────────────────────────────

template <>
class Task<void> {
public:
    struct promise_type {
        Task get_return_object() {
            return Task{Handle::from_promise(*this)};
        }

        std::suspend_never initial_suspend() { return {}; }

        auto final_suspend() noexcept {
            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }

                std::coroutine_handle<> await_suspend(
                    std::coroutine_handle<> h) noexcept {
                    auto& prom = Handle::from_address(h.address()).promise();
                    if (prom.continuation_) {
                        return prom.continuation_;
                    }
                    return std::noop_coroutine();
                }

                void await_resume() noexcept {}
            };
            return FinalAwaiter{};
        }

        void return_void() {}

        void unhandled_exception() {
            exception_ = std::current_exception();
        }

        // Members
        std::exception_ptr      exception_;
        std::coroutine_handle<> continuation_;
    };

    using Handle = std::coroutine_handle<promise_type>;

    // ── Constructors / Destructor ──────────────────────────────────────────

    explicit Task(Handle h) : handle_(h) {}

    Task(Task&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() {
        if (handle_) handle_.destroy();
    }

    // ── Awaitable interface ────────────────────────────────────────────────

    bool await_ready() const noexcept {
        return handle_.done();
    }

    void await_suspend(std::coroutine_handle<> caller) {
        handle_.promise().continuation_ = caller;
    }

    void await_resume() {
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }

    // ── Start without co_await ─────────────────────────────────────────────

    void start() {
        if (handle_ && !handle_.done()) {
            scheduleTaskResume(std::coroutine_handle<>(handle_));
        }
    }

    // ── Access ─────────────────────────────────────────────────────────────

    bool isDone() const noexcept { return handle_.done(); }

    std::coroutine_handle<> coroHandle() const { return handle_; }

private:
    Handle handle_;
};

} // namespace miniui
