
extern "C" {
#include "SW/_SW.c"
}

#if defined(USE_GL_RENDER)
#include "GLExt.cpp"
#include "GLExtDSAEmu.cpp"
#include "BufferGL.cpp"
#include "ContextGL.cpp"
#include "ProgramGL.cpp"
#include "RastStateGL.cpp"
#include "SamplerGL.cpp"
#include "ShaderGL.cpp"
#include "TextureGL.cpp"
#include "TextureAtlasGL.cpp"
#include "VaoGL.cpp"
#elif defined(USE_SW_RENDER)
#include "ContextSW.cpp"
#include "ProgramSW.cpp"
#include "TextureSW.cpp"
#endif

#include "Anim.cpp"
#include "Camera.cpp"
#include "Context.cpp"
#include "CPUFeatures.cpp"
#include "Material.cpp"
#include "Mesh.cpp"
#include "TaskExecutor.cpp"
#include "Texture.cpp"
#include "TextureRegion.cpp"
#include "TextureSplitter.cpp"
#include "Utils.cpp"

