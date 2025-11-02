#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/sample_lights_interface.h"

void Eng::ExSampleLights::Execute_HWRT(FgContext &ctx) {
    FgAllocBuf &unif_sh_data_buf = ctx.AccessROBuffer(args_->shared_data);
    FgAllocBuf &random_seq_buf = ctx.AccessROBuffer(args_->random_seq);

    FgAllocBuf &geo_data_buf = ctx.AccessROBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = ctx.AccessROBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = ctx.AccessROBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = ctx.AccessROBuffer(args_->ndx_buf);
    [[maybe_unused]] FgAllocBuf &rt_tlas_buf = ctx.AccessROBuffer(args_->tlas_buf);

    FgAllocTex &albedo_tex = ctx.AccessROTexture(args_->albedo_tex);
    FgAllocTex &depth_tex = ctx.AccessROTexture(args_->depth_tex);
    FgAllocTex &norm_tex = ctx.AccessROTexture(args_->norm_tex);
    FgAllocTex &spec_tex = ctx.AccessROTexture(args_->spec_tex);

    FgAllocTex &out_diffuse_tex = ctx.AccessRWTexture(args_->out_diffuse_tex);
    FgAllocTex &out_specular_tex = ctx.AccessRWTexture(args_->out_specular_tex);

    if (!args_->lights_buf) {
        return;
    }

    FgAllocBuf &lights_buf = ctx.AccessROBuffer(args_->lights_buf);
    FgAllocBuf &nodes_buf = ctx.AccessROBuffer(args_->nodes_buf);

    Ren::ApiContext *api_ctx = ctx.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(args_->tlas);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::RANDOM_SEQ_BUF_SLOT, *random_seq_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHT_NODES_BUF_SLOT, *nodes_buf.ref},
        {Ren::eBindTarget::AccStruct, SampleLights::TLAS_SLOT, *acc_struct},
        {Ren::eBindTarget::SBufRO, SampleLights::GEO_DATA_BUF_SLOT, *geo_data_buf.ref},
        {Ren::eBindTarget::SBufRO, SampleLights::MATERIAL_BUF_SLOT, *materials_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::VTX_BUF1_SLOT, *vtx_buf1.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::NDX_BUF_SLOT, *ndx_buf.ref},
        {Ren::eBindTarget::TexSampled, SampleLights::ALBEDO_TEX_SLOT, *albedo_tex.ref},
        {Ren::eBindTarget::TexSampled, SampleLights::DEPTH_TEX_SLOT, {*depth_tex.ref, 1}},
        {Ren::eBindTarget::TexSampled, SampleLights::NORM_TEX_SLOT, *norm_tex.ref},
        {Ren::eBindTarget::TexSampled, SampleLights::SPEC_TEX_SLOT, *spec_tex.ref},
        {Ren::eBindTarget::ImageRW, SampleLights::OUT_DIFFUSE_IMG_SLOT, *out_diffuse_tex.ref},
        {Ren::eBindTarget::ImageRW, SampleLights::OUT_SPECULAR_IMG_SLOT, *out_specular_tex.ref}};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{(view_state_->ren_res[0] + SampleLights::GRP_SIZE_X - 1u) / SampleLights::GRP_SIZE_X,
                   (view_state_->ren_res[1] + SampleLights::GRP_SIZE_Y - 1u) / SampleLights::GRP_SIZE_Y, 1u};

    SampleLights::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.lights_count = view_state_->stochastic_lights_count;
    uniform_params.frame_index = view_state_->frame_index;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_sample_lights_->prog()->descr_set_layouts()[0], bindings,
                                         ctx.descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_sample_lights_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_sample_lights_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    api_ctx->vkCmdPushConstants(cmd_buf, pi_sample_lights_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);
    api_ctx->vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}
