#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>
#include <type_traits>
#include <vector>

namespace miniui {

/// Fixed-size thread pool based on C++20 std::jthread and std::stop_token.
///
/// Usage:
///   ThreadPool pool(4);
///   auto future = pool.submit([]{ return 42; });
///   int result = future.get();
class ThreadPool {
public:
    /// Create a pool with `numThreads` worker threads.
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());

    /// Destructor — calls shutdown() if the pool is still running.
    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    /// Submit a callable for execution on the pool.
    /// Returns a std::future<ReturnType> for the caller to retrieve the result.
    /// Throws std::runtime_error if the pool has been shut down.
    template <typename F>
    auto submit(F&& func) -> std::future<std::invoke_result_t<F>>;

    /// Gracefully shut down: stop accepting tasks, finish pending tasks,
    /// and join all worker threads.
    void shutdown();

private:
    /// The main loop executed by every worker thread.
    void workerLoop(std::stop_token stop);

    std::vector<std::jthread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

// ---------------------------------------------------------------------------
// Template implementation (must be in header)
// ---------------------------------------------------------------------------

template <typename F>
auto ThreadPool::submit(F&& func) -> std::future<std::invoke_result_t<F>> {
    using ReturnType = std::invoke_result_t<F>;

    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::forward<F>(func));

    std::future<ReturnType> result = task->get_future();

    {
        std::unique_lock lock(mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool: submit on stopped pool");
        }
        tasks_.emplace([task]() { (*task)(); });
    }

    cv_.notify_one();
    return result;
}

} // namespace miniui
