#include "RpRTGI.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/rt_gi_interface.h"

void Eng::RpRTGI::Execute_HWRT(RpBuilder &builder) {
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &noise_tex = builder.GetReadTexture(pass_data_->noise_tex);
    RpAllocTex &depth_tex = builder.GetReadTexture(pass_data_->depth_tex);
    RpAllocTex &normal_tex = builder.GetReadTexture(pass_data_->normal_tex);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocBuf &ray_counter_buf = builder.GetReadBuffer(pass_data_->ray_counter);
    RpAllocBuf &ray_list_buf = builder.GetReadBuffer(pass_data_->ray_list);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(pass_data_->indir_args);
    RpAllocBuf &tlas_buf = builder.GetReadBuffer(pass_data_->tlas_buf);
    RpAllocTex &dummy_black = builder.GetReadTexture(pass_data_->dummy_black);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(pass_data_->lights_buf);
    RpAllocTex &shadowmap_tex = builder.GetReadTexture(pass_data_->shadowmap_tex);
    RpAllocTex &ltc_luts_tex = builder.GetReadTexture(pass_data_->ltc_luts_tex);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(pass_data_->cells_buf);
    RpAllocBuf &items_buf = builder.GetReadBuffer(pass_data_->items_buf);

    RpAllocTex &irradiance_tex = builder.GetReadTexture(pass_data_->irradiance_tex);
    RpAllocTex &distance_tex = builder.GetReadTexture(pass_data_->distance_tex);
    RpAllocTex &offset_tex = builder.GetReadTexture(pass_data_->offset_tex);

    RpAllocBuf *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (pass_data_->stoch_lights_buf) {
        stoch_lights_buf = &builder.GetReadBuffer(pass_data_->stoch_lights_buf);
        light_nodes_buf = &builder.GetReadBuffer(pass_data_->light_nodes_buf);

        if (!stoch_lights_buf->tbos[0] || stoch_lights_buf->tbos[0]->params().size != stoch_lights_buf->ref->size()) {
            stoch_lights_buf->tbos[0] =
                builder.ctx().CreateTexture1D("Stoch Lights Buf TBO", stoch_lights_buf->ref,
                                              Ren::eTexFormat::RawRGBA32F, 0, stoch_lights_buf->ref->size());
        }
        if (!light_nodes_buf->tbos[0] || light_nodes_buf->tbos[0]->params().size != light_nodes_buf->ref->size()) {
            light_nodes_buf->tbos[0] =
                builder.ctx().CreateTexture1D("Stoch Lights Nodes Buf TBO", light_nodes_buf->ref,
                                              Ren::eTexFormat::RawRGBA32F, 0, light_nodes_buf->ref->size());
        }
    }

    RpAllocTex &out_gi_tex = builder.GetWriteTexture(pass_data_->out_gi_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(pass_data_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::Tex2DSampled, RTGI::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::AccStruct, RTGI::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBufRO, RTGI::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBufRO, RTGI::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBufRO, RTGI::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGI::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::IRRADIANCE_TEX_SLOT, *irradiance_tex.arr},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::DISTANCE_TEX_SLOT, *distance_tex.arr},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::OFFSET_TEX_SLOT, *offset_tex.arr},
        {Ren::eBindTarget::Image2D, RTGI::OUT_GI_IMG_SLOT, *out_gi_tex.ref}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->tbos[0]);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->tbos[0]);
    }

    const Ren::Pipeline &pi = pass_data_->two_bounce ? (stoch_lights_buf ? pi_rt_gi_2bounce_[1] : pi_rt_gi_2bounce_[0])
                                                     : (stoch_lights_buf ? pi_rt_gi_[1] : pi_rt_gi_[0]);

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTGI::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = view_state_->frame_index;
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}

void Eng::RpRTGI::Execute_SWRT(RpBuilder &builder) {
    RpAllocBuf &geo_data_buf = builder.GetReadBuffer(pass_data_->geo_data);
    RpAllocBuf &materials_buf = builder.GetReadBuffer(pass_data_->materials);
    RpAllocBuf &vtx_buf1 = builder.GetReadBuffer(pass_data_->vtx_buf1);
    RpAllocBuf &vtx_buf2 = builder.GetReadBuffer(pass_data_->vtx_buf2);
    RpAllocBuf &ndx_buf = builder.GetReadBuffer(pass_data_->ndx_buf);
    RpAllocBuf &rt_blas_buf = builder.GetReadBuffer(pass_data_->swrt.rt_blas_buf);
    RpAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(pass_data_->shared_data);
    RpAllocTex &noise_tex = builder.GetReadTexture(pass_data_->noise_tex);
    RpAllocTex &depth_tex = builder.GetReadTexture(pass_data_->depth_tex);
    RpAllocTex &normal_tex = builder.GetReadTexture(pass_data_->normal_tex);
    RpAllocTex &env_tex = builder.GetReadTexture(pass_data_->env_tex);
    RpAllocBuf &ray_counter_buf = builder.GetReadBuffer(pass_data_->ray_counter);
    RpAllocBuf &ray_list_buf = builder.GetReadBuffer(pass_data_->ray_list);
    RpAllocBuf &indir_args_buf = builder.GetReadBuffer(pass_data_->indir_args);
    RpAllocBuf &rt_tlas_buf = builder.GetReadBuffer(pass_data_->tlas_buf);
    RpAllocBuf &prim_ndx_buf = builder.GetReadBuffer(pass_data_->swrt.prim_ndx_buf);
    RpAllocBuf &meshes_buf = builder.GetReadBuffer(pass_data_->swrt.meshes_buf);
    RpAllocBuf &mesh_instances_buf = builder.GetReadBuffer(pass_data_->swrt.mesh_instances_buf);
    RpAllocTex &dummy_black = builder.GetReadTexture(pass_data_->dummy_black);
    RpAllocBuf &lights_buf = builder.GetReadBuffer(pass_data_->lights_buf);
    RpAllocTex &shadowmap_tex = builder.GetReadTexture(pass_data_->shadowmap_tex);
    RpAllocTex &ltc_luts_tex = builder.GetReadTexture(pass_data_->ltc_luts_tex);
    RpAllocBuf &cells_buf = builder.GetReadBuffer(pass_data_->cells_buf);
    RpAllocBuf &items_buf = builder.GetReadBuffer(pass_data_->items_buf);

    RpAllocTex &irradiance_tex = builder.GetReadTexture(pass_data_->irradiance_tex);
    RpAllocTex &distance_tex = builder.GetReadTexture(pass_data_->distance_tex);
    RpAllocTex &offset_tex = builder.GetReadTexture(pass_data_->offset_tex);

    RpAllocBuf *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (pass_data_->stoch_lights_buf) {
        stoch_lights_buf = &builder.GetReadBuffer(pass_data_->stoch_lights_buf);
        light_nodes_buf = &builder.GetReadBuffer(pass_data_->light_nodes_buf);

        if (!stoch_lights_buf->tbos[0] || stoch_lights_buf->tbos[0]->params().size != stoch_lights_buf->ref->size()) {
            stoch_lights_buf->tbos[0] =
                builder.ctx().CreateTexture1D("Stoch Lights Buf TBO", stoch_lights_buf->ref,
                                              Ren::eTexFormat::RawRGBA32F, 0, stoch_lights_buf->ref->size());
        }
        if (!light_nodes_buf->tbos[0] || light_nodes_buf->tbos[0]->params().size != light_nodes_buf->ref->size()) {
            light_nodes_buf->tbos[0] =
                builder.ctx().CreateTexture1D("Stoch Lights Nodes Buf TBO", light_nodes_buf->ref,
                                              Ren::eTexFormat::RawRGBA32F, 0, light_nodes_buf->ref->size());
        }
    }

    RpAllocTex &out_gi_tex = builder.GetWriteTexture(pass_data_->out_gi_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    if (!vtx_buf1.tbos[0] || vtx_buf1.tbos[0]->params().size != vtx_buf1.ref->size()) {
        vtx_buf1.tbos[0] =
            ctx.CreateTexture1D("Vertex Buf 1 TBO", vtx_buf1.ref, Ren::eTexFormat::RawRGBA32F, 0, vtx_buf1.ref->size());
    }

    if (!vtx_buf2.tbos[0] || vtx_buf2.tbos[0]->params().size != vtx_buf2.ref->size()) {
        vtx_buf2.tbos[0] = ctx.CreateTexture1D("Vertex Buf 2 TBO", vtx_buf2.ref, Ren::eTexFormat::RawRGBA32UI, 0,
                                               vtx_buf2.ref->size());
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

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
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
        {Ren::eBindTarget::UTBuf, RTGI::MESHES_BUF_SLOT, *meshes_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, RTGI::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGI::VTX_BUF1_SLOT, *vtx_buf1.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::VTX_BUF2_SLOT, *vtx_buf2.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::NDX_BUF_SLOT, *ndx_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, RTGI::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTGI::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGI::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTGI::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::IRRADIANCE_TEX_SLOT, *irradiance_tex.arr},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::DISTANCE_TEX_SLOT, *distance_tex.arr},
        {Ren::eBindTarget::Tex2DArraySampled, RTGI::OFFSET_TEX_SLOT, *offset_tex.arr},
        {Ren::eBindTarget::Image2D, RTGI::OUT_GI_IMG_SLOT, *out_gi_tex.ref}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->tbos[0]);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->tbos[0]);
    }

    const Ren::Pipeline &pi = pass_data_->two_bounce ? (stoch_lights_buf ? pi_rt_gi_2bounce_[1] : pi_rt_gi_2bounce_[0])
                                                     : (stoch_lights_buf ? pi_rt_gi_[1] : pi_rt_gi_[0]);

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTGI::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = view_state_->frame_index;
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}
