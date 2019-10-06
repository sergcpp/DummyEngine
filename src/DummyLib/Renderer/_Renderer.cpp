
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "Renderer.cpp"
#include "Renderer_DrawList.cpp"
#include "Renderer_Frontend.cpp"

#if defined(USE_GL_RENDER)
#include "FrameBufGL.cpp"
#include "Renderer_Backend_GLES3.cpp"
#endif