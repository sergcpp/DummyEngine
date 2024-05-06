#include "DummyApp.h"

#include <cstring>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

#include <vtune/ittnotify.h>
__itt_domain *__g_itt_domain = __itt_domain_create("Global"); // NOLINT

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#elif defined(USE_SW_RENDER)

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

#if !defined(__ANDROID__)
class AuxGfxThread : public Sys::ThreadWorker {
    Display *dpy_;
    GLXContext gl_ctx_;

  public:
    AuxGfxThread(Display *dpy, GLXContext gl_ctx) : dpy_(dpy), gl_ctx_(gl_ctx) {
        AddTask([this]() {
            __itt_thread_set_name("AuxGfxThread");
            glXMakeCurrent(dpy_, None, gl_ctx_);
        });
    }

    ~AuxGfxThread() override { AddTask(::glXMakeCurrent, dpy_, None, nullptr); }
};
#endif
} // namespace

extern "C" {
// Enable High Performance Graphics while using Integrated Graphics
DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;     // Nvidia
DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

#if !defined(__ANDROID__)
typedef GLXContext (*GLXCREATECONTEXTATTIBSARBPROC)(Display *, GLXFBConfig, GLXContext,
                                                    Bool, const int *);
typedef void (*GLXSWAPINTERVALEXTPROC)(Display *dpy, GLXDrawable drawable, int interval);
#endif

DummyApp::DummyApp() { g_app = this; }

DummyApp::~DummyApp() = default;

int DummyApp::Init(const int w, const int h, const AppParams &app_params) {
#if !defined(__ANDROID__)
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        fprintf(stderr, "dpy is null\n");
        return -1;
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
    GLXFBConfig *fbc =
        glXChooseFBConfig(dpy_, DefaultScreen(dpy_), attribute_list, &element_count);

    if (!fbc) {
        fprintf(stderr, "fbc is null\n");
        return -1;
    }

    XVisualInfo *vi = glXGetVisualFromFBConfig(dpy_, *fbc);
    if (!vi) {
        fprintf(stderr, "vi is null\n");
        return -1;
    }

    XSetWindowAttributes swa;
    swa.colormap =
        XCreateColormap(dpy_, RootWindow(dpy_, vi->screen), vi->visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask |
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

    win_ = XCreateWindow(dpy_, RootWindow(dpy_, vi->screen), 0, 0, w, h, 0, vi->depth,
                         InputOutput, vi->visual,
                         CWBorderPixel | CWColormap | CWEventMask, &swa);

    Atom wm_delete = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy_, win_, &wm_delete, 1);

    XMapWindow(dpy_, win_);
    XStoreName(dpy_, win_, "View (GL)");

    auto glXCreateContextAttribsARB = (GLXCREATECONTEXTATTIBSARBPROC)glXGetProcAddress(
        (const GLubyte *)"glXCreateContextAttribsARB");
    if (!glXCreateContextAttribsARB) {
        fprintf(stderr, "glXCreateContextAttribsARB was not loaded\n");
        return -1;
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
        return -1;
    }
    gl_ctx_aux_ = glXCreateContextAttribsARB(dpy_, *fbc, gl_ctx_main_, true, attribs);
    if (!gl_ctx_aux_) {
        fprintf(stderr, "ctx is null\n");
        return -1;
    }

    printf("Extensions: %s \n\n\n", glXQueryExtensionsString(dpy_, DefaultScreen(dpy_)));

    auto glXSwapIntervalEXT =
        (GLXSWAPINTERVALEXTPROC)glXGetProcAddress((const GLubyte *)"glXSwapIntervalEXT");
    if (glXSwapIntervalEXT) {
        glXSwapIntervalEXT(dpy_, glXGetCurrentDrawable(), 1);
    }

    glXMakeCurrent(dpy_, win_, gl_ctx_main_);
    glViewport(0, 0, w, h);
#endif

    try {
        Viewer::PrepareAssets("pc");
        log_ = std::make_unique<LogStdout>();
        viewer_ = std::make_unique<Viewer>(w, h, nullptr, app_params, log_.get());
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
    glXMakeCurrent(dpy_, None, nullptr);
    glXDestroyContext(dpy_, gl_ctx_aux_);
    glXDestroyContext(dpy_, gl_ctx_main_);
    XDestroyWindow(dpy_, win_);
    XCloseDisplay(dpy_);
#endif
}

void DummyApp::Frame() { viewer_->Frame(); }

void DummyApp::Resize(int w, int h) { viewer_->Resize(w, h); }

void DummyApp::AddEvent(Eng::RawInputEv type, const uint32_t key_code, const float x,
                        const float y, const float dx, const float dy) {
    if (!input_manager_) {
        return;
    }

    Eng::InputManager::Event evt;
    evt.type = type;
    evt.key_code = key_code;
    evt.point.x = x;
    evt.point.y = y;
    evt.move.dx = dx;
    evt.move.dy = dy;
    evt.time_stamp = Sys::GetTimeUs();

    input_manager_->AddRawInputEvent(evt);
}

#if !defined(__ANDROID__)
int DummyApp::Run(int argc, char *argv[]) {
    int w = 1024, h = 576;
    AppParams app_params;
    fullscreen_ = false;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (strcmp(arg, "--prepare_assets") == 0) {
            Viewer::PrepareAssets(argv[i + 1]);
            i++;
        } else if (strcmp(arg, "--norun") == 0) {
            return 0;
        } else if ((strcmp(arg, "--width") == 0 || strcmp(arg, "-w") == 0) &&
                   (i + 1 < argc)) {
            w = std::atoi(argv[++i]);
        } else if ((strcmp(arg, "--height") == 0 || strcmp(arg, "-h") == 0) &&
                   (i + 1 < argc)) {
            h = std::atoi(argv[++i]);
        } else if (strcmp(arg, "--fullscreen") == 0 || strcmp(arg, "-fs") == 0) {
            fullscreen_ = true;
        } else if (strcmp(arg, "--validation_level") == 0 || strcmp(arg, "-vl") == 0) {
            app_params.validation_level = std::atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--scene") == 0 || strcmp(argv[i], "-s") == 0) && (++i != argc)) {
            app_params.scene_name = argv[i];
        }
    }

    if (Init(w, h, app_params) < 0) {
        return -1;
    }

    __itt_thread_set_name("Main Thread");

    while (!terminated()) {
        __itt_frame_begin_v3(__g_itt_domain, nullptr);

        this->PollEvents();

        this->Frame();

#if defined(USE_GL_RENDER)
        uint64_t swap_start = Sys::GetTimeUs();
        glXSwapBuffers(dpy_, win_);
        uint64_t swap_end = Sys::GetTimeUs();

        /*auto swap_interval = viewer_->GetComponent<Eng::TimeInterval>(SWAP_TIMER_KEY);
        if (swap_interval) {
            swap_interval->start_timepoint_us = swap_start;
            swap_interval->end_timepoint_us = swap_end;
        }*/
#elif defined(USE_SW_RENDER)
        // TODO
#endif
        __itt_frame_end_v3(__g_itt_domain, nullptr);
    }

    this->Destroy();

    return 0;
}

#undef None

void DummyApp::PollEvents() {
    if (!input_manager_) {
        return;
    }

    static float last_p1_pos[2] = {0.0f, 0.0f};
    static int last_window_size[2] = {0, 0};

    XEvent xev;
    while (XCheckWindowEvent(dpy_, win_,
                             (ExposureMask | StructureNotifyMask | KeyPressMask |
                              KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask),
                             &xev)) {
        Eng::InputManager::Event evt;

        if (xev.type == KeyPress) {
            const uint32_t scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                           key_code = ScancodeToHID(scan_code);

            if (key_code == Eng::KeyEscape) {
                quit_ = true;
            } else {
                evt.type = Eng::RawInputEv::KeyDown;
                evt.key_code = key_code;
            }
        } else if (xev.type == KeyRelease) {
            const uint32_t scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                           key_code = ScancodeToHID(scan_code);

            evt.type = Eng::RawInputEv::KeyUp;
            evt.key_code = key_code;
        } else if (xev.type == ButtonPress &&
                   (xev.xbutton.button >= Button1 && xev.xbutton.button <= Button5)) {
            if (xev.xbutton.button == Button1) {
                evt.type = Eng::RawInputEv::P1Down;
            } else if (xev.xbutton.button == Button3) {
                evt.type = Eng::RawInputEv::P2Down;
            } else if (xev.xbutton.button == Button4 || xev.xbutton.button == Button5) {
                evt.type = Eng::RawInputEv::MouseWheel;
                evt.move.dx = (xev.xbutton.button == Button4) ? 1.0f : -1.0f;
            }
            evt.point.x = float(xev.xbutton.x);
            evt.point.y = float(xev.xbutton.y);
        } else if (xev.type == ButtonRelease &&
                   (xev.xbutton.button >= Button1 && xev.xbutton.button <= Button5)) {
            if (xev.xbutton.button == Button1) {
                evt.type = Eng::RawInputEv::P1Up;
            } else if (xev.xbutton.button == Button3) {
                evt.type = Eng::RawInputEv::P2Up;
            }
            evt.point.x = float(xev.xbutton.x);
            evt.point.y = float(xev.xbutton.y);
        } else if (xev.type == MotionNotify) {
            evt.type = Eng::RawInputEv::P1Move;
            evt.point.x = float(xev.xmotion.x);
            evt.point.y = float(xev.xmotion.y);
            evt.move.dx = evt.point.x - last_p1_pos[0];
            evt.move.dy = evt.point.y - last_p1_pos[1];

            last_p1_pos[0] = evt.point.x;
            last_p1_pos[1] = evt.point.y;
        } else if (xev.type == ConfigureNotify) {
            if (xev.xconfigure.width != last_window_size[0] ||
                xev.xconfigure.height != last_window_size[1]) {

                Resize(xev.xconfigure.width, xev.xconfigure.height);

                evt.type = Eng::RawInputEv::Resize;
                evt.point.x = (float)xev.xconfigure.width;
                evt.point.y = (float)xev.xconfigure.height;

                last_window_size[0] = xev.xconfigure.width;
                last_window_size[1] = xev.xconfigure.height;
            }
        }

        if (evt.type != Eng::RawInputEv::None) {
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
