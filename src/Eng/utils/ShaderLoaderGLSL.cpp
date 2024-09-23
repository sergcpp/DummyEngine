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

Ren::ShaderRef Eng::ShaderLoader::LoadGLSL(Ren::Context &ctx, std::string_view name, const Param *params) {
    using namespace ShaderLoaderInternal;

    temp_param_str_.clear();
    temp_param_def_.clear();

    temp_param_str_ += name;
    const int name_len = int(temp_param_str_.length());
    if (temp_param_str_.length() < 10) {
        return {};
    }

    const Ren::eShaderType type = ShaderTypeFromName(temp_param_str_);
    if (type == Ren::eShaderType::_Count) {
        ctx.log()->Error("Shader name is not correct (%s)", name.data());
    }

    ParamsToString(params, temp_param_str_, temp_param_def_);

    Ren::eShaderLoadStatus status;
    Ren::ShaderRef ret = ctx.LoadShaderGLSL(temp_param_str_, {}, type, &status);
    if (!ret->ready()) {
        const std::string shader_src = ReadGLSLContent(name, ctx.log());
        ret->Init(shader_src, type, &status, ctx.log());
        if (status == Ren::eShaderLoadStatus::SetToDefault) {
            ctx.log()->Error("Error loading shader %s", name.data());
        }
    }
    return ret;
}
