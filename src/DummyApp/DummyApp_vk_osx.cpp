#include "DummyApp.h"

#include <objc/runtime.h>
#include <objc/message.h>

#include <Carbon/Carbon.h>

#define cls objc_getClass
#define sel sel_getUid
#define msg ((id (*)(id, SEL, ...))objc_msgSend)
#define cls_msg ((id (*)(Class, SEL, ...))objc_msgSend)

typedef id (*MethodImp)(id, SEL, ...);
typedef MethodImp (*get_method_imp)(Class, SEL);
#define method ((get_method_imp)class_getMethodImplementation)

id nsstring(const char *str) {
    return cls_msg(cls("NSString"), sel("stringWithUTF8String:"), str);
}

typedef enum NSApplicationActivationPolicy {
    NSApplicationActivationPolicyRegular   = 0,
    NSApplicationActivationPolicyAccessory = 1,
    NSApplicationActivationPolicyERROR     = 2,
} NSApplicationActivationPolicy;

typedef enum NSWindowStyleMask {
    NSWindowStyleMaskBorderless     = 0,
    NSWindowStyleMaskTitled         = 1 << 0,
    NSWindowStyleMaskClosable       = 1 << 1,
    NSWindowStyleMaskMiniaturizable = 1 << 2,
    NSWindowStyleMaskResizable      = 1 << 3,
} NSWindowStyleMask;

typedef enum NSBackingStoreType {
    NSBackingStoreBuffered = 2,
} NSBackingStoreType;

// metal bindings
extern "C" {
id MTLCreateSystemDefaultDevice();
}
typedef enum MTLPixelFormat {
    MTLPixelFormatBGRA8Unorm = 80,
    MTLPixelFormatRGBA8Unorm = 70,
} MTLPixelFormat;

// app delegate bits from https://gist.github.com/andsve/2a154a82faa806b3b1d6d71f18a2ad24
Class AppDelegate;
Ivar AppDelegate_AppData;
SEL AppDelegate_frameSel;

BOOL app_should_terminate_after_last_window(id self, SEL cmd)
{
    //app_context *ctx = (app_context *) object_getIvar(self, AppDelegate_AppData);
    return 1;//ctx->terminate_after_last_window;
}

static void init_delegate_class()
{
    //AppData app_data_instance;
    AppDelegate = objc_allocateClassPair(objc_getClass("NSObject"), "AppDelegate", 0);
    AppDelegate_frameSel = sel("frame:");
    class_addMethod(AppDelegate, sel("applicationShouldTerminateAfterLastWindowClosed:"), (IMP) app_should_terminate_after_last_window, "B@:");
    //class_addMethod(AppDelegate, sel("frame:"), (IMP) render_frame, "v@:@");
    //class_addIvar(AppDelegate, "app_data", sizeof(app_context *), log2(sizeof(app_context *)), "@");
    objc_registerClassPair(AppDelegate);
    AppDelegate_AppData = class_getInstanceVariable(AppDelegate, "app_data");
    printf("AppDelegate_AppData: %lx\n", (uintptr_t) AppDelegate_AppData);
}


#include <vtune/ittnotify.h>
__itt_domain *__g_itt_domain = __itt_domain_create("Global"); // NOLINT

#include <Eng/ViewerBase.h>
#include <Eng/input/InputManager.h>
#include <Sys/DynLib.h>
#include <Sys/ThreadWorker.h>
#include <Sys/Time_.h>

#include "../DummyLib/Viewer.h"

namespace Ren {
//extern Display  *g_dpy;
//extern Window    g_win;
extern void *g_metal_layer;
}

namespace {
DummyApp *g_app = nullptr;


} // namespace

extern "C" {
// Enable High Performance Graphics while using Integrated Graphics
DLL_EXPORT int32_t NvOptimusEnablement = 0x00000001;     // Nvidia
DLL_EXPORT int AmdPowerXpressRequestHighPerformance = 1; // AMD
}

DummyApp::DummyApp() { g_app = this; }

DummyApp::~DummyApp() = default;

int DummyApp::Init(const int w, const int h, const int validation_level, const char *device_name) {
    init_delegate_class();

    // id app = [NSApplication sharedApplication];
    id app = cls_msg(cls("NSApplication"), sel("sharedApplication"));

    // [app setActivationPolicy:NSApplicationActivationPolicyRegular];
    msg(app, sel("setActivationPolicy:"), NSApplicationActivationPolicyRegular);

    struct CGRect frame_rect = { { 0.0, 0.0 }, { double(w), double(h) } };

    // id window = [[NSWindow alloc] initWithContentRect:frameRect styleMask:NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable backing:NSBackingStoreBuffered defer:NO];
    id window = msg(cls_msg(cls("NSWindow"), sel("alloc")),
                    sel("initWithContentRect:styleMask:backing:defer:"),
                    frame_rect,
                    NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable,
                    NSBackingStoreBuffered,
                    false);
    msg(window, sel("setTitle:"), nsstring("View (VK)"));

    id metal_device = MTLCreateSystemDefaultDevice();

    Class CAMetalLayer = cls("CAMetalLayer");
    id metal_layer = cls_msg(CAMetalLayer, sel("layer"));

    msg(metal_layer, sel("setDevice:"), metal_device);
    msg(metal_layer, sel("setPixelFormat:"), MTLPixelFormatBGRA8Unorm);
    msg(metal_layer, sel("setFrame:"), frame_rect);
    //msg(metal_layer, sel("contentsScale:"), 1.0f);

    Ren::g_metal_layer = metal_layer;

    try {
        Viewer::PrepareAssets("pc");
        log_ = std::make_unique<LogStdout>();
        viewer_ = std::make_unique<Viewer>(w, h, nullptr, validation_level, log_.get(), device_name);

        auto *input_manager = viewer_->input_manager();
        input_manager_ = input_manager;

        // [window makeKeyAndOrderFront:nil];
        msg(window, sel("makeKeyAndOrderFront:"), nil);

        // [app activateIgnoringOtherApps:YES];
        msg(app, sel("activateIgnoringOtherApps:"), true);

        // id delegate = [[AppDelegate alloc] init]
        // [app setDelegate:delegate]
        id delegate = msg(cls_msg(AppDelegate, sel("alloc")), sel("init"));
        msg(app, sel("setDelegate:"), delegate);

        // delegate.app_data = &appData;
        //object_setIvar(delegate, AppDelegate_AppData, (id) &g_context);

        id view = msg(window, sel("contentView"));
        printf("view: %lx\n", (uintptr_t)view);
        msg(view, sel("setFrame:"), frame_rect);

        msg(view, sel("setWantsLayer:"), YES); // otherwise there will be no layer!
        id viewLayer = msg(view, sel("layer"));

        msg(viewLayer, sel("addSublayer:"), metal_layer);

        app_ = app;
    } catch (std::exception &e) {
        fprintf(stderr, "%s", e.what());
        return -1;
    }

    return 0;
}

void DummyApp::Destroy() {
    viewer_ = {};
#if !defined(__ANDROID__)
    //XDestroyWindow(dpy_, win_);
    //XCloseDisplay(dpy_); // this is done in ContextVK.cpp (https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/issues/1894)
#endif
}

void DummyApp::Frame() { viewer_->Frame(); }

void DummyApp::Resize(int w, int h) { viewer_->Resize(w, h); }

void DummyApp::AddEvent(Eng::RawInputEv type, const uint32_t key_code, const float x,
                        const float y, const float dx, const float dy) {
    auto *input_manager = viewer_->input_manager();
    if (!input_manager) {
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

    input_manager->AddRawInputEvent(evt);
}

#if !defined(__ANDROID__)
int DummyApp::Run(int argc, char *argv[]) {
    int w = 1280, h = 720;
    fullscreen_ = false;
    int validation_level = 0;
    const char *device_name = nullptr;

#ifndef NDEBUG
    validation_level = 1;
#endif

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
        } else if (strcmp(arg, "--validation_level") == 0 || strcmp(arg, "-vl") == 0) {
            validation_level = std::atoi(argv[++i]);
        }
    }

    if (Init(w, h, validation_level, device_name) < 0) {
        return -1;
    }

    __itt_thread_set_name("Main Thread");

    while (!terminated()) {
        __itt_frame_begin_v3(__g_itt_domain, nullptr);

        this->PollEvents();

        this->Frame();

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

    // read events
    id event = method(cls("NSApplication"), sel("nextEventMatchingMask:untilDate:inMode:dequeue:"))(reinterpret_cast<id>(app_), sel("nextEventMatchingMask:untilDate:inMode:dequeue:"), INT_MAX, 0, nsstring("kCFRunLoopDefaultMode"), 1);
    if (event) {
        //printf("event: %lx\n", (uintptr_t) event);

        id type = msg(event, sel("type"));

        msg(reinterpret_cast<id>(app_), sel("sendEvent:"), event);
    }

#if 0
    static float last_p1_pos[2] = {0.0f, 0.0f};
    static int last_window_size[2] = {0, 0};

    XEvent xev;
    while (XCheckWindowEvent(dpy_, win_,
                             (ExposureMask | StructureNotifyMask | KeyPressMask |
                              KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask),
                             &xev)) {
        InputManager::Event evt;

        if (xev.type == KeyPress) {
            const uint32_t scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                           key_code = ScancodeToHID(scan_code);

            if (key_code == KeyEscape) {
                quit_ = true;
            } else {
                evt.type = RawInputEv::KeyDown;
                evt.key_code = key_code;
            }
        } else if (xev.type == KeyRelease) {
            const uint32_t scan_code = uint32_t(xev.xkey.keycode - KeycodeOffset),
                           key_code = ScancodeToHID(scan_code);

            evt.type = RawInputEv::KeyUp;
            evt.key_code = key_code;
        } else if (xev.type == ButtonPress &&
                   (xev.xbutton.button >= Button1 && xev.xbutton.button <= Button5)) {
            if (xev.xbutton.button == Button1) {
                evt.type = RawInputEv::P1Down;
            } else if (xev.xbutton.button == Button3) {
                evt.type = RawInputEv::P2Down;
            } else if (xev.xbutton.button == Button4 || xev.xbutton.button == Button5) {
                evt.type = RawInputEv::MouseWheel;
                evt.move.dx = (xev.xbutton.button == Button4) ? 1.0f : -1.0f;
            }
            evt.point.x = float(xev.xbutton.x);
            evt.point.y = float(xev.xbutton.y);
        } else if (xev.type == ButtonRelease &&
                   (xev.xbutton.button >= Button1 && xev.xbutton.button <= Button5)) {
            if (xev.xbutton.button == Button1) {
                evt.type = RawInputEv::P1Up;
            } else if (xev.xbutton.button == Button3) {
                evt.type = RawInputEv::P2Up;
            }
            evt.point.x = float(xev.xbutton.x);
            evt.point.y = float(xev.xbutton.y);
        } else if (xev.type == MotionNotify) {
            evt.type = RawInputEv::P1Move;
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

                evt.type = RawInputEv::Resize;
                evt.point.x = (float)xev.xconfigure.width;
                evt.point.y = (float)xev.xconfigure.height;

                last_window_size[0] = xev.xconfigure.width;
                last_window_size[1] = xev.xconfigure.height;
            }
        }

        if (evt.type != RawInputEv::None) {
            evt.time_stamp = Sys::GetTimeUs();
            input_manager->AddRawInputEvent(evt);
        }
    }

    if (XCheckTypedWindowEvent(dpy_, win_, ClientMessage, &xev)) {
        if (strcmp(XGetAtomName(dpy_, xev.xclient.message_type), "WM_PROTOCOLS") == 0) {
            quit_ = true;
        }
    }
#endif
}

#endif
