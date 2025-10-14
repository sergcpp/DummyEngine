#include "ExRTGI.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/rt_gi_interface.h"

void Eng::ExRTGI::Execute_HWRT(FgContext &ctx) {
    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocTex &noise_tex = ctx.AccessROTexture(args_->noise_tex);
    FgAllocTex &depth_tex = ctx.AccessROTexture(args_->depth_tex);
    FgAllocTex &normal_tex = ctx.AccessROTexture(args_->normal_tex);
    FgAllocTex &env_tex = ctx.AccessROTexture(args_->env_tex);
    FgAllocBuf &ray_counter_buf = ctx.AccessROBuffer(args_->ray_counter);
    FgAllocBuf &ray_list_buf = ctx.AccessROBuffer(args_->ray_list);
    FgAllocBuf &indir_args_buf = ctx.AccessROBuffer(args_->indir_args);
    [[maybe_unused]] FgAllocBuf &tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);
    FgAllocBuf &lights_buf = ctx.AccessROBuffer(args_->lights_buf);
    FgAllocTex &shadow_depth_tex = ctx.AccessROTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = ctx.AccessROTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = ctx.AccessROTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = ctx.AccessROBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = ctx.AccessROBuffer(args_->items_buf);

    FgAllocTex &irr_tex = ctx.AccessROTexture(args_->irradiance_tex);
    FgAllocTex &dist_tex = ctx.AccessROTexture(args_->distance_tex);
    FgAllocTex &off_tex = ctx.AccessROTexture(args_->offset_tex);

    FgAllocBuf *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        stoch_lights_buf = &ctx.AccessROBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &ctx.AccessROBuffer(args_->light_nodes_buf);
    }

    FgAllocTex &out_gi_tex = ctx.AccessRWTexture(args_->out_gi_tex);

    Ren::ApiContext *api_ctx = ctx.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::TexSampled, RTGI::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::AccStruct, RTGI::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBufRO, RTGI::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBufRO, RTGI::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGI::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGI::ITEMS_BUF_SLOT, *items_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::IRRADIANCE_TEX_SLOT, *irr_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::DISTANCE_TEX_SLOT, *dist_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::OFFSET_TEX_SLOT, *off_tex.ref},
        {Ren::eBindTarget::ImageRW, RTGI::OUT_GI_IMG_SLOT, *out_gi_tex.ref}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->ref);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->ref);
    }

    const Ren::Pipeline &pi = args_->two_bounce ? (stoch_lights_buf ? *pi_rt_gi_2bounce_[1] : *pi_rt_gi_2bounce_[0])
                                                : (stoch_lights_buf ? *pi_rt_gi_[1] : *pi_rt_gi_[0]);

    VkDescriptorSet descr_sets[2];
    descr_sets[0] =
        PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings, ctx.descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTGI::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = view_state_->frame_index;
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}

void Eng::ExRTGI::Execute_SWRT(FgContext &ctx) {
    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &rt_blas_buf = ctx.AccessROBuffer(args_->swrt.rt_blas_buf);
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocTex &noise_tex = ctx.AccessROTexture(args_->noise_tex);
    FgAllocTex &depth_tex = ctx.AccessROTexture(args_->depth_tex);
    FgAllocTex &normal_tex = ctx.AccessROTexture(args_->normal_tex);
    FgAllocTex &env_tex = ctx.AccessROTexture(args_->env_tex);
    FgAllocBuf &ray_counter_buf = ctx.AccessROBuffer(args_->ray_counter);
    FgAllocBuf &ray_list_buf = ctx.AccessROBuffer(args_->ray_list);
    FgAllocBuf &indir_args_buf = ctx.AccessROBuffer(args_->indir_args);
    FgAllocBuf &rt_tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = ctx.AccessROBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = ctx.AccessROBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &lights_buf = ctx.AccessROBuffer(args_->lights_buf);
    FgAllocTex &shadow_depth_tex = ctx.AccessROTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = ctx.AccessROTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = ctx.AccessROTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = ctx.AccessROBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = ctx.AccessROBuffer(args_->items_buf);

    FgAllocTex &irr_tex = ctx.AccessROTexture(args_->irradiance_tex);
    FgAllocTex &dist_tex = ctx.AccessROTexture(args_->distance_tex);
    FgAllocTex &off_tex = ctx.AccessROTexture(args_->offset_tex);

    FgAllocBuf *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        stoch_lights_buf = &ctx.AccessROBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &ctx.AccessROBuffer(args_->light_nodes_buf);
    }

    FgAllocTex &out_gi_tex = ctx.AccessRWTexture(args_->out_gi_tex);

    Ren::ApiContext *api_ctx = ctx.ren_ctx().api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::TexSampled, RTGI::NORM_TEX_SLOT, *normal_tex.ref},
        //{Ren::eBindTarget::TexSampled, RTGI::FLAT_NORM_TEX_SLOT, *flat_normal_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGI::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGI::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGI::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGI::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGI::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, RTGI::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTGI::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTGI::ITEMS_BUF_SLOT, *items_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::IRRADIANCE_TEX_SLOT, *irr_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::DISTANCE_TEX_SLOT, *dist_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::OFFSET_TEX_SLOT, *off_tex.ref},
        {Ren::eBindTarget::ImageRW, RTGI::OUT_GI_IMG_SLOT, *out_gi_tex.ref}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->ref);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGI::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->ref);
    }

    const Ren::Pipeline &pi =
        args_->two_bounce ? *pi_rt_gi_2bounce_[stoch_lights_buf != nullptr] : *pi_rt_gi_[stoch_lights_buf != nullptr];

    VkDescriptorSet descr_sets[2];
    descr_sets[0] =
        PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings, ctx.descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTGI::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = view_state_->frame_index;
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}
