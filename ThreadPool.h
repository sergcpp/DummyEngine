#pragma once

#ifndef __EMSCRIPTEN__
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef ENABLE_ITT_API
#include <vtune/ittnotify.h>

extern __itt_domain *__g_itt_domain;
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Sys {
class ThreadPool {
  public:
    explicit ThreadPool(size_t threads_count, const char *threads_name = nullptr);
    ~ThreadPool();

    template <class F, class... Args>
    std::future<typename std::result_of<F(Args...)>::type> Enqueue(F &&f,
                                                                   Args &&... args);

    size_t workers_count() const { return workers_.size(); }

  private:
    // need to keep track of threads so we can join them
    std::vector<std::thread> workers_;
    // the task queue
    std::queue<std::function<void()>> tasks_;

    // synchronization
    std::mutex q_mtx_;
    std::condition_variable condition_;
    bool stop_;
};

// the constructor just launches some amount of workers_
inline ThreadPool::ThreadPool(size_t threads_count, const char *threads_name)
    : stop_(false) {
    for (size_t i = 0; i < threads_count; ++i)
        workers_.emplace_back([this, i, threads_name] {
#ifdef ENABLE_ITT_API
            if (threads_name) {
                char name_buf[64];
                sprintf(name_buf, "%s_%i", threads_name, int(i));
                __itt_thread_set_name(name_buf);
            } else {
                __itt_thread_set_name("Worker thread");
            }
#endif

            for (;;) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(q_mtx_);
                    condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                    if (stop_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }

                task();
            }
        });
}

// add new work item to the pool
template <class F, class... Args>
std::future<typename std::result_of<F(Args...)>::type>
ThreadPool::Enqueue(F &&f, Args &&... args) {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(q_mtx_);

        // don't allow enqueueing after stopping the pool
        if (stop_) {
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        }

        tasks_.emplace([task]() { (*task)(); });
    }
    condition_.notify_one();

    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool() {
    {
        std::lock_guard<std::mutex> lock(q_mtx_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread &worker : workers_) {
        worker.join();
    }
}

class QThreadPool {
  public:
    explicit QThreadPool(int threads_count, int q_count,
                         const char *threads_name = nullptr);
    ~QThreadPool();

    template <class F, class... Args>
    std::future<typename std::result_of<F(Args...)>::type> Enqueue(int q_index, F &&f,
                                                                   Args &&... args);
    int workers_count() const { return int(workers_.size()); }
    int queue_count() const { return int(tasks_.size()); }

  private:
    std::vector<std::thread> workers_;
    std::vector<std::queue<std::function<void()>>> tasks_;
    std::unique_ptr<std::atomic_bool[]> q_active_;

    std::mutex q_mtx_;
    std::condition_variable condition_;
    bool stop_;
};

inline QThreadPool::QThreadPool(const int threads_count, const int q_count,
                                const char *threads_name)
    : stop_(false) {
    workers_.reserve(threads_count);
    tasks_.resize(q_count);
    q_active_.reset(new std::atomic_bool[q_count]);
    for (int i = 0; i < q_count; ++i) {
        q_active_[i] = false;
    }
    for (int i = 0; i < threads_count; ++i) {
        workers_.emplace_back([this, i, threads_name] {
#ifdef ENABLE_ITT_API
            char name_buf[64];
            if (threads_name) {
                sprintf(name_buf, "%s_%i", threads_name, int(i));
            } else {
                sprintf(name_buf, "worker_thread_%i", int(i));
            }
            __itt_thread_set_name(name_buf);
#endif

            for (;;) {
                std::function<void()> task;
                int q_index = -1;

                {
                    std::unique_lock<std::mutex> lock(q_mtx_);
                    condition_.wait(lock, [this, i, &q_index] {
                        for (int j = 0; j < int(tasks_.size()); j++) {
                            if (!q_active_[j] && !tasks_[j].empty()) {
                                q_index = j;
                                return true;
                            }
                        }

                        return stop_;
                    });
                    if (stop_ && q_index == -1) {
                        return;
                    }
                    task = std::move(tasks_[q_index].front());
                    tasks_[q_index].pop();
                    q_active_[q_index] = true;
                }

                task();
                q_active_[q_index] = false;
            }
        });
    }
}

template <class F, class... Args>
std::future<typename std::result_of<F(Args...)>::type>
QThreadPool::Enqueue(const int q_index, F &&f, Args &&... args) {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(q_mtx_);

        // don't allow enqueueing after stopping the pool
        if (stop_) {
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        }

        tasks_[q_index].emplace([task]() { (*task)(); });
    }
    condition_.notify_one();

    return res;
}

inline QThreadPool::~QThreadPool() {
    {
        std::lock_guard<std::mutex> lock(q_mtx_);
        stop_ = true;
    }
    condition_.notify_all();
    for (std::thread &worker : workers_) {
        worker.join();
    }
}

} // namespace Sys
#else

#include <future>

namespace Sys {
class ThreadPool {
  public:
    ThreadPool(size_t);

    template <class F, class... Args>
    auto Enqueue(F &&f, Args &&... args)
        -> std::future<typename std::result_of<F(Args...)>::type>;
};

// the constructor just launches some amount of workers_
inline ThreadPool::ThreadPool(size_t threads) {}

// add new work item to the pool
template <class F, class... Args>
auto ThreadPool::Enqueue(F &&f, Args &&... args)
    -> std::future<typename std::result_of<F(Args...)>::type> {

    return std::future<typename std::result_of<F(Args...)>>(f(args...));
}
} // namespace Sys

#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif