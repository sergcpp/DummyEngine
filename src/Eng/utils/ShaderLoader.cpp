#include "ShaderLoader.h"

#include <fstream>

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

Eng::ShaderLoader::ShaderLoader(Ren::Context &ctx) : ctx_(ctx) {
    // prevent reallocation
    ctx.storages().vtx_inputs.Reserve(32);
    ctx.storages().render_passes.Reserve(128);
    ctx.storages().shaders.Reserve(2048);
    ctx.storages().programs.Reserve(1024);
    ctx.storages().pipelines.Reserve(2048);
}

Eng::ShaderLoader::~ShaderLoader() {
    // destroy renderpasses
    for (const Ren::RenderPassHandle rp : render_passes_) {
        ctx_.ReleaseRenderPass(rp);
    }
    // destroy pipelines
    for (const Ren::PipelineHandle pi : pipelines_) {
        ctx_.ReleasePipeline(pi);
    }
    // destroy programs
    for (const Ren::ProgramHandle pr : programs_) {
        ctx_.ReleaseProgram(pr);
    }
    // destroy shaders
    for (auto it = shaders_.begin(); it != shaders_.end(); ++it) {
        ctx_.ReleaseShader(it->val);
    }
    // vertex inputs
    for (const Ren::VertexInputHandle vi : vtx_inputs_) {
        ctx_.ReleaseVertexInput(vi);
    }
}

void Eng::ShaderLoader::LoadPipelineCache(const char *base_path) {
#if defined(REN_VK_BACKEND)
    { // Load pipeline cache
        std::vector<uint8_t> cache_data;

        std::string file_name = base_path;
        file_name += std::to_string(ctx_.device_id());
        file_name += ".vk_cache";

        std::ifstream in_file(file_name, std::ios::binary | std::ios::ate);
        if (in_file) {
            const size_t in_file_size = size_t(in_file.tellg());
            in_file.seekg(0, std::ios::beg);
            cache_data.resize(in_file_size);
            in_file.read((char *)cache_data.data(), in_file_size);
        }

        ctx_.InitPipelineCache(cache_data);
    }
#endif
}

void Eng::ShaderLoader::WritePipelineCache(const char *base_path) {
#if defined(REN_VK_BACKEND)
    const size_t data_size = ctx_.WritePipelineCache({});
    if (data_size) {
        std::vector<uint8_t> data(data_size);
        const size_t written_size = ctx_.WritePipelineCache(data);
        if (written_size != data_size) {
            ctx_.log()->Error("Failed to write pipeline cache");
        }

        std::string file_name = base_path;
        file_name += std::to_string(ctx_.device_id());
        file_name += ".vk_cache";

        { // Write out file
            std::ofstream out_file(file_name + "_temp", std::ios::binary);
            out_file.write((char *)data.data(), data_size);
            if (!out_file.good() || out_file.tellp() != data_size) {
                ctx_.log()->Error("Failed to write pipeline cache");
            }
        }

        remove(file_name.c_str());
        if (rename((file_name + "_temp").c_str(), file_name.c_str()) != 0) {
            ctx_.log()->Error("Failed to rename pipeline cache");
        }
    } else {
        ctx_.log()->Error("Failed to write pipeline cache");
    }
#endif
}

Ren::VertexInputHandle Eng::ShaderLoader::FindOrCreateVertexInput(Ren::Span<const Ren::VtxAttribDesc> attribs) {
    std::lock_guard<std::mutex> _(mtx_);
    const auto it = lower_bound(
        std::begin(vtx_inputs_), std::end(vtx_inputs_), attribs,
        [this](const Ren::VertexInputHandle lhs_handle, Ren::Span<const Ren::VtxAttribDesc> attribs) {
            return Ren::Span<const Ren::VtxAttribDesc>(ctx_.storages().vtx_inputs[lhs_handle].attribs) < attribs;
        });
    if (it != std::end(vtx_inputs_) &&
        Ren::Span<const Ren::VtxAttribDesc>(ctx_.storages().vtx_inputs[*it].attribs) == attribs) {
        return *it;
    }
    const Ren::VertexInputHandle ret = ctx_.CreateVertexInput(attribs);
    vtx_inputs_.insert(it, ret);
    return ret;
}

Ren::RenderPassHandle Eng::ShaderLoader::FindOrCreateRenderPass(const Ren::RenderTargetInfo &depth_rt,
                                                                Ren::Span<const Ren::RenderTargetInfo> color_rts) {
    std::lock_guard<std::mutex> _(mtx_);

    const auto it =
        partition_point(std::begin(render_passes_), std::end(render_passes_), [&](const Ren::RenderPassHandle lhs) {
            return ctx_.storages().render_passes[lhs].LessThan(depth_rt, color_rts);
        });
    if (it != std::end(render_passes_) && ctx_.storages().render_passes[*it].Equals(depth_rt, color_rts)) {
        return *it;
    }

    const Ren::RenderPassHandle ret = ctx_.CreateRenderPass(depth_rt, color_rts);
    if (ret) {
        render_passes_.insert(it, ret);
    }
    return ret;
}

Ren::RenderPassHandle Eng::ShaderLoader::FindOrCreateRenderPass(const Ren::RenderTarget &depth_rt,
                                                                Ren::Span<const Ren::RenderTarget> color_rts) {
    Ren::SmallVector<Ren::RenderTargetInfo, 4> color_infos;
    Ren::RenderTargetInfo depth_info;
    { //
        std::lock_guard<std::mutex> _(mtx_);
        for (int i = 0; i < color_rts.size(); ++i) {
            const auto &[img_main, img_cold] = ctx_.storages().images[color_rts[i].img];
            const Ren::eImageLayout layout = ImageLayoutForState(img_main.resource_state);
            color_infos.emplace_back(img_cold.params.format, uint8_t(img_cold.params.samples), layout,
                                     color_rts[i].load, color_rts[i].store, color_rts[i].stencil_load,
                                     color_rts[i].stencil_store);
        }
        if (depth_rt) {
            const auto &[img_main, img_cold] = ctx_.storages().images[depth_rt.img];
            const Ren::eImageLayout layout = ImageLayoutForState(img_main.resource_state);
            depth_info = {img_cold.params.format,
                          uint8_t(img_cold.params.samples),
                          layout,
                          depth_rt.load,
                          depth_rt.store,
                          depth_rt.stencil_load,
                          depth_rt.stencil_store};
        }
    }
    return FindOrCreateRenderPass(depth_info, color_infos);
}

Ren::ShaderHandle Eng::ShaderLoader::FindOrCreateShader(std::string_view name) {
    using namespace ShaderLoaderInternal;

    const Ren::eShaderType type = ShaderTypeFromName(name);
    if (type == Ren::eShaderType::_Count) {
        ctx_.log()->Error("Shader name is not correct (%s)", name.data());
        return {};
    }

    { // Try to find shader
        std::lock_guard<std::mutex> _(mtx_);
        if (Ren::ShaderHandle *p_ret = shaders_.Find(name)) {
            return *p_ret;
        }
    }

    Ren::ShaderHandle ret;
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

            const Ren::String name_str{name};

            std::lock_guard<std::mutex> _(mtx_);
            ret = ctx_.CreateShader(name_str, spv_data, type);
            if (ret) {
                shaders_.Insert(name_str, ret);
            }
        } else {
            ctx_.log()->Error("Error loading shader data %s", name.data());
            return {};
        }
    }
#endif
#if defined(REN_GL_BACKEND)
    const std::string shader_src = ReadGLSLContent(name, ctx_.log());
    if (!shader_src.empty()) {
        const Ren::String name_str{name};

        std::lock_guard<std::mutex> _(mtx_);
        ret = ctx_.CreateShader(name_str, shader_src, type);
        if (ret) {
            shaders_.Insert(name_str, ret);
        }
    } else {
        ctx_.log()->Error("Error loading shader %s", name.data());
        return {};
    }
#endif
    return ret;
}

Ren::ProgramHandle Eng::ShaderLoader::FindOrCreateProgram(std::string_view vs_name, std::string_view fs_name,
                                                          std::string_view tcs_name, std::string_view tes_name,
                                                          std::string_view gs_name) {
    const Ren::ShaderHandle vs_handle = FindOrCreateShader(vs_name);
    const Ren::ShaderHandle fs_handle = FindOrCreateShader(fs_name);
    if (!vs_handle || !fs_handle) {
        ctx_.log()->Error("Error loading shaders %s/%s", vs_name.data(), fs_name.data());
        return {};
    }

    Ren::ShaderHandle tcs_handle, tes_handle;
    if (!tcs_name.empty() && !tes_name.empty()) {
        tcs_handle = FindOrCreateShader(tcs_name);
        tes_handle = FindOrCreateShader(tes_name);
        if (!tcs_handle || !tes_handle) {
            ctx_.log()->Error("Error loading shaders %s/%s", tcs_name.data(), tes_name.data());
            return {};
        }
    }
    Ren::ShaderHandle gs_handle;
    if (!gs_name.empty()) {
        gs_handle = FindOrCreateShader(gs_name);
        if (!gs_handle) {
            ctx_.log()->Error("Error loading shader %s", gs_name.data());
            return {};
        }
    }

    std::array<Ren::ShaderROHandle, int(Ren::eShaderType::_Count)> temp_shaders;
    temp_shaders[int(Ren::eShaderType::Vertex)] = vs_handle;
    temp_shaders[int(Ren::eShaderType::Fragment)] = fs_handle;
    temp_shaders[int(Ren::eShaderType::TesselationControl)] = tcs_handle;
    temp_shaders[int(Ren::eShaderType::TesselationEvaluation)] = tes_handle;
    temp_shaders[int(Ren::eShaderType::Geometry)] = gs_handle;

    std::lock_guard<std::mutex> _(mtx_);

    const auto it = lower_bound(std::begin(programs_), std::end(programs_), temp_shaders,
                                [this](const Ren::ProgramHandle lhs_handle,
                                       const std::array<Ren::ShaderROHandle, int(Ren::eShaderType::_Count)> &shaders) {
                                    return ctx_.storages().programs[lhs_handle].first.shaders < shaders;
                                });
    if (it != std::end(programs_) && ctx_.storages().programs[*it].first.shaders == temp_shaders) {
        return *it;
    }

    const Ren::ProgramHandle ret = ctx_.CreateProgram(vs_handle, fs_handle, tcs_handle, tes_handle, gs_handle);
    if (ret) {
        programs_.insert(it, ret);
    }
    return ret;
}

Ren::ProgramHandle Eng::ShaderLoader::FindOrCreateProgram(std::string_view cs_name) {
    const Ren::ShaderHandle cs_handle = FindOrCreateShader(cs_name);

    std::array<Ren::ShaderROHandle, int(Ren::eShaderType::_Count)> temp_shaders;
    temp_shaders[int(Ren::eShaderType::Compute)] = cs_handle;

    std::lock_guard<std::mutex> _(mtx_);

    const auto it = lower_bound(std::begin(programs_), std::end(programs_), temp_shaders,
                                [this](const Ren::ProgramHandle lhs_handle,
                                       const std::array<Ren::ShaderROHandle, int(Ren::eShaderType::_Count)> &shaders) {
                                    return ctx_.storages().programs[lhs_handle].first.shaders < shaders;
                                });
    if (it != std::end(programs_) && ctx_.storages().programs[*it].first.shaders == temp_shaders) {
        return *it;
    }

    assert(cs_handle);

    const Ren::ProgramHandle ret = ctx_.CreateProgram(cs_handle);
    if (ret) {
        programs_.insert(it, ret);
    }
    return ret;
}

#if defined(REN_VK_BACKEND)
Ren::ProgramHandle Eng::ShaderLoader::FindOrCreateProgram2(std::string_view rgs_name, std::string_view chs_name,
                                                           std::string_view ahs_name, std::string_view ms_name,
                                                           std::string_view is_name) {
    const Ren::ShaderROHandle rgs_handle = FindOrCreateShader(rgs_name);

    Ren::ShaderROHandle chs_handle, ahs_handle, ms_handle;
    if (!chs_name.empty()) {
        chs_handle = FindOrCreateShader(chs_name);
    }
    if (!ahs_name.empty()) {
        ahs_handle = FindOrCreateShader(ahs_name);
    }
    if (!ms_name.empty()) {
        ms_handle = FindOrCreateShader(ms_name);
    }

    Ren::ShaderROHandle is_handle;
    if (!is_name.empty()) {
        is_handle = FindOrCreateShader(is_name);
    }

    std::array<Ren::ShaderROHandle, int(Ren::eShaderType::_Count)> temp_shaders;
    temp_shaders[int(Ren::eShaderType::RayGen)] = rgs_handle;
    temp_shaders[int(Ren::eShaderType::ClosestHit)] = chs_handle;
    temp_shaders[int(Ren::eShaderType::AnyHit)] = ahs_handle;
    temp_shaders[int(Ren::eShaderType::Miss)] = ms_handle;
    temp_shaders[int(Ren::eShaderType::Intersection)] = is_handle;

    std::lock_guard<std::mutex> _(mtx_);

    const auto it = lower_bound(std::begin(programs_), std::end(programs_), temp_shaders,
                                [this](const Ren::ProgramHandle lhs,
                                       const std::array<Ren::ShaderROHandle, int(Ren::eShaderType::_Count)> &shaders) {
                                    return ctx_.storages().programs[lhs].first.shaders < shaders;
                                });
    if (it != std::end(programs_) && ctx_.storages().programs[*it].first.shaders == temp_shaders) {
        return *it;
    }

    const Ren::ProgramHandle ret = ctx_.CreateProgram2(rgs_handle, chs_handle, ahs_handle, ms_handle, is_handle);
    if (ret) {
        programs_.insert(it, ret);
    }
    return ret;
}
#endif

Ren::PipelineHandle Eng::ShaderLoader::FindOrCreatePipeline(const Ren::RastState &rast_state,
                                                            const Ren::ProgramROHandle prog,
                                                            const Ren::VertexInputROHandle vtx_input,
                                                            const Ren::RenderPassROHandle render_pass,
                                                            const uint32_t subpass_index) {
    const auto less_than = [&](const Ren::PipelineHandle lhs) {
        return ctx_.storages().pipelines[lhs].first.LessThan(rast_state, prog, vtx_input, render_pass);
    };

    { // Try to find pipeline
        std::lock_guard<std::mutex> _(mtx_);
        const auto it = partition_point(std::begin(pipelines_), std::end(pipelines_), less_than);
        if (it != std::end(pipelines_) &&
            ctx_.storages().pipelines[*it].first.Equals(rast_state, prog, vtx_input, render_pass)) {
            return *it;
        }
    }

    // Pipeline initialization is done with no lock as it is the heaviest part
    Ren::PipelineMain pi_main = {};
    Ren::PipelineCold pi_cold = {};
    if (!Pipeline_Init(ctx_.api(), ctx_.storages(), pi_main, pi_cold, rast_state, prog, vtx_input, render_pass,
                       subpass_index, ctx_.log())) {
        return {};
    }

    std::lock_guard<std::mutex> _(mtx_);
    const Ren::PipelineHandle ret = ctx_.CreatePipeline(std::move(pi_main), std::move(pi_cold));
    if (ret) {
        const auto it = partition_point(std::begin(pipelines_), std::end(pipelines_), less_than);
        pipelines_.insert(it, ret);
    }
    return ret;
}

Ren::PipelineHandle Eng::ShaderLoader::FindOrCreatePipeline(std::string_view cs_name, const int subgroup_size) {
    const Ren::ProgramHandle prog_handle = FindOrCreateProgram(cs_name);
    return FindOrCreatePipeline(prog_handle, subgroup_size);
}

Ren::PipelineHandle Eng::ShaderLoader::FindOrCreatePipeline(const Ren::ProgramROHandle prog, const int subgroup_size) {
    const auto less_than = [&](const Ren::PipelineHandle lhs) {
        return ctx_.storages().pipelines[lhs].first.LessThan({}, prog, {}, {});
    };

    { // Try to find pipeline
        std::lock_guard<std::mutex> _(mtx_);
        const auto it = partition_point(std::begin(pipelines_), std::end(pipelines_), less_than);
        if (it != std::end(pipelines_) && ctx_.storages().pipelines[*it].first.Equals({}, prog, {}, {})) {
            return *it;
        }
        const bool is_rt_pipeline = bool(ctx_.storages().programs[prog].first.shaders[int(Ren::eShaderType::RayGen)]);
        if (is_rt_pipeline) {
            // Has to be initialized under lock due to buffer allocation
            const Ren::PipelineHandle ret = ctx_.CreatePipeline(prog, subgroup_size);
            if (ret) {
                pipelines_.insert(it, ret);
            }
            return ret;
        }
    }

    // Pipeline initialization is done with no lock as it is the heaviest part
    Ren::PipelineMain pi_main = {};
    Ren::PipelineCold pi_cold = {};
    if (!Pipeline_Init(ctx_.api(), ctx_.storages().shaders, ctx_.storages().programs, ctx_.storages().buffers, pi_main,
                       pi_cold, prog, ctx_.log(), subgroup_size)) {
        return {};
    }

    std::lock_guard<std::mutex> _(mtx_);
    const Ren::PipelineHandle ret = ctx_.CreatePipeline(std::move(pi_main), std::move(pi_cold));
    if (ret) {
        const auto it = partition_point(std::begin(pipelines_), std::end(pipelines_), less_than);
        pipelines_.insert(it, ret);
    }
    return ret;
}
