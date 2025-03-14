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

Ren::VertexInputRef Eng::ShaderLoader::LoadVertexInput(Ren::Span<const Ren::VtxAttribDesc> attribs,
                                                       const Ren::BufRef &elem_buf) {
    std::lock_guard<std::mutex> _(mtx_);

    Ren::VertexInputRef ref = vtx_inputs_.LowerBound([&](const Ren::VertexInput &vi) {
        if (vi.elem_buf < elem_buf) {
            return true;
        } else if (vi.elem_buf == elem_buf) {
            return Ren::Span<const Ren::VtxAttribDesc>(vi.attribs) < attribs;
        }
        return false;
    });
    if (!ref || ref->elem_buf != elem_buf || Ren::Span<const Ren::VtxAttribDesc>(ref->attribs) != attribs) {
        ref = vtx_inputs_.Insert(attribs, elem_buf);
    }
    return ref;
}

Ren::RenderPassRef Eng::ShaderLoader::LoadRenderPass(const Ren::RenderTargetInfo &depth_rt,
                                                     Ren::Span<const Ren::RenderTargetInfo> color_rts) {
    std::lock_guard<std::mutex> _(mtx_);

    Ren::RenderPassRef ref =
        render_passes_.LowerBound([&](const Ren::RenderPass &rp) { return rp.LessThan(depth_rt, color_rts); });
    if (!ref || !ref->Equals(depth_rt, color_rts)) {
        ref = render_passes_.Insert(ctx_.api_ctx(), depth_rt, color_rts, ctx_.log());
    }
    return ref;
}

Ren::ProgramRef Eng::ShaderLoader::LoadProgram(std::string_view vs_name, std::string_view fs_name,
                                               std::string_view tcs_name, std::string_view tes_name,
                                               std::string_view gs_name) {
    Ren::ShaderRef vs_ref = LoadShader(vs_name);
    Ren::ShaderRef fs_ref = LoadShader(fs_name);
    if (!vs_ref || !fs_ref) {
        ctx_.log()->Error("Error loading shaders %s/%s", vs_name.data(), fs_name.data());
        return {};
    }

    Ren::ShaderRef tcs_ref, tes_ref;
    if (!tcs_name.empty() && !tes_name.empty()) {
        tcs_ref = LoadShader(tcs_name);
        tes_ref = LoadShader(tes_name);
        if (!tcs_ref || !tes_ref) {
            ctx_.log()->Error("Error loading shaders %s/%s", tcs_name.data(), tes_name.data());
            return {};
        }
    }
    Ren::ShaderRef gs_ref;
    if (!gs_name.empty()) {
        gs_ref = LoadShader(gs_name);
        if (!gs_ref) {
            ctx_.log()->Error("Error loading shader %s", gs_name.data());
            return {};
        }
    }

    std::lock_guard<std::mutex> _(mtx_);

    std::array<Ren::ShaderRef, int(Ren::eShaderType::_Count)> temp_shaders;
    temp_shaders[int(Ren::eShaderType::Vertex)] = vs_ref;
    temp_shaders[int(Ren::eShaderType::Fragment)] = fs_ref;
    temp_shaders[int(Ren::eShaderType::TesselationControl)] = tcs_ref;
    temp_shaders[int(Ren::eShaderType::TesselationEvaluation)] = tes_ref;
    temp_shaders[int(Ren::eShaderType::Geometry)] = gs_ref;
    Ren::ProgramRef ref = programs_.LowerBound([&](const Ren::Program &p) { return p.shaders() < temp_shaders; });
    if (!ref || ref->shaders() != temp_shaders) {
        assert(vs_ref && fs_ref);
        ref = programs_.Insert(ctx_.api_ctx(), std::move(vs_ref), std::move(fs_ref), std::move(tcs_ref),
                               std::move(tes_ref), std::move(gs_ref), ctx_.log());
    }
    return ref;
}

Ren::ProgramRef Eng::ShaderLoader::LoadProgram(std::string_view cs_name) {
    Ren::ShaderRef cs_ref = LoadShader(cs_name);

    std::array<Ren::ShaderRef, int(Ren::eShaderType::_Count)> temp_shaders;
    temp_shaders[int(Ren::eShaderType::Compute)] = cs_ref;

    Ren::ProgramRef ret;
    { // find program
        std::lock_guard<std::mutex> _(mtx_);
        ret = programs_.LowerBound([&](const Ren::Program &p) { return p.shaders() < temp_shaders; });
    }
    if (!ret || ret->shaders() != temp_shaders) {
        assert(cs_ref);
        Ren::Program new_program(ctx_.api_ctx(), std::move(cs_ref), ctx_.log());

        std::lock_guard<std::mutex> _(mtx_);
        ret = programs_.Insert(std::move(new_program));
        assert(programs_.CheckUnique());
    }
    return ret;
}

#if defined(REN_VK_BACKEND)
Ren::ProgramRef Eng::ShaderLoader::LoadProgram2(std::string_view raygen_name, std::string_view closesthit_name,
                                                std::string_view anyhit_name, std::string_view miss_name,
                                                std::string_view intersection_name) {
    Ren::ShaderRef raygen_ref = LoadShader(raygen_name);

    Ren::ShaderRef closesthit_ref, anyhit_ref;
    if (!closesthit_name.empty()) {
        closesthit_ref = LoadShader(closesthit_name);
    }
    if (!anyhit_name.empty()) {
        anyhit_ref = LoadShader(anyhit_name);
    }

    Ren::ShaderRef miss_ref = LoadShader(miss_name);

    Ren::ShaderRef intersection_ref;
    if (!intersection_name.empty()) {
        intersection_ref = LoadShader(intersection_name);
    }

    std::lock_guard<std::mutex> _(mtx_);

    std::array<Ren::ShaderRef, int(Ren::eShaderType::_Count)> temp_shaders;
    temp_shaders[int(Ren::eShaderType::RayGen)] = raygen_ref;
    temp_shaders[int(Ren::eShaderType::ClosestHit)] = closesthit_ref;
    temp_shaders[int(Ren::eShaderType::AnyHit)] = anyhit_ref;
    temp_shaders[int(Ren::eShaderType::Miss)] = miss_ref;
    temp_shaders[int(Ren::eShaderType::Intersection)] = intersection_ref;
    Ren::ProgramRef ref = programs_.LowerBound([&](const Ren::Program &p) { return p.shaders() < temp_shaders; });
    if (!ref || ref->shaders() != temp_shaders) {
        assert(raygen_ref);
        ref = programs_.Insert(ctx_.api_ctx(), std::move(raygen_ref), std::move(closesthit_ref), std::move(anyhit_ref),
                               std::move(miss_ref), std::move(intersection_ref), ctx_.log(), 0);
    }
    return ref;
}
#endif

Ren::ShaderRef Eng::ShaderLoader::LoadShader(std::string_view name) {
    using namespace ShaderLoaderInternal;

    std::lock_guard<std::mutex> _(mtx_);

    const Ren::eShaderType type = ShaderTypeFromName(name);
    if (type == Ren::eShaderType::_Count) {
        ctx_.log()->Error("Shader name is not correct (%s)", name.data());
        return {};
    }

    Ren::ShaderRef ret = shaders_.FindByName(name);
    if (!ret) {
        ret = shaders_.Insert(name, ctx_.api_ctx(), Ren::Span<const uint8_t>{}, type, ctx_.log());
    }
    if (!ret->ready()) {
#if defined(REN_VK_BACKEND)
        if (ctx_.capabilities.spirv) {
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

                ret->Init(spv_data, type, ctx_.log());
                if (ret->ready()) {
                    return ret;
                }
            }
        }
#endif
#if defined(REN_GL_BACKEND)
        const std::string shader_src = ReadGLSLContent(name, ctx_.log());
        if (!shader_src.empty()) {
            ret->Init(shader_src, type, ctx_.log());
            if (!ret->ready()) {
                ctx_.log()->Error("Error loading shader %s", name.data());
            }
        } else {
            ctx_.log()->Error("Error loading shader %s", name.data());
        }
#endif
    }

    return ret;
}

Ren::PipelineRef Eng::ShaderLoader::LoadPipeline(std::string_view cs_name, const int subgroup_size) {
    Ren::ProgramRef prog_ref = LoadProgram(cs_name);
    return LoadPipeline(prog_ref, subgroup_size);
}

Ren::PipelineRef Eng::ShaderLoader::LoadPipeline(const Ren::ProgramRef &prog, const int subgroup_size) {
    Ren::PipelineRef ret;
    { // find pipeline
        std::lock_guard<std::mutex> _(mtx_);
        ret = pipelines_.LowerBound([&](const Ren::Pipeline &pi) { return pi.LessThan({}, prog, {}, {}); });
    }
    if (!ret || !ret->Equals({}, prog, {}, {})) {
        assert(prog);
        Ren::Pipeline new_pipeline(ctx_.api_ctx(), prog, ctx_.log(), subgroup_size);

        std::lock_guard<std::mutex> _(mtx_);
        ret = pipelines_.Insert(std::move(new_pipeline));
        assert(ret.strong_refs() == 1);
        persistent_pipelines_.push_back(ret);
        assert(pipelines_.CheckUnique());
    }
    return ret;
}

Ren::PipelineRef Eng::ShaderLoader::LoadPipeline(const Ren::RastState &rast_state, const Ren::ProgramRef &prog,
                                                 const Ren::VertexInputRef &vtx_input,
                                                 const Ren::RenderPassRef &render_pass, const uint32_t subpass_index) {
    Ren::PipelineRef ret;
    { // find pipeline
        std::lock_guard<std::mutex> _(mtx_);
        ret = pipelines_.LowerBound(
            [&](const Ren::Pipeline &pi) { return pi.LessThan(rast_state, prog, vtx_input, render_pass); });
    }
    if (!ret || !ret->Equals(rast_state, prog, vtx_input, render_pass)) {
        Ren::Pipeline new_pipeline(ctx_.api_ctx(), rast_state, prog, vtx_input, render_pass, subpass_index, ctx_.log());

        std::lock_guard<std::mutex> _(mtx_);
        ret = pipelines_.Insert(std::move(new_pipeline));
        assert(ret.strong_refs() == 1);
        persistent_pipelines_.push_back(ret);
        assert(pipelines_.CheckUnique());
    }
    return ret;
}
