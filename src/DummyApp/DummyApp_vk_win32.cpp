#include "DummyApp.h"

#include <optick/optick.h>
#include <vtune/ittnotify.h>
__itt_domain *__g_itt_domain = __itt_domain_create("Global");

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <cctype>

#include <iostream>

#include <Windows.h>

#include <Eng/Log.h>
#include <Eng/ViewerBase.h>
#include <Eng/input/InputManager.h>
#include <Sys/DynLib.h>
#include <Sys/ThreadWorker.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"
#include "resource.h"

namespace {
DummyApp *g_app = nullptr;

uint32_t ScancodeFromLparam(const LPARAM lparam) {
    return ((lparam >> 16) & 0x7f) | ((lparam & (1 << 24)) != 0 ? 0x80 : 0);
}

static const unsigned char ScancodeToHID_table[256] = {
    0,  41,  30,  31,  32,  33, 34, 35, 36, 37, 38, 39,  45,  46, 42, 43, 20,  26, 8,  21, 23, 28, 24, 12, 18, 19,
    47, 48,  158, 224, 4,   22, 7,  9,  10, 11, 13, 14,  15,  51, 52, 53, 225, 49, 29, 27, 6,  25, 5,  17, 16, 54,
    55, 56,  229, 0,   226, 44, 57, 58, 59, 60, 61, 62,  63,  64, 65, 66, 67,  72, 71, 0,  0,  0,  0,  0,  0,  0,
    0,  0,   0,   0,   0,   0,  0,  0,  0,  68, 69, 0,   0,   0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,   0,   0,   0,   0,  0,  0,  0,  0,  0,  0,   0,   0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,   0,   0,   0,   0,  0,  0,  0,  0,  0,  0,   0,   0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  228, 0,   0,   0,   0,  0,  0,  0,  0,  0,  0,   0,   0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  70,  230, 0,   0,   0,  0,  0,  0,  0,  0,  0,   0,   0,  0,  0,  0,   74, 82, 75, 0,  80, 0,  79, 0,  77,
    81, 78,  73,  76,  0,   0,  0,  0,  0,  0,  0,  227, 231, 0,  0,  0,  0,   0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,   0,   0,   0,   0,  0,  0,  0,  0,  0,  0,   0,   0,  0,  0,  0,   0,  0,  0,  0,  0};

uint32_t ScancodeToHID(const uint32_t scancode) {
    if (scancode >= 256) {
        return 0;
    }
    return uint32_t(ScancodeToHID_table[scancode]);
}

class AuxGfxThread : public Sys::ThreadWorker {
    HDC device_ctx_;
    HGLRC gl_ctx_;

  public:
    AuxGfxThread(HDC device_ctx, HGLRC gl_ctx) : device_ctx_(device_ctx), gl_ctx_(gl_ctx) {
        AddTask([this]() { __itt_thread_set_name("AuxGfxThread"); });
    }

    ~AuxGfxThread() override {}
};
} // namespace

namespace Ren {
extern HWND g_win;
}

extern "C" {
// Enable High Performance Graphics while using Integrated Graphics
DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;     // Nvidia
DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

DummyApp::DummyApp() { g_app = this; }

DummyApp::~DummyApp() {}

LRESULT CALLBACK WindowProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    static float last_p1_pos[2] = {0, 0}, last_p2_pos[2] = {0, 0};

    switch (uMsg) {
    case WM_CLOSE: {
        PostQuitMessage(0);
        break;
    }
    case WM_LBUTTONDOWN: {
        const float px = float(LOWORD(lParam)), py = float(HIWORD(lParam));
        g_app->AddEvent(Eng::eInputEvent::P1Down, 0, px, py, 0, 0);
        break;
    }
    case WM_RBUTTONDOWN: {
        const float px = float(LOWORD(lParam)), py = float(HIWORD(lParam));
        g_app->AddEvent(Eng::eInputEvent::P2Down, 0, px, py, 0, 0);
        break;
    }
    case WM_MBUTTONDOWN: {
        const float px = float(LOWORD(lParam)), py = float(HIWORD(lParam));
        g_app->AddEvent(Eng::eInputEvent::MButtonDown, 0, px, py, 0, 0);
        break;
    }
    case WM_LBUTTONUP: {
        const float px = float(LOWORD(lParam)), py = float(HIWORD(lParam));
        g_app->AddEvent(Eng::eInputEvent::P1Up, 0, px, py, 0, 0);
        break;
    }
    case WM_RBUTTONUP: {
        const float px = float(LOWORD(lParam)), py = float(HIWORD(lParam));
        g_app->AddEvent(Eng::eInputEvent::P2Up, 0, px, py, 0, 0);
        break;
    }
    case WM_MBUTTONUP: {
        const float px = float(LOWORD(lParam)), py = float(HIWORD(lParam));
        g_app->AddEvent(Eng::eInputEvent::MButtonUp, 0, px, py, 0, 0);
        break;
    }
    case WM_MOUSEMOVE: {
        const float px = float(LOWORD(lParam)), py = float(HIWORD(lParam));
        g_app->AddEvent(Eng::eInputEvent::P1Move, 0, px, py, px - last_p1_pos[0], py - last_p1_pos[1]);

        last_p1_pos[0] = px;
        last_p1_pos[1] = py;
        break;
    }
    case WM_KEYDOWN: {
        const uint32_t scan_code = ScancodeFromLparam(lParam), key_code = ScancodeToHID(scan_code);
        g_app->AddEvent(Eng::eInputEvent::KeyDown, key_code, 0, 0, 0, 0);
        break;
    }
    case WM_KEYUP: {
        const uint32_t scan_code = ScancodeFromLparam(lParam), key_code = ScancodeToHID(scan_code);
        g_app->AddEvent(Eng::eInputEvent::KeyUp, key_code, 0, 0, 0, 0);
        break;
    }
    case WM_MOUSEWHEEL: {
        WORD _delta = HIWORD(wParam);
        const auto delta = reinterpret_cast<const short &>(_delta);
        const float wheel_motion = float(delta / WHEEL_DELTA);
        g_app->AddEvent(Eng::eInputEvent::MouseWheel, 0, last_p1_pos[0], last_p1_pos[1], wheel_motion, 0);
        break;
    }
    case WM_SIZE: {
        const int w = LOWORD(lParam), h = HIWORD(lParam);
        g_app->Resize(w, h);
        g_app->AddEvent(Eng::eInputEvent::Resize, 0, float(w), float(h), 0, 0);
        break;
    }
    default: {
        break;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int DummyApp::Init(const int w, const int h, const AppParams &app_params) {
    [[maybe_unused]] const BOOL dpi_result = SetProcessDPIAware();

    WNDCLASSEX window_class = {};
    window_class.cbSize = sizeof(WNDCLASSEX);
    window_class.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.lpszClassName = "MainWindowClass";
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
    window_class.hIcon = LoadIcon(window_class.hInstance, MAKEINTRESOURCE(IDI_ICON1));
    window_class.hIconSm = window_class.hIcon;
    RegisterClassEx(&window_class);

    RECT rect;
    rect.left = rect.top = 0;
    rect.right = w;
    rect.bottom = h;

    BOOL ret = ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, false);
    if (!ret) {
        return -1;
    }

    int win_pos[] = {CW_USEDEFAULT, CW_USEDEFAULT};

    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    if (!app_params.ref_name.empty()) {
        style &= ~WS_THICKFRAME;
        style &= ~WS_MINIMIZEBOX;
        style &= ~WS_MAXIMIZEBOX;
    }
    if (app_params.noshow) {
        style &= ~WS_VISIBLE;
    }

    window_handle_ =
        ::CreateWindowEx(NULL, "MainWindowClass", "View [Vulkan]", style, win_pos[0], win_pos[1], rect.right - rect.left,
                         rect.bottom - rect.top, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
    device_context_ = GetDC(window_handle_);

    Ren::g_win = window_handle_;

    try {
        Viewer::PrepareAssets("pc");
        log_ = std::make_unique<LogStdout>();
        viewer_ = std::make_unique<Viewer>(w, h, app_params, log_.get());
        input_manager_ = viewer_->input_manager();
    } catch (std::exception &e) {
        fprintf(stderr, "%s", e.what());
        return -1;
    }

    return 0;
}

void DummyApp::Destroy() {
    OPTICK_SHUTDOWN();
    viewer_ = {};

    ReleaseDC(window_handle_, device_context_);
    device_context_ = nullptr;
    DestroyWindow(window_handle_);
    window_handle_ = nullptr;

    UnregisterClass("MainWindowClass", GetModuleHandle(nullptr));
}

int DummyApp::Run(int argc, char *argv[]) {
    int w = 1280, h = 720;
    fullscreen_ = false;
    AppParams app_params;
    ParseArgs(argc, argv, w, h, app_params);

    if (Init(w, h, app_params) < 0) {
        return -1;
    }

    __itt_thread_set_name("Main Thread");

    while (!viewer_->terminated) {
        OPTICK_FRAME("Main Thread");
        __itt_frame_begin_v3(__g_itt_domain, nullptr);

        MSG msg;
        while (PeekMessage(&msg, nullptr, NULL, NULL, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                viewer_->terminated = true;
            } else {
                DispatchMessage(&msg);
            }
        }

        this->Frame();

        __itt_frame_end_v3(__g_itt_domain, nullptr);
    }

    const int exit_status = viewer_->exit_status;

    this->Destroy();

    return exit_status;
}

void DummyApp::PollEvents() {}
