#include "ExRTGICache.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/Vk/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/rt_gi_cache_interface.h"

void Eng::ExRTGICache::Execute_HWRT(const FgContext &fg) {
    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    [[maybe_unused]] const Ren::BufferROHandle tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::ImageROHandle off = fg.AccessROImage(args_->offset);

    const Ren::BufferRWHandle out_ray_hits = fg.AccessRWBuffer(args_->out_ray_hits);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    VkCommandBuffer cmd_buf = fg.cmd_buf();

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                     {Ren::eBindTarget::AccStruct, RTGICache::TLAS_SLOT, args_->tlas},
                                     {Ren::eBindTarget::SBufRO, RTGICache::GEO_DATA_BUF_SLOT, geo_data},
                                     {Ren::eBindTarget::SBufRO, RTGICache::MATERIAL_BUF_SLOT, materials},
                                     {Ren::eBindTarget::SBufRO, RTGICache::VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::SBufRO, RTGICache::NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::TexSampled, RTGICache::OFFSET_TEX_SLOT, off},
                                     {Ren::eBindTarget::SBufRW, RTGICache::OUT_RAY_HITS_BUF_SLOT, out_ray_hits}};

    const Ren::PipelineMain &pi = storages.pipelines[pi_rt_gi_cache_[args_->partial_update]].first;
    const Ren::ProgramMain &pr = storages.programs[pi.prog].first;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api, storages, pr.descr_set_layouts[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.pipeline);
    api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout, 0, 2, descr_sets, 0, nullptr);

    RTGICache::Params uniform_params = {};
    uniform_params.volume_index = view_state_->volume_to_update;
    uniform_params.stoch_lights_count = view_state_->stochastic_lights_count_cache;
    uniform_params.pass_hash = view_state_->probe_ray_hash;
    uniform_params.oct_index = (args_->probe_volumes[view_state_->volume_to_update].updates_count - 1) % 8;
    uniform_params.grid_origin = Ren::Vec4f{args_->probe_volumes[view_state_->volume_to_update].origin, 0.0f};
    uniform_params.grid_scroll = Ren::Vec4i{args_->probe_volumes[view_state_->volume_to_update].scroll, 0};
    uniform_params.grid_scroll_diff = Ren::Vec4i{args_->probe_volumes[view_state_->volume_to_update].scroll_diff, 0};
    uniform_params.grid_spacing = Ren::Vec4f{args_->probe_volumes[view_state_->volume_to_update].spacing, 0.0f};
    uniform_params.quat_rot = view_state_->probe_ray_rotator;

    api.vkCmdPushConstants(cmd_buf, pi.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params), &uniform_params);

    api.vkCmdDispatch(cmd_buf, (PROBE_TOTAL_RAYS_COUNT / RTGICache::GRP_SIZE_X),
                      PROBE_VOLUME_RES_X * PROBE_VOLUME_RES_Z, PROBE_VOLUME_RES_Y);
}
