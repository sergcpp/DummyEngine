#include "ExRTShadows.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/rt_shadows_interface.h"

void Eng::ExRTShadows::Execute(FgContext &fg) {
    LazyInit(fg.ren_ctx(), fg.sh());
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExRTShadows::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        pi_rt_shadows_ = sh.LoadPipeline(
            ctx.capabilities.hwrt ? "internal/rt_shadows_hwrt.comp.glsl" : "internal/rt_shadows_swrt.comp.glsl", 32);
        if (!pi_rt_shadows_) {
            ctx.log()->Error("ExRTShadows: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}

void Eng::ExRTShadows::Execute_SWRT(FgContext &fg) {
    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Texture &noise_tex = fg.AccessROTexture(args_->noise_tex);
    const Ren::Texture &depth_tex = fg.AccessROTexture(args_->depth_tex);
    const Ren::Texture &normal_tex = fg.AccessROTexture(args_->normal_tex);
    const Ren::Buffer &rt_blas_buf = fg.AccessROBuffer(args_->swrt.blas_buf);
    const Ren::Buffer &rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::Buffer &prim_ndx_buf = fg.AccessROBuffer(args_->swrt.prim_ndx_buf);
    const Ren::Buffer &mesh_instances_buf = fg.AccessROBuffer(args_->swrt.mesh_instances_buf);
    const Ren::Buffer &tile_list_buf = fg.AccessROBuffer(args_->tile_list_buf);
    const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(args_->indir_args);

    Ren::Texture &out_shadow_tex = fg.AccessRWTexture(args_->out_shadow_tex);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::TexSampled, RTShadows::NOISE_TEX_SLOT, noise_tex},
        {Ren::eBindTarget::TexSampled, RTShadows::DEPTH_TEX_SLOT, {depth_tex, 1}},
        {Ren::eBindTarget::TexSampled, RTShadows::NORM_TEX_SLOT, normal_tex},
        {Ren::eBindTarget::SBufRO, RTShadows::GEO_DATA_BUF_SLOT, geo_data_buf},
        {Ren::eBindTarget::SBufRO, RTShadows::MATERIAL_BUF_SLOT, materials_buf},
        {Ren::eBindTarget::UTBuf, RTShadows::BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, RTShadows::TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, RTShadows::PRIM_NDX_BUF_SLOT, prim_ndx_buf},
        {Ren::eBindTarget::UTBuf, RTShadows::MESH_INSTANCES_BUF_SLOT, mesh_instances_buf},
        {Ren::eBindTarget::UTBuf, RTShadows::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, RTShadows::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRO, RTShadows::TILE_LIST_SLOT, tile_list_buf},
        {Ren::eBindTarget::ImageRW, RTShadows::OUT_SHADOW_IMG_SLOT, out_shadow_tex}};

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    DispatchComputeIndirect(fg.cmd_buf(), *pi_rt_shadows_, indir_args_buf, 0, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExRTShadows::Execute_HWRT(FgContext &fg) { assert(false && "Not implemented!"); }
#endif