#pragma once

#include <atomic>

namespace Sys {
class SpinlockMutex {
    std::atomic_flag flag_;
public:
    SpinlockMutex() { /*: flag_(ATOMIC_FLAG_INIT)*/
        flag_.clear();
    }
    SpinlockMutex(const SpinlockMutex &rhs) {
        flag_.clear();
    }

    void lock() {
        while (flag_.test_and_set(std::memory_order_acquire));
    }

    void unlock() {
        flag_.clear(std::memory_order_release);
    }
};
}
