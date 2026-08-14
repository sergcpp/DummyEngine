#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>
#include <Ren/Image.h>
#include <Ren/RastState.h>
#include <Ren/Vk/VKCtx.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_Structs.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/sample_lights_interface.h"

void Eng::ExSampleLights::Execute_HWRT(const FgContext &fg) {
    using namespace SampleLights;

    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);

    const Ren::ImageROHandle tcbn_1d = fg.AccessROImage(args_->tcbn_1d);
    const Ren::ImageROHandle tcbn_2d = fg.AccessROImage(args_->tcbn_2d);

    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    [[maybe_unused]] const Ren::BufferROHandle rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);

    const Ren::ImageROHandle albedo = fg.AccessROImage(args_->albedo);
    const Ren::ImageROHandle depth = fg.AccessROImage(args_->depth);
    const Ren::ImageROHandle norm = fg.AccessROImage(args_->norm);
    const Ren::ImageROHandle spec = fg.AccessROImage(args_->spec);

    const Ren::ImageRWHandle out_diffuse = fg.AccessRWImage(args_->out_diffuse);
    const Ren::ImageRWHandle out_specular = fg.AccessRWImage(args_->out_specular);

    if (!args_->lights) {
        return;
    }

    const Ren::BufferROHandle lights = fg.AccessROBuffer(args_->lights);
    const Ren::BufferROHandle nodes = fg.AccessROBuffer(args_->nodes);

    const Ren::ApiContext &api = fg.ren_ctx().api();
    const Ren::StoragesRef &storages = fg.storages();

    const Ren::Binding bindings[] = {{Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
                                     {Ren::eBindTarget::UTBuf, LIGHTS_BUF_SLOT, lights},
                                     {Ren::eBindTarget::UTBuf, LIGHT_NODES_BUF_SLOT, nodes},
                                     {Ren::eBindTarget::AccStruct, TLAS_SLOT, args_->tlas},
                                     {Ren::eBindTarget::SBufRO, GEO_DATA_BUF_SLOT, geo_data},
                                     {Ren::eBindTarget::SBufRO, MATERIAL_BUF_SLOT, materials},
                                     {Ren::eBindTarget::UTBuf, VTX_BUF1_SLOT, vtx_buf1},
                                     {Ren::eBindTarget::UTBuf, NDX_BUF_SLOT, ndx_buf},
                                     {Ren::eBindTarget::TexSampled, ALBEDO_TEX_SLOT, albedo},
                                     {Ren::eBindTarget::TexSampled, DEPTH_TEX_SLOT, {depth, 1}},
                                     {Ren::eBindTarget::TexSampled, NORM_TEX_SLOT, norm},
                                     {Ren::eBindTarget::TexSampled, SPEC_TEX_SLOT, spec},
                                     {Ren::eBindTarget::TexSampled, TCBN_1D_TEX_SLOT, tcbn_1d},
                                     {Ren::eBindTarget::TexSampled, TCBN_2D_TEX_SLOT, tcbn_2d},
                                     {Ren::eBindTarget::ImageRW, OUT_DIFFUSE_IMG_SLOT, out_diffuse},
                                     {Ren::eBindTarget::ImageRW, OUT_SPECULAR_IMG_SLOT, out_specular}};

    const auto grp_count = Ren::Vec3u(Ren::DivCeil(view_state_->ren_res[0], GRP_SIZE_X),
                                      Ren::DivCeil(view_state_->ren_res[1], GRP_SIZE_Y), 1u);

    Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.lights_count = view_state_->stochastic_lights_count;
    uniform_params.frame_index = (view_state_->frame_index % 256);

    const Ren::PipelineMain &pi = storages.pipelines[pi_sample_lights_].first;
    const Ren::ProgramMain &pr = storages.programs[pi.prog].first;

    VkDescriptorSet descr_sets[2];
    descr_sets[0] = PrepareDescriptorSet(api, storages, pr.descr_set_layouts[0], bindings, fg.descr_alloc(), fg.log());
    descr_sets[1] = bindless_tex_->rt_inline_textures.descr_set;

    VkCommandBuffer cmd_buf = fg.cmd_buf();

    api.vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.pipeline);
    api.vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_COMPUTE, pi.layout, 0, 2, descr_sets, 0, nullptr);

    api.vkCmdPushConstants(cmd_buf, pi.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uniform_params), &uniform_params);
    api.vkCmdDispatch(cmd_buf, grp_count[0], grp_count[1], grp_count[2]);
}
