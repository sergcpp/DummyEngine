#include "test_common.h"

#include "../GL.h"
#include "../Context.h"
#include "../Material.h"

#if defined(_WIN32)
#include <Windows.h>

#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x00000002

#else
#include <SDL2/SDL.h>
#endif

class ProgramTest : public Ren::Context {
#if defined(_WIN32)
    HINSTANCE hInstance;
    HWND hWnd;
    HDC hDC;
    HGLRC hRC;
#else
    SDL_Window *window_;
    void *gl_ctx_;
#endif
public:
    ProgramTest() {
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
        wc.lpszClassName = "ProgramTest";

        if (!RegisterClass(&wc)) {
            throw std::runtime_error("Cannot register window class!");
        }

        hWnd = CreateWindow("ProgramTest", "!!", WS_OVERLAPPEDWINDOW |
                            WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                            0, 0, 100, 100, NULL, NULL, hInstance, NULL);

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

        HGLRC temp_context = wglCreateContext(hDC);
        wglMakeCurrent(hDC, temp_context);

        typedef HGLRC(APIENTRY * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);
        static PFNWGLCREATECONTEXTATTRIBSARBPROC pfnCreateContextAttribsARB =
            reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(wglGetProcAddress("wglCreateContextAttribsARB"));

        int attriblist[] = { WGL_CONTEXT_MAJOR_VERSION_ARB, 4, WGL_CONTEXT_MINOR_VERSION_ARB, 3, WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB, 0, 0 };

        hRC = pfnCreateContextAttribsARB(hDC, 0, attriblist);
        wglMakeCurrent(hDC, hRC);

        wglDeleteContext(temp_context);
#else
        SDL_Init(SDL_INIT_VIDEO);

        window_ = SDL_CreateWindow("View", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 256, 256, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
        gl_ctx_ = SDL_GL_CreateContext(window_);
#endif
        Context::Init(256, 256);
    }

    ~ProgramTest() {
#if defined(_WIN32)
        wglMakeCurrent(NULL, NULL);
        ReleaseDC(hWnd, hDC);
        wglDeleteContext(hRC);
        DestroyWindow(hWnd);
        UnregisterClass("ProgramTest", hInstance);
#else
        SDL_GL_DeleteContext(gl_ctx_);
        SDL_DestroyWindow(window_);
#ifndef EMSCRIPTEN
        SDL_Quit();
#endif
#endif
    }
};

void test_program() {
    {
        // Load program
        ProgramTest test;

        const char vs_src[] =
R"(
#version 100

/*
ATTRIBUTES
    aVertexPosition : 0
    aVertexPosition1 : 1
UNIFORMS
    uMVPMatrix : 0
*/

attribute vec3 aVertexPosition;
uniform mat4 uMVPMatrix;

void main(void) {
    gl_Position = uMVPMatrix * vec4(aVertexPosition, 1.0);
})";

        const char fs_src[] =
R"(
#version 100

#ifdef GL_ES
	precision mediump float;
#else
	#define lowp
	#define mediump
	#define highp
#endif
/*
UNIFORMS
    asdasd : 1
*/
uniform vec3 col;

void main(void) {
    gl_FragColor = vec4(col, 1.0);
})";

        Ren::eProgLoadStatus status;
        Ren::ProgramRef p = test.LoadProgramGLSL("constant", nullptr, nullptr, &status);

        require(status == Ren::ProgSetToDefault);
        require(p->name() == "constant");
        require(p->prog_id() == 0); // not initialized
        require(p->ready() == false);

        test.LoadProgramGLSL("constant", vs_src, fs_src, &status);

        require(status == Ren::ProgCreatedFromData);

        require(p->name() == "constant");

        require(p->ready() == true);

        require(p->attribute(0).name == "aVertexPosition");
        require(p->attribute(0).loc != -1);
        require(p->attribute(1).name.empty());
        require(p->attribute(1).loc == -1);

        require(p->uniform(0).name == "uMVPMatrix");
        require(p->uniform(0).loc != -1);
        require(p->uniform(1).name == "col");
        require(p->uniform(1).loc != -1);
    }

    {
        // Load compute
        ProgramTest test;

        const char cs_source[] =
R"(
#version 430

/*
UNIFORMS
    delta : 0
*/

uniform vec4 delta;

struct AttribData {
	vec4 p;
	vec4 c;
};

layout(std430, binding = 0) buffer dest_buffer {
	AttribData data[];
} inout_buffer;

layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

void main() {
	uint global_index = gl_GlobalInvocationID.x;
	uint work_size = gl_WorkGroupSize.x * gl_NumWorkGroups.x;

	inout_buffer.data[global_index].p = inout_buffer.data[global_index].p + delta;
	inout_buffer.data[global_index].c = vec4(1.0, 0.0, 1.0, 1.0);
})";

        Ren::eProgLoadStatus status;
        Ren::ProgramRef p = test.LoadProgramGLSL("sample", cs_source, &status);

        require(p->uniform(0).name == "delta");
        require(p->uniform(0).loc != -1);

        struct AttribData {
            Ren::Vec4f p, c;
        };

        auto buf = Ren::Buffer{ "buf", sizeof(AttribData) * 128 };

        std::vector<AttribData> _data;
        for (int i = 0; i < 128; i++) {
            _data.push_back({ { 0.0f, float(i), 0.0f, 0.0f }, Ren::Vec4f{ 0.0f } });
        }

        uint32_t offset = buf.Alloc(128 * sizeof(AttribData), _data.data());
        require(offset == 0);

        glUseProgram(p->prog_id());
        glUniform4f(p->uniform("delta").loc, 0.0f, -0.1f, 0.0f, 0.0f);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, buf.buf_id());

        glDispatchCompute(2, 1, 1);

        glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 128 * sizeof(AttribData), _data.data());

        for (int i = 0; i < 128; i++) {
            require(_data[i].p[0] == Approx(0.0));
            require(_data[i].p[1] == Approx(double(i) - 0.1f));
            require(_data[i].p[2] == Approx(0.0));
            require(_data[i].p[3] == Approx(0.0));

            require(_data[i].c[0] == Approx(1.0));
            require(_data[i].c[1] == Approx(0.0));
            require(_data[i].c[2] == Approx(1.0));
            require(_data[i].c[3] == Approx(1.0));
        }

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
    }
}
