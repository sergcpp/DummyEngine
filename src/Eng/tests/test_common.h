#pragma once

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <atomic>

extern bool g_stop_on_fail;
extern std::atomic_bool g_tests_success;

static bool handle_assert(const bool passed, const char *assert, const char *file, const long line, const bool fatal) {
    if (!passed) {
        printf("Assertion failed %s in %s at line %d\n", assert, file, int(line));
        g_tests_success = false;
        if (fatal) {
            exit(-1);
        }
    }
    return passed;
}

#define require(x) handle_assert(x, #x, __FILE__, __LINE__, g_stop_on_fail)
#define require_fatal(x) handle_assert(x, #x, __FILE__, __LINE__, true)
#define require_return(x)                                                                                              \
    {                                                                                                                  \
        const bool __res = bool(x);                                                                                    \
        handle_assert(__res, #x, __FILE__, __LINE__, false);                                                           \
        if (!__res) {                                                                                                  \
            return;                                                                                                    \
        }                                                                                                              \
    }

#define require_throws(expr)                                                                                           \
    {                                                                                                                  \
        bool _ = false;                                                                                                \
        try {                                                                                                          \
            expr;                                                                                                      \
        } catch (...) {                                                                                                \
            _ = true;                                                                                                  \
        }                                                                                                              \
        assert(_);                                                                                                     \
    }

#define require_nothrow(expr)                                                                                          \
    {                                                                                                                  \
        bool _ = false;                                                                                                \
        try {                                                                                                          \
            expr;                                                                                                      \
        } catch (...) {                                                                                                \
            _ = true;                                                                                                  \
        }                                                                                                              \
        assert(!_);                                                                                                    \
    }

class Approx {
  public:
    explicit Approx(const double val) : val(val), eps(0.001) { require(eps > 0); }

    const Approx &epsilon(const double _eps) {
        eps = _eps;
        return *this;
    }

    double val, eps;
};

inline bool operator==(const double val, const Approx &app) { return std::abs(val - app.val) < app.eps; }
inline bool operator==(const float val, const Approx &app) { return std::abs(val - app.val) < app.eps; }

/////////////////////////////////////////////////////////////////////////////////////////////

#include "../Log.h"
#include <Ren/Log.h>
#include <Snd/Log.h>
#include <atomic>
#include <cstdarg>
#include <mutex>
#include <string_view>

extern std::atomic_bool g_log_contains_errors;

class LogErr final : public Ren::ILog, public Snd::ILog {
    FILE *err_out_ = nullptr;
    std::mutex mtx_;

  public:
    LogErr() {
#pragma warning(suppress : 4996)
        err_out_ = fopen("errors.txt", "w");
    }
    ~LogErr() override { fclose(err_out_); }

    void Info(const char *fmt, ...) override {
        // ignored
    }
    void Warning(const char *fmt, ...) override {
        // ignored
    }
    void Error(const char *fmt, ...) override {
        std::lock_guard<std::mutex> _(mtx_);

        va_list vl;
        va_start(vl, fmt);
        vfprintf(err_out_, fmt, vl);
        va_end(vl);
        putc('\n', err_out_);
        fflush(err_out_);
        g_log_contains_errors = true;
    }
};

/////////////////////////////////////////////////////////////////////////////////////////////

#include <Ren/Context.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#if defined(USE_GL_RENDER)
#include <GL/glx.h>

typedef GLXContext (*GLXCREATECONTEXTATTIBSARBPROC)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
typedef void (*GLXSWAPINTERVALEXTPROC)(Display *dpy, GLXDrawable drawable, int interval);
#elif defined(USE_VK_RENDER)
namespace Ren {
extern Display *g_dpy;
extern Window g_win;
} // namespace Ren
#endif
#endif

class TestContext : public Ren::Context {
#if defined(USE_GL_RENDER)
#if defined(_WIN32)
    HWND hWnd;
    HDC hDC;
    HGLRC hRC;
#else
    Display *dpy_ = nullptr;
    Window win_ = {};
    GLXContext gl_ctx_main_ = {};
#endif
#endif
    Ren::ILog *log_;

  public:
    TestContext(const int w, const int h, std::string_view device_name, int validation_level, bool nohwrt,
                Ren::ILog *log)
        : log_(log) {
#if defined(USE_GL_RENDER)
#if defined(_WIN32)
        WNDCLASSEX window_class = {};
        window_class.cbSize = sizeof(WNDCLASSEX);
        window_class.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
        window_class.lpfnWndProc = ::DefWindowProc;
        window_class.hInstance = GetModuleHandle(nullptr);
        window_class.lpszClassName = "TestClass";
        window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&window_class);

        hWnd = CreateWindow("TestClass", "!!", WS_OVERLAPPEDWINDOW /*| WS_VISIBLE*/, 0, 0, w, h, NULL, NULL,
                            GetModuleHandle(NULL), NULL);

        if (hWnd == NULL) {
            throw std::runtime_error("Cannot create window!");
        }

        hDC = GetDC(hWnd);

        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;

        int pf = ChoosePixelFormat(hDC, &pfd);
        if (pf == 0) {
            throw std::runtime_error("Cannot find pixel format!");
        }

        if (SetPixelFormat(hDC, pf, &pfd) == FALSE) {
            throw std::runtime_error("Cannot set pixel format!");
        }

        DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);
#else
        dpy_ = XOpenDisplay(nullptr);
        if (!dpy_) {
            throw std::runtime_error("dpy is null!");
        }

        static const int attribute_list[] = {GLX_X_RENDERABLE,
                                             True,
                                             GLX_DRAWABLE_TYPE,
                                             GLX_WINDOW_BIT,
                                             GLX_RENDER_TYPE,
                                             GLX_RGBA_BIT,
                                             GLX_X_VISUAL_TYPE,
                                             GLX_TRUE_COLOR,
                                             GLX_RED_SIZE,
                                             8,
                                             GLX_GREEN_SIZE,
                                             8,
                                             GLX_BLUE_SIZE,
                                             8,
                                             GLX_ALPHA_SIZE,
                                             0,
                                             GLX_DEPTH_SIZE,
                                             0,
                                             GLX_STENCIL_SIZE,
                                             0,
                                             GLX_DOUBLEBUFFER,
                                             True,
                                             None};

        int element_count = 0;
        GLXFBConfig *fbc = glXChooseFBConfig(dpy_, DefaultScreen(dpy_), attribute_list, &element_count);

        if (!fbc) {
            throw std::runtime_error("fbc is null!");
        }

        XVisualInfo *vi = glXGetVisualFromFBConfig(dpy_, *fbc);
        if (!vi) {
            throw std::runtime_error("vi is null!");
        }

        XSetWindowAttributes swa;
        swa.colormap = XCreateColormap(dpy_, RootWindow(dpy_, vi->screen), vi->visual, AllocNone);
        swa.border_pixel = 0;
        swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask |
                         ButtonReleaseMask | PointerMotionMask;

        win_ = XCreateWindow(dpy_, RootWindow(dpy_, vi->screen), 0, 0, 256, 256, 0, vi->depth, InputOutput, vi->visual,
                             CWBorderPixel | CWColormap | CWEventMask, &swa);

        Atom wm_delete = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy_, win_, &wm_delete, 1);

        XMapWindow(dpy_, win_);
        XStoreName(dpy_, win_, "View (GL)");

        auto glXCreateContextAttribsARB =
            (GLXCREATECONTEXTATTIBSARBPROC)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
        if (!glXCreateContextAttribsARB) {
            throw std::runtime_error("glXCreateContextAttribsARB was not loaded!");
        }

        int attribs[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                         4,
                         GLX_CONTEXT_MINOR_VERSION_ARB,
                         3,
                         GLX_CONTEXT_FLAGS_ARB,
                         0,
                         GLX_CONTEXT_PROFILE_MASK_ARB,
                         GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
                         0};

        gl_ctx_main_ = glXCreateContextAttribsARB(dpy_, *fbc, nullptr, true, attribs);
        if (!gl_ctx_main_) {
            fprintf(stderr, "ctx is null\n");
            throw std::runtime_error("ctx is null!");
        }

        auto glXSwapIntervalEXT = (GLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte *)"glXSwapIntervalEXT");
        if (glXSwapIntervalEXT) {
            glXSwapIntervalEXT(dpy_, glXGetCurrentDrawable(), 1);
        }

        glXMakeCurrent(dpy_, win_, gl_ctx_main_);
        glViewport(0, 0, 256, 256);
#endif
#endif
        if (!Ren::Context::Init(w, h, log_, validation_level, nohwrt, device_name)) {
            throw std::runtime_error("Initialization failed!");
        }
    }

    ~TestContext() {
#if defined(USE_GL_RENDER)
#if defined(_WIN32)
        wglMakeCurrent(NULL, NULL);
        ReleaseDC(hWnd, hDC);
        wglDeleteContext(hRC);
        DestroyWindow(hWnd);
        UnregisterClass("TestClass", GetModuleHandle(nullptr));
#else
        glXMakeCurrent(dpy_, None, nullptr);
        glXDestroyContext(dpy_, gl_ctx_main_);
        XDestroyWindow(dpy_, win_);
        XCloseDisplay(dpy_);
#endif
#endif
    }
};

#undef Success
#undef None
#undef True
#undef False
#undef Convex