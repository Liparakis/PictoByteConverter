#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <stdexcept>

namespace pb {

/**
 * Lightweight fixed-size thread pool with an optional bounded job queue.
 *
 * - submit() blocks when the queue is full (back-pressure for RAM control).
 * - wait_all() blocks until all submitted jobs have finished.
 * - queue_limit == 0 means unbounded.
 */
class ThreadPool {
public:
    /**
     * @param thread_count  Worker threads to spawn (0 → hardware_concurrency, min 1).
     * @param queue_limit   Max pending jobs before submit() blocks (0 = unlimited).
     */
    explicit ThreadPool(unsigned int thread_count = 0, std::size_t queue_limit = 0)
        : queue_limit_(queue_limit)
    {
        const unsigned int n = [thread_count] {
            const unsigned int hw = thread_count ? thread_count
                                                 : std::thread::hardware_concurrency();
            return hw == 0 ? 1u : hw;
        }();

        workers_.reserve(n);
        for (unsigned int i = 0; i < n; ++i)
            workers_.emplace_back([this] { workerLoop(); });
    }

    ~ThreadPool() noexcept {
        {
            std::scoped_lock lk(mutex_);
            stop_ = true;
        }
        cv_job_.notify_all();
        cv_space_.notify_all(); // unblock any blocked submit()

        for (auto& t : workers_)
            if (t.joinable()) t.join();
    }

    // Non-copyable, non-movable (workers hold 'this').
    ThreadPool(const ThreadPool&)            = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * Enqueue a job. Blocks if the bounded queue is full.
     * @throws std::runtime_error if the pool is stopping.
     */
    void submit(std::function<void()> fn) {
        {
            std::unique_lock lk(mutex_);
            cv_space_.wait(lk, [this] {
                return stop_ || queue_limit_ == 0 || jobs_.size() < queue_limit_;
            });
            if (stop_) throw std::runtime_error("ThreadPool is stopping");
            jobs_.push(std::move(fn));
        }
        cv_job_.notify_one();
    }

    /** Blocks until every submitted job has completed. */
    void wait_all() {
        std::unique_lock lk(mutex_);
        cv_done_.wait(lk, [this] { return jobs_.empty() && active_ == 0; });
    }

    [[nodiscard]] unsigned int thread_count() const noexcept {
        return static_cast<unsigned int>(workers_.size());
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> job;

            {
                std::unique_lock lk(mutex_);
                cv_job_.wait(lk, [this] { return stop_ || !jobs_.empty(); });

                if (stop_ && jobs_.empty()) return;

                job = std::move(jobs_.front());
                jobs_.pop();
                ++active_;
            }

            // Notify outside the lock: a queue slot is free, and a submit()
            // waiter can be unblocked without contending with our job execution.
            cv_space_.notify_one();

            job();

            {
                std::scoped_lock lk(mutex_);
                --active_;
            }
            cv_done_.notify_all();
        }
    }

    // --- data ---
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> jobs_;

    mutable std::mutex        mutex_;
    std::condition_variable   cv_job_;   // worker wait: job available
    std::condition_variable   cv_space_; // submit wait:  queue has room
    std::condition_variable   cv_done_;  // wait_all:     idle

    int         active_      = 0;     // jobs currently executing (under mutex_)
    bool        stop_        = false; // shutdown flag             (under mutex_)
    std::size_t queue_limit_ = 0;     // 0 = unbounded             (const after ctor)
};

} // namespace pb