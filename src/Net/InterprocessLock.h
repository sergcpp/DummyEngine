#pragma once

#include <stdexcept>

#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace Net {
class InterprocessLock {
#ifdef _WIN32
    HANDLE handle_ = {};
#else
    int handle_ = 0;
#endif
    std::string name_;
  public:
    explicit InterprocessLock(const char *name) {
#ifdef _WIN32
        name_ = "Global\\";
        name_ += name;

        handle_ = CreateMutex(NULL, FALSE, name_.c_str());
        if (!handle_) {
            throw std::runtime_error("Failed to create mutex!");
        }

        const DWORD res = WaitForSingleObject(handle_, INFINITE);
        if (res != WAIT_OBJECT_0) {
            throw std::runtime_error("Failed to acquire mutex ownership!");
        }
#else
#ifndef __APPLE__
        name_ = "/run/lock/";
#else
        name_ = "/tmp/";
#endif
        name_ += name;

        handle_ = open(name_.c_str(), O_RDWR | O_CREAT, 0644);
        if (handle_ == -1) {
            throw std::runtime_error("Failed to create semaphore!");
        }

        const int res = lockf(handle_, F_LOCK, 0);
        if (res != 0) {
            int res1 = errno;
            throw std::runtime_error("sem_wait failed!");
        }
#endif
    }
    ~InterprocessLock() {
#ifdef _WIN32
        ReleaseMutex(handle_);
        CloseHandle(handle_);
#else
        lockf(handle_, F_UNLCK, 0);
        close(handle_);
#endif
    }

    InterprocessLock(const InterprocessLock &rhs) = delete;
    InterprocessLock(InterprocessLock &&rhs) noexcept {
#ifdef _WIN32
        handle_ = rhs.handle_;
        rhs.handle_ = {};
#endif
    }

    InterprocessLock &operator=(const InterprocessLock &rhs) = delete;
    InterprocessLock &operator=(InterprocessLock &&rhs) noexcept {
#ifdef _WIN32
        if (handle_) {
            CloseHandle(handle_);
        }
        handle_ = rhs.handle_;
        rhs.handle_ = {};
#endif
        return (*this);
    }
};
} // namespace Net
