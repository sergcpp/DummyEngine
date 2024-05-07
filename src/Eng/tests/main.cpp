
#include <atomic>
#include <cstdio>
#include <string_view>

#include <Sys/ThreadPool.h>

#include <vtune/ittnotify.h>
__itt_domain *__g_itt_domain = __itt_domain_create("Global");

#include "../Eng.h"
#include "../scene/SceneManager.h"
#include "test_common.h"

// void test_object_pool();
void test_cmdline();
void test_materials(Sys::ThreadPool &threads, bool full, std::string_view device_name, int validation_level,
                    bool nohwrt);
void test_unicode();
void test_widgets();

bool g_stop_on_fail = false;
std::atomic_bool g_tests_success{true};
std::atomic_bool g_log_contains_errors{false};

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

bool InitAndDestroyFakeGLContext();
#endif

int main(int argc, char *argv[]) {
    Eng::LogStdout log;
    { // PrepareAssets
        Sys::ThreadPool prep_threads(4);
        Eng::SceneManager::PrepareAssets("assets", "assets_pc", "pc", &prep_threads, &log);
    }
    puts(" ---------------");
    for (int i = 0; i < argc; ++i) {
        printf("%s ", argv[i]);
    }
    printf("\n");

    printf("Eng Version: %s\n", Eng::Version());
    puts(" ---------------");

    std::string_view device_name;
    int threads_count = 1;
    int validation_level = 1;
    bool full = false, nohwrt = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--prepare_assets") == 0) {
            Sys::ThreadPool prep_threads(4);
            Eng::SceneManager::PrepareAssets("assets", "assets_pc", "pc", &prep_threads, &log);
            return 0;
        } else if ((strcmp(argv[i], "--device") == 0 || strcmp(argv[i], "-d") == 0) && (++i != argc)) {
            device_name = argv[i];
        } else if (strcmp(argv[i], "-j") == 0 && (++i != argc)) {
            threads_count = atoi(argv[++i]);
        } else if (strncmp(argv[i], "-j", 2) == 0) {
            threads_count = atoi(&argv[i][2]);
        } else if (strcmp(argv[i], "--validation_level") == 0 || strcmp(argv[i], "-vl") == 0) {
            validation_level = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--nohwrt") == 0) {
            // nohwrt = true;
        } else if (strcmp(argv[i], "--full") == 0) {
            full = true;
        }
    }

#ifdef _WIN32
    // Stupid workaround that should not exist.
    // Make sure vulkan will be able to use discrete Intel GPU when dual Xe/Arc GPUs are available.
    InitAndDestroyFakeGLContext();
#endif

    Sys::ThreadPool mt_run_pool(threads_count);

    // test_object_pool();
    test_cmdline();
    test_unicode();
    test_widgets();
    puts(" ---------------");
    test_materials(mt_run_pool, full, device_name, validation_level, nohwrt);

    return g_tests_success ? 0 : -1;
}

//
// Dirty workaround for Intel discrete GPU
//
#ifdef _WIN32
extern "C" {
// Enable High Performance Graphics while using Integrated Graphics
__declspec(dllexport) int32_t NvOptimusEnablement = 1;                  // Nvidia
__declspec(dllexport) int32_t AmdPowerXpressRequestHighPerformance = 1; // AMD
}
bool InitAndDestroyFakeGLContext() {
    HWND fake_window = ::CreateWindowEx(NULL, NULL, "FakeWindow", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                        256, 256, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);

    HDC fake_dc = GetDC(fake_window);

    PIXELFORMATDESCRIPTOR pixel_format = {};
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_SUPPORT_OPENGL;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 24;
    pixel_format.cAlphaBits = 8;
    pixel_format.cDepthBits = 0;

    int pix_format_id = ChoosePixelFormat(fake_dc, &pixel_format);
    if (pix_format_id == 0) {
        printf("ChoosePixelFormat() failed\n");
        return false;
    }

    if (!SetPixelFormat(fake_dc, pix_format_id, &pixel_format)) {
        // printf("SetPixelFormat() failed (0x%08x)\n", GetLastError());
        return false;
    }

    HGLRC fake_rc = wglCreateContext(fake_dc);

    wglDeleteContext(fake_rc);
    ReleaseDC(fake_window, fake_dc);
    DestroyWindow(fake_window);

    return true;
}
#else
extern "C" {
__attribute__((visibility("default"))) int32_t NvOptimusEnablement = 1;                  // Nvidia
__attribute__((visibility("default"))) int32_t AmdPowerXpressRequestHighPerformance = 1; // AMD
}
#endif
