#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/RastState.h>
#include <Ren/Texture.h>
#include <Ren/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../PrimDraw.h"
#include "../Renderer_Structs.h"
#include "../shaders/sample_lights_interface.h"

void Eng::ExSampleLights::Execute_HWRT(FgContext &fg) {
    using namespace SampleLights;

    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Buffer &random_seq_buf = fg.AccessROBuffer(args_->random_seq);

    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    [[maybe_unused]] const Ren::Buffer &rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);

    const Ren::Texture &albedo_tex = fg.AccessROTexture(args_->albedo_tex);
    const Ren::Texture &depth_tex = fg.AccessROTexture(args_->depth_tex);
    const Ren::Texture &norm_tex = fg.AccessROTexture(args_->norm_tex);
    const Ren::Texture &spec_tex = fg.AccessROTexture(args_->spec_tex);

    Ren::Texture &out_diffuse_tex = fg.AccessRWTexture(args_->out_diffuse_tex);
    Ren::Texture &out_specular_tex = fg.AccessRWTexture(args_->out_specular_tex);

    if (!args_->lights_buf) {
        return;
    }

    const Ren::Buffer &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    const Ren::Buffer &nodes_buf = fg.AccessROBuffer(args_->nodes_buf);

    Ren::ApiContext *api_ctx = fg.ren_ctx().api_ctx();

    auto *acc_struct = static_cast<Ren::AccStructureVK *>(args_->tlas);

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
                                     {Ren::eBindTarget::UTBuf, RANDOM_SEQ_BUF_SLOT, random_seq_buf},
                                     {Ren::eBindTarget::UTBuf, LIGHTS_BUF_SLOT, lights_buf},
                                     {Ren::eBindTarget::UTBuf, LIGHT_NODES_BUF_SLOT, nodes_buf},
                                     {Ren::eBindTarget::AccStruct, TLAS_SLOT, *acc_struct},
                                     {Ren::eBindTarget::SBufRO, GEO_DATA_BUF_SLOT, geo_data_buf},
                                     {Ren::eBindTarget::SBufRO, MATERIAL_BUF_SLOT, materials_buf},
                                     {Ren::eBindTarget::UTBuf, VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::UTBuf, NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::TexSampled, ALBEDO_TEX_SLOT, albedo_tex},
                                     {Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {depth_tex, 1}},
                                     {Ren::eBindTarget::TexSampled, NORM_TEX_SLOT, norm_tex},
                                     {Ren::eBindTarget::TexSampled, SPEC_TEX_SLOT, spec_tex},
                                     {Ren::eBindTarget::ImageRW, OUT_DIFFUSE_IMG_SLOT, out_diffuse_tex},
                                     {Ren::eBindTarget::ImageRW, OUT_SPECULAR_IMG_SLOT, out_specular_tex}};

    const Ren::Vec3u grp_count = Ren::Vec3u{(view_state_->ren_res[0] + GRP_SIZE_X - 1u) / GRP_SIZE_X,
                                            (view_state_->ren_res[1] + GRP_SIZE_Y - 1u) / GRP_SIZE_Y, 1u};

    Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.lights_count = view_state_->stochastic_lights_count;
    uniform_params.frame_index = view_state_->frame_index;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api_ctx, pi_sample_lights_->prog()->descr_set_layouts()[0], bindings,
                                         fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    VkCommandBuffer cmd_buf = api_ctx->draw_cmd_buf[api_ctx->backend_frame];

    api_ctx->vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_sample_lights_->handle());
    api_ctx->vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi_sample_lights_->layout(), 0, 2,
                                     descr_sets, 0, nullptr);

    api_ctx->vkCmdPushConstants(cmd_buf, pi_sample_lights_->layout(), VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                sizeof(uniform_params), &uniform_params);
    api_ctx->vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}
