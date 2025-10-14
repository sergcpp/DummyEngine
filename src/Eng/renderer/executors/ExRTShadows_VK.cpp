#include "ExRTShadows.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/rt_shadows_interface.h"

void Eng::ExRTShadows::Execute_HWRT_Pipeline(FgContext &ctx) {
    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocTex &noise_tex = ctx.AccessROTexture(args_->noise_tex);
    FgAllocTex &depth_tex = ctx.AccessROTexture(args_->depth_tex);
    FgAllocTex &normal_tex = ctx.AccessROTexture(args_->normal_tex);
    [[maybe_unused]] FgAllocBuf &tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);

    FgAllocTex &out_shadow_tex = ctx.AccessRWTexture(args_->out_shadow_tex);

    Ren::ApiContext *api_ctx = ctx.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<const Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = ctx.cmd_buf();

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                     {Ren::eBindTarget::TexSampled, RTShadows::NOISE_TEX_SLOT, *noise_tex.ref},
                                     {Ren::eBindTarget::TexSampled, RTShadows::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                     {Ren::eBindTarget::TexSampled, RTShadows::NORM_TEX_SLOT, *normal_tex.ref},
                                     {Ren::eBindTarget::AccStruct, RTShadows::TLAS_SLOT, *acc_struct},
                                     {Ren::eBindTarget::SBufRO, RTShadows::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
                                     {Ren::eBindTarget::SBufRO, RTShadows::MATERIAL_BUF_SLOT, *materials_buf.ref},
                                     {Ren::eBindTarget::SBufRO, RTShadows::VTX_BUF1_SLOT, *vtx_buf1.ref},
                                     {Ren::eBindTarget::SBufRO, RTShadows::NDX_BUF_SLOT, *ndx_buf.ref},
                                     {Ren::eBindTarget::ImageRW, RTShadows::OUT_SHADOW_IMG_SLOT, *out_shadow_tex.ref}};

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_rt_shadows_->prog()->descr_set_layouts()[0], bindings,
                                         ctx.descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_rt_shadows_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_rt_shadows_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u(view_state_->ren_res);
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_rt_shadows_->layout(),
                                VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0,
                                sizeof(uniform_params), &uniform_params);

    // vkCmdTraceRaysIndirectKHR(cmd_buf, pi_rt_shadows_.rgen_table(), pi_rt_shadows_.miss_table(),
    //                           pi_rt_shadows_.hit_table(), pi_rt_shadows_.call_table(),
    //                           indir_args_buf.ref->vk_device_address());
}

void Eng::ExRTShadows::Execute_HWRT_Inline(FgContext &ctx) {
    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocTex &noise_tex = ctx.AccessROTexture(args_->noise_tex);
    FgAllocTex &depth_tex = ctx.AccessROTexture(args_->depth_tex);
    FgAllocTex &normal_tex = ctx.AccessROTexture(args_->normal_tex);
    [[maybe_unused]] FgAllocBuf &tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);
    FgAllocBuf &tile_list_buf = ctx.AccessROBuffer(args_->tile_list_buf);
    FgAllocBuf &indir_args_buf = ctx.AccessROBuffer(args_->indir_args);

    FgAllocTex &out_shadow_tex = ctx.AccessRWTexture(args_->out_shadow_tex);

    Ren::ApiContext *api_ctx = ctx.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<const Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
                                     {Ren::eBindTarget::TexSampled, RTShadows::NOISE_TEX_SLOT, *noise_tex.ref},
                                     {Ren::eBindTarget::TexSampled, RTShadows::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
                                     {Ren::eBindTarget::TexSampled, RTShadows::NORM_TEX_SLOT, *normal_tex.ref},
                                     {Ren::eBindTarget::AccStruct, RTShadows::TLAS_SLOT, *acc_struct},
                                     {Ren::eBindTarget::SBufRO, RTShadows::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
                                     {Ren::eBindTarget::SBufRO, RTShadows::MATERIAL_BUF_SLOT, *materials_buf.ref},
                                     {Ren::eBindTarget::SBufRO, RTShadows::VTX_BUF1_SLOT, *vtx_buf1.ref},
                                     {Ren::eBindTarget::SBufRO, RTShadows::NDX_BUF_SLOT, *ndx_buf.ref},
                                     {Ren::eBindTarget::SBufRO, RTShadows::TILE_LIST_SLOT, *tile_list_buf.ref},
                                     {Ren::eBindTarget::ImageRW, RTShadows::OUT_SHADOW_IMG_SLOT, *out_shadow_tex.ref}};

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_rt_shadows_->prog()->descr_set_layouts()[0], bindings,
                                         ctx.descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_rt_shadows_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_rt_shadows_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_rt_shadows_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(), 0);
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

    Ren::ApiContext *api_ctx = ctx.ren_ctx().api_ctx();

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
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

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_rt_shadows_->prog()->descr_set_layouts()[0], bindings,
                                         ctx.descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_rt_shadows_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_rt_shadows_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_rt_shadows_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(), 0);
}
