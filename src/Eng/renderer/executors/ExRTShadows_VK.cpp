#include "ExRTShadows.h"

#include <Ren/Context.h>
#include <Ren/Image.h>
#include <Ren/RastState.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/rt_shadows_interface.h"

void Eng::ExRTShadows::Execute_HWRT(FgContext &fg) {
    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Image &noise_tex = fg.AccessROImage(args_->noise_tex);
    const Ren::Image &depth_tex = fg.AccessROImage(args_->depth_tex);
    const Ren::Image &normal_tex = fg.AccessROImage(args_->normal_tex);
    [[maybe_unused]] const Ren::Buffer &tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::Buffer &tile_list_buf = fg.AccessROBuffer(args_->tile_list_buf);
    const Ren::Buffer &indir_args_buf = fg.AccessROBuffer(args_->indir_args);

    Ren::Image &out_shadow_tex = fg.AccessRWImage(args_->out_shadow_tex);

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<const Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                     {Ren::eBindTarget::TexSampled, RTShadows::NOISE_TEX_SLOT, noise_tex},
                                     {Ren::eBindTarget::TexSampled, RTShadows::DEPTH_TEX_SLOT, {depth_tex, 1}},
                                     {Ren::eBindTarget::TexSampled, RTShadows::NORM_TEX_SLOT, normal_tex},
                                     {Ren::eBindTarget::AccStruct, RTShadows::TLAS_SLOT, *acc_struct},
                                     {Ren::eBindTarget::SBufRO, RTShadows::GEO_DATA_BUF_SLOT, geo_data_buf},
                                     {Ren::eBindTarget::SBufRO, RTShadows::MATERIAL_BUF_SLOT, materials_buf},
                                     {Ren::eBindTarget::SBufRO, RTShadows::VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::SBufRO, RTShadows::NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::SBufRO, RTShadows::TILE_LIST_SLOT, tile_list_buf},
                                     {Ren::eBindTarget::ImageRW, RTShadows::OUT_SHADOW_IMG_SLOT, out_shadow_tex}};

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_rt_shadows_->prog()->descr_set_layouts()[0], bindings,
                                         fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_rt_shadows_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_rt_shadows_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    RTShadows::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;

    api_ctx->vkCmdPushConstants(cmd_buf, pi_rt_shadows_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.vk_handle(), 0);
}
