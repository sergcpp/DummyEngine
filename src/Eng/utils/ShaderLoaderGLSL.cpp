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

Ren::eShaderType ShaderTypeFromName(std::string_view name);
} // namespace ShaderLoaderInternal

std::string Eng::ShaderLoader::ReadGLSLContent(std::string_view name, Ren::ILog *log) {
    using namespace ShaderLoaderInternal;

    std::string file_path = SHADERS_PATH;
    file_path += name;

    Sys::AssetFile in_file(file_path);
    if (!in_file) {
        log->Error("Cannon open %s", file_path.c_str());
        return {};
    }
    const size_t in_file_size = in_file.size();

    std::string temp_buf;
    temp_buf.resize(in_file_size + 1);
    in_file.Read(&temp_buf[0], in_file_size);
    temp_buf[in_file_size] = '\0';

    return temp_buf;
}
