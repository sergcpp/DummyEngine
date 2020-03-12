#include "DummyApp.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

#ifndef RELEASE_FINAL
#include <vtune/ittnotify.h>
#endif

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#elif defined(USE_SW_RENDER)

#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <cctype>

#include <iostream>

#include <Windows.h>

#include <Eng/GameBase.h>
#include <Eng/Input/InputManager.h>
#include <Sys/DynLib.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"

namespace {
    DummyApp *g_app = nullptr;

    uint32_t ScancodeFromLparam(LPARAM lparam) {
        return ((lparam >> 16) & 0x7f) | ((lparam & (1 << 24)) != 0 ? 0x80 : 0);
    }

    static const unsigned char ScancodeToHID_table[256] = {
        0,41,30,31,32,33,34,35,36,37,38,39,45,46,42,43,20,26,8,21,23,28,24,12,18,19,
        47,48,158,224,4,22,7,9,10,11,13,14,15,51,52,53,225,49,29,27,6,25,5,17,16,54,
        55,56,229,0,226,44,57,58,59,60,61,62,63,64,65,66,67,72,71,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,68,69,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,228,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,70,230,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,74,82,75,0,80,0,79,0,77,81,78,73,76,0,0,0,0,0,0,0,227,231,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    uint32_t ScancodeToHID(uint32_t scancode) {
        if (scancode >= 256) {
            return 0;
        }
        return (uint32_t)ScancodeToHID_table[scancode];
    }
}

extern "C" {
    // Enable High Performance Graphics while using Integrated Graphics
    DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;        // Nvidia
    DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;    // AMD
}

typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC) (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int *attribList);
typedef BOOL (WINAPI* PFNWGLSWAPINTERVALEXTPROC) (int interval);

#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_STENCIL_BITS_ARB              0x2023
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_TYPE_RGBA_ARB                 0x202B

#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092

#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001

DummyApp::DummyApp() {
    g_app = this;
}

DummyApp::~DummyApp() {

}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static float last_p1_pos[2] = { 0.0f, 0.0f }, last_p2_pos[2] = { 0.0f, 0.0f };

    switch (uMsg) {
    case WM_CLOSE: {
        PostQuitMessage(0);
        break;
    }
    case WM_LBUTTONDOWN: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(EvP1Down, 0, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_RBUTTONDOWN: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(EvP2Down, 0, px, py, 0.0f, 0.0f);
        break;
    }
     case WM_LBUTTONUP: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(EvP1Up, 0, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_RBUTTONUP: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(EvP2Up, 0, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_MOUSEMOVE: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(EvP1Move, 0, px, py, px - last_p1_pos[0], py - last_p1_pos[1]);

        last_p1_pos[0] = px;
        last_p1_pos[1] = py;
        break;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        } else {
            const uint32_t
                scan_code = ScancodeFromLparam(lParam),
                key_code = ScancodeToHID(scan_code);

            g_app->AddEvent(EvKeyDown, key_code, 0.0f, 0.0f, 0.0f, 0.0f);
        }
        break;
    }
    case WM_KEYUP: {
        const uint32_t
            scan_code = ScancodeFromLparam(lParam),
            key_code = ScancodeToHID(scan_code);

        g_app->AddEvent(EvKeyUp, key_code, 0.0f, 0.0f, 0.0f, 0.0f);
        break;
    }
    case WM_MOUSEWHEEL: {
        WORD _delta = HIWORD(wParam);
        auto delta = reinterpret_cast<const short &>(_delta);
        const float wheel_motion = float(delta / WHEEL_DELTA);
        g_app->AddEvent(EvMouseWheel, 0, 0.0f, 0.0f, wheel_motion, 0.0f);
        break;
    }
    case WM_SIZE: {
        const int w = LOWORD(lParam), h = HIWORD(lParam);
        g_app->Resize(w, h);
        g_app->AddEvent(EvResize, 0, (float)w, (float)h, 0.0f, 0.0f);
    }
    default: {
        break;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int DummyApp::Init(int w, int h) {
    BOOL dpi_result = SetProcessDPIAware();
    (void)dpi_result;

    WNDCLASSEX window_class = {};
    window_class.cbSize = sizeof(WNDCLASSEX);
    window_class.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = GetModuleHandle(NULL);
    window_class.lpszClassName = "MainWindowClass";
    window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassEx(&window_class);

    RECT rect;
    rect.left = rect.top = 0;
    rect.right = w;
    rect.bottom = h;
    BOOL ret = ::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW | WS_VISIBLE, false);
    if (!ret) {
        return -1;
    }

    HWND fake_window = ::CreateWindowEx(NULL, "MainWindowClass", "View",
                                        WS_OVERLAPPEDWINDOW,
                                        CW_USEDEFAULT, CW_USEDEFAULT,
                                        rect.right - rect.left,
                                        rect.bottom - rect.top,
                                        NULL,
                                        NULL,
                                        GetModuleHandle(NULL),
                                        NULL);

    HDC fake_dc = GetDC(fake_window);

    PIXELFORMATDESCRIPTOR pixel_format;
    ZeroMemory(&pixel_format, sizeof(pixel_format));
    pixel_format.nSize = sizeof(pixel_format);
    pixel_format.nVersion = 1;
    pixel_format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pixel_format.iPixelType = PFD_TYPE_RGBA;
    pixel_format.cColorBits = 32;
    pixel_format.cAlphaBits = 8;
    pixel_format.cDepthBits = 0;

    int pix_format_id = ChoosePixelFormat(fake_dc, &pixel_format);
    if (pix_format_id == 0) {
        std::cerr << "ChoosePixelFormat() failed\n";
        return -1;
    }

    if (!SetPixelFormat(fake_dc, pix_format_id, &pixel_format)) {
        std::cerr << "SetPixelFormat() failed\n";
        return -1;
    }

    HGLRC fake_rc = wglCreateContext(fake_dc);

    if (!fake_rc) {
        std::cerr << "wglCreateContext() failed\n";
        return -1;
    }

    if (!wglMakeCurrent(fake_dc, fake_rc)) {
        std::cerr << "wglMakeCurrent() failed\n";
        return -1;
    }

    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
    wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));
    if (wglChoosePixelFormatARB == nullptr) {
        std::cerr << "wglGetProcAddress() failed\n";
        return -1;
    }

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
    wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
    if (wglCreateContextAttribsARB == nullptr) {
        std::cerr << "wglGetProcAddress() failed\n";
        return -1;
    }

    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = nullptr;
    wglSwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(wglGetProcAddress("wglSwapIntervalEXT"));

    window_handle_ = ::CreateWindowEx(NULL, "MainWindowClass", "View",
                                      WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                      CW_USEDEFAULT, CW_USEDEFAULT,
                                      rect.right - rect.left,
                                      rect.bottom - rect.top,
                                      NULL,
                                      NULL,
                                      GetModuleHandle(NULL),
                                      NULL);

    device_context_ = GetDC(window_handle_);

    const int pixel_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
        WGL_COLOR_BITS_ARB, 24,
        0
    };

    UINT format_count;
    BOOL status = wglChoosePixelFormatARB(device_context_, pixel_attribs, NULL, 1, &pix_format_id, &format_count);

    if (!status || format_count == 0) {
        std::cerr << "wglChoosePixelFormatARB() failed\n";
        return -1;
    }

    PIXELFORMATDESCRIPTOR PFD;
    int res = DescribePixelFormat(device_context_, pix_format_id, sizeof(PFD), &PFD);
    if (!res) {
        std::cerr << "DescribePixelFormat() failed\n";
        return -1;
    }
    ret = SetPixelFormat(device_context_, pix_format_id, &PFD);
    if (!ret) {
        std::cerr << "SetPixelFormat() failed\n";
        return -1;
    }

    int context_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    gl_ctx_ = wglCreateContextAttribsARB(device_context_, 0, context_attribs);
    if (!gl_ctx_) {
        std::cerr << "wglCreateContextAttribsARB() failed\n";
        return -1;
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(fake_rc);
    ReleaseDC(fake_window, fake_dc);
    DestroyWindow(fake_window);
    if (!wglMakeCurrent(device_context_, gl_ctx_)) {
        std::cerr << "wglMakeCurrent() failed\n";
        return -1;
    }

    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(0);
    }

    try {
        Viewer::PrepareAssets("pc");
        viewer_.reset(new Viewer(w, h, nullptr));

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

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(gl_ctx_);
    ReleaseDC(window_handle_, device_context_);
    DestroyWindow(window_handle_);

    UnregisterClass("MainWindowClass", GetModuleHandle(NULL));
}

void DummyApp::Frame() {
    viewer_->Frame();
}

void DummyApp::Resize(int w, int h) {
    if (viewer_) {
        viewer_->Resize(w, h);
    }
}

void DummyApp::AddEvent(int type, uint32_t key_code, float x, float y, float dx, float dy) {
    std::shared_ptr<InputManager> input_manager = input_manager_.lock();
    if (!input_manager) return;

    InputManager::Event evt;
    evt.type = (RawInputEvent)type;
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
        }
    }

    if (Init(w, h) < 0) {
        return -1;
    }

#ifndef RELEASE_FINAL
    __itt_thread_set_name("Main Thread");

    __itt_domain *pFrameDomain = __itt_domain_create("Frame domain");
    pFrameDomain->flags = 1;
#endif

    MSG msg;
    bool done = false;
    while (!done) {
#ifndef RELEASE_FINAL
        __itt_frame_begin_v3(pFrameDomain, nullptr);
#endif

        while (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                done = true;
            } else {
                DispatchMessage(&msg);
            }
        }

        this->Frame();

        SwapBuffers(device_context_);
#ifndef RELEASE_FINAL
        __itt_frame_end_v3(pFrameDomain, nullptr);
#endif
    }

    this->Destroy();

    return 0;
}

void DummyApp::PollEvents() {
}
