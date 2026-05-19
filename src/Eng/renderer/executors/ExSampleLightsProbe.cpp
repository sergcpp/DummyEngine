#include "ExSampleLightsProbe.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/sample_lights_interface.h"

void Eng::ExSampleLightsProbe::Execute(const FgContext &fg) {
    LazyInit(fg);
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExSampleLightsProbe::LazyInit(const FgContext &fg) {
    auto &ctx = fg.ren_ctx();
    auto &sh = fg.sh();
    if (!initialized_) {
        auto hwrt_select = [&ctx](std::string_view hwrt_shader, std::string_view swrt_shader) {
            return ctx.capabilities.hwrt ? hwrt_shader : swrt_shader;
        };
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        if (ctx.capabilities.hwrt) {
            pi_sample_lights_[0] =
                sh.FindOrCreatePipeline(subgroup_select("internal/sample_lights_probe@MIS;HWRT.comp.glsl",
                                                        "internal/sample_lights_probe@MIS;HWRT;NO_SUBGROUP.comp.glsl"),
                                        32);
            pi_sample_lights_[1] = sh.FindOrCreatePipeline(
                subgroup_select("internal/sample_lights_probe@MIS;PARTIAL;HWRT.comp.glsl",
                                "internal/sample_lights_probe@MIS;PARTIAL;HWRT;NO_SUBGROUP.comp.glsl"),
                32);
        } else {
            pi_sample_lights_[0] =
                sh.FindOrCreatePipeline(subgroup_select("internal/sample_lights_probe@MIS.comp.glsl",
                                                        "internal/sample_lights_probe@MIS;NO_SUBGROUP.comp.glsl"),
                                        32);
            pi_sample_lights_[1] = sh.FindOrCreatePipeline(
                subgroup_select("internal/sample_lights_probe@MIS;PARTIAL.comp.glsl",
                                "internal/sample_lights_probe@MIS;PARTIAL;NO_SUBGROUP.comp.glsl"),
                32);
        }
        initialized_ = true;
    }
}

void Eng::ExSampleLightsProbe::Execute_SWRT(const FgContext &fg) {
    using namespace SampleLights;

    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle random_seq = fg.AccessROBuffer(args_->random_seq);

    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle rt_blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas_buf);

    const Ren::BufferROHandle rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::BufferROHandle prim_ndx = fg.AccessROBuffer(args_->swrt.prim_ndx);
    const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(args_->swrt.mesh_instances);

    const Ren::ImageROHandle offset = fg.AccessROImage(args_->offset);

    const Ren::BufferRWHandle out_sh1_data = fg.AccessRWBuffer(args_->out_sh1_data);

    if (!args_->lights) {
        return;
    }

    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::BufferROHandle nodes = fg.AccessROBuffer(args_->nodes);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::UTBuf, RANDOM_SEQ_BUF_SLOT, random_seq},
        {Ren::eBindTarget::UTBuf, LIGHTS_BUF_SLOT, lights},
        {Ren::eBindTarget::UTBuf, LIGHT_NODES_BUF_SLOT, nodes},
        {Ren::eBindTarget::UTBuf, BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, PRIM_NDX_BUF_SLOT, prim_ndx},
        {Ren::eBindTarget::UTBuf, MESH_INSTANCES_BUF_SLOT, mesh_instances},
        {Ren::eBindTarget::SBufRO, GEO_DATA_BUF_SLOT, geo_data},
        {Ren::eBindTarget::SBufRO, MATERIAL_BUF_SLOT, materials},
        {Ren::eBindTarget::UTBuf, VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::TexSampled, OFFSET_TEX_SLOT, offset},
        {Ren::eBindTarget::SBufRW, OUT_SH1_DATA_BUF_SLOT, out_sh1_data}};

    const auto grp_count = Ren::Vec3u{1u, PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y};

    // TODO: Avoid accessing cold data
    const Ren::BufferCold &lights_cold = fg.storages().buffers[lights].second;

    Params2 uniform_params;
    uniform_params.volume_index = view_state_->volume_to_update;
    uniform_params.stoch_lights_count = view_state_->stochastic_lights_count_cache;
    uniform_params.pass_hash = view_state_->probe_ray_hash;
    uniform_params.oct_index = (args_->probe_volumes[view_state_->volume_to_update].updates_count - 1) % 8;
    uniform_params.grid_origin = Ren::Vec4f{args_->probe_volumes[view_state_->volume_to_update].origin, 0.0f};
    uniform_params.grid_scroll = Ren::Vec4i{args_->probe_volumes[view_state_->volume_to_update].scroll, 0};
    uniform_params.grid_scroll_diff = Ren::Vec4i{args_->probe_volumes[view_state_->volume_to_update].scroll_diff, 0};
    uniform_params.grid_spacing = Ren::Vec4f{args_->probe_volumes[view_state_->volume_to_update].spacing, 0.0f};

    const Ren::PipelineHandle pi = pi_sample_lights_[args_->partial_update];
    DispatchCompute(fg.cmd_buf(), pi, fg.storages(), grp_count, bindings, &uniform_params, sizeof(uniform_params),
                    fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExSampleLightsProbe::Execute_HWRT(const FgContext &fg) { assert(false && "Not implemented!"); }
#endif