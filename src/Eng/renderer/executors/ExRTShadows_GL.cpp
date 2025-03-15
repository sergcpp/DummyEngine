#include "ExRTShadows.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../shaders/rt_shadows_interface.h"

void Eng::ExRTShadows::Execute_HWRT_Pipeline(FgBuilder &builder) { assert(false && "Not implemented!"); }

void Eng::ExRTShadows::Execute_HWRT_Inline(FgBuilder &builder) { assert(false && "Not implemented!"); }

void Eng::ExRTShadows::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocTex &noise_tex = builder.GetReadTexture(args_->noise_tex);
    FgAllocTex &depth_tex = builder.GetReadTexture(args_->depth_tex);
    FgAllocTex &normal_tex = builder.GetReadTexture(args_->normal_tex);
    FgAllocBuf &rt_blas_buf = builder.GetReadBuffer(args_->swrt.blas_buf);
    FgAllocBuf &rt_tlas_buf = builder.GetReadBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = builder.GetReadBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = builder.GetReadBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &textures_buf = builder.GetReadBuffer(args_->swrt.textures_buf);
    FgAllocBuf &tile_list_buf = builder.GetReadBuffer(args_->tile_list_buf);
    FgAllocBuf &indir_args_buf = builder.GetReadBuffer(args_->indir_args);

    FgAllocTex &out_shadow_tex = builder.GetWriteTexture(args_->out_shadow_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::SBufRO, BIND_BINDLESS_TEX, *textures_buf.ref},
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTShadows::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTShadows::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::Tex2DSampled, RTShadows::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::SBufRO, RTShadows::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTShadows::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, RTShadows::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTShadows::TILE_LIST_SLOT, *tile_list_buf.ref},
        {Ren::eBindTarget::Image2D, RTShadows::OUT_SHADOW_IMG_SLOT, *out_shadow_tex.ref}};

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    DispatchComputeIndirect(*pi_rt_shadows_, *indir_args_buf.ref, 0, bindings, &uniform_params, sizeof(uniform_params),
                            nullptr, ctx.log());
}
