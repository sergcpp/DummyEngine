#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Eng/input/InputManager.h>

#include <DummyLib/utils/Log.h>

#if !defined(__ANDROID__)
#if defined(_WIN32)
#ifndef _WINDEF_
struct HWND__; // Forward or never
typedef HWND__ *HWND;
struct HDC__;
typedef HDC__ *HDC;
struct HGLRC__;
typedef HGLRC__ *HGLRC;
#endif
#elif defined(__linux__)
#include <X11/Xlib.h>
typedef struct _XDisplay Display;

struct __GLXcontextRec; // Forward declaration from GLX.h.
typedef struct __GLXcontextRec *GLXContext;
typedef GLXContext GLContext;
#endif

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;
#endif

namespace Eng {
class ILog;
class ViewerBase;
} // namespace Eng

struct AppParams;

class DummyApp {
    bool fullscreen_ = false, minimized_ = false, quit_ = false;

#if !defined(__ANDROID__)
#if defined(_WIN32)
    HWND window_handle_ = {};
    HDC device_context_ = {};
    HGLRC gl_ctx_main_ = {}, gl_ctx_aux_ = {};
#elif defined(__linux__)
    Display *dpy_ = nullptr;
    Window win_ = {};
    GLXContext gl_ctx_main_ = {}, gl_ctx_aux_ = {};
#elif defined(__APPLE__)
    void *app_ = nullptr;
#else
#if defined(REN_GL_BACKEND)
    void *gl_ctx_main_ = nullptr;
#elif defined(REN_SW_BACKEND)
    SDL_Renderer *renderer_ = nullptr;
    SDL_Texture *texture_ = nullptr;
#endif

    SDL_Window *window_ = nullptr;
#endif

    void PollEvents();
#endif

    std::unique_ptr<LogStdout> log_;
    std::unique_ptr<Eng::ViewerBase> viewer_;
    Eng::InputManager *input_manager_ = nullptr;

    void ParseArgs(int argc, char *argv[], int &w, int &h, AppParams &out_params);

  public:
    DummyApp();
    ~DummyApp();

    int Init(int w, int h, const AppParams &app_params);
    void Destroy();

    void Frame();
    void Resize(int w, int h);

    void AddEvent(Eng::eInputEvent type, uint32_t key_code, float x, float y, float dx, float dy);

#if !defined(__ANDROID__)
    int Run(int argc, char *argv[]);
#endif

    const char *Version() const;

    bool terminated() const { return quit_; }
};
