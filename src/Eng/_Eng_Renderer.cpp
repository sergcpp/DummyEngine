
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "Renderer/Renderer.cpp"
#include "Renderer/Renderer_DrawList.cpp"
#include "Renderer/Renderer_Frontend.cpp"

#if defined(USE_GL_RENDER)
#include "Renderer/FrameBufGL.cpp"
#include "Renderer/Renderer_Backend_GLES3.cpp"
#endif