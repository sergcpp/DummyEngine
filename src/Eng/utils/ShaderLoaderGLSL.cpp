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

Ren::eShaderType ShaderTypeFromName(const char *name, int len);
} // namespace ShaderLoaderInternal

std::string Eng::ShaderLoader::ReadGLSLContent(const char *name, int name_len, const char *prelude, Ren::ILog *log) {
    using namespace ShaderLoaderInternal;

    std::string file_path = SHADERS_PATH;
    file_path += name_len == -1 ? name : std::string(name, name_len);

    Sys::AssetFile in_file(file_path);
    if (!in_file) {
        log->Error("Cannon open %s", file_path.c_str());
        return {};
    }
    const size_t in_file_size = in_file.size();

    std::string temp_buf;
    temp_buf.resize(temp_buf.size() + in_file_size);
    in_file.Read(&temp_buf[0], in_file_size);

    Sys::MemBuf in_buf{(const uint8_t *)&temp_buf[0], temp_buf.size()};
    std::istream in_stream(&in_buf);

    std::string dst_buf;

    std::string line;
    int line_counter = 0;

    while (std::getline(in_stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line = line.substr(0, line.size() - 1);
        }

        if (prelude && line.rfind("#version ") == std::string::npos && line.rfind("#extension ") == std::string::npos) {
            dst_buf += prelude;
            dst_buf += line;
            dst_buf += "\r\n";
            prelude = nullptr;
        } else if (line.rfind("#include ") == 0) {
            const size_t n1 = line.find_first_of('\"');
            const size_t n2 = line.find_last_of('\"');

            const std::string file_name = line.substr(n1 + 1, n2 - n1 - 1);

            const char *slash_pos = strrchr(name, '/');
            std::string full_path;
            if (slash_pos) {
                full_path = std::string(name, size_t(slash_pos - name + 1)) + file_name;
            } else {
                full_path = file_name;
            }

            dst_buf += "#line 0\r\n";
            dst_buf += ReadGLSLContent(full_path.c_str(), -1, nullptr, log);
            dst_buf += "\r\n#line ";
            dst_buf += std::to_string(line_counter) + "\r\n";
        } else {
            dst_buf += line;
            dst_buf += "\r\n";
        }
        line_counter++;
    }

    return dst_buf;
}

Ren::ShaderRef Eng::ShaderLoader::LoadGLSL(Ren::Context &ctx, const char *name, const Param *params) {
    using namespace ShaderLoaderInternal;

    temp_param_str_.clear();
    temp_param_def_.clear();

    temp_param_str_ += name;
    const int name_len = int(temp_param_str_.length());
    if (temp_param_str_.length() < 10) {
        return {};
    }

    const Ren::eShaderType type = ShaderTypeFromName(temp_param_str_.c_str(), name_len);
    if (type == Ren::eShaderType::_Count) {
        ctx.log()->Error("Shader name is not correct (%s)", name);
    }

    ParamsToString(params, temp_param_str_, temp_param_def_);

    Ren::eShaderLoadStatus status;
    Ren::ShaderRef ret = ctx.LoadShaderGLSL(temp_param_str_.c_str(), nullptr, type, &status);
    if (!ret->ready()) {
        const std::string shader_src =
            ReadGLSLContent(name, name_len, !temp_param_def_.empty() ? temp_param_def_.c_str() : nullptr, ctx.log());
        ret->Init(shader_src.c_str(), type, &status, ctx.log());
        if (status == Ren::eShaderLoadStatus::SetToDefault) {
            ctx.log()->Error("Error loading shader %s", name);
        }
    }
    return ret;
}
