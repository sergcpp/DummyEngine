#pragma once

#include <cassert>

#include <condition_variable>
#include <functional>
#include <future>

#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>

#include "SmallVector.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#undef GetObject
#undef DrawText
#undef near
#undef far
#else
#include <sched.h>
#endif

// #include <optick/optick.h>
// #include <vtune/ittnotify.h>
// extern __itt_domain * __g_itt_domain;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

namespace Sys {
enum class eThreadPriority { Low, Normal, High };

struct Task {
    std::function<void()> func;
    SmallVector<short, 8> dependents;
    std::atomic_int dependencies = {};

    Task() = default;
    Task(const Task &rhs) : func(rhs.func), dependents(rhs.dependents), dependencies(rhs.dependencies.load()) {}
};

struct TaskList {
    SmallVector<Task, 16> tasks;
    SmallVector<short, 16> tasks_order, tasks_pos;

    template <class F, class... Args> short AddTask(F &&f, Args &&...args) {
        //using return_type = typename std::invoke_result_t<F, Args...>;

        const auto ret = short(tasks.size());
        Task &t = tasks.emplace_back();
        t.func = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
        return ret;
    }

    bool AddDependency(const short id, const short dep) {
        if (id == dep) {
            return false;
        }
        if (std::find(std::begin(tasks[dep].dependents), std::end(tasks[dep].dependents), id) ==
            std::end(tasks[dep].dependents)) {
            tasks[dep].dependents.push_back(id);
            ++tasks[id].dependencies;
            return true;
        }
        return false;
    }

    void Sort(const bool keep_close = false) {
        tasks_order.clear();

        SmallVector<short, 128> task_dependencies(tasks.size());
        for (short i = 0; i < short(tasks.size()); ++i) {
            task_dependencies[i] = tasks[i].dependencies;
        }

        for (short i = 0; i < short(tasks.size()); ++i) {
            if (!tasks[i].dependencies) {
                tasks_order.push_back(i);
            }
        }
        for (short i = 0; i < short(tasks_order.size()); ++i) {
            for (const short id : tasks[tasks_order[i]].dependents) {
                if (--task_dependencies[id] == 0) {
                    if (keep_close) {
                        tasks_order.insert(std::begin(tasks_order) + i + 1, id);
                    } else {
                        tasks_order.push_back(id);
                    }
                }
            }
        }

        tasks_pos.resize(tasks_order.size());
        for (short i = 0; i < short(tasks_order.size()); ++i) {
            tasks_pos[tasks_order[i]] = i;
        }
    }

    bool HasCycles() const { return tasks_order.size() != tasks.size(); }
};

class ThreadPool {
  public:
    explicit ThreadPool(int threads_count, eThreadPriority priority = eThreadPriority::Normal,
                        const char *threads_name = nullptr);
    ~ThreadPool();

    template <class F, class... Args>
    std::future<typename std::invoke_result_t<F, Args...>> Enqueue(F &&f, Args &&...args);

    std::future<void> Enqueue(const TaskList &task_list);
    std::future<void> Enqueue(TaskList &&task_list);

    template <class UnaryFunction> void ParallelFor(int from, int to, UnaryFunction &&f);

    int workers_count() const { return int(workers_.size()); }

    bool SetPriority(int i, eThreadPriority priority);
    bool SetPriority(const eThreadPriority priority) {
        bool ret = true;
        for (int i = 0; i < int(workers_.size()); ++i) {
            ret &= SetPriority(i, priority);
        }
        return ret;
    }

  private:
    std::vector<std::thread> workers_;
    std::deque<SmallVector<Task, 16>> task_lists_;
    std::atomic_int active_tasks_ = {};

    // synchronization
    std::mutex q_mtx_;
    std::condition_variable condition_;
    bool stop_;
};

// the constructor just launches some amount of workers_
inline ThreadPool::ThreadPool(const int threads_count, const eThreadPriority priority, const char *threads_name)
    : stop_(false) {
    for (int i = 0; i < threads_count; ++i) {
        workers_.emplace_back([this, i, threads_name] {
            char name_buf[64] = "Worker thread";
            if (threads_name) {
                snprintf(name_buf, sizeof(name_buf), "%s_%i", threads_name, int(i));
            }
            //__itt_thread_set_name(name_buf);
            // OPTICK_THREAD(name_buf);

            for (;;) {
                std::function<void()> task;
                Task *cur_tasks = nullptr;
                SmallVector<short, 8> dependents;

                {
                    std::unique_lock<std::mutex> lock(q_mtx_);
                    condition_.wait(lock, [this] { return stop_ || active_tasks_ != 0; });
                    if (stop_ && task_lists_.empty()) {
                        return;
                    }

                    // Find task we can execute
                    for (int l = 0; l < int(task_lists_.size()) && !task; ++l) {
                        auto &list = task_lists_[l];
                        for (int i = int(list.size()) - 1; i >= 0; --i) {
                            if (list[i].func && list[i].dependencies == 0) {
                                cur_tasks = list.data();
                                task = std::move(list[i].func);
                                list[i].func = nullptr;
                                dependents = std::move(list[i].dependents);
                                --active_tasks_;
                                while (!list.empty() && !list.back().func) {
                                    list.pop_back();
                                }
                                break;
                            }
                        }
                    }

                    while (!task_lists_.empty() && task_lists_.front().empty()) {
                        task_lists_.pop_front();
                    }

                    assert(!task_lists_.empty() || active_tasks_ == 0);
                }

                if (task) {
                    task();

                    for (const int i : dependents) {
                        if (cur_tasks && cur_tasks[i].dependencies.fetch_sub(1) == 1) {
                            ++active_tasks_;
                            condition_.notify_one();
                        }
                    }
                }
            }
        });
    }
    SetPriority(priority);
}

inline bool ThreadPool::SetPriority(const int i, const eThreadPriority priority) {
#ifdef _WIN32
    int win32_priority = THREAD_PRIORITY_NORMAL;
    if (priority == eThreadPriority::Low) {
        win32_priority = THREAD_PRIORITY_BELOW_NORMAL;
    } else if (priority == eThreadPriority::High) {
        win32_priority = THREAD_PRIORITY_HIGHEST;
    }
    const BOOL res = SetThreadPriority(workers_[i].native_handle(), win32_priority);
    return (res == TRUE);
#else
    int posix_policy;
    sched_param param;
    if (pthread_getschedparam(workers_[i].native_handle(), &posix_policy, &param) != 0) {
        return false;
    }

    posix_policy = SCHED_RR;
#ifndef __APPLE__
    if (priority == eThreadPriority::Low) {
        posix_policy = SCHED_IDLE;
    }
#endif

    return (0 == pthread_setschedparam(workers_[i].native_handle(), posix_policy, &param));
#endif
}

// add new work item to the pool
template <class F, class... Args>
std::future<typename std::invoke_result_t<F, Args...>> ThreadPool::Enqueue(F &&f, Args &&...args) {
    using return_type = typename std::invoke_result_t<F, Args...>;

    auto task =
        std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(q_mtx_);

        // don't allow enqueueing after stopping the pool
        if (stop_) {
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        }

        task_lists_.emplace_back();
        task_lists_.back().emplace_back();
        task_lists_.back().back().func = [task]() { (*task)(); };

        ++active_tasks_;
    }
    condition_.notify_one();

    return res;
}

inline std::future<void> ThreadPool::Enqueue(const TaskList &task_list) {
    auto final_task = std::make_shared<std::packaged_task<void()>>([]() {});

    std::future<void> res = final_task->get_future();
    {
        std::unique_lock<std::mutex> lock(q_mtx_);

        // don't allow enqueueing after stopping the pool
        if (stop_) {
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        }

        task_lists_.emplace_back();

        task_lists_.back().emplace_back();
        task_lists_.back().back().func = [final_task]() { (*final_task)(); };
        task_lists_.back().back().dependencies = int(task_list.tasks_order.size());

        for (int i = int(task_list.tasks_order.size()) - 1; i >= 0; --i) {
            task_lists_.back().emplace_back(task_list.tasks[task_list.tasks_order[i]]);
            for (short &k : task_lists_.back().back().dependents) {
                k = short(task_list.tasks_order.size() - task_list.tasks_pos[k]);
            }
            if (task_lists_.back().back().dependencies == 0) {
                ++active_tasks_;
            }
            task_lists_.back().back().dependents.push_back(0);
        }
    }
    condition_.notify_all();

    return res;
}

inline std::future<void> ThreadPool::Enqueue(TaskList &&task_list) {
    auto final_task = std::make_shared<std::packaged_task<void()>>([]() {});

    std::future<void> res = final_task->get_future();
    {
        std::unique_lock<std::mutex> lock(q_mtx_);

        // don't allow enqueueing after stopping the pool
        if (stop_) {
            throw std::runtime_error("Enqueue on stopped ThreadPool");
        }

        task_lists_.emplace_back();

        task_lists_.back().emplace_back();
        task_lists_.back().back().func = [final_task]() { (*final_task)(); };
        task_lists_.back().back().dependencies = int(task_list.tasks_order.size());

        for (int i = int(task_list.tasks_order.size()) - 1; i >= 0; --i) {
            task_lists_.back().emplace_back(std::move(task_list.tasks[task_list.tasks_order[i]]));
            for (short &k : task_lists_.back().back().dependents) {
                k = short(task_list.tasks_order.size() - task_list.tasks_pos[k]);
            }
            if (task_lists_.back().back().dependencies == 0) {
                ++active_tasks_;
            }
            task_lists_.back().back().dependents.push_back(0);
        }
    }
    condition_.notify_all();

    return res;
}

template <class UnaryFunction> inline void ThreadPool::ParallelFor(const int from, const int to, UnaryFunction &&f) {
    TaskList loop_tasks;

    for (int i = from; i < to; ++i) {
        loop_tasks.AddTask(f, i);
        loop_tasks.tasks_order.push_back(i - from);
    }

    if (loop_tasks.tasks.empty()) {
        return;
    }

    Enqueue(loop_tasks).wait();
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
    assert(active_tasks_ == 0);
}

} // namespace Sys

#ifdef _MSC_VER
#pragma warning(pop)
#endif
