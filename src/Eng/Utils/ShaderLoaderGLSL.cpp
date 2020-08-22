#include "ShaderLoader.h"

#include <Ren/Context.h>
#include <Sys/AssetFile.h>

namespace ShaderLoaderInternal {
#if defined(__ANDROID__)
const char *SHADERS_PATH = "./assets/shaders/";
#else
const char *SHADERS_PATH = "./assets_pc/shaders/";
#endif

Ren::eShaderType ShaderTypeFromName(const char *name, const int len) {
    Ren::eShaderType type;
    if (std::strncmp(name + len - 10, ".vert.glsl", 10) == 0) {
        type = Ren::eShaderType::Vert;
    } else if (std::strncmp(name + len - 10, ".frag.glsl", 10) == 0) {
        type = Ren::eShaderType::Frag;
    } else if (std::strncmp(name + len - 10, ".tesc.glsl", 10) == 0) {
        type = Ren::eShaderType::Tesc;
    } else if (std::strncmp(name + len - 10, ".tese.glsl", 10) == 0) {
        type = Ren::eShaderType::Tese;
    } else if (std::strncmp(name + len - 10, ".comp.glsl", 10) == 0) {
        type = Ren::eShaderType::Comp;
    } else {
        type = Ren::eShaderType::_Count;
    }
    return type;
}

const bool TryToLoadSPIRV = false;
} // namespace ShaderLoaderInternal

ShaderLoader::ShaderLoader() {
    temp_param_str_.reserve(1024);
    temp_param_def_.reserve(4096);
}

std::string ShaderLoader::ReadGLSLContent(const char *name, int name_len,
                                          const char *prelude, Ren::ILog *log) {
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

        if (prelude && line.rfind("#version ") == std::string::npos &&
            line.rfind("#extension ") == std::string::npos) {
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

int ShaderLoader::ParamsToString(const Param *params, std::string &out_str,
                                 std::string &out_def) {
    const Param *param = params;
    while (param->key) {
        out_str += out_str.empty() ? '@' : ';';
        out_def += "#define ";
        out_str += param->key;
        out_def += param->key;
        if (param->val) {
            out_str += "=";
            out_str += param->val;
            out_def += " ";
            out_def += param->val;
        }
        out_def += '\n';
        ++param;
    }
    return int(param - params);
}

int ShaderLoader::ParamsStringToDef(const char *params, std::string &out_def) {
    if (!params || params[0] != '@') {
        return 0;
    }

    int count = 0;

    const char *p1 = params + 1;
    const char *p2 = p1 + 1;
    while (*p2) {
        if (*p2 == '=') {
            out_def += "#define ";
            out_def += std::string(p1, p2);
            out_def += " ";

            p1 = p2 + 1;
            while (p2 && *p2 && *p2 != ';') {
                ++p2;
            }

            out_def += std::string(p1, p2);
            out_def += '\n';

            if (*p2) {
                p1 = ++p2;
            }
            ++count;
        } else if (*p2 == ';') {
            out_def += "#define ";
            out_def += std::string(p1, p2);
            out_def += '\n';
            p1 = ++p2;
            ++count;
        }

        if (*p2) {
            ++p2;
        }
    }

    if (p1 != p2) {
        out_def += "#define ";
        out_def += std::string(p1, p2);
        out_def += '\n';
        ++count;
    }

    return count;
}

Ren::ShaderRef ShaderLoader::LoadGLSL(Ren::Context &ctx, const char *name) {
    using namespace ShaderLoaderInternal;

    const char *params = strchr(name, '@');
    const int name_len = params ? int(params - name) : (int)strlen(name);
    if (name_len < 10) {
        ctx.log()->Error("Shader name is not correct (%s)", name);
        return {};
    }

    const Ren::eShaderType type = ShaderTypeFromName(name, name_len);
    if (type == Ren::eShaderType::_Count) {
        ctx.log()->Error("Shader name is not correct (%s)", name);
        return {};
    }

    Ren::eShaderLoadStatus status;
    Ren::ShaderRef ret = ctx.LoadShaderGLSL(name, nullptr, type, &status);
    if (!ret->ready()) {
        temp_param_def_.clear();

#ifndef __ANDROID__
        if (TryToLoadSPIRV && ctx.capabilities.gl_spirv) {
            std::string spv_name = SHADERS_PATH;
            spv_name += name;
            const size_t n = spv_name.rfind(".glsl");
            assert(n != std::string::npos);
            spv_name.replace(n + 1, 4, "spv");

            Sys::AssetFile spv_file(spv_name);
            if (spv_file) {
                const size_t spv_data_size = spv_file.size();

                std::unique_ptr<uint8_t[]> spv_data(new uint8_t[spv_data_size]);
                spv_file.Read((char*)&spv_data[0], spv_data_size);

                ret->Init(&spv_data[0], int(spv_data_size), type, &status, ctx.log());
                if (status == Ren::eShaderLoadStatus::CreatedFromData) {
                    return ret;
                }
            }
        }
#endif

        const int params_cnt = ParamsStringToDef(params, temp_param_def_);
        assert(params_cnt != -1);

        const std::string shader_src = ReadGLSLContent(
            name, name_len, !temp_param_def_.empty() ? temp_param_def_.c_str() : nullptr,
            ctx.log());
        if (!shader_src.empty()) {
            ret->Init(shader_src.c_str(), type, &status, ctx.log());
            if (status == Ren::eShaderLoadStatus::SetToDefault) {
                ctx.log()->Error("Error loading shader %s", name);
            }
        } else {
            ctx.log()->Error("Error loading shader %s", name);
        }
    }

    return ret;
}

Ren::ShaderRef ShaderLoader::LoadGLSL(Ren::Context &ctx, const char *name,
                                      const Param *params) {
    return LoadGLSL(ctx, name, &params, 1);
}

Ren::ShaderRef ShaderLoader::LoadGLSL(Ren::Context &ctx, const char *name,
                                      const Param *const *params,
                                      const int params_count) {
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

    for (int i = 0; i < params_count; i++) {
        const Param *param = params[i];
        const int params_cnt = ParamsToString(param, temp_param_str_, temp_param_def_);
        assert(params_cnt != -1);
    }

    Ren::eShaderLoadStatus status;
    Ren::ShaderRef ret =
        ctx.LoadShaderGLSL(temp_param_str_.c_str(), nullptr, type, &status);
    if (!ret->ready()) {
        const std::string shader_src = ReadGLSLContent(
            name, name_len, !temp_param_def_.empty() ? temp_param_def_.c_str() : nullptr,
            ctx.log());
        ret->Init(shader_src.c_str(), type, &status, ctx.log());
        if (status == Ren::eShaderLoadStatus::SetToDefault) {
            ctx.log()->Error("Error loading shader %s", name);
        }
    }
    return ret;
}

Ren::ProgramRef ShaderLoader::LoadProgram(Ren::Context &ctx, const char *name,
                                          const char *vs_name, const char *fs_name,
                                          const char *tcs_name, const char *tes_name) {
    Ren::eProgLoadStatus status;
    Ren::ProgramRef ret = ctx.LoadProgram(name, {}, {}, {}, {}, &status);
    if (!ret->ready()) {
        ctx.log()->Info("Loading %s", name);
        Ren::ShaderRef vs_ref = LoadGLSL(ctx, vs_name);
        Ren::ShaderRef fs_ref = LoadGLSL(ctx, fs_name);
        if (!vs_ref->ready() || !fs_ref->ready()) {
            ctx.log()->Error("Error loading program %s", name);
            return {};
        }

        Ren::ShaderRef tcs_ref, tes_ref;
        if (tcs_name && tes_name) {
            tcs_ref = LoadGLSL(ctx, tcs_name);
            tes_ref = LoadGLSL(ctx, tes_name);
            if (!tcs_ref->ready() || !tes_ref->ready()) {
                ctx.log()->Error("Error loading program %s", name);
                return {};
            }
        }
        ret->Init(vs_ref, fs_ref, tcs_ref, tes_ref, &status, ctx.log());
        if (status == Ren::eProgLoadStatus::SetToDefault) {
            ctx.log()->Error("Error loading program %s", name);
        }
    }
    return ret;
}

Ren::ProgramRef ShaderLoader::LoadProgram(Ren::Context &ctx, const char *name,
                                          const char *cs_name) {
    Ren::eProgLoadStatus status;
    Ren::ProgramRef ret = ctx.LoadProgram(name, {}, {}, {}, {}, &status);
    if (!ret->ready()) {
        ctx.log()->Info("Loading %s", name);
        Ren::ShaderRef cs_ref = LoadGLSL(ctx, cs_name);
        ret->Init(cs_ref, &status, ctx.log());
        if (status == Ren::eProgLoadStatus::SetToDefault) {
            ctx.log()->Error("Error loading program %s", name);
        }
    }
    return ret;
}