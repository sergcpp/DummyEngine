
#include "Anim.cpp"
#include "Buffer.cpp"
#include "Camera.cpp"
#include "Context.cpp"
#include "Material.cpp"
#include "Mesh.cpp"
#include "RenderThread.cpp"
#include "TextureSplitter.cpp"
#include "Utils.cpp"

extern "C" {
#include "SW/_SW.c"
}

#if defined(USE_GL_RENDER)
#include "GLExt.cpp"
#include "ContextGL.cpp"
#include "ProgramGL.cpp"
#include "TextureGL.cpp"
#elif defined(USE_SW_RENDER)
#include "ContextSW.cpp"
#include "ProgramSW.cpp"
#include "TextureSW.cpp"
#endif
