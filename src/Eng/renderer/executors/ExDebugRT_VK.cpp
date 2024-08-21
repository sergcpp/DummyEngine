#include "ExDebugRT.h"

#include <Ren/Context.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
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
    FgAllocTex &dummy_black = builder.GetReadTexture(args_->dummy_black);
    FgAllocTex &shadowmap_tex = builder.GetReadTexture(args_->shadowmap_tex);
    FgAllocTex &ltc_luts_tex = builder.GetReadTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = builder.GetReadBuffer(args_->items_buf);

    FgAllocTex *irradiance_tex = nullptr, *distance_tex = nullptr, *offset_tex = nullptr;
    if (args_->irradiance_tex) {
        irradiance_tex = &builder.GetReadTexture(args_->irradiance_tex);
        distance_tex = &builder.GetReadTexture(args_->distance_tex);
        offset_tex = &builder.GetReadTexture(args_->offset_tex);
    }

    FgAllocTex &output_tex = builder.GetWriteTexture(args_->output_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    const auto *acc_struct = static_cast<const Ren::AccStructureVK *>(tlas_to_debug_);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::AccStruct, RTDebug::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::Tex2DSampled, RTDebug::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Tex2DSampled, RTDebug::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTDebug::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::Image2D, RTDebug::OUT_IMG_SLOT, *output_tex.ref}};
    if (irradiance_tex) {
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTDebug::IRRADIANCE_TEX_SLOT, *irradiance_tex->arr);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTDebug::DISTANCE_TEX_SLOT, *distance_tex->arr);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTDebug::OFFSET_TEX_SLOT, *offset_tex->arr);
    }

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi_debug_hwrt_.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_hwrt_.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_debug_hwrt_.layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->scr_res[0];
    uniform_params.img_size[1] = view_state_->scr_res[1];
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_debug_hwrt_.layout(), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdTraceRaysKHR(cmd_buf, pi_debug_hwrt_.rgen_table(), pi_debug_hwrt_.miss_table(),
                               pi_debug_hwrt_.hit_table(), pi_debug_hwrt_.call_table(),
                               uint32_t(view_state_->scr_res[0]), uint32_t(view_state_->scr_res[1]), 1);
}

void Eng::ExDebugRT::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data_buf);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials_buf);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &vtx_buf2 = builder.GetReadBuffer(args_->vtx_buf2);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    FgAllocBuf &lights_buf = builder.GetReadBuffer(args_->lights_buf);
    FgAllocBuf &rt_blas_buf = builder.GetReadBuffer(args_->swrt.rt_blas_buf);
    FgAllocBuf &rt_tlas_buf = builder.GetReadBuffer(args_->swrt.rt_tlas_buf);
    FgAllocBuf &prim_ndx_buf = builder.GetReadBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &meshes_buf = builder.GetReadBuffer(args_->swrt.meshes_buf);
    FgAllocBuf &mesh_instances_buf = builder.GetReadBuffer(args_->swrt.mesh_instances_buf);
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocTex &env_tex = builder.GetReadTexture(args_->env_tex);
    FgAllocTex &dummy_black = builder.GetReadTexture(args_->dummy_black);
    FgAllocTex &shadowmap_tex = builder.GetReadTexture(args_->shadowmap_tex);
    FgAllocTex &ltc_luts_tex = builder.GetReadTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = builder.GetReadBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = builder.GetReadBuffer(args_->items_buf);

    FgAllocTex *irradiance_tex = nullptr, *distance_tex = nullptr, *offset_tex = nullptr;
    if (args_->irradiance_tex) {
        irradiance_tex = &builder.GetReadTexture(args_->irradiance_tex);
        distance_tex = &builder.GetReadTexture(args_->distance_tex);
        offset_tex = &builder.GetReadTexture(args_->offset_tex);
    }

    FgAllocTex &output_tex = builder.GetWriteTexture(args_->output_tex);

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

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF1_SLOT, *vtx_buf1.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::VTX_BUF2_SLOT, *vtx_buf2.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::NDX_BUF_SLOT, *ndx_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::BLAS_BUF_SLOT, *rt_blas_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::TLAS_BUF_SLOT, *rt_tlas_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::MESHES_BUF_SLOT, *meshes_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.tbos[0]},
        {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2DSampled, RTDebug::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTDebug::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2DSampled, RTDebug::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Image2D, RTDebug::OUT_IMG_SLOT, *output_tex.ref}};
    if (irradiance_tex) {
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTDebug::IRRADIANCE_TEX_SLOT, *irradiance_tex->arr);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTDebug::DISTANCE_TEX_SLOT, *distance_tex->arr);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArraySampled, RTDebug::OFFSET_TEX_SLOT, *offset_tex->arr);
    }

    const auto grp_count =
        Ren::Vec3u{(view_state_->act_res[0] + RTDebug::LOCAL_GROUP_SIZE_X - 1u) / RTDebug::LOCAL_GROUP_SIZE_X,
                   (view_state_->act_res[1] + RTDebug::LOCAL_GROUP_SIZE_Y - 1u) / RTDebug::LOCAL_GROUP_SIZE_Y, 1u};

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->act_res[0];
    uniform_params.img_size[1] = view_state_->act_res[1];
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.root_node = args_->swrt.root_node;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi_debug_swrt_.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_debug_swrt_.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_debug_swrt_.layout(), 0, 2, descr_sets,
                                     0, nullptr);

    api_ctx->vkCmdPushConstants(cmd_buf, pi_debug_swrt_.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}