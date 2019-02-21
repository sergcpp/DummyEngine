#include "PlatformSDL.h"

//#include <SDL2/SDL.h>

#include "AssetFileIO.h"
#include "DynLib.h"

#if defined(__WIN32__) && !defined(__GNUC__)
#define SDLCALL __cdecl
#else
#define SDLCALL
#endif

#define SDL_WINDOWPOS_UNDEFINED_MASK    0x1FFF0000
#define SDL_WINDOWPOS_UNDEFINED_DISPLAY(X)  (SDL_WINDOWPOS_UNDEFINED_MASK|(X))
#define SDL_WINDOWPOS_UNDEFINED         SDL_WINDOWPOS_UNDEFINED_DISPLAY(0)

namespace Sys {
const uint32_t SDL_INIT_VIDEO = 0x00000020;

const uint32_t SDL_WINDOW_OPENGL = 0x00000002;
const uint32_t SDL_WINDOW_RESIZABLE = 0x00000020;

const uint32_t SDL_RENDERER_SOFTWARE = 0x00000001;
const uint32_t SDL_RENDERER_ACCELERATED = 0x00000002;
}

typedef uint32_t Uint32;

Sys::Platform::~Platform() {
    Release();
}

int Sys::Platform::Init(const std::string &window_name, int w, int h) {
    Release();

    sdl_lib_ = DynLib{ "SDL2.dll" };
    if (!sdl_lib_) {
        sdl_lib_ = DynLib{ "SDL2.so" };
    }
    if (!sdl_lib_) {
        return -1;
    }

    int SDLCALL(*SDL_Init)(Uint32 flags);
    SDL_Window * SDLCALL(*SDL_CreateWindow)(const char *title, int x, int y, int w, int h, Uint32 flags);
    void *SDLCALL(*SDL_GL_CreateContext)(SDL_Window *window);
    int SDLCALL(*SDL_GL_SetSwapInterval)(int interval);
    SDL_Renderer * SDLCALL(*SDL_CreateRenderer)(SDL_Window * window, int index, Uint32 flags);
    SDL_Texture * SDLCALL(*SDL_CreateTexture)(SDL_Renderer * renderer, Uint32 format, int access, int w, int h);

    SDL_Init = (decltype(SDL_Init))sdl_lib_.GetProcAddress("SDL_Init");
    SDL_CreateWindow = (decltype(SDL_CreateWindow))sdl_lib_.GetProcAddress("SDL_CreateWindow");
    SDL_GL_CreateContext = (decltype(SDL_GL_CreateContext))sdl_lib_.GetProcAddress("SDL_GL_CreateContext");
    SDL_GL_SetSwapInterval = (decltype(SDL_GL_SetSwapInterval))sdl_lib_.GetProcAddress("SDL_GL_SetSwapInterval");
    SDL_CreateRenderer = (decltype(SDL_CreateRenderer))sdl_lib_.GetProcAddress("SDL_CreateRenderer");
    SDL_CreateTexture = (decltype(SDL_CreateTexture))sdl_lib_.GetProcAddress("SDL_CreateTexture");

    if (!SDL_Init || !SDL_CreateWindow || !SDL_GL_CreateContext || !SDL_GL_SetSwapInterval || !SDL_CreateRenderer || !SDL_CreateTexture) {
        return -1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        return -1;
    }

    window_ = SDL_CreateWindow(window_name.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window_) return -1;

    width_ = w;
    height_ = h;

#if defined(USE_GL_RENDER)
    gl_ctx_ = SDL_GL_CreateContext(window_);
#if !defined(__EMSCRIPTEN__)
    SDL_GL_SetSwapInterval(0);
#endif
#elif defined(USE_SW_RENDER)
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) return -1;

    //texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!texture_) return -1;
#endif

    Sys::InitWorker();

    return 0;
}

void Sys::Platform::Release() {
    void SDLCALL(*SDL_GL_DeleteContext)(void *context);
    void SDLCALL(*SDL_DestroyTexture)(SDL_Texture * texture);
    void SDLCALL(*SDL_DestroyRenderer)(SDL_Renderer * renderer);
    void SDLCALL(*SDL_DestroyWindow)(SDL_Window * window);

    SDL_GL_DeleteContext = (decltype(SDL_GL_DeleteContext))sdl_lib_.GetProcAddress("SDL_GL_DeleteContext");
    SDL_DestroyTexture = (decltype(SDL_DestroyTexture))sdl_lib_.GetProcAddress("SDL_DestroyTexture");
    SDL_DestroyRenderer = (decltype(SDL_DestroyRenderer))sdl_lib_.GetProcAddress("SDL_DestroyRenderer");
    SDL_DestroyWindow = (decltype(SDL_DestroyWindow))sdl_lib_.GetProcAddress("SDL_DestroyWindow");

    sdl_lib_ = {};

#if defined(USE_GL_RENDER)
    if (gl_ctx_) {
        SDL_GL_DeleteContext(gl_ctx_);
        gl_ctx_ = nullptr;
    }
#elif defined(USE_SW_RENDER)
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
#endif
    if (window_) {
        width_ = 0;
        height_ = 0;
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        //SDL_Quit();
    }

    Sys::StopWorker();
}

void Sys::Platform::DrawPixels(const void *pixels) {
#if defined(USE_GL_RENDER)
#elif defined(USE_SW_RENDER)
    if (pixels) {
        //SDL_UpdateTexture(texture_, NULL, pixels, width_ * sizeof(Uint32));
        //SDL_RenderClear(renderer_);
        //SDL_RenderCopy(renderer_, texture_, NULL, NULL);
    }
#endif
}

void Sys::Platform::EndFrame() {
#if defined(USE_GL_RENDER)
    //SDL_GL_SwapWindow(window_);
#elif defined(USE_SW_RENDER)
    //SDL_RenderPresent(renderer_);
#endif
}