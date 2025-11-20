#include "ExRTGICache.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/rt_gi_cache_interface.h"

void Eng::ExRTGICache::Execute_HWRT(FgContext &fg) {
    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Image &env_tex = fg.AccessROImage(args_->env_tex);
    [[maybe_unused]] const Ren::Buffer &tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::Buffer &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    const Ren::Image &shadow_depth_tex = fg.AccessROImage(args_->shadow_depth_tex);
    const Ren::Image &shadow_color_tex = fg.AccessROImage(args_->shadow_color_tex);
    const Ren::Image &ltc_luts_tex = fg.AccessROImage(args_->ltc_luts_tex);
    const Ren::Buffer &cells_buf = fg.AccessROBuffer(args_->cells_buf);
    const Ren::Buffer &items_buf = fg.AccessROBuffer(args_->items_buf);
    const Ren::Image &irr_tex = fg.AccessROImage(args_->irradiance_tex);
    const Ren::Image &dist_tex = fg.AccessROImage(args_->distance_tex);
    const Ren::Image &off_tex = fg.AccessROImage(args_->offset_tex);

    const Ren::Buffer *random_seq_buf = nullptr, *stoch_lights_buf = nullptr, *light_nodes_buf = nullptr;
    if (args_->stoch_lights_buf) {
        random_seq_buf = &fg.AccessROBuffer(args_->random_seq);
        stoch_lights_buf = &fg.AccessROBuffer(args_->stoch_lights_buf);
        light_nodes_buf = &fg.AccessROBuffer(args_->light_nodes_buf);
    }

    Ren::Image &out_ray_data_tex = fg.AccessRWImage(args_->out_ray_data_tex);

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 16> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
        {Ren::eBindTarget::TexSampled, RTGICache::ENV_TEX_SLOT, env_tex},
        {Ren::eBindTarget::AccStruct, RTGICache::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBufRO, RTGICache::GEO_DATA_BUF_SLOT, geo_data_buf},
        {Ren::eBindTarget::SBufRO, RTGICache::MATERIAL_BUF_SLOT, materials_buf},
        {Ren::eBindTarget::SBufRO, RTGICache::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::SBufRO, RTGICache::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::SBufRO, RTGICache::LIGHTS_BUF_SLOT, lights_buf},
        {Ren::eBindTarget::TexSampled, RTGICache::SHADOW_DEPTH_TEX_SLOT, shadow_depth_tex},
        {Ren::eBindTarget::TexSampled, RTGICache::SHADOW_COLOR_TEX_SLOT, shadow_color_tex},
        {Ren::eBindTarget::TexSampled, RTGICache::LTC_LUTS_TEX_SLOT, ltc_luts_tex},
        {Ren::eBindTarget::UTBuf, RTGICache::CELLS_BUF_SLOT, cells_buf},
        {Ren::eBindTarget::UTBuf, RTGICache::ITEMS_BUF_SLOT, items_buf},
        {Ren::eBindTarget::TexSampled, RTGICache::IRRADIANCE_TEX_SLOT, irr_tex},
        {Ren::eBindTarget::TexSampled, RTGICache::DISTANCE_TEX_SLOT, dist_tex},
        {Ren::eBindTarget::TexSampled, RTGICache::OFFSET_TEX_SLOT, off_tex},
        {Ren::eBindTarget::ImageRW, RTGICache::OUT_RAY_DATA_IMG_SLOT, out_ray_data_tex}};
    if (stoch_lights_buf) {
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::RANDOM_SEQ_BUF_SLOT, *random_seq_buf);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::STOCH_LIGHTS_BUF_SLOT, *stoch_lights_buf);
        bindings.emplace_back(Ren::eBindTarget::UTBuf, RTGICache::LIGHT_NODES_BUF_SLOT, *light_nodes_buf);
    }

    const Ren::Pipeline &pi = *pi_rt_gi_cache_[stoch_lights_buf != nullptr][args_->partial_update];

    VkDescriptorSet descr_sets[2];
    descr_sets[0] =
        PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTGICache::Params uniform_params = {};
    uniform_params.volume_index = view_state_->volume_to_update;
    uniform_params.stoch_lights_count = view_state_->stochastic_lights_count_cache;
    uniform_params.pass_hash = view_state_->probe_ray_hash;
    uniform_params.oct_index = (args_->probe_volumes[view_state_->volume_to_update].updates_count - 1) % 8;
    uniform_params.grid_origin = Ren::Vec4f(args_->probe_volumes[view_state_->volume_to_update].origin[0],
                                            args_->probe_volumes[view_state_->volume_to_update].origin[1],
                                            args_->probe_volumes[view_state_->volume_to_update].origin[2], 0.0f);
    uniform_params.grid_scroll = Ren::Vec4i(args_->probe_volumes[view_state_->volume_to_update].scroll[0],
                                            args_->probe_volumes[view_state_->volume_to_update].scroll[1],
                                            args_->probe_volumes[view_state_->volume_to_update].scroll[2], 0.0f);
    uniform_params.grid_scroll_diff = Ren::Vec4i(args_->probe_volumes[view_state_->volume_to_update].scroll_diff[0],
                                                 args_->probe_volumes[view_state_->volume_to_update].scroll_diff[1],
                                                 args_->probe_volumes[view_state_->volume_to_update].scroll_diff[2], 0);
    uniform_params.grid_spacing = Ren::Vec4f(args_->probe_volumes[view_state_->volume_to_update].spacing[0],
                                             args_->probe_volumes[view_state_->volume_to_update].spacing[1],
                                             args_->probe_volumes[view_state_->volume_to_update].spacing[2], 0.0f);
    uniform_params.quat_rot = view_state_->probe_ray_rotator;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatch(cmd_buf, (PROBE_TOTAL_RAYS_COUNT / RTGICache::GRP_SIZE_X),
                           PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y);
}
