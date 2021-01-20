#include "test_common.h"

#include "../Context.h"
#include "../Material.h"

#ifdef USE_GL_RENDER

#if defined(_WIN32)
#include <Windows.h>
#else
#include <SDL2/SDL.h>
#endif

class MaterialTest : public Ren::Context {
#if defined(_WIN32)
    HINSTANCE hInstance;
    HWND hWnd;
    HDC hDC;
    HGLRC hRC;
#else
    SDL_Window *window_;
    void *gl_ctx_main_;
#endif
    Ren::LogNull log_;

  public:
    MaterialTest() {
#if defined(_WIN32)
        hInstance = GetModuleHandle(NULL);
        WNDCLASS wc;
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = ::DefWindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = "MaterialTest";

        if (!RegisterClass(&wc)) {
            throw std::runtime_error("Cannot register window class!");
        }

        hWnd = CreateWindow("MaterialTest", "!!",
                            WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0, 0,
                            100, 100, NULL, NULL, hInstance, NULL);

        if (hWnd == NULL) {
            throw std::runtime_error("Cannot create window!");
        }

        hDC = GetDC(hWnd);

        PIXELFORMATDESCRIPTOR pfd;
        memset(&pfd, 0, sizeof(pfd));
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;

        int pf = ChoosePixelFormat(hDC, &pfd);
        if (pf == 0) {
            throw std::runtime_error("Cannot find pixel format!");
        }

        if (SetPixelFormat(hDC, pf, &pfd) == FALSE) {
            throw std::runtime_error("Cannot set pixel format!");
        }

        DescribePixelFormat(hDC, pf, sizeof(PIXELFORMATDESCRIPTOR), &pfd);

        hRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hRC);
#else
        SDL_Init(SDL_INIT_VIDEO);

        window_ =
            SDL_CreateWindow("View", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                             256, 256, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        gl_ctx_main_ = SDL_GL_CreateContext(window_);
#endif
        Context::Init(256, 256, &log_);
    }

    ~MaterialTest() {
#if defined(_WIN32)
        wglMakeCurrent(NULL, NULL);
        ReleaseDC(hWnd, hDC);
        wglDeleteContext(hRC);
        DestroyWindow(hWnd);
        UnregisterClass("MaterialTest", hInstance);
#else
        SDL_GL_DeleteContext(gl_ctx_main_);
        SDL_DestroyWindow(window_);
#ifndef EMSCRIPTEN
        SDL_Quit();
#endif
#endif
    }
};

#else
#include "../SW/SW.h"
class MaterialTest : public Ren::Context {
  public:
    MaterialTest() { Ren::Context::Init(256, 256); }
};
#endif

static Ren::ProgramRef OnProgramNeeded(const char *name, const char *arg1,
                                       const char *arg2) {
    return {};
}

static Ren::Tex2DRef OnTextureNeeded(const char *name) { return {}; }

void test_material() {
    { // Load material
        MaterialTest test;

        auto on_program_needed = [&test](const char *name, const char *arg1,
                                         const char *arg2, const char *arg3,
                                         const char *arg4) {
            Ren::eProgLoadStatus status;
#if defined(USE_GL_RENDER)
            return test.LoadProgram(name, {}, {}, {}, {}, &status);
#elif defined(USE_SW_RENDER)
            Ren::Attribute _attrs[] = {{}};
            Ren::Uniform _unifs[] = {{}};
            return test.LoadProgramSW(name, nullptr, nullptr, 0, _attrs, _unifs, &status);
#endif
        };

        auto on_texture_needed = [&test](const char *name, const uint8_t color[4], uint32_t flags) {
            Ren::eTexLoadStatus status;
            Ren::Tex2DParams p;
            return test.LoadTexture2D(name, nullptr, 0, p, &status);
        };

        const char *mat_src = "gl_program: constant constant.vs constant.fs\n"
                              "sw_program: constant\n"
                              "flag: alpha_test\n"
                              "texture: checker.tga\n"
                              "texture: checker.tga signed\n"
                              "texture: metal_01.tga\n"
                              "texture: checker.tga\n"
                              "param: 0 1 2 3\n"
                              "param: 0.5 1.2 11 15";

        Ren::eMatLoadStatus status;
        Ren::MaterialRef m_ref = test.LoadMaterial("mat1", nullptr, &status,
                                                   on_program_needed, on_texture_needed);
        require(status == Ren::eMatLoadStatus::SetToDefault);

        { require(!m_ref->ready()); }

        test.LoadMaterial("mat1", mat_src, &status, on_program_needed, on_texture_needed);

        require(status == Ren::eMatLoadStatus::CreatedFromData);
        require(m_ref->flags() & uint32_t(Ren::eMaterialFlags::AlphaTest));
        require(m_ref->ready());
        require(m_ref->name() == "mat1");

        Ren::ProgramRef p = m_ref->programs[0];

        require(p->name() == "constant");
        require(!p->ready());

        Ren::Tex2DRef t0 = m_ref->textures[0];
        Ren::Tex2DRef t1 = m_ref->textures[1];
        Ren::Tex2DRef t2 = m_ref->textures[2];
        Ren::Tex2DRef t3 = m_ref->textures[3];

        require(t0 == t1);
        require(t0 == t3);

        require(t0->name() == "checker.tga");
        require(t1->name() == "checker.tga");
        require(t2->name() == "metal_01.tga");
        require(t3->name() == "checker.tga");

        require(m_ref->params[0] == Ren::Vec4f(0, 1, 2, 3));
        require(m_ref->params[1] == Ren::Vec4f(0.5f, 1.2f, 11, 15));
    }
}
