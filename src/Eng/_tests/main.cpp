
#include <atomic>
#include <cstdio>

#include <Sys/AssetFileIO.h>
#include <Sys/ThreadPool.h>

#include <vtune/ittnotify.h>
__itt_domain *__g_itt_domain = __itt_domain_create("Global");

#include "../Scene/SceneManager.h"
#include "test_common.h"

// void test_object_pool();
void test_cmdline();
void test_materials(Sys::ThreadPool &threads, const char *device_name, int validation_level);
void test_unicode();
void test_widgets();

bool g_stop_on_fail = false;
std::atomic_bool g_tests_success{true};
std::atomic_bool g_log_contains_errors{false};

int main(int argc, char *argv[]) {
    LogStdout log;
    SceneManager::PrepareAssets("assets", "assets_pc", "pc", nullptr, &log);
    puts(" ---------------");

    Sys::InitWorker();

    // test_object_pool();
    test_cmdline();
    test_unicode();
    test_widgets();

    const char *device_name = nullptr;
    bool multithreaded = false;
    int validation_level = 2;

    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "--device") == 0 || strcmp(argv[i], "-d") == 0) && (++i != argc)) {
            device_name = argv[i];
        } else if (strcmp(argv[i], "--mt") == 0) {
            multithreaded = true;
        } else if (strcmp(argv[i], "--validation_level") == 0 || strcmp(argv[i], "-vl") == 0) {
            validation_level = std::atoi(argv[++i]);
        }
    }

    multithreaded = false;
    Sys::ThreadPool mt_run_pool(multithreaded ? 4 : 1);

    puts(" ---------------");

    test_materials(mt_run_pool, device_name, validation_level);

    Sys::StopWorker();
}
