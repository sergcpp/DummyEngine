#include "ExDebugRT.h"

#include <Ren/Context.h>
#include <Ren/Image.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/rt_debug_interface.h"

void Eng::ExDebugRT::Execute_HWRT(FgContext &fg) {
    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data_buf);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials_buf);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &vtx_buf2 = fg.AccessROBuffer(args_->vtx_buf2);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Image &env_tex = fg.AccessROImage(args_->env_tex);
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

    Ren::Image &output_tex = fg.AccessRWImage(args_->output_tex);

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    const auto *acc_struct = static_cast<const Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
        {Ren::eBindTarget::AccStruct, RTDebug::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::TexSampled, RTDebug::ENV_TEX_SLOT, env_tex},
        {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, geo_data_buf},
        {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, materials_buf},
        {Ren::eBindTarget::SBufRO, RTDebug::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::SBufRO, RTDebug::VTX_BUF2_SLOT, vtx_buf2},
        {Ren::eBindTarget::SBufRO, RTDebug::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, lights_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, cells_buf},
        {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, items_buf},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_DEPTH_TEX_SLOT, shadow_depth_tex},
        {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_COLOR_TEX_SLOT, shadow_color_tex},
        {Ren::eBindTarget::TexSampled, RTDebug::LTC_LUTS_TEX_SLOT, ltc_luts_tex},
        {Ren::eBindTarget::ImageRW, RTDebug::OUT_IMG_SLOT, output_tex}};
    if (irr_tex) {
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::IRRADIANCE_TEX_SLOT, *irr_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::DISTANCE_TEX_SLOT, *dist_tex);
        bindings.emplace_back(Ren::eBindTarget::TexSampled, RTDebug::OFFSET_TEX_SLOT, *off_tex);
    }

    VkDescriptorSet descr_sets[2];
    descr_sets[0] =
        PrepareDescriptorSet(api_ctx, pi_debug_->prog()->descr_set_layouts()[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_textures.descr_set;

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
