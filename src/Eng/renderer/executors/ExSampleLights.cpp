#include "ExSampleLights.h"

#include <Ren/Context.h>
#include <Ren/DrawCall.h>

#include "../../utils/ShaderLoader.h"
#include "../shaders/sample_lights_interface.h"

void Eng::ExSampleLights::Execute(FgContext &fg) {
    LazyInit(fg.ren_ctx(), fg.sh());
    if (fg.ren_ctx().capabilities.hwrt) {
        Execute_HWRT(fg);
    } else {
        Execute_SWRT(fg);
    }
}

void Eng::ExSampleLights::LazyInit(Ren::Context &ctx, Eng::ShaderLoader &sh) {
    if (!initialized_) {
        auto subgroup_select = [&ctx](std::string_view subgroup_shader, std::string_view nosubgroup_shader) {
            return ctx.capabilities.subgroup ? subgroup_shader : nosubgroup_shader;
        };

        if (ctx.capabilities.hwrt) {
            pi_sample_lights_ = sh.LoadPipeline(subgroup_select("internal/sample_lights@HWRT.comp.glsl",
                                                                "internal/sample_lights@HWRT;NO_SUBGROUP.comp.glsl"));
        } else {
            pi_sample_lights_ = sh.LoadPipeline(
                subgroup_select("internal/sample_lights.comp.glsl", "internal/sample_lights@NO_SUBGROUP.comp.glsl"));
        }
        initialized_ = true;
    }
}

void Eng::ExSampleLights::Execute_SWRT(FgContext &fg) {
    const Ren::Buffer &unif_sh_data_buf = fg.AccessROBuffer(args_->shared_data);
    const Ren::Buffer &random_seq_buf = fg.AccessROBuffer(args_->random_seq);

    const Ren::Buffer &geo_data_buf = fg.AccessROBuffer(args_->geo_data);
    const Ren::Buffer &materials_buf = fg.AccessROBuffer(args_->materials);
    const Ren::Buffer &vtx_buf1 = fg.AccessROBuffer(args_->vtx_buf1);
    const Ren::Buffer &ndx_buf = fg.AccessROBuffer(args_->ndx_buf);
    const Ren::Buffer &rt_blas_buf = fg.AccessROBuffer(args_->swrt.rt_blas_buf);

    const Ren::Buffer &rt_tlas_buf = fg.AccessROBuffer(args_->tlas_buf);
    const Ren::Buffer &prim_ndx_buf = fg.AccessROBuffer(args_->swrt.prim_ndx_buf);
    const Ren::Buffer &mesh_instances_buf = fg.AccessROBuffer(args_->swrt.mesh_instances_buf);

    const Ren::Image &albedo_tex = fg.AccessROImage(args_->albedo_tex);
    const Ren::Image &depth_tex = fg.AccessROImage(args_->depth_tex);
    const Ren::Image &norm_tex = fg.AccessROImage(args_->norm_tex);
    const Ren::Image &spec_tex = fg.AccessROImage(args_->spec_tex);

    Ren::Image &out_diffuse_tex = fg.AccessRWImage(args_->out_diffuse_tex);
    Ren::Image &out_specular_tex = fg.AccessRWImage(args_->out_specular_tex);

    if (!args_->lights_buf) {
        return;
    }

    const Ren::Buffer &lights_buf = fg.AccessROBuffer(args_->lights_buf);
    const Ren::Buffer &nodes_buf = fg.AccessROBuffer(args_->nodes_buf);

    const Ren::Binding bindings[] = {
        {Ren::eBindTarget::UBuf, BIND_UB_SHARED_DATA_BUF, unif_sh_data_buf},
        {Ren::eBindTarget::BindlessDescriptors, BIND_BINDLESS_TEX, bindless_tex_->rt_inline_textures},
        {Ren::eBindTarget::UTBuf, SampleLights::RANDOM_SEQ_BUF_SLOT, random_seq_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHTS_BUF_SLOT, lights_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::LIGHT_NODES_BUF_SLOT, nodes_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::BLAS_BUF_SLOT, rt_blas_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::TLAS_BUF_SLOT, rt_tlas_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::PRIM_NDX_BUF_SLOT, prim_ndx_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::MESH_INSTANCES_BUF_SLOT, mesh_instances_buf},
        {Ren::eBindTarget::SBufRO, SampleLights::GEO_DATA_BUF_SLOT, geo_data_buf},
        {Ren::eBindTarget::SBufRO, SampleLights::MATERIAL_BUF_SLOT, materials_buf},
        {Ren::eBindTarget::UTBuf, SampleLights::VTX_BUF1_SLOT, vtx_buf1},
        {Ren::eBindTarget::UTBuf, SampleLights::NDX_BUF_SLOT, ndx_buf},
        {Ren::eBindTarget::TexSampled, SampleLights::ALBEDO_TEX_SLOT, albedo_tex},
        {Ren::eBindTarget::TexSampled, SampleLights::DEPTH_TEX_SLOT, {depth_tex, 1}},
        {Ren::eBindTarget::TexSampled, SampleLights::NORM_TEX_SLOT, norm_tex},
        {Ren::eBindTarget::TexSampled, SampleLights::SPEC_TEX_SLOT, spec_tex},
        {Ren::eBindTarget::ImageRW, SampleLights::OUT_DIFFUSE_IMG_SLOT, out_diffuse_tex},
        {Ren::eBindTarget::ImageRW, SampleLights::OUT_SPECULAR_IMG_SLOT, out_specular_tex}};

    const Ren::Vec3u grp_count =
        Ren::Vec3u{(view_state_->ren_res[0] + SampleLights::GRP_SIZE_X - 1u) / SampleLights::GRP_SIZE_X,
                   (view_state_->ren_res[1] + SampleLights::GRP_SIZE_Y - 1u) / SampleLights::GRP_SIZE_Y, 1u};

    SampleLights::Params uniform_params;
    uniform_params.img_size = Ren::Vec2u{view_state_->ren_res};
    uniform_params.lights_count = uint32_t(lights_buf.size() / sizeof(light_item_t));
    uniform_params.frame_index = view_state_->frame_index;

    DispatchCompute(*pi_sample_lights_, grp_count, bindings, &uniform_params, sizeof(uniform_params), fg.descr_alloc(),
                    fg.log());
}

#if defined(REN_GL_BACKEND)
void Eng::ExSampleLights::Execute_HWRT(FgContext &fg) { assert(false && "Not implemented!"); }
#endif