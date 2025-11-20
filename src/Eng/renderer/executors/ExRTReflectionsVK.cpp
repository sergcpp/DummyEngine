#include "ExRTReflections.h"

#include <Ren/Context.h>
#include <Ren/Image.h>
#include <Ren/RastState.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/rt_reflections_interface.h"

void Eng::ExRTReflections::Execute_HWRT(FgContext &fg) {
    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &vtx_buf2 = fg.AccessROBuffer(args_->vtx_buf2);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Image &depth_tex = fg.AccessROImage(args_->depth_tex);
    const Ren::Image &normal_tex = fg.AccessROImage(args_->normal_tex);
    const Ren::Image &env_tex = fg.AccessROImage(args_->env_tex);
    const Ren::Buffer &ray_counter_buf = fg.AccessROBuffer(args_->ray_counter);
    const Ren::Buffer &ray_list_buf = fg.AccessROBuffer(args_->ray_list);
    const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(args_->indir_args);
    [[maybe_unused]] const Ren::Buffer &tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::Buffer &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    const Ren::Image &shadow_depth_tex = fg.AccessROImage(args_->shadow_depth_tex);
    const Ren::Image &shadow_color_tex = fg.AccessROImage(args_->shadow_color_tex);
    const Ren::Image &ltc_luts_tex = fg.AccessROImage(args_->ltc_luts_tex);
    const Ren::Buffer &cells_buf = fg.AccessROBuffer(args_->cells_buf);
    const Ren::Buffer &items_buf = fg.AccessROBuffer(args_->items_buf);

    const Ren::Image *irr_tex = nullptr, *dist_tex = nullptr, *off_tex = nullptr;
    if (args_->irradiance_tex) {
        irr_tex = &fg.AccessROImage(args_->irradiance_tex);
        dist_tex = &fg.AccessROImage(args_->distance_tex);
        off_tex = &fg.AccessROImage(args_->offset_tex);
    }

    const Ren::Buffer *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        stoch_lights_buf = &fg.AccessROBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &fg.AccessROBuffer(args_->light_nodes_buf);
    }

    const Ren::Buffer *oit_depth_buf = nullptr;
    const Ren::Image *noise_tex = nullptr;
    if (args_->oit_depth_buf) {
        oit_depth_buf = &fg.AccessROBuffer(args_->oit_depth_buf);
    } else {
        noise_tex = &fg.AccessROImage(args_->noise_tex);
    }

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<const Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
        {Ren::eBindTarget::TexSampled, RTReflections::DEPTH_TEX_SLOT, {depth_tex, 1}},
        {Ren::eBindTarget::TexSampled, RTReflections::NORM_TEX_SLOT, normal_tex},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_COUNTER_SLOT, ray_counter_buf},
        {Ren::eBindTarget::SBufRO, RTReflections::RAY_LIST_SLOT, ray_list_buf},
        {Ren::eBindTarget::TexSampled, RTReflections::ENV_TEX_SLOT, env_tex},
        {Ren::eBindTarget::AccStruct, RTReflections::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBufRO, RTReflections::GEO_DATA_BUF_SLOT, geo_data_buf},
        {Ren::eBindTarget::SBufRO, RTReflections::MATERIAL_BUF_SLOT, materials_buf},
        {Ren::eBindTarget::SBufRO, RTReflections::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::SBufRO, RTReflections::VTX_BUF2_SLOT, vtx_buf2},
        {Ren::eBindTarget::SBufRO, RTReflections::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRO, RTReflections::LIGHTS_BUF_SLOT, lights_buf},
        {Ren::eBindTarget::TexSampled, RTReflections::SHADOW_DEPTH_TEX_SLOT, shadow_depth_tex},
        {Ren::eBindTarget::TexSampled, RTReflections::SHADOW_COLOR_TEX_SLOT, shadow_color_tex},
        {Ren::eBindTarget::TexSampled, RTReflections::LTC_LUTS_TEX_SLOT, ltc_luts_tex},
        {Ren::eBindTarget::UTBuf, RTReflections::CELLS_BUF_SLOT, cells_buf},
        {Ren::eBindTarget::UTBuf, RTReflections::ITEMS_BUF_SLOT, items_buf}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::IRRADIANCE_TEX_SLOT, *irr_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::DISTANCE_TEX_SLOT, *dist_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::OFFSET_TEX_SLOT, *off_tex);
    }
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::LIGHT_NODES_BUF_SLOT, *light_nodes_buf);
    }
    if (noise_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTReflections::NOISE_TEX_SLOT, *noise_tex);
    }
    if (oit_depth_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTReflections::OIT_DEPTH_BUF_SLOT, *oit_depth_buf);
    }
    for (int i = 0; i < OIT_REFLECTION_LAYERS && args_->out_refl_tex[i]; ++i) {
        Ren::Image &out_refl_tex = fg.AccessRWImage(args_->out_refl_tex[i]);
        bindings.emplace_back(Ren::eBindTarget::ImageRW, RTReflections::OUT_REFL_IMG_SLOT, i, 1, out_refl_tex);
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

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}
