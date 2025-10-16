#include "ExRTReflections.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/rt_reflections_interface.h"

void Eng::ExRTReflections::Execute_HWRT(FgContext &fg) {
    FgAllocBuf &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = fg.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &vtx_buf2 = fg.AccessROBuffer(args_->vtx_buf2);
    FgAllocBuf &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    FgAllocTex &depth_tex = fg.AccessROTexture(args_->depth_tex);
    FgAllocTex &normal_tex = fg.AccessROTexture(args_->normal_tex);
    FgAllocTex &env_tex = fg.AccessROTexture(args_->env_tex);
    FgAllocBuf &ray_counter_buf = fg.AccessROBuffer(args_->ray_counter);
    FgAllocBuf &ray_list_buf = fg.AccessROBuffer(args_->ray_list);
    FgAllocBuf &indir_args_buf = fg.AccessROBuffer(args_->indir_args);
    [[maybe_unused]] FgAllocBuf &tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    FgAllocBuf &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    FgAllocTex &shadow_depth_tex = fg.AccessROTexture(args_->shadow_depth_tex);
    FgAllocTex &shadow_color_tex = fg.AccessROTexture(args_->shadow_color_tex);
    FgAllocTex &ltc_luts_tex = fg.AccessROTexture(args_->ltc_luts_tex);
    FgAllocBuf &cells_buf = fg.AccessROBuffer(args_->cells_buf);
    FgAllocBuf &items_buf = fg.AccessROBuffer(args_->items_buf);

    FgAllocTex *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
    if (args_->irradiance_tex) {
        irr_tex = &fg.AccessROTexture(args_->irradiance_tex);
        dist_tex = &fg.AccessROTexture(args_->distance_tex);
        off_tex = &fg.AccessROTexture(args_->offset_tex);
    }

    FgAllocBuf *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        stoch_lights_buf = &fg.AccessROBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &fg.AccessROBuffer(args_->light_nodes_buf);
    }

    FgAllocBuf *oit_depth_buf = nullptr;
    FgAllocTex *noise_tex = nullptr;
    if (args_->oit_depth_buf) {
        oit_depth_buf = &fg.AccessROBuffer(args_->oit_depth_buf);
    } else {
        noise_tex = &fg.AccessROTexture(args_->noise_tex);
    }

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<const Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::TexSampled, RTReflections::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::ENV_TEX_SLOT, *env_tex.ref},
        {Ren::eBindTarget::AccStruct, RTReflections::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBufRO, RTReflections::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::VTX_BUF2_SLOT, *vtx_buf2.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRO, RTReflections::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::SHADOW_DEPTH_TEX_SLOT, *shadow_depth_tex.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::SHADOW_COLOR_TEX_SLOT, *shadow_color_tex.ref},
        {Ren::eBindTarget::TexSampled, RTReflections::LTC_LUTS_TEX_SLOT, *ltc_luts_tex.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::CELLS_BUF_SLOT, *cells_buf.ref},
        {Ren::eBindTarget::UTBuf, RTReflections::ITEMS_BUF_SLOT, *items_buf.ref}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::IRRADIANCE_TEX_SLOT, *irr_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::DISTANCE_TEX_SLOT, *dist_tex->ref);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::OFFSET_TEX_SLOT, *off_tex->ref);
    }
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf->ref);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::LIGHT_NODES_BUF_SLOT, *light_nodes_buf->ref);
    }
    if (noise_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::NOISE_TEX_SLOT, *noise_tex->ref);
    }
    if (oit_depth_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::OIT_DEPTH_BUF_SLOT, *oit_depth_buf->ref);
    }
    for (int i = 0; i < OIT_REFLECTION_LAYERS && args_->out_refl_tex[i]; ++i) {
        FgAllocTex &out_refl_tex = fg.AccessRWTexture(args_->out_refl_tex[i]);
        bindings.emplace_back(Ren::eBindTarget::ImageRW, RTReflections::OUT_REFL_IMG_SLOT, i, 1, *out_refl_tex.ref);
    }

    const Ren::Pipeline &pi = *pi_rt_reflections_[stoch_lights_buf != nullptr];

    VkDescriptorSet descr_sets[2];
    descr_sets[0] =
        PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTReflections::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}
