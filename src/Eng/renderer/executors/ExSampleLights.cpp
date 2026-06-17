#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../Renderer_DrawList.h"
#include "../framegraph/FgBuilder.h"
#include "../shaders/sample_lights_interface.h"

void Eng::ExSampleLights::Execute(const FgContext &fg) {
    LazyInit(fg);
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExSampleLights::LazyInit(const FgContext &fg) {
    auto &ctx = fg.ren_ctx();
    auto &sh = fg.sh();
    if (!initialized_) {
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        if (ctx.capabilities.hwrt) {
            pi_sample_lights_ = sh.FindOrCreatePipeline(subgroup_select(
                "internal/sample_lights@HWRT.comp.glsl", "internal/sample_lights@HWRT;NO_SUBGROUP.comp.glsl"));
        } else {
            pi_sample_lights_ = sh.FindOrCreatePipeline(
                subgroup_select("internal/sample_lights.comp.glsl", "internal/sample_lights@NO_SUBGROUP.comp.glsl"));
        }
        initialized_ = true;
    }
}

void Eng::ExSampleLights::Execute_SWRT(const FgContext &fg) {
    const Ren::BufferROHandle unif_sh_data = fg.AccessROBuffer(args_->shared_data);
    const Ren::BufferROHandle random_seq = fg.AccessROBuffer(args_->random_seq);

    const Ren::BufferROHandle geo_data = fg.AccessROBuffer(args_->geo_data);
    const Ren::BufferROHandle materials = fg.AccessROBuffer(args_->materials);
    const Ren::BufferROHandle vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::BufferROHandle ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::BufferROHandle rt_blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas_buf);

    const Ren::BufferROHandle rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::BufferROHandle prim_ndx = fg.AccessROBuffer(args_->swrt.prim_ndx);
    const Ren::BufferROHandle mesh_instances = fg.AccessROBuffer(args_->swrt.mesh_instances);

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

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::UTBuf, SampleLights::RANDOM_SEQ_BUF_SLOT, random_seq},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHTS_BUF_SLOT, lights},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHT_NODES_BUF_SLOT, nodes},
        {Ren::eBindTarget::UTBuf, SampleLights::BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::PRIM_NDX_BUF_SLOT, prim_ndx},
        {Ren::eBindTarget::UTBuf, SampleLights::MESH_INSTANCES_BUF_SLOT, mesh_instances},
        {Ren::eBindTarget::SBufRO, SampleLights::GEO_DATA_BUF_SLOT, geo_data},
        {Ren::eBindTarget::SBufRO, SampleLights::MATERIAL_BUF_SLOT, materials},
        {Ren::eBindTarget::UTBuf, SampleLights::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, SampleLights::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::TexSampled, SampleLights::ALBEDO_TEX_SLOT, albedo},
        {Ren::eBindTarget::TexSampled, SampleLights::DEPTH_TEX_SLOT, {depth, 1}},
        {Ren::eBindTarget::TexSampled, SampleLights::NORM_TEX_SLOT, norm},
        {Ren::eBindTarget::TexSampled, SampleLights::SPEC_TEX_SLOT, spec},
        {Ren::eBindTarget::ImageRW, SampleLights::OUT_DIFFUSE_IMG_SLOT, out_diffuse},
        {Ren::eBindTarget::ImageRW, SampleLights::OUT_SPECULAR_IMG_SLOT, out_specular}};

    const auto grp_count = Ren::Vec3u(Ren::DivCeil(view_state_->ren_res[0], SampleLights::GRP_SIZE_X),
                                      Ren::DivCeil(view_state_->ren_res[1], SampleLights::GRP_SIZE_Y), 1u);

    // TODO: Avoid accessing cold data
    const Ren::BufferCold &lights_cold = fg.storages().buffers[lights].second;

    SampleLights::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.lights_count = uint32_t(lights_cold.size / sizeof(light_item_t));
    uniform_params.frame_index = view_state_->frame_index;

    DispatchCompute(fg.cmd_buf(), pi_sample_lights_, fg.storages(), grp_count, bindings, &uniform_params,
                    sizeof(uniform_params), fg.descr_alloc(), fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExSampleLights::Execute_HWRT(const FgContext &fg) { assert(false && "Not implemented!"); }
#endif