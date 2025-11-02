#include "ExRTShadows.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/rt_shadows_interface.h"

void Eng::ExRTShadows::Execute(FgContext &ctx) {
    LazyInit(ctx.ren_ctx(), ctx.sh());
    if (ctx.ren_ctx().capabilities.hwrt) {
        Execute_HWRT_Inline(ctx);
    } else {
        Execute_SWRT(ctx);
    }
}

void Eng::ExRTShadows::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        pi_rt_shadows_ = sh.LoadPipeline(ctx.capabilities.hwrt ? "internal/rt_shadows_hwrt.comp.glsl"
                                                               : "internal/rt_shadows_swrt.comp.glsl");
        if (!pi_rt_shadows_) {
            ctx.log()->Error("ExRTShadows: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}

void Eng::ExRTShadows::Execute_SWRT(FgContext &ctx) {
    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocTex &noise_tex = ctx.AccessROTexture(args_->noise_tex);
    FgAllocTex &depth_tex = ctx.AccessROTexture(args_->depth_tex);
    FgAllocTex &normal_tex = ctx.AccessROTexture(args_->normal_tex);
    FgAllocBuf &rt_blas_buf = ctx.AccessROBuffer(args_->swrt.blas_buf);
    FgAllocBuf &rt_tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = ctx.AccessROBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = ctx.AccessROBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &tile_list_buf = ctx.AccessROBuffer(args_->tile_list_buf);
    FgAllocBuf &indir_args_buf = ctx.AccessROBuffer(args_->indir_args);

    FgAllocTex &out_shadow_tex = ctx.AccessRWTexture(args_->out_shadow_tex);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::TexSampled, RTShadows::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::TexSampled, RTShadows::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::TexSampled, RTShadows::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::SBufRO, RTShadows::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTShadows::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTShadows::TILE_LIST_SLOT, *tile_list_buf.ref},
        {Ren::eBindTarget::ImageRW, RTShadows::OUT_SHADOW_IMG_SLOT, *out_shadow_tex.ref}};

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    DispatchComputeIndirect(ctx.cmd_buf(), *pi_rt_shadows_, *indir_args_buf.ref, 0, bindings, &uniform_params,
                            sizeof(uniform_params), ctx.descr_alloc(), ctx.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExRTShadows::Execute_HWRT_Pipeline(FgContext &ctx) { assert(false && "Not implemented!"); }

void Eng::ExRTShadows::Execute_HWRT_Inline(FgContext &ctx) { assert(false && "Not implemented!"); }
#endif