#include "RpRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/rt_reflections_interface.h"

void Eng::RpRTReflections::Execute_HWRT_Pipeline(RpBuilder &builder) {
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
    RpAllocTex &ltc_luts_tex = builder.GetReadTexture(pass_data_->ltc_luts_tex);

    RpAllocTex &out_refl_tex = builder.GetWriteTexture(pass_data_->out_refl_tex);
    RpAllocTex &out_raylen_tex = builder.GetWriteTexture(pass_data_->out_raylen_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    auto *acc_struct = static_cast<const Ren::AccStructureVK *>(pass_data_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::SBuf, RTReflections::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::AccStruct, RTReflections::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBuf, RTReflections::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBuf, RTReflections::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBuf, RTReflections::NDX_BUF_SLOT, *ndx_buf.ref},
        //{Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 0, *lm_tex[0]->ref},
        //{Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 1, *lm_tex[1]->ref},
        //{Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 2, *lm_tex[2]->ref},
        //{Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 3, *lm_tex[3]->ref},
        //{Ren::eBindTarget::Tex2D, RTReflections::LMAP_TEX_SLOTS, 4, *lm_tex[4]->ref},
        {Ren::eBindTarget::SBuf, RTReflections::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::Image2D, RTReflections::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
        {Ren::eBindTarget::Image2D, RTReflections::OUT_RAYLEN_IMG_SLOT, *out_raylen_tex.ref}};

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi_rt_reflections_.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_rt_reflections_.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_rt_reflections_.layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    RTReflections::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_rt_reflections_.layout(),
                                VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdTraceRaysIndirectKHR(cmd_buf, pi_rt_reflections_.rgen_table(), pi_rt_reflections_.miss_table(),
                                       pi_rt_reflections_.hit_table(), pi_rt_reflections_.call_table(),
                                       indir_args_buf.ref->vk_device_address());
}

void Eng::RpRTReflections::Execute_HWRT_Inline(RpBuilder &builder) {
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

    RpAllocTex *irradiance_tex = nullptr, *distance_tex = nullptr, *offset_tex = nullptr;
    if (pass_data_->irradiance_tex) {
        irradiance_tex = &builder.GetReadTexture(pass_data_->irradiance_tex);
        distance_tex = &builder.GetReadTexture(pass_data_->distance_tex);
        offset_tex = &builder.GetReadTexture(pass_data_->offset_tex);
    }

    RpAllocTex &out_refl_tex = builder.GetWriteTexture(pass_data_->out_refl_tex);
    RpAllocTex &out_raylen_tex = builder.GetWriteTexture(pass_data_->out_raylen_tex);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    auto *acc_struct = static_cast<const Ren::AccStructureVK *>(pass_data_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::SBuf, RTReflections::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::AccStruct, RTReflections::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBuf, RTReflections::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBuf, RTReflections::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBuf, RTReflections::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::TBuf, RTReflections::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Image2D, RTReflections::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
        {Ren::eBindTarget::Image2D, RTReflections::OUT_RAYLEN_IMG_SLOT, *out_raylen_tex.ref}};
    if (irradiance_tex) {
        bindings.emplace_back(Ren::eBindTarget::Tex2DArray, RTReflections::IRRADIANCE_TEX_SLOT, *irradiance_tex->arr);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArray, RTReflections::DISTANCE_TEX_SLOT, *distance_tex->arr);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArray, RTReflections::OFFSET_TEX_SLOT, *offset_tex->arr);
    }

    const Ren::Pipeline &pi =
        pass_data_->four_bounces
            ? (irradiance_tex ? pi_rt_reflections_4bounce_inline_[1] : pi_rt_reflections_4bounce_inline_[0])
            : (irradiance_tex ? pi_rt_reflections_inline_[1] : pi_rt_reflections_inline_[0]);

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    const Ren::Vec3f &grid_origin = pass_data_->probe_volume->origin;
    const Ren::Vec3i &grid_scroll = pass_data_->probe_volume->scroll;
    const Ren::Vec3f &grid_spacing = pass_data_->probe_volume->spacing;

    RTReflections::Params uniform_params;
    uniform_params.grid_origin = Ren::Vec4f(grid_origin[0], grid_origin[1], grid_origin[2], 0.0f);
    uniform_params.grid_scroll = Ren::Vec4i(grid_scroll[0], grid_scroll[1], grid_scroll[2], 0.0f);
    uniform_params.grid_spacing = Ren::Vec4f(grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f);
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}

void Eng::RpRTReflections::Execute_SWRT(RpBuilder &builder) {
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

    RpAllocTex *irradiance_tex = nullptr, *distance_tex = nullptr, *offset_tex = nullptr;
    if (pass_data_->irradiance_tex) {
        irradiance_tex = &builder.GetReadTexture(pass_data_->irradiance_tex);
        distance_tex = &builder.GetReadTexture(pass_data_->distance_tex);
        offset_tex = &builder.GetReadTexture(pass_data_->offset_tex);
    }

    RpAllocTex &out_refl_tex = builder.GetWriteTexture(pass_data_->out_refl_tex);
    RpAllocTex &out_raylen_tex = builder.GetWriteTexture(pass_data_->out_raylen_tex);

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
        {Ren::eBindTarget::Tex2D, RTReflections::DEPTH_TEX_SLOT, *depth_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::SBuf, RTReflections::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::TBuf, RTReflections::BLAS_BUF_SLOT, *rt_blas_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::TLAS_BUF_SLOT, *rt_tlas_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::MESHES_BUF_SLOT, *meshes_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.tbos[0]},
        {Ren::eBindTarget::SBuf, RTReflections::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBuf, RTReflections::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::TBuf, RTReflections::VTX_BUF1_SLOT, *vtx_buf1.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::VTX_BUF2_SLOT, *vtx_buf2.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::NDX_BUF_SLOT, *ndx_buf.tbos[0]},
        {Ren::eBindTarget::SBuf, RTReflections::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::SHADOW_TEX_SLOT, *shadowmap_tex.ref},
        {Ren::eBindTarget::Tex2D, RTReflections::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::TBuf, RTReflections::CELLS_BUF_SLOT, *cells_buf.tbos[0]},
        {Ren::eBindTarget::TBuf, RTReflections::ITEMS_BUF_SLOT, *items_buf.tbos[0]},
        {Ren::eBindTarget::Image2D, RTReflections::OUT_REFL_IMG_SLOT, *out_refl_tex.ref},
        {Ren::eBindTarget::Image2D, RTReflections::OUT_RAYLEN_IMG_SLOT, *out_raylen_tex.ref}};
    if (irradiance_tex) {
        bindings.emplace_back(Ren::eBindTarget::Tex2DArray, RTReflections::IRRADIANCE_TEX_SLOT, *irradiance_tex->arr);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArray, RTReflections::DISTANCE_TEX_SLOT, *distance_tex->arr);
        bindings.emplace_back(Ren::eBindTarget::Tex2DArray, RTReflections::OFFSET_TEX_SLOT, *offset_tex->arr);
    }

    const Ren::Pipeline &pi =
        pass_data_->four_bounces
            ? (irradiance_tex ? pi_rt_reflections_4bounce_swrt_[1] : pi_rt_reflections_4bounce_swrt_[0])
            : (irradiance_tex ? pi_rt_reflections_swrt_[1] : pi_rt_reflections_swrt_[0]);

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = Ren::PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings,
                                              ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    const Ren::Vec3f &grid_origin = pass_data_->probe_volume->origin;
    const Ren::Vec3i &grid_scroll = pass_data_->probe_volume->scroll;
    const Ren::Vec3f &grid_spacing = pass_data_->probe_volume->spacing;

    RTReflections::Params uniform_params;
    uniform_params.grid_origin = Ren::Vec4f(grid_origin[0], grid_origin[1], grid_origin[2], 0.0f);
    uniform_params.grid_scroll = Ren::Vec4i(grid_scroll[0], grid_scroll[1], grid_scroll[2], 0.0f);
    uniform_params.grid_spacing = Ren::Vec4f(grid_spacing[0], grid_spacing[1], grid_spacing[2], 0.0f);
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}

void Eng::RpRTReflections::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized) {
        if (ctx.capabilities.raytracing) {
            Ren::ProgramRef rt_reflections_prog = sh.LoadProgram(
                ctx, "rt_reflections", "internal/rt_reflections.rgen.glsl", "internal/rt_reflections.rchit.glsl",
                "internal/rt_reflections.rahit.glsl", "internal/rt_reflections.rmiss.glsl", {});
            assert(rt_reflections_prog->ready());

            if (!pi_rt_reflections_.Init(ctx.api_ctx(), std::move(rt_reflections_prog), ctx.log())) {
                ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
            }

            if (ctx.capabilities.ray_query) {
                Ren::ProgramRef rt_reflections_inline_prog =
                    sh.LoadProgram(ctx, "rt_reflections_inline", "internal/rt_reflections_hwrt.comp.glsl");
                assert(rt_reflections_inline_prog->ready());

                if (!pi_rt_reflections_inline_[0].Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog),
                                                       ctx.log())) {
                    ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
                }

                rt_reflections_inline_prog = sh.LoadProgram(ctx, "rt_reflections_inline_gi",
                                                            "internal/rt_reflections_hwrt.comp.glsl@GI_CACHE");
                assert(rt_reflections_inline_prog->ready());

                if (!pi_rt_reflections_inline_[1].Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog),
                                                       ctx.log())) {
                    ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
                }

                rt_reflections_inline_prog = sh.LoadProgram(ctx, "rt_reflections_4bounce_inline",
                                                            "internal/rt_reflections_hwrt.comp.glsl@FOUR_BOUNCES");
                assert(rt_reflections_inline_prog->ready());

                if (!pi_rt_reflections_4bounce_inline_[0].Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog),
                                                               ctx.log())) {
                    ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
                }

                rt_reflections_inline_prog =
                    sh.LoadProgram(ctx, "rt_reflections_4bounce_inline_gi",
                                   "internal/rt_reflections_hwrt.comp.glsl@FOUR_BOUNCES;GI_CACHE");
                assert(rt_reflections_inline_prog->ready());

                if (!pi_rt_reflections_4bounce_inline_[1].Init(ctx.api_ctx(), std::move(rt_reflections_inline_prog),
                                                               ctx.log())) {
                    ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
                }
            }
        }

        Ren::ProgramRef rt_reflections_swrt_prog =
            sh.LoadProgram(ctx, "rt_reflections_swrt", "internal/rt_reflections_swrt.comp.glsl");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_swrt_[0].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_swrt_prog =
            sh.LoadProgram(ctx, "rt_reflections_swrt_gi", "internal/rt_reflections_swrt.comp.glsl@GI_CACHE");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_swrt_[1].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_swrt_prog =
            sh.LoadProgram(ctx, "rt_reflections_4bounce_swrt", "internal/rt_reflections_swrt.comp.glsl@FOUR_BOUNCES");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_4bounce_swrt_[0].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        rt_reflections_swrt_prog = sh.LoadProgram(ctx, "rt_reflections_4bounce_swrt_gi",
                                                  "internal/rt_reflections_swrt.comp.glsl@FOUR_BOUNCES;GI_CACHE");
        assert(rt_reflections_swrt_prog->ready());

        if (!pi_rt_reflections_4bounce_swrt_[1].Init(ctx.api_ctx(), std::move(rt_reflections_swrt_prog), ctx.log())) {
            ctx.log()->Error("RpRTReflections: Failed to initialize pipeline!");
        }

        initialized = true;
    }
}