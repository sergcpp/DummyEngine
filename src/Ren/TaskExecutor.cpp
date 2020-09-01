#include "TaskExecutor.h"

#include <thread>

namespace Ren {
    std::thread::id g_main_thread_id;
}

Ren::TaskList::TaskList() {
    done_event = std::make_shared<std::atomic_bool>();
    *(std::atomic_bool*)done_event.get() = false;
}

void Ren::TaskList::Submit(TaskExecutor *r) {
    std::shared_ptr<void> done = done_event;
    r->AddTaskList(std::move(*this));
    done_event = done;
}

void Ren::TaskList::Wait() {
#ifndef __EMSCRIPTEN__
    while (!*(std::atomic_bool*)done_event.get()) {
        std::this_thread::yield();
    }
#endif
}

void Ren::TaskExecutor::AddTaskList(TaskList &&list) {
#ifndef __EMSCRIPTEN__
    std::lock_guard<std::mutex> lck(add_list_mtx_);
    task_lists_.Push(std::move(list));
#else
    for (Task &t : list) {
        t.func(t.arg);
    }
    *(std::atomic_bool*)list.done_event.get() = true;
#endif
}

void Ren::TaskExecutor::AddSingleTask(TaskFunc func, void *arg) {
#ifndef __EMSCRIPTEN__
    TaskList list;
    list.push_back({ func, arg });
    list.Submit(this);
#else
    func(arg);
#endif
}

void Ren::TaskExecutor::ProcessSingleTask(TaskFunc func, void *arg) {
#ifndef __EMSCRIPTEN__
    TaskList list;
    list.push_back({ func, arg });
    list.Submit(this);
    list.Wait();
#else
    func(arg);
#endif
}

bool Ren::TaskExecutor::ProcessTasks() {
#ifndef __EMSCRIPTEN__
    TaskList list;
    if (task_lists_.Pop(list)) {
        for (Task &t : list) {
            t.func(t.arg);
        }
        *(std::atomic_bool*)list.done_event.get() = true;
        return true;
    }
    return false;
#else
    return false;
#endif
}

void Ren::RegisterAsMainThread() {
    g_main_thread_id = std::this_thread::get_id();
}

bool Ren::IsMainThread() {
    return std::this_thread::get_id() == g_main_thread_id;
}