#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/sample_lights_interface.h"

void Eng::ExSampleLights::Execute_HWRT(FgBuilder &builder) {
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocBuf &random_seq_buf = builder.GetReadBuffer(args_->random_seq);

    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    [[maybe_unused]] FgAllocBuf &rt_tlas_buf = builder.GetReadBuffer(args_->tlas_buf);

    FgAllocTex &albedo_tex = builder.GetReadTexture(args_->albedo_tex);
    FgAllocTex &depth_tex = builder.GetReadTexture(args_->depth_tex);
    FgAllocTex &norm_tex = builder.GetReadTexture(args_->norm_tex);
    FgAllocTex &spec_tex = builder.GetReadTexture(args_->spec_tex);

    FgAllocTex &out_diffuse_tex = builder.GetWriteTexture(args_->out_diffuse_tex);
    FgAllocTex &out_specular_tex = builder.GetWriteTexture(args_->out_specular_tex);

    if (!args_->lights_buf) {
        return;
    }

    FgAllocBuf &lights_buf = builder.GetReadBuffer(args_->lights_buf);
    FgAllocBuf &nodes_buf = builder.GetReadBuffer(args_->nodes_buf);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

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

    const Ren::Vec3u grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SampleLights::LOCAL_GROUP_SIZE_X - 1u) / SampleLights::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SampleLights::LOCAL_GROUP_SIZE_Y - 1u) / SampleLights::LOCAL_GROUP_SIZE_Y, 1u};

    SampleLights::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.lights_count = view_state_->stochastic_lights_count;
    uniform_params.frame_index = view_state_->frame_index;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_sample_lights_->prog()->descr_set_layouts()[0], bindings,
                                         ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_sample_lights_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_sample_lights_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    api_ctx->vkCmdPushConstants(cmd_buf, pi_sample_lights_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);
    api_ctx->vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}

void Eng::ExSampleLights::Execute_SWRT(FgBuilder &builder) {
    FgAllocBuf &unif_sh_data_buf = builder.GetReadBuffer(args_->shared_data);
    FgAllocBuf &random_seq_buf = builder.GetReadBuffer(args_->random_seq);

    FgAllocBuf &geo_data_buf = builder.GetReadBuffer(args_->geo_data);
    FgAllocBuf &materials_buf = builder.GetReadBuffer(args_->materials);
    FgAllocBuf &vtx_buf1 = builder.GetReadBuffer(args_->vtx_buf1);
    FgAllocBuf &ndx_buf = builder.GetReadBuffer(args_->ndx_buf);
    FgAllocBuf &rt_blas_buf = builder.GetReadBuffer(args_->swrt.rt_blas_buf);

    FgAllocBuf &rt_tlas_buf = builder.GetReadBuffer(args_->tlas_buf);
    FgAllocBuf &prim_ndx_buf = builder.GetReadBuffer(args_->swrt.prim_ndx_buf);
    FgAllocBuf &mesh_instances_buf = builder.GetReadBuffer(args_->swrt.mesh_instances_buf);

    FgAllocTex &albedo_tex = builder.GetReadTexture(args_->albedo_tex);
    FgAllocTex &depth_tex = builder.GetReadTexture(args_->depth_tex);
    FgAllocTex &norm_tex = builder.GetReadTexture(args_->norm_tex);
    FgAllocTex &spec_tex = builder.GetReadTexture(args_->spec_tex);

    FgAllocTex &out_diffuse_tex = builder.GetWriteTexture(args_->out_diffuse_tex);
    FgAllocTex &out_specular_tex = builder.GetWriteTexture(args_->out_specular_tex);

    if (!args_->lights_buf) {
        return;
    }

    FgAllocBuf &lights_buf = builder.GetReadBuffer(args_->lights_buf);
    FgAllocBuf &nodes_buf = builder.GetReadBuffer(args_->nodes_buf);

    Ren::Context &ctx = builder.ctx();
    Ren::ApiContext *api_ctx = ctx.api_ctx();

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, *unif_sh_data_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::RANDOM_SEQ_BUF_SLOT, *random_seq_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHTS_BUF_SLOT, *lights_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHT_NODES_BUF_SLOT, *nodes_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::BLAS_BUF_SLOT, *rt_blas_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::TLAS_BUF_SLOT, *rt_tlas_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::PRIM_NDX_BUF_SLOT, *prim_ndx_buf.ref},
        {Ren::eBindTarget::UTBuf, SampleLights::MESH_INSTANCES_BUF_SLOT, *mesh_instances_buf.ref},
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

    const Ren::Vec3u grp_count = Ren::Vec3u{
        (view_state_->act_res[0] + SampleLights::LOCAL_GROUP_SIZE_X - 1u) / SampleLights::LOCAL_GROUP_SIZE_X,
        (view_state_->act_res[1] + SampleLights::LOCAL_GROUP_SIZE_Y - 1u) / SampleLights::LOCAL_GROUP_SIZE_Y, 1u};

    SampleLights::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{uint32_t(view_state_->act_res[0]), uint32_t(view_state_->act_res[1])};
    uniform_params.lights_count = view_state_->stochastic_lights_count;
    uniform_params.frame_index = view_state_->frame_index;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_sample_lights_->prog()->descr_set_layouts()[0], bindings,
                                         ctx.default_descr_alloc(), ctx.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures_descr_set;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_sample_lights_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_sample_lights_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    api_ctx->vkCmdPushConstants(cmd_buf, pi_sample_lights_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);
    api_ctx->vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}
