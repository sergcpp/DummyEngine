#include "ExRTDiffuse.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/Vk/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/rt_diffuse_interface.h"

void Eng::ExRTDiffuse::Execute_HWRT(const FgContext &fg) {
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::ImageROHandle tcbn = fg.AccessROImage(args_->tcbn);
    const Ren::ImageROHandle depth = fg.AccessROImage(args_->depth);
    const Ren::ImageROHandle normal = fg.AccessROImage(args_->normal);
    const Ren::BufferROHandle ray_list = fg.AccessROBuffer(args_->ray_list);
    const Ren::BufferROHandle indir_args = fg.AccessROBuffer(args_->indir_args);
    [[maybe_unused]] const Ren::BufferROHandle rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::BufferROHandle cache_entries = fg.AccessROBuffer(args_->cache_entries);
    const Ren::BufferROHandle cache_voxels = fg.AccessROBuffer(args_->cache_voxels);

    const Ren::ImageRWHandle out_color = fg.AccessRWImage(args_->out_color);
    const Ren::BufferRWHandle inout_ray_counter = fg.AccessRWBuffer(args_->inout_ray_counter);
    const Ren::BufferRWHandle out_ray_hits = fg.AccessRWBuffer(args_->out_ray_hits);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    VkCommandBuffer cmd_buf = api.draw_cmd_buf[api.backend_frame];

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                     {Ren::eBindTarget::TexSampled, RTDiffuse::DEPTH_TEX_SLOT, {depth, 1}},
                                     {Ren::eBindTarget::TexSampled, RTDiffuse::NORM_TEX_SLOT, normal},
                                     {Ren::eBindTarget::TexSampled, RTDiffuse::TCBN_2D_TEX_SLOT, tcbn},
                                     {Ren::eBindTarget::SBufRO, RTDiffuse::RAY_LIST_SLOT, ray_list},
                                     {Ren::eBindTarget::AccStruct, RTDiffuse::TLAS_SLOT, args_->tlas},
                                     {Ren::eBindTarget::SBufRO, RTDiffuse::GEO_DATA_BUF_SLOT, geo_data},
                                     {Ren::eBindTarget::SBufRO, RTDiffuse::MATERIAL_BUF_SLOT, materials},
                                     {Ren::eBindTarget::SBufRO, RTDiffuse::VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::SBufRO, RTDiffuse::NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::SBufRO, RTDiffuse::CACHE_ENTRIES_BUF_SLOT, cache_entries},
                                     {Ren::eBindTarget::SBufRO, RTDiffuse::CACHE_VOXELS_BUF_SLOT, cache_voxels},
                                     {Ren::eBindTarget::ImageRW, RTDiffuse::OUT_GI_IMG_SLOT, out_color},
                                     {Ren::eBindTarget::SBufRW, RTDiffuse::RAY_COUNTER_SLOT, inout_ray_counter},
                                     {Ren::eBindTarget::SBufRW, RTDiffuse::OUT_RAY_HITS_BUF_SLOT, out_ray_hits}};

    const Ren::PipelineMain &pi = storages.pipelines[pi_rt_diffuse_].first;
    const Ren::ProgramMain &pr = storages.programs[pi.prog].first;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api, storages, pr.descr_set_layouts[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.pipeline);
    api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout, 0, 2, descr_sets, 0, nullptr);

    RTDiffuse::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = (view_state_->frame_index % 256);
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    api.vkCmdPushConstants(cmd_buf, pi.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params), &uniform_params);

    const auto &[indir_args_main, indir_args_cold] = storages.buffers[indir_args];
    api.vkCmdDispatchIndirect(cmd_buf, indir_args_main.buf, VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}
