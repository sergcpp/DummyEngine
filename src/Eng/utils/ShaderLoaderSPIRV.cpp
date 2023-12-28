#include "ShaderLoader.h"

#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/MemBuf.h>

namespace ShaderLoaderInternal {
#if defined(__ANDROID__)
extern const char *SHADERS_PATH;
#else
extern const char *SHADERS_PATH;
#endif

Ren::eShaderType ShaderTypeFromName(const char *name, const int len);
} // namespace ShaderLoaderInternal
