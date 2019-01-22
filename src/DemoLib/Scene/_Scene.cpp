
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#endif

#include "Renderer.cpp"
#include "SceneManager.cpp"
#include "SceneManager_BVH.cpp"
#include "SceneManager_PT.cpp"

#if defined(USE_GL_RENDER)
#include "FrameBufGL.cpp"
#include "Renderer_GLES3.cpp"
#endif