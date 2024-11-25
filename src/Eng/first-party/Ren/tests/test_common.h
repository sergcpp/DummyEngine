#pragma once

#include <cmath>
#include <cstdlib>

#include <stdexcept>
#include <string>

static void handle_assert(bool passed, const char* assert, const char* file, long line) {
    if (!passed) {
        printf("Assertion failed %s in %s at line %ld\n", assert, file, line);
        exit(-1);
    }
}

#define require(x) handle_assert(x, #x , __FILE__, __LINE__ )

#define require_throws(expr) {          \
            bool _ = false;             \
            try {                       \
                expr;                   \
            } catch (...) {             \
                _ = true;               \
            }                           \
            require(_);                 \
        }

#define require_nothrow(expr) {         \
            bool _ = false;             \
            try {                       \
                expr;                   \
            } catch (...) {             \
                _ = true;               \
            }                           \
            require(!_);                \
        }

class Approx {
public:
    explicit Approx(double val) : val(val), eps(0.001) {
        require(eps > 0);
    }

    const Approx &epsilon(double _eps) {
        eps = _eps;
        return *this;
    }

    double val, eps;
};

inline bool operator==(double val, const Approx &app) {
    return std::abs(val - app.val) < app.eps;
}

inline bool operator==(float val, const Approx &app) {
    return std::abs(val - app.val) < app.eps;
}

/////////////////////////////////////////////////////////////////////////////////////////////

#include "../Context.h"
#include "../Log.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#undef min
#undef max
#elif defined(__linux__)
#include <X11/Xlib.h>
#if defined(REN_GL_BACKEND)
#include <GL/glx.h>

typedef GLXContext (*GLXCREATECONTEXTATTIBSARBPROC)(Display *, GLXFBConfig, GLXContext,
                                                    Bool, const int *);
typedef void (*GLXSWAPINTERVALEXTPROC)(Display *dpy, GLXDrawable drawable, int interval);
#elif defined(REN_VK_BACKEND)
namespace Ren {
extern Display  *g_dpy;
extern Window    g_win;
}
#endif
#endif

class TestContext : public Ren::Context {
#if defined(_WIN32)
    HWND hWnd;
    HDC hDC;
    [[maybe_unused]] HGLRC hRC;
#else
    Display         *dpy_ = nullptr;
    Window          win_ = {};
#if defined(REN_GL_BACKEND)
    GLXContext      gl_ctx_main_ = {};
#endif
#endif
    Ren::LogNull log_;

  public:
    TestContext() {
#if defined(_WIN32)
        WNDCLASSEX window_class = {};
        window_class.cbSize = sizeof(WNDCLASSEX);
        window_class.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
        window_class.lpfnWndProc = ::DefWindowProc;
        window_class.hInstance = GetModuleHandle(nullptr);
        window_class.lpszClassName = "TestClass";
        window_class.hCursor = LoadCursor(nullptr, IDC_ARROW);
        RegisterClassEx(&window_class);

        hWnd = CreateWindow("TestClass", "!!", WS_OVERLAPPEDWINDOW /*| WS_VISIBLE*/, 0, 0, 100, 100, NULL, NULL,
                            GetModuleHandle(NULL), NULL);

        if (hWnd == NULL) {
            throw std::runtime_error("Cannot create window!");
        }

        hDC = GetDC(hWnd);

#if defined(REN_GL_BACKEND)
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
#endif
#else
        dpy_ = XOpenDisplay(nullptr);
        if (!dpy_) {
            throw std::runtime_error("dpy is null!");
        }

#if defined(REN_GL_BACKEND)
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
        GLXFBConfig *fbc =
            glXChooseFBConfig(dpy_, DefaultScreen(dpy_), attribute_list, &element_count);

        if (!fbc) {
            throw std::runtime_error("fbc is null!");
        }

        XVisualInfo *vi = glXGetVisualFromFBConfig(dpy_, *fbc);
        if (!vi) {
            throw std::runtime_error("vi is null!");
        }

        XSetWindowAttributes swa;
        swa.colormap =
            XCreateColormap(dpy_, RootWindow(dpy_, vi->screen), vi->visual, AllocNone);
        swa.border_pixel = 0;
        swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                         ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

        win_ = XCreateWindow(dpy_, RootWindow(dpy_, vi->screen), 0, 0, 256, 256, 0, vi->depth,
                             InputOutput, vi->visual,
                             CWBorderPixel | CWColormap | CWEventMask, &swa);

        Atom wm_delete = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(dpy_, win_, &wm_delete, 1);

        XMapWindow(dpy_, win_);
        XStoreName(dpy_, win_, "View [OpenGL]");

        auto glXCreateContextAttribsARB = (GLXCREATECONTEXTATTIBSARBPROC)glXGetProcAddress(
            (const GLubyte *)"glXCreateContextAttribsARB");
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

        auto glXSwapIntervalEXT =
            (GLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte *)"glXSwapIntervalEXT");
        if (glXSwapIntervalEXT) {
            glXSwapIntervalEXT(dpy_, glXGetCurrentDrawable(), 1);
        }

        glXMakeCurrent(dpy_, win_, gl_ctx_main_);
        glViewport(0, 0, 256, 256);
#elif defined(REN_VK_BACKEND)
        const int screen = XDefaultScreen(dpy_);
        Visual *visual = XDefaultVisual(dpy_, screen);
        const int depth  = DefaultDepth(dpy_, screen);

        XSetWindowAttributes swa;
        swa.colormap =
            XCreateColormap(dpy_, RootWindow(dpy_, screen), visual, AllocNone);
        swa.border_pixel = 0;
        swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                         ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

        win_ = XCreateWindow(dpy_, RootWindow(dpy_, screen), 0, 0, 256, 256, 0, depth,
                             InputOutput, visual,
                             CWBorderPixel | CWColormap | CWEventMask, &swa);

        Atom wm_delete = XInternAtom(dpy_, "WM_DELETE_WINDOW", 0);
        XSetWMProtocols(dpy_, win_, &wm_delete, 1);

        XMapWindow(dpy_, win_);

        Ren::g_dpy = dpy_;
        Ren::g_win = win_;
#endif

#endif
        Context::Init(256, 256, &log_, 2 /* validation_level */, false, false, {});
    }

    ~TestContext() {
#if defined(_WIN32)
#if defined(REN_GL_BACKEND)
        wglMakeCurrent(NULL, NULL);
        ReleaseDC(hWnd, hDC);
        wglDeleteContext(hRC);
#endif
        DestroyWindow(hWnd);
        UnregisterClass("TestClass", GetModuleHandle(nullptr));
#else
#if defined(REN_GL_BACKEND)
        glXMakeCurrent(dpy_, None, nullptr);
        glXDestroyContext(dpy_, gl_ctx_main_);
#endif
        XDestroyWindow(dpy_, win_);
#if defined(REN_GL_BACKEND)
        XCloseDisplay(dpy_);
#endif
#endif
    }
};

#undef Success
#undef None