#include "DummyApp.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#elif defined(USE_SW_RENDER)

#endif

#if !defined(__ANDROID__)
#include <GL/glx.h>
#endif

#include <Eng/GameBase.h>
#include <Eng/Input/InputManager.h>
#include <Sys/DynLib.h>
#include <Sys/Log.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"

namespace {
    DummyApp *g_app = nullptr;

    const int KeycodeOffset = 8;

    static const unsigned char ScancodeToHID_table[256] = {
            0,41,30,31,32,33,34,35,36,37,38,39,45,46,42,43,20,26,8,21,23,28,24,12,18,19,
            47,48,158,224,4,22,7,9,10,11,13,14,15,51,52,53,225,49,29,27,6,25,5,17,16,54,
            55,56,229,85,226,44,57,58,59,60,61,62,63,64,65,66,67,83,71,95,96,97,86,92,
            93,94,87,89,90,91,98,99,0,0,100,68,69,0,0,0,0,0,0,0,88,228,84,154,230,0,74,
            82,75,80,79,77,81,78,73,76,0,0,0,0,0,103,0,72,0,0,0,0,0,227,231,0,0,0,0,0,0,
            0,0,0,0,0,0,118,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,104,105,106,107,108,109,110,111,112,113,114,115,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
            0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    uint32_t ScancodeToHID(uint32_t scancode) {
        if (scancode >= 256) {
            return 0;
        }
        return ScancodeToHID_table[scancode];
    }
}

extern "C" {
    // Enable High Performance Graphics while using Integrated Graphics
    DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;        // Nvidia
    DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;    // AMD
}

#if !defined(__ANDROID__)
typedef GLXContext (*GLXCREATECONTEXTATTIBSARBPROC)(Display *, GLXFBConfig, GLXContext, Bool, const int *);
#endif

DummyApp::DummyApp() {
    g_app = this;
}

DummyApp::~DummyApp() {

}

int DummyApp::Init(int w, int h) {
#if !defined(__ANDROID__)
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        fprintf(stderr, "dpy is null\n");
        return -1;
    }

    static int attribute_list[] = {
            GLX_X_RENDERABLE    , True,
            GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
            GLX_RENDER_TYPE     , GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
            GLX_RED_SIZE        , 8,
            GLX_GREEN_SIZE      , 8,
            GLX_BLUE_SIZE       , 8,
            GLX_ALPHA_SIZE      , 8,
            GLX_DEPTH_SIZE      , 0,
            GLX_STENCIL_SIZE    , 0,
            GLX_DOUBLEBUFFER    , True,
            //GLX_SAMPLE_BUFFERS  , 1,
            //GLX_SAMPLES         , 4,
            None
    };

    int element_count = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(dpy_, DefaultScreen(dpy_), attribute_list, &element_count);

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
    swa.colormap = XCreateColormap(dpy_, RootWindow(dpy_, vi->screen), vi->visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
    
    win_ = XCreateWindow(dpy_, RootWindow(dpy_, vi->screen), 0, 0, w, h, 0, vi->depth, InputOutput, vi->visual, CWBorderPixel | CWColormap | CWEventMask, &swa);
    
    XMapWindow(dpy_, win_);
    XStoreName(dpy_, win_, "View");

    Atom wmDelete = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy_, win_, &wmDelete, 1);

    auto glXCreateContextAttribsARB = (GLXCREATECONTEXTATTIBSARBPROC)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
    if (!glXCreateContextAttribsARB) {
        fprintf(stderr, "glXCreateContextAttribsARB was not loaded\n");
        return -1;
    }

    int attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_FLAGS_ARB, 0,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        0
    };

    ctx_ = glXCreateContextAttribsARB(dpy_, *fbc, nullptr, true, attribs);
    if (!ctx_) {
        fprintf(stderr, "ctx is null\n");
        return -1;
    }

    glXMakeCurrent(dpy_, win_, ctx_);
    glViewport(0, 0, w, h);
#endif

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
#if !defined(__ANDROID__)
    glXMakeCurrent(dpy_, None, nullptr);
    glXDestroyContext(dpy_, ctx_);
    XDestroyWindow(dpy_, win_);
    XCloseDisplay(dpy_);
#endif
}

void DummyApp::Frame() {
    viewer_->Frame();
}

void DummyApp::Resize(int w, int h) {
    viewer_->Resize(w, h);
}

void DummyApp::AddEvent(int type, uint32_t key_code, float x, float y, float dx, float dy) {
    auto input_manager = viewer_->GetComponent<InputManager>(INPUT_MANAGER_KEY);
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

#if !defined(__ANDROID__)
int DummyApp::Run(const std::vector<std::string> &args) {
    int w = 1024, h = 576;
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

    while (!terminated()) {
        this->PollEvents();

        this->Frame();

#if defined(USE_GL_RENDER)
        uint64_t swap_start = Sys::GetTimeUs();
        glXSwapBuffers(dpy_, win_);
        uint64_t swap_end = Sys::GetTimeUs();

        auto swap_interval = viewer_->GetComponent<TimeInterval>(SWAP_TIMER_KEY);
        if (swap_interval) {
            swap_interval->start_timepoint_us = swap_start;
            swap_interval->end_timepoint_us = swap_end;
        }
#elif defined(USE_SW_RENDER)
        // TODO
#endif
    }

    this->Destroy();

    return 0;
}

void DummyApp::PollEvents() {
    std::shared_ptr<InputManager> input_manager = input_manager_.lock();
    if (!input_manager) return;

    static float last_p1_pos[2] = { 0.0f, 0.0f };
    static int last_window_size[2] = { 0, 0 };

    XEvent xev;
    while (XCheckWindowEvent(dpy_, win_, (ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask), &xev)) {
        InputManager::Event evt;

        if (xev.type == KeyPress) {
            const uint32_t
                scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                key_code = ScancodeToHID(scan_code);

            if (key_code == KeyEscape) {
                quit_ = true;
            } else {
                const uint32_t key_code = ScancodeToHID(scan_code);

                evt.type = RawInputEvent::EvKeyDown;
                evt.key_code = key_code;
            }
        } else if (xev.type == KeyRelease) {
            const uint32_t
                    scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                    key_code = ScancodeToHID(scan_code);

            evt.type = RawInputEvent::EvKeyUp;
            evt.key_code = key_code;
        } else if (xev.type == ButtonPress) {
            evt.type = RawInputEvent::EvP1Down;
            evt.point.x = (float)xev.xbutton.x;
            evt.point.y = (float)xev.xbutton.y;
        } else if (xev.type == ButtonRelease) {
            evt.type = RawInputEvent::EvP1Up;
            evt.point.x = (float)xev.xbutton.x;
            evt.point.y = (float)xev.xbutton.y;
        } else if (xev.type == MotionNotify) {
            evt.type = RawInputEvent::EvP1Move;
            evt.point.x = (float)xev.xmotion.x;
            evt.point.y = (float)xev.xmotion.y;
            evt.move.dx = evt.point.x - last_p1_pos[0];
            evt.move.dy = evt.point.y - last_p1_pos[1];

            last_p1_pos[0] = evt.point.x;
            last_p1_pos[1] = evt.point.y;
        } else if (xev.type == ConfigureNotify) {
            if (xev.xconfigure.width != last_window_size[0] || xev.xconfigure.height != last_window_size[1]) {

                Resize(xev.xconfigure.width, xev.xconfigure.height);

                evt.type = RawInputEvent::EvResize;
                evt.point.x = xev.xconfigure.width;
                evt.point.y = xev.xconfigure.height;

                last_window_size[0] = xev.xconfigure.width;
                last_window_size[1] = xev.xconfigure.height;
            }
        } else if (xev.type == ClientMessage) {
            printf("Destroy!!!\n");
        } else {

        }

        if (evt.type != RawInputEvent::EvNone) {
            evt.time_stamp = Sys::GetTimeUs();
            input_manager->AddRawInputEvent(evt);
        }
    }

    
}

#endif
