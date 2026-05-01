#include "ExRTDiffuse.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/rt_diffuse_interface.h"

void Eng::ExRTDiffuse::Execute(const FgContext &fg) {
    LazyInit(fg);
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExRTDiffuse::LazyInit(const FgContext &fg) {
    auto &ctx = fg.ren_ctx();
    auto &sh = fg.sh();
    if (!initialized_) {
        auto hwrt_select = [&ctx](std::string_view hwrt_shader, std::string_view swrt_shader) {
            return ctx.capabilities.hwrt ? hwrt_shader : swrt_shader;
        };
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        if (args_->second_bounce) {
            pi_rt_diffuse_ = sh.FindOrCreatePipeline(
                hwrt_select(subgroup_select("internal/rt_diffuse_hwrt@SECOND.comp.glsl",
                                            "internal/rt_diffuse_hwrt@SECOND;NO_SUBGROUP.comp.glsl"),
                            subgroup_select("internal/rt_diffuse_swrt@SECOND.comp.glsl",
                                            "internal/rt_diffuse_swrt@SECOND;NO_SUBGROUP.comp.glsl")),
                32);
        } else {
            pi_rt_diffuse_ = sh.FindOrCreatePipeline(
                hwrt_select(subgroup_select("internal/rt_diffuse_hwrt@FIRST.comp.glsl",
                                            "internal/rt_diffuse_hwrt@FIRST;NO_SUBGROUP.comp.glsl"),
                            subgroup_select("internal/rt_diffuse_swrt@FIRST.comp.glsl",
                                            "internal/rt_diffuse_swrt@FIRST;NO_SUBGROUP.comp.glsl")),
                32);
        }

        initialized_ = true;
    }
}

void Eng::ExRTDiffuse::Execute_SWRT(const FgContext &fg) {
    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle rt_blas = fg.AccessROBuffer(args_->swrt.rt_blas);
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle noise = fg.AccessROImage(args_->noise);
    const Ren::ImageROHandle depth = fg.AccessROImage(args_->depth);
    const Ren::ImageROHandle normal = fg.AccessROImage(args_->normal);
    const Ren::BufferROHandle ray_list = fg.AccessROBuffer(args_->ray_list);
    const Ren::BufferROHandle indir_args = fg.AccessROBuffer(args_->indir_args);
    const Ren::BufferROHandle rt_tlas = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::BufferROHandle prim_ndx = fg.AccessROBuffer(args_->swrt.prim_ndx);
    const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(args_->swrt.mesh_instances);

    const Ren::BufferHandle inout_ray_counter = fg.AccessRWBuffer(args_->inout_ray_counter);
    const Ren::BufferHandle out_ray_hits = fg.AccessRWBuffer(args_->out_ray_hits);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::TexSampled, RTDiffuse::DEPTH_TEX_SLOT, {depth, 1}},
        {Ren::eBindTarget::TexSampled, RTDiffuse::NORM_TEX_SLOT, normal},
        {Ren::eBindTarget::TexSampled, RTDiffuse::NOISE_TEX_SLOT, noise},
        {Ren::eBindTarget::SBufRO, RTDiffuse::RAY_LIST_SLOT, ray_list},
        {Ren::eBindTarget::UTBuf, RTDiffuse::BLAS_BUF_SLOT, rt_blas},
        {Ren::eBindTarget::UTBuf, RTDiffuse::TLAS_BUF_SLOT, rt_tlas},
        {Ren::eBindTarget::UTBuf, RTDiffuse::PRIM_NDX_BUF_SLOT, prim_ndx},
        {Ren::eBindTarget::UTBuf, RTDiffuse::MESH_INSTANCES_BUF_SLOT, mesh_instances},
        {Ren::eBindTarget::SBufRO, RTDiffuse::GEO_DATA_BUF_SLOT, geo_data},
        {Ren::eBindTarget::SBufRO, RTDiffuse::MATERIAL_BUF_SLOT, materials},
        {Ren::eBindTarget::UTBuf, RTDiffuse::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, RTDiffuse::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRW, RTDiffuse::RAY_COUNTER_SLOT, inout_ray_counter},
        {Ren::eBindTarget::SBufRW, RTDiffuse::OUT_RAY_HITS_BUF_SLOT, out_ray_hits}};

    RTDiffuse::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = view_state_->frame_index;
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    DispatchComputeIndirect(fg.cmd_buf(), pi_rt_diffuse_, fg.storages(), indir_args,
                            sizeof(VkTraceRaysIndirectCommandKHR), bindings, &uniform_params, sizeof(uniform_params),
                            fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExRTDiffuse::Execute_HWRT(const FgContext &fg) { assert(false && "Not implemented!"); }
#endif
