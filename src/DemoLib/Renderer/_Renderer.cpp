
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "Renderer.cpp"
#include "TextureAtlas.cpp"

#if defined(USE_GL_RENDER)
#include "FrameBufGL.cpp"
#include "Renderer_GLES3.cpp"
#endif