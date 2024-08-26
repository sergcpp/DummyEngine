#include "ShaderLoader.h"

#include <Ren/Context.h>
#include <Sys/AssetFile.h>
#include <Sys/MemBuf.h>

namespace ShaderLoaderInternal {
#if defined(__ANDROID__)
const char *SHADERS_PATH = "./assets/shaders/";
#else
const char *SHADERS_PATH = "./assets_pc/shaders/";
#endif

Ren::eShaderType ShaderTypeFromName(std::string_view name) {
    Ren::eShaderType type;
    if (std::strncmp(name.data() + name.length() - 10, ".vert.glsl", 10) == 0) {
        type = Ren::eShaderType::Vertex;
    } else if (std::strncmp(name.data() + name.length() - 10, ".frag.glsl", 10) == 0) {
        type = Ren::eShaderType::Fragment;
    } else if (std::strncmp(name.data() + name.length() - 10, ".tesc.glsl", 10) == 0) {
        type = Ren::eShaderType::TesselationControl;
    } else if (std::strncmp(name.data() + name.length() - 10, ".tese.glsl", 10) == 0) {
        type = Ren::eShaderType::TesselationEvaluation;
    } else if (std::strncmp(name.data() + name.length() - 10, ".geom.glsl", 10) == 0) {
        type = Ren::eShaderType::Geometry;
    } else if (std::strncmp(name.data() + name.length() - 10, ".comp.glsl", 10) == 0) {
        type = Ren::eShaderType::Compute;
    } else if (std::strncmp(name.data() + name.length() - 10, ".rgen.glsl", 10) == 0) {
        type = Ren::eShaderType::RayGen;
    } else if (std::strncmp(name.data() + name.length() - 11, ".rchit.glsl", 11) == 0) {
        type = Ren::eShaderType::ClosestHit;
    } else if (std::strncmp(name.data() + name.length() - 11, ".rahit.glsl", 11) == 0) {
        type = Ren::eShaderType::AnyHit;
    } else if (std::strncmp(name.data() + name.length() - 11, ".rmiss.glsl", 11) == 0) {
        type = Ren::eShaderType::Miss;
    } else if (std::strncmp(name.data() + name.length() - 10, ".rint.glsl", 10) == 0) {
        type = Ren::eShaderType::Intersection;
    } else {
        type = Ren::eShaderType::_Count;
    }
    return type;
}
} // namespace ShaderLoaderInternal

Eng::ShaderLoader::ShaderLoader() {
    temp_param_str_.reserve(1024);
    temp_param_def_.reserve(4096);
}

int Eng::ShaderLoader::ParamsToString(const Param *params, std::string &out_str, std::string &out_def) {
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

int Eng::ShaderLoader::ParamsStringToDef(const char *params, std::string &out_def) {
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

Ren::ProgramRef Eng::ShaderLoader::LoadProgram(Ren::Context &ctx, std::string_view vs_name, std::string_view fs_name,
                                               std::string_view tcs_name, std::string_view tes_name,
                                               std::string_view gs_name) {
    std::string prog_name = std::string(vs_name);
    prog_name += "&" + std::string(fs_name);
    if (!tcs_name.empty()) {
        prog_name += "&" + std::string(tcs_name);
    }
    if (!tes_name.empty()) {
        prog_name += "&" + std::string(tes_name);
    }
    if (!gs_name.empty()) {
        prog_name += "&" + std::string(gs_name);
    }

    Ren::eProgLoadStatus status;
    Ren::ProgramRef ret = ctx.LoadProgram(prog_name, {}, {}, {}, {}, {}, &status);
    if (!ret->ready()) {
        // ctx.log()->Info("Loading %s", prog_name.c_str());
        Ren::ShaderRef vs_ref = LoadShader(ctx, vs_name);
        Ren::ShaderRef fs_ref = LoadShader(ctx, fs_name);
        if (!vs_ref || !fs_ref) {
            ctx.log()->Error("Error loading shaders %s/%s", vs_name.data(), fs_name.data());
            return {};
        }

        Ren::ShaderRef tcs_ref, tes_ref;
        if (!tcs_name.empty() && !tes_name.empty()) {
            tcs_ref = LoadShader(ctx, tcs_name);
            tes_ref = LoadShader(ctx, tes_name);
            if (!tcs_ref || !tes_ref) {
                ctx.log()->Error("Error loading shaders %s/%s", tcs_name.data(), tes_name.data());
                return {};
            }
        }
        Ren::ShaderRef gs_ref;
        if (!gs_name.empty()) {
            gs_ref = LoadShader(ctx, gs_name);
            if (!gs_ref) {
                ctx.log()->Error("Error loading shader %s", gs_name.data());
                return {};
            }
        }
        ret->Init(vs_ref, fs_ref, tcs_ref, tes_ref, gs_ref, &status, ctx.log());
        if (status == Ren::eProgLoadStatus::SetToDefault) {
            ctx.log()->Error("Error loading program %s", prog_name.c_str());
        }
    }
    return ret;
}

Ren::ProgramRef Eng::ShaderLoader::LoadProgram(Ren::Context &ctx, std::string_view cs_name) {
    Ren::eProgLoadStatus status;
    Ren::ProgramRef ret = ctx.LoadProgram(cs_name, {}, &status);
    if (!ret->ready()) {
        // ctx.log()->Info("Loading %s", cs_name.data());
        Ren::ShaderRef cs_ref = LoadShader(ctx, cs_name);
        ret->Init(cs_ref, &status, ctx.log());
        if (status == Ren::eProgLoadStatus::SetToDefault) {
            ctx.log()->Error("Error loading program %s", cs_name.data());
        }
    }
    return ret;
}

#if defined(USE_VK_RENDER)
Ren::ProgramRef Eng::ShaderLoader::LoadProgram2(Ren::Context &ctx, std::string_view raygen_name,
                                                std::string_view closesthit_name, std::string_view anyhit_name,
                                                std::string_view miss_name, std::string_view intersection_name) {
    std::string prog_name = std::string(raygen_name);
    prog_name += "&" + std::string(closesthit_name);
    if (!anyhit_name.empty()) {
        prog_name += "&" + std::string(anyhit_name);
    }
    if (!miss_name.empty()) {
        prog_name += "&" + std::string(miss_name);
    }
    if (!intersection_name.empty()) {
        prog_name += "&" + std::string(intersection_name);
    }

    Ren::eProgLoadStatus status;
    Ren::ProgramRef ret = ctx.LoadProgram2(prog_name, {}, {}, {}, {}, {}, &status);
    if (!ret->ready()) {
        // ctx.log()->Info("Loading %s", prog_name.c_str());
        Ren::ShaderRef raygen_ref = LoadShader(ctx, raygen_name);

        Ren::ShaderRef closesthit_ref, anyhit_ref;
        if (!closesthit_name.empty()) {
            closesthit_ref = LoadShader(ctx, closesthit_name);
        }
        if (!anyhit_name.empty()) {
            anyhit_ref = LoadShader(ctx, anyhit_name);
        }

        Ren::ShaderRef miss_ref = LoadShader(ctx, miss_name);

        Ren::ShaderRef intersection_ref;
        if (!intersection_name.empty()) {
            intersection_ref = LoadShader(ctx, intersection_name);
        }

        ret->Init2(std::move(raygen_ref), std::move(closesthit_ref), std::move(anyhit_ref), std::move(miss_ref),
                   std::move(intersection_ref), &status, ctx.log());
        if (status == Ren::eProgLoadStatus::SetToDefault) {
            ctx.log()->Error("Error loading program %s", prog_name.c_str());
        }
    }
    return ret;
}
#endif

Ren::ShaderRef Eng::ShaderLoader::LoadShader(Ren::Context &ctx, std::string_view name) {
    using namespace ShaderLoaderInternal;

    const Ren::eShaderType type = ShaderTypeFromName(name);
    if (type == Ren::eShaderType::_Count) {
        ctx.log()->Error("Shader name is not correct (%s)", name.data());
        return {};
    }

    Ren::eShaderLoadStatus status;
    Ren::ShaderRef ret = ctx.LoadShaderGLSL(name, {}, type, &status);
    if (!ret->ready()) {
        temp_param_def_.clear();

#if defined(USE_VK_RENDER)
        if (ctx.capabilities.spirv) {
            std::string spv_name = SHADERS_PATH;
            spv_name += name;
            const size_t n = spv_name.rfind(".glsl");
            assert(n != std::string::npos);

#if defined(NDEBUG)
            spv_name.replace(n + 1, 4, "spv");
#else
            spv_name.replace(n + 1, 4, "spv_dbg");
#endif

            Sys::AssetFile spv_file(spv_name);
            if (spv_file) {
                const size_t spv_data_size = spv_file.size();

                std::vector<uint8_t> spv_data(spv_data_size);
                spv_file.Read((char *)&spv_data[0], spv_data_size);

                ret->Init(spv_data, type, &status, ctx.log());
                if (status == Ren::eShaderLoadStatus::CreatedFromData) {
                    return ret;
                }
            }
        }
#endif

        const std::string shader_src = ReadGLSLContent(name, ctx.log());
        if (!shader_src.empty()) {
            ret->Init(shader_src, type, &status, ctx.log());
            if (status == Ren::eShaderLoadStatus::SetToDefault) {
                ctx.log()->Error("Error loading shader %s", name.data());
            }
        } else {
            ctx.log()->Error("Error loading shader %s", name.data());
        }
    }

    return ret;
}