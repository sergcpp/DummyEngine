#pragma once

#ifndef __EMSCRIPTEN__
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>

namespace Sys {
class ThreadWorker {
public:
    ThreadWorker();
    virtual ~ThreadWorker();

    bool Stop();

    template<class F, class... Args>
    auto AddTask(F &&f, Args &&... args)
    ->std::future<typename std::result_of<F(Args...)>::type>;

private:
    std::thread worker_;

    std::queue<std::function<void()> > tasks_;

    std::mutex queue_mtx_;
    std::condition_variable cnd_;
    bool stop_, stopped_;
};

inline ThreadWorker::ThreadWorker() : stop_(false), stopped_(false) {
    worker_ = std::thread(
    [this] {
        for (; ;) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(this->queue_mtx_);
                this->cnd_.wait(lock, [this] { return this->stop_ || !this->tasks_.empty(); });
                if (this->stop_ && this->tasks_.empty()) {
                    this->stopped_ = true;
                    return;
                }
                task = std::move(this->tasks_.front());
                this->tasks_.pop();
            }

            task();
        }
    });
}

bool ThreadWorker::Stop() {
    bool ret;
    {
        std::unique_lock<std::mutex> lock(queue_mtx_);
        stop_ = true;
        ret = stopped_;
    }
    cnd_.notify_all();
    return ret;
}

template<class F, class... Args>
auto ThreadWorker::AddTask(F &&f, Args &&... args)
-> std::future<typename std::result_of<F(Args...)>::type> {
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()> >(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                );

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mtx_);

        // don't allow enqueueing after stopping thread
        if (stop_) {
            throw std::runtime_error("AddTask on stopped ThreadWorker");
        }

        tasks_.emplace([task]() {
            (*task)();
        });
    }
    cnd_.notify_one();
    return res;
}

inline ThreadWorker::~ThreadWorker() {
    {
        std::unique_lock<std::mutex> lock(queue_mtx_);
        stop_ = true;
    }
    cnd_.notify_all();
    worker_.join();
}
}
#else
namespace Sys {
class ThreadWorker {
public:
    ThreadWorker() {}

    virtual ~ThreadWorker() {}

    template<class F, class... Args>
    void AddTask(F &&f, Args &&... args) {
        f(args...);
    }
};
}
#endif // __EMSCRIPTEN__
