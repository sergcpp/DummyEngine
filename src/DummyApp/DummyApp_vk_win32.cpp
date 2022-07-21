#include "DummyApp.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

#include <vtune/ittnotify.h>
__itt_domain *__g_itt_domain = __itt_domain_create("Global");

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <cctype>

#include <iostream>

#include <Windows.h>

#include <Eng/GameBase.h>
#include <Eng/Input/InputManager.h>
#include <Sys/DynLib.h>
#include <Sys/ThreadWorker.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"

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

extern "C" {
// Enable High Performance Graphics while using Integrated Graphics
DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;     // Nvidia
DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

DummyApp::DummyApp() { g_app = this; }

DummyApp::~DummyApp() {}

LRESULT CALLBACK WindowProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam) {
    static float last_p1_pos[2] = {0.0f, 0.0f}, last_p2_pos[2] = {0.0f, 0.0f};

    switch (uMsg) {
    case WM_CLOSE: {
        PostQuitMessage(0);
        break;
    }
    case WM_LBUTTONDOWN: {
        const float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);
        g_app->AddEvent(RawInputEv::P1Down, 0, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_RBUTTONDOWN: {
        const float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);
        g_app->AddEvent(RawInputEv::P2Down, 0, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_LBUTTONUP: {
        const float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);
        g_app->AddEvent(RawInputEv::P1Up, 0, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_RBUTTONUP: {
        const float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);
        g_app->AddEvent(RawInputEv::P2Up, 0, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_MOUSEMOVE: {
        const float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);
        g_app->AddEvent(RawInputEv::P1Move, 0, px, py, px - last_p1_pos[0], py - last_p1_pos[1]);

        last_p1_pos[0] = px;
        last_p1_pos[1] = py;
        break;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        } else {
            const uint32_t scan_code = ScancodeFromLparam(lParam), key_code = ScancodeToHID(scan_code);
            g_app->AddEvent(RawInputEv::KeyDown, key_code, 0.0f, 0.0f, 0.0f, 0.0f);
        }
        break;
    }
    case WM_KEYUP: {
        const uint32_t scan_code = ScancodeFromLparam(lParam), key_code = ScancodeToHID(scan_code);
        g_app->AddEvent(RawInputEv::KeyUp, key_code, 0.0f, 0.0f, 0.0f, 0.0f);
        break;
    }
    case WM_MOUSEWHEEL: {
        WORD _delta = HIWORD(wParam);
        const auto delta = reinterpret_cast<const short &>(_delta);
        const float wheel_motion = float(delta / WHEEL_DELTA);
        g_app->AddEvent(RawInputEv::MouseWheel, 0, 0.0f, 0.0f, wheel_motion, 0.0f);
        break;
    }
    case WM_SIZE: {
        const int w = LOWORD(lParam), h = HIWORD(lParam);
        g_app->Resize(w, h);
        g_app->AddEvent(RawInputEv::Resize, 0, float(w), float(h), 0.0f, 0.0f);
    }
    default: {
        break;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int DummyApp::Init(const int w, const int h, const char *device_name) {
    const BOOL dpi_result = SetProcessDPIAware();
    (void)dpi_result;

    WNDCLASSEX window_class = {};
    window_class.cbSize = sizeof(WNDCLASSEX);
    window_class.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = GetModuleHandle(nullptr);
    window_class.lpszClassName = "MainWindowClass";
    window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
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

    window_handle_ = ::CreateWindowEx(NULL, "MainWindowClass", "View (VK)", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      win_pos[0], win_pos[1], rect.right - rect.left, rect.bottom - rect.top, nullptr,
                                      nullptr, GetModuleHandle(nullptr), nullptr);

    device_context_ = GetDC(window_handle_);

    try {
        Viewer::PrepareAssets("pc");

        auto aux_gfx_thread = std::make_shared<AuxGfxThread>(device_context_, gl_ctx_aux_);
        viewer_.reset(new Viewer(w, h, nullptr, device_name, std::move(aux_gfx_thread)));

        auto input_manager = viewer_->GetComponent<InputManager>(INPUT_MANAGER_KEY);
        input_manager_ = input_manager;
    } catch (std::exception &e) {
        fprintf(stderr, "%s", e.what());
        return -1;
    }

    return 0;
}

void DummyApp::Destroy() {
    viewer_.reset();

    ReleaseDC(window_handle_, device_context_);
    device_context_ = nullptr;
    DestroyWindow(window_handle_);
    window_handle_ = nullptr;

    UnregisterClass("MainWindowClass", GetModuleHandle(nullptr));
}

void DummyApp::Frame() {
    if (!minimized_) {
        viewer_->Frame();
    }
}

void DummyApp::Resize(const int w, const int h) {
    minimized_ = (w == 0 || h == 0);
    if (viewer_ && !minimized_) {
        viewer_->Resize(w, h);
    }
}

void DummyApp::AddEvent(const RawInputEv type, const uint32_t key_code, const float x, const float y, const float dx,
                        const float dy) {
    std::shared_ptr<InputManager> input_manager = input_manager_.lock();
    if (!input_manager) {
        return;
    }

    InputManager::Event evt;
    evt.type = type;
    evt.key_code = key_code;
    evt.point.x = x;
    evt.point.y = y;
    evt.move.dx = dx;
    evt.move.dy = dy;
    evt.time_stamp = Sys::GetTimeUs();

    input_manager->AddRawInputEvent(evt);
}

int DummyApp::Run(int argc, char *argv[]) {
    int w = 1280, h = 720;
    fullscreen_ = false;
    const char *device_name = nullptr;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--prepare_assets") == 0) {
            Viewer::PrepareAssets(argv[i + 1]);
            i++;
        } else if (strcmp(arg, "--norun") == 0) {
            return 0;
        } else if ((strcmp(arg, "--width") == 0 || strcmp(arg, "-w") == 0) && (i + 1 < argc)) {
            w = std::atoi(argv[++i]);
        } else if ((strcmp(arg, "--height") == 0 || strcmp(arg, "-h") == 0) && (i + 1 < argc)) {
            h = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--fullscreen") == 0 || strcmp(arg, "-fs") == 0) {
            fullscreen_ = true;
        } else if (strcmp(arg, "--device") == 0 || strcmp(arg, "-d") == 0) {
            device_name = argv[++i];
        }
    }

    if (Init(w, h, device_name) < 0) {
        return -1;
    }

    __itt_thread_set_name("Main Thread");

    bool done = false;
    while (!done) {
        __itt_frame_begin_v3(__g_itt_domain, nullptr);

        MSG msg;
        while (PeekMessage(&msg, nullptr, NULL, NULL, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                done = true;
            } else {
                DispatchMessage(&msg);
            }
        }

        this->Frame();

        __itt_frame_end_v3(__g_itt_domain, nullptr);
    }

    this->Destroy();

    return 0;
}

void DummyApp::PollEvents() {}
