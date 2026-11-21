#pragma once

#include <stdexcept>

#include <chrono>
#include <string>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Net {
class InterprocessLock {
  public:
    struct try_to_lock_t {};

    explicit InterprocessLock(const char *name, const char *lock_dir = nullptr)
            : InterprocessLock(name, false, lock_dir) {}

    InterprocessLock(const char *name, try_to_lock_t, const char *lock_dir = nullptr)
            : InterprocessLock(name, true, lock_dir) {}

    // bounded wait: polls until acquired or timeout_ms elapses, then throws
    InterprocessLock(const char *name, int timeout_ms, const char *lock_dir = nullptr)
            : InterprocessLock(name, true, lock_dir) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (!owned_ && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            try_lock();
        }
        if (!owned_) {
            close_handle();
            throw std::runtime_error("Failed to acquire lock ownership!");
        }
    }

    ~InterprocessLock() {
        unlock();
        close_handle();
    }

    InterprocessLock(const InterprocessLock &rhs) = delete;
    InterprocessLock &operator=(const InterprocessLock &rhs) = delete;

    InterprocessLock(InterprocessLock &&rhs) noexcept
            : handle_(rhs.handle_), name_(std::move(rhs.name_)), owned_(rhs.owned_) {
        rhs.handle_ = SENTINEL;
        rhs.owned_ = false;
    }

    InterprocessLock &operator=(InterprocessLock &&rhs) noexcept {
        if (this != &rhs) {
            unlock();
            close_handle();
            handle_ = rhs.handle_;
            name_ = std::move(rhs.name_);
            owned_ = rhs.owned_;
            rhs.handle_ = SENTINEL;
            rhs.owned_ = false;
        }
        return (*this);
    }

    [[nodiscard]] bool locked() const { return owned_; }

    bool try_lock() {
        if (owned_ || handle_ == SENTINEL) {
            return owned_;
        }
#ifdef _WIN32
        const DWORD res = WaitForSingleObject(handle_, 0);
        owned_ = (res == WAIT_OBJECT_0 || res == WAIT_ABANDONED);
#else
        owned_ = (flock(handle_, LOCK_EX | LOCK_NB) == 0);
#endif
        return owned_;
    }

    void unlock() {
        if (!owned_) {
            return;
        }
#ifdef _WIN32
        ReleaseMutex(handle_);
#else
        flock(handle_, LOCK_UN);
#endif
        owned_ = false;
    }

  private:
    InterprocessLock(const char *name, bool try_only, const char *lock_dir) {
#ifdef _WIN32
        (void) lock_dir;
        name_ = "Local\\"; // same-session locks: Global\ needs SeCreateGlobalPrivilege
        name_ += name;

        handle_ = CreateMutexA(nullptr, FALSE, name_.c_str());
        if (handle_ == nullptr) {
            throw std::runtime_error("Failed to create mutex!");
        }

        // WAIT_ABANDONED means the previous owner died holding it: ownership is
        // transferred to us, so a crashed run must not poison every later one
        const DWORD res = WaitForSingleObject(handle_, try_only ? 0 : INFINITE);
        if (res == WAIT_OBJECT_0 || res == WAIT_ABANDONED) {
            owned_ = true;
        } else if (res != WAIT_TIMEOUT) {
            close_handle();
            throw std::runtime_error("Failed to acquire mutex ownership!");
        }
#else
        const char *env_dir = lock_dir ? nullptr : std::getenv("TMPDIR");
        const char *dir = lock_dir ? lock_dir : (env_dir && *env_dir ? env_dir : "/tmp");
        name_ = std::string(dir);
        if (!name_.empty() && name_.back() != '/') {
            name_ += '/';
        }
        name_ += name;

        handle_ = open(name_.c_str(), O_RDWR | O_CREAT, 0644);
        if (handle_ == SENTINEL) {
            throw std::runtime_error("Failed to create lock file!");
        }

        if (flock(handle_, LOCK_EX | (try_only ? LOCK_NB : 0)) == 0) {
            owned_ = true;
        } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
            close_handle();
            throw std::runtime_error("Failed to acquire file lock!");
        }
#endif
    }

    void close_handle() {
        if (handle_ == SENTINEL) {
            return;
        }
#ifdef _WIN32
        CloseHandle(handle_);
#else
        close(handle_);
#endif
        handle_ = SENTINEL;
    }

#ifdef _WIN32
    static constexpr HANDLE SENTINEL = nullptr;
    HANDLE handle_ = SENTINEL;
#else
    static constexpr int SENTINEL = -1;
    int handle_ = SENTINEL;
#endif
    std::string name_;
    bool owned_ = false;
};
} // namespace Net
