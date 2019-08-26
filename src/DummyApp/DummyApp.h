#pragma once

#include <memory>
#include <string>
#include <vector>

#include <Eng/TimedInput.h>

#if !defined(__ANDROID__)
#if defined(_WIN32)
#ifndef _WINDEF_
struct HWND__; // Forward or never
typedef HWND__* HWND;
struct HDC__;
typedef HDC__* HDC;
struct HGLRC__;
typedef HGLRC__* HGLRC;
#endif
#elif defined(__linux__)
#include <X11/Xlib.h>
typedef struct _XDisplay Display;

struct __GLXcontextRec;  // Forward declaration from GLX.h.
typedef struct __GLXcontextRec *GLXContext;
typedef GLXContext GLContext;
#endif

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;
#endif

class GameBase;

class DummyApp {
    bool fullscreen_, quit_;

#if !defined(__ANDROID__)
#if defined(_WIN32)
    HWND            window_handle_;
    HDC             device_context_;
    HGLRC           gl_ctx_;
#elif defined(__linux__)
    Display         *dpy_;
    Window          win_;
    GLXContext      ctx_;
#else
#if defined(USE_GL_RENDER)
    void            *gl_ctx_ = nullptr;
#elif defined(USE_SW_RENDER)
    SDL_Renderer    *renderer_ = nullptr;
    SDL_Texture     *texture_ = nullptr;
#endif

    SDL_Window		*window_ = nullptr;
#endif

    
    void PollEvents();
#endif

    std::unique_ptr<GameBase>   viewer_;
    std::weak_ptr<InputManager> input_manager_;
public:
    DummyApp();
    ~DummyApp();

    int Init(int w, int h);
    void Destroy();

    void Frame();
    void Resize(int w, int h);

    void AddEvent(int type, int key, int raw_key, float x, float y, float dx, float dy);
    static bool ConvertToRawButton(int &raw_key, InputManager::RawInputButton &button);

#if !defined(__ANDROID__)
    int Run(const std::vector<std::string> &args);
#endif

    bool terminated() const {
        return quit_;
    }
};
