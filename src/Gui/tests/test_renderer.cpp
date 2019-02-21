#include "test_common.h"

#include <Ren/Context.h>
#include <Ren/GL.h>
#include <Sys/Json.h>

#include "../Renderer.h"

#ifdef USE_GL_RENDER

#include <SDL2/SDL.h>

class RendererTest : public Ren::Context {
    SDL_Window *window_;
    void *gl_ctx_;
public:
    RendererTest() {
        SDL_Init(SDL_INIT_VIDEO);

        window_ = SDL_CreateWindow("View", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 256, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        gl_ctx_ = SDL_GL_CreateContext(window_);

        Ren::Context::Init(256, 256);
    }

    ~RendererTest() {
        SDL_GL_DeleteContext(gl_ctx_);
        SDL_DestroyWindow(window_);
#ifndef EMSCRIPTEN
        SDL_Quit();
#endif
    }
};
#else

#include <Ren/SW/SW.h>

class RendererTest : public Ren::Context {
    SWcontext *ctx;
public:
    RendererTest() {
        ctx = swCreateContext(1, 1);
        swMakeCurrent(ctx);
        Ren::Context::Init(256, 256);
    }

    ~RendererTest() {
        swDeleteContext(ctx);
    }
};
#endif

void test_renderer() {
    {
        // Params test
        JsObject config;
        config[Gui::GL_DEFINES_KEY] = JsString{ "" };

        {
            // Default parameters
            RendererTest _;

            Gui::Renderer r(_, config);

            //const auto &cur = r.GetParams();

        }
    }
}