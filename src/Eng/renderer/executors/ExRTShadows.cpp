#include "ExRTShadows.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/rt_shadows_interface.h"

void Eng::ExRTShadows::Execute(const FgContext &fg) {
    LazyInit(fg);
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExRTShadows::LazyInit(const FgContext &fg) {
    auto &ctx = fg.ren_ctx();
    auto &sh = fg.sh();
    if (!initialized_) {
        pi_rt_shadows_ = sh.FindOrCreatePipeline(
            ctx.capabilities.hwrt ? "internal/rt_shadows_hwrt.comp.glsl" : "internal/rt_shadows_swrt.comp.glsl", 32);
        if (!pi_rt_shadows_) {
            ctx.log()->Error("ExRTShadows: Failed to initialize pipeline!");
        }
        initialized_ = true;
    }
}

void Eng::ExRTShadows::Execute_SWRT(const FgContext &fg) {
    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle noise = fg.AccessROImage(args_->noise);
    const Ren::ImageROHandle depth = fg.AccessROImage(args_->depth);
    const Ren::ImageROHandle normal = fg.AccessROImage(args_->normal);
    const Ren::BufferROHandle rt_blas_buf = fg.AccessROBuffer(args_->swrt.blas_buf);
    const Ren::BufferROHandle rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::BufferROHandle prim_ndx = fg.AccessROBuffer(args_->swrt.prim_ndx);
    const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(args_->swrt.mesh_instances);
    const Ren::BufferROHandle tile_list = fg.AccessROBuffer(args_->tile_list);
    const Ren::BufferROHandle indir_args = fg.AccessROBuffer(args_->indir_args);

    const Ren::ImageRWHandle out_shadow = fg.AccessRWImage(args_->out_shadow);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::TexSampled, RTShadows::NOISE_TEX_SLOT, noise},
        {Ren::eBindTarget::TexSampled, RTShadows::DEPTH_TEX_SLOT, {depth, 1}},
        {Ren::eBindTarget::TexSampled, RTShadows::NORM_TEX_SLOT, normal},
        {Ren::eBindTarget::SBufRO, RTShadows::GEO_DATA_BUF_SLOT, geo_data},
        {Ren::eBindTarget::SBufRO, RTShadows::MATERIAL_BUF_SLOT, materials},
        {Ren::eBindTarget::UTBuf, RTShadows::BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, RTShadows::TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, RTShadows::PRIM_NDX_BUF_SLOT, prim_ndx},
        {Ren::eBindTarget::UTBuf, RTShadows::MESH_INSTANCES_BUF_SLOT, mesh_instances},
        {Ren::eBindTarget::UTBuf, RTShadows::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, RTShadows::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRO, RTShadows::TILE_LIST_SLOT, tile_list},
        {Ren::eBindTarget::ImageRW, RTShadows::OUT_SHADOW_IMG_SLOT, out_shadow}};

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    DispatchComputeIndirect(fg.cmd_buf(), pi_rt_shadows_, fg.storages(), indir_args, 0, bindings, &uniform_params,
                            sizeof(uniform_params), fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExRTShadows::Execute_HWRT(const FgContext &fg) { assert(false && "Not implemented!"); }
#endif