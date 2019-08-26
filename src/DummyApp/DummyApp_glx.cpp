#include "DummyApp.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <html5.h>
#endif

#if defined(USE_GL_RENDER)
#include <Ren/GL.h>
#elif defined(USE_SW_RENDER)

#endif

#include <GL/glx.h>

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

typedef GLXContext (*GLXCREATECONTEXTATTIBSARBPROC)(Display *, GLXFBConfig, GLXContext, Bool, const int *);

DummyApp::DummyApp() : quit_(false) {
    g_app = this;
}

DummyApp::~DummyApp() {

}

int DummyApp::Init(int w, int h) {
    dpy_ = XOpenDisplay(0);
    if (!dpy_) {
        fprintf(stderr, "dpy is null\n");
        return -1;
    }

    int element_count = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(dpy_, DefaultScreen(dpy_), 0, &element_count);

    if (!fbc) {
        fprintf(stderr, "fbc is null\n");
        return -1;
    }

    static int attribute_list[] = {
        GLX_RGBA, GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_DEPTH_SIZE, 0,
        None    
    };

    XVisualInfo *vi = glXChooseVisual(dpy_, DefaultScreen(dpy_), attribute_list);
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

    Atom wmDelete = XInternAtom(dpy_, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy_, win_, &wmDelete, 1);

    GLXCREATECONTEXTATTIBSARBPROC glXCreateContextAttribsARB = (GLXCREATECONTEXTATTIBSARBPROC)glXGetProcAddress((const GLubyte *)"glXCreateContextAttribsARB");
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

    ctx_ = glXCreateContextAttribsARB(dpy_, *fbc, 0, true, attribs);
    if (!ctx_) {
        fprintf(stderr, "ctx is null\n");
        return -1;
    }

    glXMakeCurrent(dpy_, win_, ctx_);
    glViewport(0, 0, w, h);

    try {
        // TODO: make it work on linux
        //Viewer::PrepareAssets("pc");
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

    glXMakeCurrent(dpy_, None, nullptr);
    glXDestroyContext(dpy_, ctx_);
    XDestroyWindow(dpy_, win_);
    XCloseDisplay(dpy_);
}

void DummyApp::Frame() {
    viewer_->Frame();
}

void DummyApp::Resize(int w, int h) {
    viewer_->Resize(w, h);
}

void DummyApp::AddEvent(int type, int key, int raw_key, float x, float y, float dx, float dy) {
    auto input_manager = viewer_->GetComponent<InputManager>(INPUT_MANAGER_KEY);
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

bool DummyApp::ConvertToRawButton(int &raw_key, InputManager::RawInputButton &button) {
    //printf("%x\n", raw_key);

    switch (raw_key) {
        case 0x6f:
            button = InputManager::RAW_INPUT_BUTTON_UP;
            break;
        case 0x74:
            button = InputManager::RAW_INPUT_BUTTON_DOWN;
            break;
        case 0x71:
            button = InputManager::RAW_INPUT_BUTTON_LEFT;
            break;
        case 0x72:
            button = InputManager::RAW_INPUT_BUTTON_RIGHT;
            break;
        case 0x08:
            button = InputManager::RAW_INPUT_BUTTON_BACKSPACE;
            break;
        case 0x09:
            button = InputManager::RAW_INPUT_BUTTON_TAB;
            break;
        default: {
            button = InputManager::RAW_INPUT_BUTTON_OTHER;
            raw_key = std::tolower(raw_key);
        } break;
    }
    return true;
}

void DummyApp::PollEvents() {
    std::shared_ptr<InputManager> input_manager = input_manager_.lock();
    if (!input_manager) return;

    static float last_p1_pos[2] = { 0.0f, 0.0f };
    static int last_window_size[2] = { 0, 0 };

    XEvent xev;
    while (XCheckWindowEvent(dpy_, win_, (ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask), &xev)) {
        InputManager::RawInputButton button;
        InputManager::Event evt;

        if (xev.type == KeyPress) {
            int raw_key = xev.xkey.keycode;

            char buf[2] = {};
            KeySym keysym_return;
            int len = XLookupString(&xev.xkey, buf, 1, &keysym_return, nullptr);
            
            if (len == 1) {
                raw_key = int(buf[0]);
            }

            if (raw_key == 0x1b) {
                quit_ = true;
            } else if (ConvertToRawButton(raw_key, button)) {
                evt.type = InputManager::RAW_INPUT_KEY_DOWN;
                evt.key = button;
                evt.raw_key = raw_key;
            }
        } else if (xev.type == KeyRelease) {
            int raw_key = xev.xkey.keycode;

            char buf[2] = {};
            KeySym keysym_return;
            int len = XLookupString(&xev.xkey, buf, 1, &keysym_return, nullptr);
            
            if (len == 1) {
                raw_key = int(buf[0]);
            }

            if (ConvertToRawButton(raw_key, button)) {
                evt.type = InputManager::RAW_INPUT_KEY_UP;
                evt.key = button;
                evt.raw_key = raw_key;
            }
        } else if (xev.type == ButtonPress) {
            evt.type = InputManager::RAW_INPUT_P1_DOWN;
            evt.point.x = xev.xbutton.x;
            evt.point.y = xev.xbutton.y;
        } else if (xev.type == ButtonRelease) {
            evt.type = InputManager::RAW_INPUT_P1_UP;
            evt.point.x = xev.xbutton.x;
            evt.point.y = xev.xbutton.y;
        } else if (xev.type == MotionNotify) {
            evt.type = InputManager::RAW_INPUT_P1_MOVE;
            evt.point.x = (float)xev.xmotion.x;
            evt.point.y = (float)xev.xmotion.y;
            evt.move.dx = evt.point.x - last_p1_pos[0];
            evt.move.dy = evt.point.y - last_p1_pos[1];

            last_p1_pos[0] = evt.point.x;
            last_p1_pos[1] = evt.point.y;
        } else if (xev.type == ConfigureNotify) {
            if (xev.xconfigure.width != last_window_size[0] || xev.xconfigure.height != last_window_size[1]) {

                Resize(xev.xconfigure.width, xev.xconfigure.height);

                last_window_size[0] = xev.xconfigure.width;
                last_window_size[1] = xev.xconfigure.height;
            }
        } else if (xev.type == ClientMessage) {
            printf("Destroy!!!\n");
        } else {

        }

        if (evt.type != InputManager::RAW_INPUT_NONE) {
            evt.time_stamp = Sys::GetTimeUs();
            input_manager->AddRawInputEvent(evt);
        }
    }

    
}

#endif
