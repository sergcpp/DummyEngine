#include "ExRTGI.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"

#include "../shaders/rt_gi_interface.h"

void Eng::ExRTGI::Execute_HWRT(FgBuilder &builder) { assert(false && "Not implemented!"); }

void Eng::ExRTGI::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    FgAllocBuf &rt_blas_buf = builder.GetReadBuffer(args_->swrt.rt_blas_buf);
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocTex &noise_tex = builder.GetReadTexture(args_->noise_tex);
    FgAllocTex &depth_tex = builder.GetReadTexture(args_->depth_tex);
    FgAllocTex &normal_tex = builder.GetReadTexture(args_->normal_tex);
    FgAllocTex &env_tex = builder.GetReadTexture(args_->env_tex);
    FgAllocBuf &ray_counter_buf = builder.GetReadBuffer(args_->ray_counter);
    FgAllocBuf &ray_list_buf = builder.GetReadBuffer(args_->ray_list);
    FgAllocBuf &indir_args_buf = builder.GetReadBuffer(args_->indir_args);
    FgAllocBuf &rt_tlas_buf = builder.GetReadBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = builder.GetReadBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = builder.GetReadBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &textures_buf = builder.GetReadBuffer(args_->swrt.textures_buf);
    FgAllocBuf &lights_buf = builder.GetReadBuffer(args_->lights_buf);
    FgAllocTex &shadow_depth_tex = builder.GetReadTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = builder.GetReadTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = builder.GetReadTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = builder.GetReadBuffer(args_->items_buf);

    FgAllocTex &irr_tex = builder.GetReadTexture(args_->irradiance_tex);
    FgAllocTex &dist_tex = builder.GetReadTexture(args_->distance_tex);
    FgAllocTex &off_tex = builder.GetReadTexture(args_->offset_tex);

    FgAllocBuf *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        stoch_lights_buf = &builder.GetReadBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &builder.GetReadBuffer(args_->light_nodes_buf);

        if (!stoch_lights_buf->tbos[0] || stoch_lights_buf->tbos[0]->params().size != stoch_lights_buf->ref->size()) {
            stoch_lights_buf->tbos[0] =
                builder.ctx().CreateTextureBuffer("Stoch Lights Buf TBO", stoch_lights_buf->ref,
                                                  Ren::eTexFormat::RGBA32F, 0, stoch_lights_buf->ref->size());
        }
        if (!light_nodes_buf->tbos[0] || light_nodes_buf->tbos[0]->params().size != light_nodes_buf->ref->size()) {
            light_nodes_buf->tbos[0] =
                builder.ctx().CreateTextureBuffer("Stoch Lights Nodes Buf TBO", light_nodes_buf->ref,
                                                  Ren::eTexFormat::RGBA32F, 0, light_nodes_buf->ref->size());
        }
    }

    FgAllocTex &out_gi_tex = builder.GetWriteTexture(args_->out_gi_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    if (!vtx_buf1.tbos[0] || vtx_buf1.tbos[0]->params().size != vtx_buf1.ref->size()) {
        vtx_buf1.tbos[0] = ctx.CreateTextureBuffer("Vertex Buf 1 TBO", vtx_buf1.ref, Ren::eTexFormat::RGBA32F, 0,
                                                   vtx_buf1.ref->size());
    }

    if (!ndx_buf.tbos[0] || ndx_buf.tbos[0]->params().size != ndx_buf.ref->size()) {
        ndx_buf.tbos[0] =
            ctx.CreateTextureBuffer("Index Buf TBO", ndx_buf.ref, Ren::eTexFormat::R32UI, 0, ndx_buf.ref->size());
    }

    if (!prim_ndx_buf.tbos[0] || prim_ndx_buf.tbos[0]->params().size != prim_ndx_buf.ref->size()) {
        prim_ndx_buf.tbos[0] = ctx.CreateTextureBuffer("Prim Ndx TBO", prim_ndx_buf.ref, Ren::eTexFormat::R32UI, 0,
                                                       prim_ndx_buf.ref->size());
    }

    if (!rt_blas_buf.tbos[0] || rt_blas_buf.tbos[0]->params().size != rt_blas_buf.ref->size()) {
        rt_blas_buf.tbos[0] = ctx.CreateTextureBuffer("RT BLAS TBO", rt_blas_buf.ref, Ren::eTexFormat::RGBA32F, 0,
                                                      rt_blas_buf.ref->size());
    }

    if (!rt_tlas_buf.tbos[0] || rt_tlas_buf.tbos[0]->params().size != rt_tlas_buf.ref->size()) {
        rt_tlas_buf.tbos[0] = ctx.CreateTextureBuffer("RT TLAS TBO", rt_tlas_buf.ref, Ren::eTexFormat::RGBA32F, 0,
                                                      rt_tlas_buf.ref->size());
    }

    if (!mesh_instances_buf.tbos[0] || mesh_instances_buf.tbos[0]->params().size != mesh_instances_buf.ref->size()) {
        mesh_instances_buf.tbos[0] = ctx.CreateTextureBuffer(
            "Mesh Instances TBO", mesh_instances_buf.ref, Ren::eTexFormat::RGBA32F, 0, mesh_instances_buf.ref->size());
    }

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::SBufRO, BIND_BINDLESS_TEX, *textures_buf.ref},
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::Tex2DSampled, RTGI::NORM_TEX_SLOT, *normal_tex.ref},
        //{Ren::eBindTarget::Tex2DSampled, RTGI::FLAT_NORM_TEX_SLOT, *flat_normal_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGI::BLAS_BUF_SLOT, *rt_blas_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::TLAS_BUF_SLOT, *rt_tlas_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, RTGI::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGI::VTX_BUF1_SLOT, *vtx_buf1.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::NDX_BUF_SLOT, *ndx_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, RTGI::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGI::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::IRRADIANCE_TEX_SLOT,
         *std::get<const Ren::Texture2DArray *>(irr_tex._ref)},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::DISTANCE_TEX_SLOT,
         *std::get<const Ren::Texture2DArray *>(dist_tex._ref)},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::OFFSET_TEX_SLOT,
         *std::get<const Ren::Texture2DArray *>(off_tex._ref)},
        {Ren::eBindTarget::Image2D, RTGI::OUT_GI_IMG_SLOT, *out_gi_tex.ref}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->tbos[0]);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->tbos[0]);
    }

    const Ren::Pipeline &pi = args_->two_bounce ? (stoch_lights_buf ? *pi_rt_gi_2bounce_[1] : *pi_rt_gi_2bounce_[0])
                                                : (stoch_lights_buf ? *pi_rt_gi_[1] : *pi_rt_gi_[0]);

    RTGI::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = view_state_->frame_index;
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    DispatchComputeIndirect(pi, *indir_args_buf.ref, sizeof(VkTraceRaysIndirectCommandKHR), bindings, &uniform_params,
                            sizeof(uniform_params), nullptr, ctx.log());
}
