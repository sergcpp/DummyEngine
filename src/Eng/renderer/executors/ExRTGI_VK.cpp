#include "ExRTGI.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../shaders/rt_gi_interface.h"

void Eng::ExRTGI::Execute_HWRT(FgContext &fg) {
    FgAllocBuf &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = fg.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    FgAllocBuf &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    FgAllocTex &noise_tex = fg.AccessROTexture(args_->noise_tex);
    FgAllocTex &depth_tex = fg.AccessROTexture(args_->depth_tex);
    FgAllocTex &normal_tex = fg.AccessROTexture(args_->normal_tex);
    FgAllocBuf &ray_counter_buf = fg.AccessROBuffer(args_->ray_counter);
    FgAllocBuf &ray_list_buf = fg.AccessROBuffer(args_->ray_list);
    FgAllocBuf &indir_args_buf = fg.AccessROBuffer(args_->indir_args);
    [[maybe_unused]] FgAllocBuf &rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);

    FgAllocBuf &out_ray_hits_buf = fg.AccessRWBuffer(args_->out_ray_hits_buf);

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(args_->tlas);

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    Ren::SmallVector<Ren::Binding, 24> bindings = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::TexSampled, RTGI::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::TexSampled, RTGI::NORM_TEX_SLOT, *normal_tex.ref},
        {Ren::eBindTarget::TexSampled, RTGI::NOISE_TEX_SLOT, *noise_tex.ref},
        {Ren::eBindTarget::SBufRW, RTGI::RAY_COUNTER_SLOT, *ray_counter_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::RAY_LIST_SLOT, *ray_list_buf.ref},
        {Ren::eBindTarget::AccStruct, RTGI::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBufRO, RTGI::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::SBufRO, RTGI::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::SBufRO, RTGI::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::SBufRW, RTGI::RAY_HITS_BUF_SLOT, *out_ray_hits_buf.ref}};

    const Ren::Pipeline &pi = *pi_rt_gi_;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] =
        PrepareDescriptorSet(api_ctx, pi.prog()->descr_set_layouts()[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout(), 0, 2, descr_sets, 0,
                                     nullptr);

    RTGI::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.pixel_spread_angle = view_state_->pixel_spread_angle;
    uniform_params.frame_index = view_state_->frame_index;
    uniform_params.lights_count = view_state_->stochastic_lights_count;

    api_ctx->vkCmdPushConstants(cmd_buf, pi.layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params),
                                &uniform_params);

    api_ctx->vkCmdDispatchIndirect(cmd_buf, indir_args_buf.ref->vk_handle(),
                                   VkDeviceSize(sizeof(VkTraceRaysIndirectCommandKHR)));
}
