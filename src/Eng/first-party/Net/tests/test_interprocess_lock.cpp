#include "test_common.h"

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

#include "../InterprocessLock.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#ifdef __APPLE__
#include <cstdint>
#include <mach-o/dyld.h>
#endif
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {
#ifdef _WIN32
    struct SelfProcess {
        HANDLE process = nullptr;
    };
#else
    struct SelfProcess {
        pid_t pid = -1;
    };
#endif

    SelfProcess spawn_self(const char *arg) {
        SelfProcess p{};
#ifdef _WIN32
        char exe[MAX_PATH] = {0};
        if (GetModuleFileNameA(nullptr, exe, MAX_PATH) == 0) {
            return p;
        }
        std::string cmd = std::string("\"") + exe + "\" " + arg;
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            return p;
        }
        CloseHandle(pi.hThread);
        p.process = pi.hProcess;
#else
        char exe[4096];
#ifdef __APPLE__
        uint32_t exe_size = sizeof(exe);
        const ssize_t n = _NSGetExecutablePath(exe, &exe_size) == 0 ? ssize_t(strlen(exe)) : -1;
#else
        const ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
#endif
        if (n <= 0) {
            return p;
        }
        exe[n] = 0;
        const pid_t pid = fork();
        if (pid == 0) {
            execl(exe, exe, arg, (char *) nullptr);
            _exit(127);
        }
        p.pid = pid;
#endif
        return p;
    }

    // process exit code; -1 = spawn failed or did not exit within the deadline
    int wait_self(SelfProcess &p) {
#ifdef _WIN32
        if (p.process == nullptr) {
            return -1;
        }
        int code = -1;
        if (WaitForSingleObject(p.process, 20000) == WAIT_OBJECT_0) {
            DWORD exit_code = 0;
            code = GetExitCodeProcess(p.process, &exit_code) ? int(exit_code) : -1;
        }
        CloseHandle(p.process);
        p.process = nullptr;
        return code;
#else
        if (p.pid == -1) {
            return -1;
        }
        int status = 0;
        for (int i = 0; i < 4000; ++i) {
            const pid_t r = waitpid(p.pid, &status, WNOHANG);
            if (r == p.pid) {
                p.pid = -1;
                return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
            }
            if (r == -1) {
                p.pid = -1;
                return -1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        kill(p.pid, SIGKILL);
        waitpid(p.pid, &status, 0);
        p.pid = -1;
        return -1;
#endif
    }
}

void test_interprocess_lock() {
    using namespace Net;

    printf("Test interprocess_lock  | ");

    { // lock/unlock round-trip in one process
        InterprocessLock lock("net_test_ipc_a");
        require(lock.locked());
        lock.unlock();
        require(!lock.locked());
        require(lock.try_lock());
        require(lock.locked());
    }
    { // a second thread cannot take a held lock, but can after release
        InterprocessLock held("net_test_ipc_b", 1000);
        bool busy_seen = false;
        std::thread thr([&]() {
            try {
                const InterprocessLock probe("net_test_ipc_b", 0);
            } catch (const std::exception &) {
                busy_seen = true;
            }
        });
        thr.join();
        require(busy_seen);

        held.unlock();
        bool acquired = false;
        std::thread thr2([&]() {
            try {
                const InterprocessLock probe("net_test_ipc_b", 500);
                acquired = probe.locked();
            } catch (const std::exception &) {
            }
        });
        thr2.join();
        require(acquired);
    }
    { // move semantics: ownership transfers, the moved-from object stays dead
        InterprocessLock a("net_test_ipc_c", 1000);
        InterprocessLock b("net_test_ipc_d", 1000);
        b = std::move(a); // b's own lock is released, b now holds ipc_c
        require(b.locked());
        require(!a.locked());
        require(!a.try_lock()); // a moved-from handle must not come back to life

        bool still_held = false;
        std::thread thr([&]() {
            try {
                const InterprocessLock probe("net_test_ipc_c", 0);
            } catch (const std::exception &) {
                still_held = true;
            }
        });
        thr.join();
        require(still_held); // destroying the moved-from `a` did not release b's lock

        b.unlock(); // b really owns it: the release is effective
        const InterprocessLock re("net_test_ipc_c", 500);
        require(re.locked());

        InterprocessLock f("net_test_ipc_g", 1000);
        const InterprocessLock moved(std::move(f)); // moved-from destructor is a no-op
        require(moved.locked());
    }
    { // construction succeeds for an ordinary interactive user (Local\ namespace,
      // /tmp lock files); also covers the fast-fail constructor mode
        InterprocessLock lock("net_test_ipc_e");
        require(lock.locked());
        InterprocessLock fast("net_test_ipc_f", 0);
        require(fast.locked());
    }
    { // a second process must report "busy" quickly, not block forever
#if defined(_WIN32) || defined(__linux__)
        auto holder = spawn_self("--lock-hold");
        std::this_thread::sleep_for(std::chrono::milliseconds(700)); // let it take the lock
        auto contender = spawn_self("--lock-hold");
        const int second_code = wait_self(contender);
        const int first_code = wait_self(holder);
        require(second_code == 3); // reported busy instead of blocking
        require(first_code == 0);  // held the lock the whole time and exited clean
#endif
    }

    printf("OK\n");
}
