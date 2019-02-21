#pragma once

#include <stdexcept>
#include <string>

#include "DynLib.h"

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

namespace Sys {
class Platform {
    DynLib          sdl_lib_;
#if defined(USE_GL_RENDER)
    void            *gl_ctx_ = nullptr;
#elif defined(USE_SW_RENDER)
    SDL_Renderer    *renderer_ = nullptr;
    SDL_Texture     *texture_ = nullptr;
#endif
    int width_ = 0, height_ = 0;
    SDL_Window      *window_ = nullptr;
public:
    Platform() {}
    Platform(const std::string &window_name, int w, int h) {
        if (Init(window_name, w, h) != 0) {
            throw std::runtime_error("Platform initialization error!");
        }
    }
    Platform(const Platform &) = delete;
    Platform &operator=(const Platform &) = delete;
    Platform &operator=(Platform &&rhs) {
        Release();

#if defined(USE_GL_RENDER)
        gl_ctx_ = rhs.gl_ctx_;
#elif defined(USE_SW_RENDER)
        renderer_ = rhs.renderer_;
        rhs.renderer_ = nullptr;
        texture_ = rhs.texture_;
        rhs.texture_ = nullptr;
#endif
        window_ = rhs.window_;
        rhs.window_ = nullptr;
        return *this;
    }
    ~Platform();

    int Init(const std::string &window_name, int w, int h);
    void Release();

    void DrawPixels(const void *pixels);
    void EndFrame();
};
}