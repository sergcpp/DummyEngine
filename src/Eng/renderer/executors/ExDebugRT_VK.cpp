#include "ExDebugRT.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/Vk/VKCtx.h>

#include "../framegraph/FgBuilder.h"
#include "../shaders/rt_debug_interface.h"

void Eng::ExDebugRT::Execute_HWRT(const FgContext &fg) {
    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle vtx_buf2 = fg.AccessROBuffer(args_->vtx_buf2);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle env = fg.AccessROImage(args_->env);
    const Ren::ImageROHandle shadow_depth = fg.AccessROImage(args_->shadow_depth);
    const Ren::ImageROHandle shadow_color = fg.AccessROImage(args_->shadow_color);
    const Ren::ImageROHandle ltc_luts = fg.AccessROImage(args_->ltc_luts);
    const Ren::BufferROHandle cells = fg.AccessROBuffer(args_->cells);
    const Ren::BufferROHandle items = fg.AccessROBuffer(args_->items);

    const Ren::ImageROHandle irr = fg.AccessROImage(args_->irradiance);
    const Ren::ImageROHandle dist = fg.AccessROImage(args_->distance);
    const Ren::ImageROHandle off = fg.AccessROImage(args_->offset);

    const Ren::BufferROHandle cache_entries = fg.AccessROBuffer(args_->cache_entries);
    const Ren::BufferROHandle cache_voxels = fg.AccessROBuffer(args_->cache_voxels);

    const Ren::ImageRWHandle output = fg.AccessRWImage(args_->output);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    VkCommandBuffer cmd_buf = fg.cmd_buf();

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                     {Ren::eBindTarget::AccStruct, RTDebug::TLAS_SLOT, args_->tlas},
                                     {Ren::eBindTarget::TexSampled, RTDebug::ENV_TEX_SLOT, env},
                                     {Ren::eBindTarget::SBufRO, RTDebug::GEO_DATA_BUF_SLOT, geo_data},
                                     {Ren::eBindTarget::SBufRO, RTDebug::MATERIAL_BUF_SLOT, materials},
                                     {Ren::eBindTarget::SBufRO, RTDebug::VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::SBufRO, RTDebug::VTX_BUF2_SLOT, vtx_buf2},
                                     {Ren::eBindTarget::SBufRO, RTDebug::NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::SBufRO, RTDebug::LIGHTS_BUF_SLOT, lights},
                                     {Ren::eBindTarget::UTBuf, RTDebug::CELLS_BUF_SLOT, cells},
                                     {Ren::eBindTarget::UTBuf, RTDebug::ITEMS_BUF_SLOT, items},
                                     {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_DEPTH_TEX_SLOT, shadow_depth},
                                     {Ren::eBindTarget::TexSampled, RTDebug::SHADOW_COLOR_TEX_SLOT, shadow_color},
                                     {Ren::eBindTarget::TexSampled, RTDebug::LTC_LUTS_TEX_SLOT, ltc_luts},
                                     {Ren::eBindTarget::TexSampled, RTDebug::IRRADIANCE_TEX_SLOT, irr},
                                     {Ren::eBindTarget::TexSampled, RTDebug::DISTANCE_TEX_SLOT, dist},
                                     {Ren::eBindTarget::TexSampled, RTDebug::OFFSET_TEX_SLOT, off},
                                     {Ren::eBindTarget::SBufRO, RTDebug::CACHE_ENTRIES_BUF_SLOT, cache_entries},
                                     {Ren::eBindTarget::SBufRO, RTDebug::CACHE_VOXELS_BUF_SLOT, cache_voxels},
                                     {Ren::eBindTarget::ImageRW, RTDebug::OUT_IMG_SLOT, output}};

    const auto &[pi_main, pi_cold] = storages.pipelines[pi_debug_];
    const Ren::ProgramMain &pr = storages.programs[pi_main.prog].first;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api, storages, pr.descr_set_layouts[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_textures.descr_set;

    api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_main.pipeline);
    api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pi_main.layout, 0, 2, descr_sets, 0,
                                nullptr);

    RTDebug::Params uniform_params;
    uniform_params.img_size[0] = view_state_->ren_res[0];
    uniform_params.img_size[1] = view_state_->ren_res[1];
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.cull_mask = args_->cull_mask;

    api.vkCmdPushConstants(cmd_buf, pi_main.layout, pr.pc_ranges[0].stageFlags, 0, sizeof(uniform_params),
                           &uniform_params);

    api.vkCmdTraceRaysKHR(cmd_buf, &pi_cold.rgen_region, &pi_cold.miss_region, &pi_cold.hit_region,
                          &pi_cold.call_region, uint32_t(view_state_->ren_res[0]), uint32_t(view_state_->ren_res[1]),
                          1);
}
