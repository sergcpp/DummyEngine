#include "ExRTShadows.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/Vk/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/rt_shadows_interface.h"

void Eng::ExRTShadows::Execute_HWRT(const FgContext &fg) {
    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::ImageROHandle tcbn = fg.AccessROImage(args_->tcbn);
    const Ren::ImageROHandle depth = fg.AccessROImage(args_->depth);
    const Ren::ImageROHandle normal = fg.AccessROImage(args_->normal);
    [[maybe_unused]] const Ren::BufferROHandle tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::BufferROHandle tile_list = fg.AccessROBuffer(args_->tile_list);
    const Ren::BufferROHandle indir_args = fg.AccessROBuffer(args_->indir_args);

    const Ren::ImageRWHandle out_shadow = fg.AccessRWImage(args_->out_shadow);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    VkCommandBuffer cmd_buf = fg.cmd_buf();

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                     {Ren::eBindTarget::TexSampled, RTShadows::TCBN_TEX_SLOT, tcbn},
                                     {Ren::eBindTarget::TexSampled, RTShadows::DEPTH_TEX_SLOT, {depth, 1}},
                                     {Ren::eBindTarget::TexSampled, RTShadows::NORM_TEX_SLOT, normal},
                                     {Ren::eBindTarget::AccStruct, RTShadows::TLAS_SLOT, args_->tlas},
                                     {Ren::eBindTarget::SBufRO, RTShadows::GEO_DATA_BUF_SLOT, geo_data},
                                     {Ren::eBindTarget::SBufRO, RTShadows::MATERIAL_BUF_SLOT, materials},
                                     {Ren::eBindTarget::SBufRO, RTShadows::VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::SBufRO, RTShadows::NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::SBufRO, RTShadows::TILE_LIST_SLOT, tile_list},
                                     {Ren::eBindTarget::ImageRW, RTShadows::OUT_SHADOW_IMG_SLOT, out_shadow}};

    const Ren::PipelineMain &pi = storages.pipelines[pi_rt_shadows_].first;
    const Ren::ProgramMain &pr = storages.programs[pi.prog].first;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api, storages, pr.descr_set_layouts[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.pipeline);
    api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout, 0, 2, descr_sets, 0, nullptr);

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = (view_state_->frame_index % 256);

    api.vkCmdPushConstants(cmd_buf, pi.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params), &uniform_params);

    const Ren::BufferMain &indir_args_buf_main = storages.buffers[indir_args].first;
    api.vkCmdDispatchIndirect(cmd_buf, indir_args_buf_main.buf, 0);
}
