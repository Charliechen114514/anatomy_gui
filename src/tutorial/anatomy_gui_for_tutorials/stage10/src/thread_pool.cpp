#include "thread_pool.h"

#include <cstdio>

namespace miniui {

ThreadPool::ThreadPool(size_t numThreads) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this](std::stop_token st) {
            workerLoop(std::move(st));
        });
    }
}

ThreadPool::~ThreadPool() {
    if (!stop_) {
        shutdown();
    }
}

void ThreadPool::workerLoop(std::stop_token stop) {
    while (!stop.stop_requested()) {
        std::function<void()> task;

        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this, &stop] {
                return stop_ || !tasks_.empty() || stop.stop_requested();
            });

            if ((stop_ || stop.stop_requested()) && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = std::move(tasks_.front());
                tasks_.pop();
            }
        }

        if (task) {
            task();   // Execute outside the lock — allows concurrency.
        }
    }
}

void ThreadPool::shutdown() {
    {
        std::unique_lock lock(mutex_);
        if (stop_) return;   // Already shut down.
        stop_ = true;
    }
    cv_.notify_all();

    for (auto& worker : workers_) {
        worker.request_stop();
    }
    workers_.clear();   // jthread destructor joins automatically.
}

} // namespace miniui
