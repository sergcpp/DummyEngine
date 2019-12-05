#include "DummyApp.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#elif defined(USE_SW_RENDER)

#endif

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include <cctype>

#include <Windows.h>

#include <Eng/GameBase.h>
#include <Eng/TimedInput.h>
#include <Sys/DynLib.h>
#include <Sys/Log.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"

namespace {
    DummyApp *g_app = nullptr;
}

extern "C" {
    // Enable High Performance Graphics while using Integrated Graphics
    DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;        // Nvidia
    DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;    // AMD
}

typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC) (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int *attribList);

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

        g_app->AddEvent(InputManager::RAW_INPUT_P1_DOWN, -1, -1, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_RBUTTONDOWN: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(InputManager::RAW_INPUT_P2_DOWN, -1, -1, px, py, 0.0f, 0.0f);
        break;
    }
     case WM_LBUTTONUP: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(InputManager::RAW_INPUT_P1_UP, -1, -1, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_RBUTTONUP: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(InputManager::RAW_INPUT_P2_UP, -1, -1, px, py, 0.0f, 0.0f);
        break;
    }
    case WM_MOUSEMOVE: {
        float px = (float)LOWORD(lParam), py = (float)HIWORD(lParam);

        g_app->AddEvent(InputManager::RAW_INPUT_P1_MOVE, -1, -1, px, py, px - last_p1_pos[0], py - last_p1_pos[1]);

        last_p1_pos[0] = px;
        last_p1_pos[1] = py;
        break;
    }
    case WM_KEYDOWN: {
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
        } else {
            InputManager::RawInputButton key;
            int raw_key = int(wParam);
            if (DummyApp::ConvertToRawButton(raw_key, key)) {
                g_app->AddEvent(InputManager::RAW_INPUT_KEY_DOWN, key, raw_key, 0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
        break;
    }
    case WM_KEYUP: {
        InputManager::RawInputButton key;
        int raw_key = int(wParam);
        if (DummyApp::ConvertToRawButton(raw_key, key)) {
            g_app->AddEvent(InputManager::RAW_INPUT_KEY_UP, key, raw_key, 0.0f, 0.0f, 0.0f, 0.0f);
        }
        break;
    }
    case WM_SIZE: {
        int w = LOWORD(lParam), h = HIWORD(lParam);
        g_app->Resize(w, h);
        g_app->AddEvent(InputManager::RAW_INPUT_RESIZE, 0, 0, (float)w, (float)h, 0.0f, 0.0f);
    }
    default: {
        break;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int DummyApp::Init(int w, int h) {
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
        LOGE("ChoosePixelFormat() failed");
        return -1;
    }

    if (!SetPixelFormat(fake_dc, pix_format_id, &pixel_format)) {
        LOGE("SetPixelFormat() failed");
        return -1;
    }

    HGLRC fake_rc = wglCreateContext(fake_dc);

    if (!fake_rc) {
        LOGE("wglCreateContext() failed");
        return -1;
    }

    if (!wglMakeCurrent(fake_dc, fake_rc)) {
        LOGE("wglMakeCurrent() failed");
        return -1;
    }

    PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = nullptr;
    wglChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(wglGetProcAddress("wglChoosePixelFormatARB"));
    if (wglChoosePixelFormatARB == nullptr) {
        LOGE("wglGetProcAddress() failed");
        return -1;
    }

    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
    wglCreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));
    if (wglCreateContextAttribsARB == nullptr) {
        LOGE("wglGetProcAddress() failed");
        return -1;
    }

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
        LOGE("wglChoosePixelFormatARB() failed");
        return -1;
    }

    PIXELFORMATDESCRIPTOR PFD;
    int res = DescribePixelFormat(device_context_, pix_format_id, sizeof(PFD), &PFD);
    if (!res) {
        LOGE("DescribePixelFormat() failed");
        return -1;
    }
    ret = SetPixelFormat(device_context_, pix_format_id, &PFD);
    if (!ret) {
        LOGE("SetPixelFormat() failed");
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
        LOGE("wglCreateContextAttribsARB() failed");
        return -1;
    }

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(fake_rc);
    ReleaseDC(fake_window, fake_dc);
    DestroyWindow(fake_window);
    if (!wglMakeCurrent(device_context_, gl_ctx_)) {
        LOGE("wglMakeCurrent() failed");
        return -1;
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

void DummyApp::AddEvent(int type, int key, int raw_key, float x, float y, float dx, float dy) {
    std::shared_ptr<InputManager> input_manager = input_manager_.lock();
    if (!input_manager) return;

    InputManager::Event evt;
    evt.type = (InputManager::RawInputEvent)type;
    evt.key = (InputManager::RawInputButton)key;
    evt.raw_key = raw_key;
    evt.point.x = x;
    evt.point.y = y;
    evt.move.dx = dx;
    evt.move.dy = dy;
    evt.time_stamp = Sys::GetTimeUs();

    input_manager->AddRawInputEvent(evt);
}

int DummyApp::Run(const std::vector<std::string> &args) {
    int w = 1280, h = 720;
    fullscreen_ = false;

    int args_count = (int)args.size();
    for (int i = 0; i < args_count; i++) {
        const std::string &arg = args[i];
        if (arg == "--prepare_assets") {
            Viewer::PrepareAssets(args[i + 1].c_str());
            i++;
        } else if (arg == "--norun") {
            return 0;
        } else if ((arg == "--width" || arg == "-w") && i < args_count) {
            w = std::atoi(args[++i].c_str());   
        } else if ((arg == "--height" || arg == "-h") && i < args_count) {
            h = std::atoi(args[++i].c_str());
        } else if ((arg == "--fullscreen") || (arg == "-fs")) {
            fullscreen_ = true;
        }
    }

    if (Init(w, h) < 0) {
        return -1;
    }

    MSG msg;
    bool done = false;
    while (!done) {
        while (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                done = true;
            } else {
                DispatchMessage(&msg);
            }
        }

        this->Frame();

        SwapBuffers(device_context_);
    }

    this->Destroy();

    return 0;
}

bool DummyApp::ConvertToRawButton(int &raw_key, InputManager::RawInputButton &button) {
    switch (raw_key) {
    case VK_UP:
        button = InputManager::RAW_INPUT_BUTTON_UP;
        break;
    case VK_DOWN:
        button = InputManager::RAW_INPUT_BUTTON_DOWN;
        break;
    case VK_LEFT:
        button = InputManager::RAW_INPUT_BUTTON_LEFT;
        break;
    case VK_RIGHT:
        button = InputManager::RAW_INPUT_BUTTON_RIGHT;
        break;
    case VK_ESCAPE:
        button = InputManager::RAW_INPUT_BUTTON_EXIT;
        break;
    case VK_TAB:
        button = InputManager::RAW_INPUT_BUTTON_TAB;
        break;
    case VK_BACK:
        button = InputManager::RAW_INPUT_BUTTON_BACKSPACE;
        break;
    case VK_SHIFT:
        button = InputManager::RAW_INPUT_BUTTON_SHIFT;
        break;
    case VK_DELETE:
        button = InputManager::RAW_INPUT_BUTTON_DELETE;
        break;
    case VK_SPACE:
        button = InputManager::RAW_INPUT_BUTTON_SPACE;
        break;
    case VK_RETURN:
        button = InputManager::RAW_INPUT_BUTTON_RETURN;
        break;
    case VK_OEM_3:
        button = InputManager::RAW_INPUT_BUTTON_OTHER;
        raw_key = (int)'`';
        break;
    case VK_OEM_MINUS:
        button = InputManager::RAW_INPUT_BUTTON_OTHER;
        raw_key = (int)'-';
        break;
    default:
        button = InputManager::RAW_INPUT_BUTTON_OTHER;
        raw_key = std::tolower(raw_key);
        break;
    }

    return true;
}

void DummyApp::PollEvents() {
}
