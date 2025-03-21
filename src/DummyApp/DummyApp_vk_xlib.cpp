#include "DummyApp.h"

#include <cstring>

#include <vtune/ittnotify.h>
__itt_domain *__g_itt_domain = __itt_domain_create("Global"); // NOLINT

#if defined(REN_GL_BACKEND)
#include <Ren/GL.h>
#elif defined(REN_SW_BACKEND)

#endif

#if !defined(__ANDROID__)
#include <GL/glx.h>
#endif

#include <Eng/ViewerBase.h>
#include <Eng/input/InputManager.h>
#include <Sys/DynLib.h>
#include <Sys/ThreadWorker.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"

namespace Ren {
extern Display  *g_dpy;
extern Window    g_win;
}

namespace {
DummyApp *g_app = nullptr;

const int KeycodeOffset = 8;

const unsigned char ScancodeToHID_table[256] = {
    0,   41, 30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  45,  46,  42,  43, 20, 26,
    8,   21, 23,  28,  24,  12,  18,  19,  47,  48,  158, 224, 4,   22,  7,   9,  10, 11,
    13,  14, 15,  51,  52,  53,  225, 49,  29,  27,  6,   25,  5,   17,  16,  54, 55, 56,
    229, 85, 226, 44,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  83, 71, 95,
    96,  97, 86,  92,  93,  94,  87,  89,  90,  91,  98,  99,  0,   0,   100, 68, 69, 0,
    0,   0,  0,   0,   0,   0,   88,  228, 84,  154, 230, 0,   74,  82,  75,  80, 79, 77,
    81,  78, 73,  76,  0,   0,   0,   0,   0,   103, 0,   72,  0,   0,   0,   0,  0,  227,
    231, 0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   118, 0,   0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  0,  0,
    0,   0,  0,   0};

uint32_t ScancodeToHID(uint32_t scancode) {
    if (scancode >= 256) {
        return 0;
    }
    return ScancodeToHID_table[scancode];
}
} // namespace

extern "C" {
// Enable High Performance Graphics while using Integrated Graphics
DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;     // Nvidia
DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

DummyApp::DummyApp() { g_app = this; }

DummyApp::~DummyApp() = default;

int DummyApp::Init(const int w, const int h, const AppParams &app_params) {
#if !defined(__ANDROID__)
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        fprintf(stderr, "dpy is null\n");
        return -1;
    }

    const int screen = XDefaultScreen(dpy_);
    Visual *visual = XDefaultVisual(dpy_, screen);
    const int depth  = DefaultDepth(dpy_, screen);

    XSetWindowAttributes swa;
    swa.colormap =
        XCreateColormap(dpy_, RootWindow(dpy_, screen), visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    win_ = XCreateWindow(dpy_, RootWindow(dpy_, screen), 0, 0, w, h, 0, depth,
                         InputOutput, visual,
                         CWBorderPixel | CWColormap | CWEventMask, &swa);

    Atom wm_delete = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy_, win_, &wm_delete, 1);

    XMapWindow(dpy_, win_);
    XStoreName(dpy_, win_, "View [Vulkan]");

    Ren::g_dpy = dpy_;
    Ren::g_win = win_;
#endif

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
    viewer_ = {};
#if !defined(__ANDROID__)
    XDestroyWindow(dpy_, win_);
    //XCloseDisplay(dpy_); // this is done in ContextVK.cpp (https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1894)
#endif
}

#if !defined(__ANDROID__)
int DummyApp::Run(int argc, char *argv[]) {
    int w = 1024, h = 576;
    fullscreen_ = false;
    AppParams app_params;
    ParseArgs(argc, argv, w, h, app_params);

    if (Init(w, h, app_params) < 0) {
        return -1;
    }

    __itt_thread_set_name("Main Thread");

    while (!terminated() && !viewer_->terminated) {
        __itt_frame_begin_v3(__g_itt_domain, nullptr);

        this->PollEvents();

        this->Frame();

#if defined(REN_GL_BACKEND)
        uint64_t swap_start = Sys::GetTimeUs();
        glXSwapBuffers(dpy_, win_);
        uint64_t swap_end = Sys::GetTimeUs();

        auto swap_interval = viewer_->GetComponent<TimeInterval>(SWAP_TIMER_KEY);
        if (swap_interval) {
            swap_interval->start_timepoint_us = swap_start;
            swap_interval->end_timepoint_us = swap_end;
        }
#elif defined(REN_SW_BACKEND)
        // TODO
#endif
        __itt_frame_end_v3(__g_itt_domain, nullptr);
    }

    const int exit_status = viewer_->exit_status;

    this->Destroy();

    return exit_status;
}

#undef None

void DummyApp::PollEvents() {
    if (!input_manager_) {
        return;
    }

    static float last_p1_pos[2] = {0, 0};
    static int last_window_size[2] = {0, 0};

    XEvent xev;
    while (XCheckWindowEvent(dpy_, win_,
                             (ExposureMask | StructureNotifyMask | KeyPressMask |
                              KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask),
                             &xev)) {
        Eng::input_event_t evt;

        if (xev.type == KeyPress) {
            const uint32_t scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                           key_code = ScancodeToHID(scan_code);

            evt.type = Eng::eInputEvent::KeyDown;
            evt.key_code = key_code;
        } else if (xev.type == KeyRelease) {
            const uint32_t scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                           key_code = ScancodeToHID(scan_code);

            evt.type = Eng::eInputEvent::KeyUp;
            evt.key_code = key_code;
        } else if (xev.type == ButtonPress &&
                   (xev.xbutton.button >= Button1 && xev.xbutton.button <= Button5)) {
            if (xev.xbutton.button == Button1) {
                evt.type = Eng::eInputEvent::P1Down;
            } else if (xev.xbutton.button == Button3) {
                evt.type = Eng::eInputEvent::P2Down;
            } else if (xev.xbutton.button == Button4 || xev.xbutton.button == Button5) {
                evt.type = Eng::eInputEvent::MouseWheel;
                evt.move[0] = (xev.xbutton.button == Button4) ? 1 : -1;
            }
            evt.point[0] = float(xev.xbutton.x);
            evt.point[1] = float(xev.xbutton.y);
        } else if (xev.type == ButtonRelease &&
                   (xev.xbutton.button >= Button1 && xev.xbutton.button <= Button5)) {
            if (xev.xbutton.button == Button1) {
                evt.type = Eng::eInputEvent::P1Up;
            } else if (xev.xbutton.button == Button3) {
                evt.type = Eng::eInputEvent::P2Up;
            }
            evt.point[0] = float(xev.xbutton.x);
            evt.point[1] = float(xev.xbutton.y);
        } else if (xev.type == MotionNotify) {
            evt.type = Eng::eInputEvent::P1Move;
            evt.point[0] = float(xev.xmotion.x);
            evt.point[1] = float(xev.xmotion.y);
            evt.move[0] = evt.point[0] - last_p1_pos[0];
            evt.move[1] = last_p1_pos[1] - evt.point[1];

            last_p1_pos[0] = evt.point[0];
            last_p1_pos[1] = evt.point[1];
        } else if (xev.type == ConfigureNotify) {
            if (xev.xconfigure.width != last_window_size[0] ||
                xev.xconfigure.height != last_window_size[1]) {

                Resize(xev.xconfigure.width, xev.xconfigure.height);

                evt.type = Eng::eInputEvent::Resize;
                evt.point[0] = float(xev.xconfigure.width);
                evt.point[1] = float(xev.xconfigure.height);

                last_window_size[0] = xev.xconfigure.width;
                last_window_size[1] = xev.xconfigure.height;
            }
        }

        if (evt.type != Eng::eInputEvent::None) {
            evt.time_stamp = Sys::GetTimeUs();
            input_manager_->AddRawInputEvent(evt);
        }
    }

    if (XCheckTypedWindowEvent(dpy_, win_, ClientMessage, &xev)) {
        if (strcmp(XGetAtomName(dpy_, xev.xclient.message_type), "WM_PROTOCOLS") == 0) {
            quit_ = true;
        }
    }
}

#endif
