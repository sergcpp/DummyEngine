#include "RpRTShadows.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../Utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../assets/shaders/internal/rt_shadows_interface.h"

void RpRTShadows::Execute_HWRT_Pipeline(RpBuilder &builder) { assert(false && "Not implemented!"); }

void RpRTShadows::Execute_HWRT_Inline(RpBuilder &builder) { assert(false && "Not implemented!"); }

void RpRTShadows::Execute_SWRT(RpBuilder &builder) {
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &noise_tex = builder.GetReadTexture(pass_data_->noise_tex);
    RpAllocTex &depth_tex = builder.GetReadTexture(pass_data_->depth_tex);
    RpAllocTex &normal_tex = builder.GetReadTexture(pass_data_->normal_tex);
    RpAllocBuf &rt_blas_buf = builder.GetReadBuffer(pass_data_->swrt.blas_buf);
    RpAllocBuf &rt_tlas_buf = builder.GetReadBuffer(pass_data_->tlas_buf);
    RpAllocBuf &prim_ndx_buf = builder.GetReadBuffer(pass_data_->swrt.prim_ndx_buf);
    RpAllocBuf &meshes_buf = builder.GetReadBuffer(pass_data_->swrt.meshes_buf);
    RpAllocBuf &mesh_instances_buf = builder.GetReadBuffer(pass_data_->swrt.mesh_instances_buf);
    RpAllocBuf &textures_buf = builder.GetReadBuffer(pass_data_->swrt.textures_buf);
    RpAllocBuf &tile_list_buf = builder.GetReadBuffer(pass_data_->tile_list_buf);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(pass_data_->indir_args);

    RpAllocTex &out_shadow_tex = builder.GetWriteTexture(pass_data_->out_shadow_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    if (!vtx_buf1.tbos[0] || vtx_buf1.tbos[0]->params().size != vtx_buf1.ref->size()) {
        vtx_buf1.tbos[0] =
            ctx.CreateTexture1D("Vertex Buf 1 TBO", vtx_buf1.ref, Ren::eTexFormat::RawRGBA32F, 0, vtx_buf1.ref->size());
    }

    if (!ndx_buf.tbos[0] || ndx_buf.tbos[0]->params().size != ndx_buf.ref->size()) {
        ndx_buf.tbos[0] =
            ctx.CreateTexture1D("Index Buf TBO", ndx_buf.ref, Ren::eTexFormat::RawR32UI, 0, ndx_buf.ref->size());
    }

    if (!prim_ndx_buf.tbos[0] || prim_ndx_buf.tbos[0]->params().size != prim_ndx_buf.ref->size()) {
        prim_ndx_buf.tbos[0] = ctx.CreateTexture1D("Prim Ndx TBO", prim_ndx_buf.ref, Ren::eTexFormat::RawR32UI, 0,
                                                   prim_ndx_buf.ref->size());
    }

    if (!rt_blas_buf.tbos[0] || rt_blas_buf.tbos[0]->params().size != rt_blas_buf.ref->size()) {
        rt_blas_buf.tbos[0] = ctx.CreateTexture1D("RT BLAS TBO", rt_blas_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                                  rt_blas_buf.ref->size());
    }

    if (!rt_tlas_buf.tbos[0] || rt_tlas_buf.tbos[0]->params().size != rt_tlas_buf.ref->size()) {
        rt_tlas_buf.tbos[0] = ctx.CreateTexture1D("RT TLAS TBO", rt_tlas_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                                  rt_tlas_buf.ref->size());
    }

    if (!mesh_instances_buf.tbos[0] || mesh_instances_buf.tbos[0]->params().size != mesh_instances_buf.ref->size()) {
        mesh_instances_buf.tbos[0] =
            ctx.CreateTexture1D("Mesh Instances TBO", mesh_instances_buf.ref, Ren::eTexFormat::RawRGBA32F, 0,
                                mesh_instances_buf.ref->size());
    }

    if (!meshes_buf.tbos[0] || meshes_buf.tbos[0]->params().size != meshes_buf.ref->size()) {
        meshes_buf.tbos[0] =
            ctx.CreateTexture1D("Meshes TBO", meshes_buf.ref, Ren::eTexFormat::RawRG32UI, 0, meshes_buf.ref->size());
    }

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::SBuf, REN_BINDLESS_TEX_SLOT, *textures_buf.ref},
        {Ren::eBindTarget::UBuf, REN_UB_SHARED_DATA_LOC, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2D, RTShadows::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::Tex2D, RTShadows::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, RTShadows::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::SBuf, RTShadows::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTShadows::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::TBuf, RTShadows::BLAS_BUF_SLOT, *rt_blas_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTShadows::TLAS_BUF_SLOT, *rt_tlas_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTShadows::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTShadows::MESHES_BUF_SLOT, *meshes_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTShadows::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTShadows::VTX_BUF1_SLOT, *vtx_buf1.tbos[0]},
        {Ren::eBindTarget::TBuf, RTShadows::NDX_BUF_SLOT, *ndx_buf.tbos[0]},
        {Ren::eBindTarget::SBuf, RTShadows::TILE_LIST_SLOT, *tile_list_buf.ref},
        {Ren::eBindTarget::Image, RTShadows::OUT_SHADOW_IMG_SLOT, *out_shadow_tex.ref}};

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = std::atan(
        2.0f * std::tan(0.5f * view_state_->vertical_fov * Ren::Pi<float>() / 180.0f) / float(view_state_->scr_res[1]));

    Ren::DispatchComputeIndirect(pi_rt_shadows_swrt_, *indir_args_buf.ref, 0, bindings, &uniform_params,
                                 sizeof(uniform_params), nullptr, ctx.log());
}

void RpRTShadows::LazyInit(Ren::Context &ctx, ShaderLoader &sh) {
    if (!initialized) {
        Ren::ProgramRef rt_shadows_swrt_prog =
            sh.LoadProgram(ctx, "rt_shadows_swrt", "internal/rt_shadows_swrt.comp.glsl");
        assert(rt_shadows_swrt_prog->ready());

        if (!pi_rt_shadows_swrt_.Init(ctx.api_ctx(), rt_shadows_swrt_prog, ctx.log())) {
            ctx.log()->Error("RpRTShadows: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}