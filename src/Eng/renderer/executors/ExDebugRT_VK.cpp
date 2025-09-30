#include "ExDebugRT.h"

#include <Ren/Context.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/rt_debug_interface.h"

void Eng::ExDebugRT::Execute_HWRT(FgBuilder &builder) {
    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data_buf);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials_buf);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(args_->vtx_buf2);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    FgAllocBuf &lights_buf = builder.GetReadBuffer(args_->lights_buf);
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocTex &env_tex = builder.GetReadTexture(args_->env_tex);
    FgAllocTex &shadow_depth_tex = builder.GetReadTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = builder.GetReadTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = builder.GetReadTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = builder.GetReadBuffer(args_->items_buf);

    FgAllocTex *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
    if (args_->irradiance_tex) {
        irr_tex = &builder.GetReadTexture(args_->irradiance_tex);
        dist_tex = &builder.GetReadTexture(args_->distance_tex);
        off_tex = &builder.GetReadTexture(args_->offset_tex);
    }

    FgAllocTex &output_tex = builder.GetWriteTexture(args_->output_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    const auto *acc_struct = static_cast<const Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::AccStruct, RTDebug::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::TexSampled, RTDebug::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, *items_buf.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::ImageRW, RTDebug::OUT_IMG_SLOT, *output_tex.ref}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::IRRADIANCE_TEX_SLOT, *irr_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::DISTANCE_TEX_SLOT, *dist_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::OFFSET_TEX_SLOT, *off_tex->ref);
    }

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_debug_->prog()->descr_set_layouts()[0], bindings,
                                         ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->ren_res[0];
    uniform_params.img_size[1] = view_state_->ren_res[1];
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.cull_mask = args_->cull_mask;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_debug_->layout(), pi_debug_->prog()->pc_range(0).stageFlags, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdTraceRaysKHR(cmd_buf, pi_debug_->rgen_table(), pi_debug_->miss_table(), pi_debug_->hit_table(),
                               pi_debug_->call_table(), uint32_t(view_state_->ren_res[0]),
                               uint32_t(view_state_->ren_res[1]), 1);
}

void Eng::ExDebugRT::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data_buf);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials_buf);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(args_->vtx_buf2);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    FgAllocBuf &lights_buf = builder.GetReadBuffer(args_->lights_buf);
    FgAllocBuf &rt_tlas_buf = builder.GetReadBuffer(args_->tlas_buf);
    FgAllocBuf &rt_blas_buf = builder.GetReadBuffer(args_->swrt.rt_blas_buf);
    FgAllocBuf &prim_ndx_buf = builder.GetReadBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = builder.GetReadBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocTex &env_tex = builder.GetReadTexture(args_->env_tex);
    FgAllocTex &shadow_depth_tex = builder.GetReadTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = builder.GetReadTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = builder.GetReadTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = builder.GetReadBuffer(args_->items_buf);

    FgAllocTex *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
    if (args_->irradiance_tex) {
        irr_tex = &builder.GetReadTexture(args_->irradiance_tex);
        dist_tex = &builder.GetReadTexture(args_->distance_tex);
        off_tex = &builder.GetReadTexture(args_->offset_tex);
    }

    FgAllocTex &output_tex = builder.GetWriteTexture(args_->output_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::TexSampled, RTDebug::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, *items_buf.ref},
        {Ren::eBindTarget::ImageRW, RTDebug::OUT_IMG_SLOT, *output_tex.ref}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::IRRADIANCE_TEX_SLOT, *irr_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::DISTANCE_TEX_SLOT, *dist_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::OFFSET_TEX_SLOT, *off_tex->ref);
    }

    const auto grp_count =
        Ren::Vec3u{(view_state_->ren_res[0] + RTDebug::LOCAL_GROUP_SIZE_X - 1u) / RTDebug::LOCAL_GROUP_SIZE_X,
                   (view_state_->ren_res[1] + RTDebug::LOCAL_GROUP_SIZE_Y - 1u) / RTDebug::LOCAL_GROUP_SIZE_Y, 1u};

    RTDebug::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.root_node = args_->swrt.root_node;
    uniform_params.cull_mask = args_->cull_mask;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_debug_->prog()->descr_set_layouts()[0], bindings,
                                         ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_debug_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_debug_->layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    api_ctx->vkCmdPushConstants(cmd_buf, pi_debug_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}
